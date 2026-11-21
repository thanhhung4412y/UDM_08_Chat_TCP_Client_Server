#include "videoclient.h"
#include "videorelay.h"
#include <QtTest>
#include <QDir>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTimer>

// In-process transport double. Exercises production state machines and disk I/O;
// it deliberately does not claim to exercise QTcpSocket or TCP framing.
class MemoryChannel : public JsonChannel {
public:
    std::function<void(const QJsonObject &)> route;
    bool holdAcks = false;
    int chunks = 0;
    qint64 maxFrame = 0;
    QList<QJsonObject> controls;
    explicit MemoryChannel(QTcpSocket *socket) : JsonChannel(socket) {}
    bool send(const QJsonObject &p) override {
        maxFrame = qMax(maxFrame, qint64(QJsonDocument(p).toJson(QJsonDocument::Compact).size()));
        if (p.value("type") == "video_chunk") ++chunks;
        else if (p.value("type") != "video_ack" && controls.size() < 100) controls.append(p);
        if (holdAcks && p.value("type") == "video_ack") return true;
        QTimer::singleShot(0, this, [this, p] { if (route) route(p); });
        return true;
    }
    void deliver(const QJsonObject &p) {
        QTimer::singleShot(0, this, [this, p] { emit packetReceived(p); });
    }
};
class Endpoint : public QObject {
public:
    QTcpSocket token;
    MemoryChannel channel{&token};
    QTemporaryDir root{QDir::currentPath() + "/offline-video-XXXXXX"};
    VideoClient video;
    int sent = 0;
    QStringList received, failures;
    Endpoint(int timeoutMs = 60000, const QString &directory = {})
        : video(&channel, &token, directory.isEmpty() ? root.path() : directory, nullptr, timeoutMs) {
        connect(&channel, &JsonChannel::packetReceived, &video, &VideoClient::handle);
        connect(&video, &VideoClient::completed, this, [this](const QString &path, const QString &, const QString &, bool outgoing) {
            if (outgoing) ++sent; else received.append(path);
        });
        connect(&video, &VideoClient::failed, this, [this](const QString &text) { failures.append(text); });
    }
};
class Rig : public QObject {
public:
    Endpoint alice, bob, eve;
    VideoRelay relay;
    Rig(int timeoutMs = 60000, const QString &bobDirectory = {})
        : alice(timeoutMs), bob(timeoutMs, bobDirectory), eve(timeoutMs),
          relay([this](QTcpSocket *s, const QJsonObject &p) {
              for (auto *e : {&alice, &bob, &eve})
                  if (&e->token == s) { e->channel.deliver(p); return true; }
              return false;
          }, [this](QTcpSocket *s) {
              return s == &alice.token ? QString("Alice") : s == &bob.token ? QString("Bob") : QString("Eve");
          }, [this] { return QList<QTcpSocket *>{&alice.token, &bob.token, &eve.token}; }, nullptr, timeoutMs) {
        for (auto *e : {&alice, &bob, &eve})
            e->channel.route = [this, e](const QJsonObject &p) { relay.handle(&e->token, p); };
    }
};
class OfflineTests : public QObject {
    Q_OBJECT
    static QString fixture(const QString &dir, qint64 size) {
        const QString path = QDir(dir).filePath("test.mp4");
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || !f.resize(size)) return {};
        if (size >= 12 && f.write(QByteArray::fromHex("000000186674797069736f6d")) != 12) return {};
        return path;
    }
    static QByteArray digest(const QString &path) {
        QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        while (!f.atEnd()) hash.addData(f.read(1024 * 1024));
        return hash.result();
    }
    static QJsonObject start(const QString &id, qint64 size = 12) {
        return {{"type", "video_start"}, {"id", id}, {"name", "test.mp4"}, {"to", "Bob"}, {"size", double(size)}};
    }
    static QString id() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
private slots:
    void framing() {
        JsonFramer decoder;
        QJsonObject packet;
        QVERIFY(decoder.append("{\"type\":"));
        QCOMPARE(decoder.next(packet), JsonFramer::NeedData);
        QVERIFY(decoder.append("\"chat\"}\n{\"type\":\"reply\"}\n"));
        QCOMPARE(decoder.next(packet), JsonFramer::Packet);
        QCOMPARE(packet.value("type").toString(), "chat");
        QCOMPARE(decoder.next(packet), JsonFramer::Packet);
        QCOMPARE(packet.value("type").toString(), "reply");
        QCOMPARE(decoder.next(packet), JsonFramer::NeedData);
        QVERIFY(decoder.append(QByteArray(VideoProtocol::MaxFrame + 1, 'x')));
        QCOMPARE(decoder.next(packet), JsonFramer::Error);
        QVERIFY(!decoder.append("x"));
        QVERIFY(decoder.size() <= VideoProtocol::MaxFrame + 1);
        decoder.clear(); QVERIFY(decoder.append("invalid\n"));
        QCOMPARE(decoder.next(packet), JsonFramer::Error);
        decoder.clear(); QVERIFY(decoder.append("[]\n"));
        QCOMPARE(decoder.next(packet), JsonFramer::Error);
    }
    void roundTrip_data() {
        QTest::addColumn<qint64>("size"); QTest::addColumn<QString>("to");
        QTest::newRow("minimum-header") << 12LL << QString("Bob");
        QTest::newRow("one-chunk") << 65536LL << QString("Bob");
        QTest::newRow("partial-final-chunk") << (17LL * 1024 * 1024 + 37) << QString("Bob");
        QTest::newRow("room") << 131073LL << QString();
    }
    void roundTrip() {
        QFETCH(qint64, size); QFETCH(QString, to);
        Rig r;
        QSignalSpy outgoingHistory(&r.alice.video, &VideoClient::historyReady);
        QSignalSpy incomingHistory(&r.bob.video, &VideoClient::historyReady);
        const QString source = fixture(r.alice.root.path(), size);
        QVERIFY(!source.isEmpty()); QVERIFY(r.alice.video.start(source, to));
        QTRY_COMPARE_WITH_TIMEOUT(r.alice.sent, 1, 30000);
        QCOMPARE(r.bob.received.size(), 1);
        QCOMPARE(QFileInfo(r.bob.received.first()).size(), size);
        QCOMPARE(digest(source), digest(r.bob.received.first()));
        QCOMPARE(r.eve.received.size(), to.isEmpty() ? 1 : 0);
        QCOMPARE(r.alice.channel.chunks, int((size + 65535) / 65536));
        QVERIFY(r.alice.channel.maxFrame < VideoProtocol::MaxFrame);
        QCOMPARE(r.relay.activeCount(), 0);
        QCOMPARE(outgoingHistory.size(), 1); QCOMPARE(incomingHistory.size(), 1);
        const auto sentMetadata = outgoingHistory.first().first().toJsonObject();
        const auto receivedMetadata = incomingHistory.first().first().toJsonObject();
        QCOMPARE(sentMetadata.value("_event_id"), receivedMetadata.value("_event_id"));
        QCOMPARE(sentMetadata.value("size").toDouble(), double(size));
        QCOMPARE(receivedMetadata.value("local_path").toString(), r.bob.received.first());
        QCOMPARE(sentMetadata.value("sha256"), receivedMetadata.value("sha256"));
        QVERIFY(r.alice.failures.isEmpty() && r.bob.failures.isEmpty() && r.eve.failures.isEmpty());
    }
    void simultaneousTransfers() {
        Rig r;
        QVERIFY(r.alice.video.start(fixture(r.alice.root.path(), 1048577), ""));
        QVERIFY(r.bob.video.start(fixture(r.bob.root.path(), 2097153), "Eve"));
        QTRY_VERIFY(r.alice.sent == 1 && r.bob.sent == 1);
        QCOMPARE(r.eve.received.size(), 2); QCOMPARE(r.bob.received.size(), 1);
        QCOMPARE(r.relay.activeCount(), 0);
    }
    void backpressureAndCancel() {
        Rig r; r.bob.channel.holdAcks = true;
        QSignalSpy outgoingHistory(&r.alice.video, &VideoClient::historyReady);
        QSignalSpy incomingHistory(&r.bob.video, &VideoClient::historyReady);
        QVERIFY(r.alice.video.start(fixture(r.alice.root.path(), 1048576), "Bob"));
        QTRY_COMPARE(r.bob.video.incomingCount(), 1);
        QTest::qWait(20); QCOMPARE(r.alice.channel.chunks, 0);
        // Release readiness manually, then withhold the first disk ACK.
        const QString transferId = r.alice.channel.controls.first().value("id").toString();
        r.relay.handle(&r.bob.token, {{"type", "video_ack"}, {"id", transferId}, {"phase", "ready"}, {"offset", 0}});
        QTRY_COMPARE(r.alice.channel.chunks, 1);
        QTest::qWait(20); QCOMPARE(r.alice.channel.chunks, 1);
        r.alice.video.cancel();
        QTRY_COMPARE(r.relay.activeCount(), 0); QTRY_COMPARE(r.bob.video.incomingCount(), 0);
        QCOMPARE(QDir(r.bob.root.path()).entryList(QDir::Files | QDir::Hidden).size(), 0);
        QCOMPARE(r.alice.sent, 0);
        QCOMPARE(outgoingHistory.size(), 0); QCOMPARE(incomingHistory.size(), 0);
    }
    void recipientDisconnect() {
        Rig r; r.bob.channel.holdAcks = true;
        QVERIFY(r.alice.video.start(fixture(r.alice.root.path(), 1048576), "Bob"));
        QTRY_COMPARE(r.bob.video.incomingCount(), 1);
        r.relay.disconnected(&r.bob.token);
        QTRY_VERIFY(!r.alice.video.isSending()); QTRY_COMPARE(r.bob.video.incomingCount(), 0);
        QCOMPARE(QDir(r.bob.root.path()).entryList(QDir::Files | QDir::Hidden).size(), 0);
    }
    void timeout() {
        Rig r(100); r.bob.channel.holdAcks = true;
        QVERIFY(r.alice.video.start(fixture(r.alice.root.path(), 1048576), "Bob"));
        QTRY_COMPARE(r.bob.video.incomingCount(), 1);
        QTRY_VERIFY(!r.alice.video.isSending()); QTRY_COMPARE(r.relay.activeCount(), 0);
        QTRY_COMPARE(r.bob.video.incomingCount(), 0);
        QCOMPARE(QDir(r.bob.root.path()).entryList(QDir::Files | QDir::Hidden).size(), 0);
    }
    void invalidSourceAndDiskError() {
        Rig r;
        QVERIFY(!r.alice.video.start("missing.mp4", "Bob"));
        const QString huge = fixture(r.alice.root.path(), VideoProtocol::MaxSize + 1);
        QVERIFY(!r.alice.video.start(huge, "Bob"));
        QFile f(huge); QVERIFY(f.open(QIODevice::WriteOnly)); QVERIFY(f.resize(20)); QVERIFY(f.write(QByteArray(20, 'x')) == 20); f.close();
        QVERIFY(!r.alice.video.start(huge, "Bob"));
        Rig blocked(60000, huge + "/not-a-directory");
        QVERIFY(blocked.alice.video.start(fixture(blocked.alice.root.path(), 131072), "Bob"));
        QTRY_VERIFY(!blocked.alice.failures.isEmpty());
        QCOMPARE(blocked.alice.sent, 0); QCOMPARE(blocked.bob.video.incomingCount(), 0);
    }
    void relayValidation_data() {
        QTest::addColumn<QString>("kind");
        for (const auto &s : {"oversize", "fraction", "traversal", "offline", "offset", "base64", "chunkSize", "overrun", "signature", "earlyEnd", "checksum", "duplicate", "foreignCancel"})
            QTest::newRow(s) << QString(s);
    }
    void relayValidation() {
        QFETCH(QString, kind);
        QTcpSocket a, b, outsider;
        QList<QJsonObject> sent;
        VideoRelay relay([&](QTcpSocket *, const QJsonObject &p) { sent.append(p); return true; },
                         [&](QTcpSocket *s) { return s == &b ? "Bob" : "Alice"; },
                         [&] { return QList<QTcpSocket *>{&a, &b}; });
        const QString transferId = id();
        auto p = start(transferId);
        if (kind == "oversize") p["size"] = double(VideoProtocol::MaxSize + 1);
        if (kind == "fraction") p["size"] = 12.5;
        if (kind == "traversal") p["name"] = "../test.mp4";
        if (kind == "offline") p["to"] = "Nobody";
        relay.handle(&a, p);
        if (relay.activeCount()) {
            if (kind == "duplicate") {
                relay.handle(&a, p); QCOMPARE(relay.activeCount(), 1);
                QCOMPARE(sent.last().value("type").toString(), "video_error"); return;
            }
            if (kind == "foreignCancel") {
                relay.handle(&outsider, {{"type", "video_cancel"}, {"id", transferId}});
                QCOMPARE(relay.activeCount(), 1); return;
            }
            relay.handle(&b, {{"type", "video_ack"}, {"id", transferId}, {"phase", "ready"}, {"offset", 0}});
            QByteArray data = QByteArray::fromHex("000000186674797069736f6d");
            if (kind == "overrun") data.append('x');
            if (kind == "signature") data.fill('x');
            if (kind == "chunkSize") data = QByteArray(65537, 'x');
            if (kind != "earlyEnd") relay.handle(&a, {{"type", "video_chunk"}, {"id", transferId},
                {"offset", kind == "offset" ? 1 : 0}, {"data", kind == "base64" ? QString("!!!!") : QString::fromLatin1(data.toBase64())}});
            if (kind == "checksum") relay.handle(&b, {{"type", "video_ack"}, {"id", transferId}, {"phase", "chunk"}, {"offset", 12}});
            if (kind == "earlyEnd" || kind == "checksum")
                relay.handle(&a, {{"type", "video_end"}, {"id", transferId}, {"sha256", QString(64, '0')}});
        }
        QCOMPARE(relay.activeCount(), 0); QCOMPARE(sent.last().value("type").toString(), "video_error");
    }
    void receiverRejectsWrongHash() {
        Endpoint receiver;
        const QString transferId = id();
        receiver.video.handle(start(transferId));
        const QByteArray bytes = QByteArray::fromHex("000000186674797069736f6d");
        receiver.video.handle({{"type", "video_chunk"}, {"id", transferId}, {"offset", 0}, {"data", QString::fromLatin1(bytes.toBase64())}});
        receiver.video.handle({{"type", "video_end"}, {"id", transferId}, {"sha256", QString(64, '0')}});
        QCOMPARE(receiver.video.incomingCount(), 0); QCOMPARE(receiver.received.size(), 0);
        QCOMPARE(QDir(receiver.root.path()).entryList(QDir::Files | QDir::Hidden).size(), 0);
    }
    void fullGiB() {
        if (!qEnvironmentVariableIsSet("CHAT_RUN_1GIB")) QSKIP("Set CHAT_RUN_1GIB=1 for full 1 GiB disk/relay test");
        Rig r;
        const QString source = fixture(r.alice.root.path(), VideoProtocol::MaxSize);
        QVERIFY(!source.isEmpty()); QVERIFY(r.alice.video.start(source, "Bob"));
        QTRY_COMPARE_WITH_TIMEOUT(r.alice.sent, 1, 600000);
        QCOMPARE(r.bob.received.size(), 1);
        QCOMPARE(QFileInfo(r.bob.received.first()).size(), VideoProtocol::MaxSize);
        QCOMPARE(digest(source), digest(r.bob.received.first()));
        QCOMPARE(r.alice.channel.chunks, 16384);
        QCOMPARE(r.relay.activeCount(), 0);
    }
};
QTEST_GUILESS_MAIN(OfflineTests)
#include "offline_tests.moc"
