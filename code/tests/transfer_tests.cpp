#include "sessionconnection.h"
#include "testserver.h"
#include "chatserver.h"
#include "videoclient.h"
#include "historystore.h"
#include "historyprotocol.h"
#include <QtTest>
#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <functional>

class Peer : public QObject {
public:
    QSslSocket socket;
    JsonChannel channel{&socket};
    QTemporaryDir dir{QDir::currentPath() + "/test-video-XXXXXX"};
    VideoClient video{&channel, &socket, dir.path()};
    QList<QJsonObject> messages;
    QStringList files, failures;
    bool autoVideo = true, joined = false;
    int chunks = 0, sent = 0;
    qint64 maxQueue = 0;
    std::function<void(const QJsonObject &)> observe;
    Peer() {
        connect(&channel, &JsonChannel::packetReceived, this, [this](const QJsonObject &p) {
            const QString type = p.value("type").toString();
            if (type == "joined") joined = true;
            if (type == "video_chunk") ++chunks;
            else if (type != "video_ack" && messages.size() < 1000) messages.append(p);
            if (observe) observe(p);
            if (autoVideo) video.handle(p);
            maxQueue = qMax(maxQueue, socket.bytesToWrite());
        });
        connect(&video, &VideoClient::completed, this, [this](const QString &p, const QString &, const QString &, bool outgoing) {
            if (outgoing) ++sent; else files.append(p);
        });
        connect(&video, &VideoClient::failed, this, [this](const QString &s) { failures.append(s); });
    }
    void join(quint16 port, const QString &name) {
        {AccountStore store;QString error;if(store.open(FixtureDatabases.value(port),error))store.create(name,TestPassword);}
        const QString expected = FixtureFingerprints.value(port);
        connect(&socket, &QSslSocket::encrypted, this, [this, name, expected] {
            if (TlsIdentity::fingerprint(socket.peerCertificate()) != expected) { socket.abort(); return; }
            channel.send({{"type", "login"}, {"name", name}, {"password", TestPassword}});
        });
        connect(&socket, &QSslSocket::sslErrors, this, [this, expected](const QList<QSslError> &errors) {
            if (TlsIdentity::fingerprint(socket.peerCertificate()) == expected) socket.ignoreSslErrors(errors);
            else socket.abort();
        });
        socket.connectToHostEncrypted("127.0.0.1", port);
    }
    bool got(const QString &type) const {
        for (const auto &p : messages) if (p.value("type").toString() == type) return true;
        return false;
    }
};

class TransferTests : public QObject {
    Q_OBJECT
    static QString fixture(const QString &dir, qint64 size, const QString &name = "sample.mp4") {
        const QString path = QDir(dir).filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || !f.resize(size)) return {};
        // Synthetic ISO BMFF payload for byte transfer testing, not a playable movie.
        if (size >= 12 && f.write(QByteArray::fromHex("000000186674797069736F6D")) != 12) return {};
        f.close();
        return path;
    }
    static QByteArray hashFile(const QString &path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        QCryptographicHash h(QCryptographicHash::Sha256);
        while (!f.atEnd()) h.addData(f.read(1024 * 1024));
        return h.result();
    }
    static QJsonObject startPacket(const QString &id, qint64 size = 12, const QString &to = "Bob") {
        return {{"type", "video_start"}, {"id", id}, {"name", "demo.mp4"},
                {"size", double(size)}, {"to", to}};
    }
    static QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
private slots:
    void sizeAndBase64Validation() {
        qint64 n;
        QVERIFY(VideoProtocol::number(double(VideoProtocol::MaxSize), n));
        QCOMPARE(n, 1073741824LL);
        QVERIFY(!VideoProtocol::number(double(VideoProtocol::MaxSize + 1), n));
        QVERIFY(!VideoProtocol::number(12.5, n));
        QVERIFY(!VideoProtocol::number("123", n));
        QVERIFY(!VideoProtocol::number(-1, n));
        QVERIFY(!VideoProtocol::validName("../x.mp4"));
        QVERIFY(!VideoProtocol::validName("a.exe"));
        QVERIFY(VideoProtocol::validName(QString::fromUtf8("video tiếng Việt.MP4")));
        QByteArray bytes;
        QVERIFY(!VideoProtocol::decode({{"data", "!!!!"}}, bytes));
        QVERIFY(!VideoProtocol::decode({{"data", QString::fromLatin1(QByteArray(65537, 'a').toBase64())}}, bytes));
    }
    void groupVideoRoutesAndWaitsForEveryRecipient() {
        QTcpSocket alice, bob, carol, outsider;
        const QHash<QTcpSocket *, QString> names{
            {&alice, "Alice"}, {&bob, "Bob"}, {&carol, "Carol"}, {&outsider, "Outsider"}};
        QHash<QTcpSocket *, QList<QJsonObject>> packets;
        VideoRelay relay([&](QTcpSocket *peer, const QJsonObject &packet) {
            packets[peer].append(packet);
            return true;
        }, [&](QTcpSocket *peer) { return names.value(peer); }, [&] { return names.keys(); });
        bool recordedAsPrivate = false;
        relay.completed = [&](const QJsonObject &) { recordedAsPrivate = true; };
        const QString target = "group:" + uuid();
        const QString id = uuid();
        const auto start = startPacket(id, 12, target);
        QVERIFY(relay.handle(&alice, start));
        QCOMPARE(relay.activeCount(), 0);
        QCOMPARE(packets.take(&alice).last().value("type").toString(), QString("video_error"));
        QVERIFY(relay.handle(&alice, start, {&alice, &bob, &carol}));
        QCOMPARE(relay.activeCount(), 1);
        QVERIFY(packets.value(&alice).isEmpty());
        QVERIFY(packets.value(&outsider).isEmpty());
        QCOMPARE(packets.value(&bob).last().value("to").toString(), target);
        QCOMPARE(packets.value(&carol).last().value("type").toString(), QString("video_start"));

        const auto ack = [&](QTcpSocket *peer, const QString &phase, int offset) {
            return relay.handle(peer, {{"type","video_ack"},{"id",id},{"phase",phase},{"offset",offset}});
        };
        QVERIFY(ack(&outsider, "ready", 0));
        QCOMPARE(packets.value(&outsider).last().value("type").toString(), QString("video_error"));
        QVERIFY(ack(&bob, "ready", 0));
        QVERIFY(packets.value(&alice).isEmpty());
        QVERIFY(ack(&carol, "ready", 0));
        QCOMPARE(packets.value(&alice).last().value("type").toString(), QString("video_ready"));

        const auto bytes = QByteArray::fromHex("000000186674797069736F6D");
        const auto data = QString::fromLatin1(bytes.toBase64());
        QVERIFY(relay.handle(&alice, {{"type","video_chunk"},{"id",id},{"offset",0},{"data",data}}));
        QCOMPARE(packets.value(&bob).last().value("data").toString(), data);
        QCOMPARE(packets.value(&carol).last().value("data").toString(), data);
        QCOMPARE(packets.value(&outsider).size(), 1);
        QVERIFY(ack(&bob, "chunk", 12));
        QCOMPARE(packets.value(&alice).last().value("type").toString(), QString("video_ready"));
        QVERIFY(ack(&carol, "chunk", 12));
        QCOMPARE(packets.value(&alice).last().value("type").toString(), QString("video_ack"));

        const auto hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
        QVERIFY(relay.handle(&alice, {{"type","video_end"},{"id",id},{"sha256",hash}}));
        QVERIFY(ack(&bob, "done", 12));
        QVERIFY(ack(&carol, "done", 12));
        QCOMPARE(packets.value(&alice).last().value("type").toString(), QString("video_done"));
        QCOMPARE(relay.activeCount(), 0);
        QVERIFY(!recordedAsPrivate);
    }
    void privateStreamingAndLegacyMessages() {
        TestServer server; QVERIFY(server.start(0));
        Peer alice, bob, eve;
        alice.join(server.port(), "Alice"); bob.join(server.port(), "Bob"); eve.join(server.port(), "Eve");
        QTRY_VERIFY(alice.joined && bob.joined && eve.joined);
        const QString path = fixture(alice.dir.path(), 17 * 1024 * 1024 + 37);
        QVERIFY(!path.isEmpty());
        QVERIFY(alice.video.start(path, "Bob"));
        alice.channel.send({{"type", "chat"}, {"text", "chat during video"}});
        alice.channel.send({{"type", "private"}, {"to", "Bob"}, {"text", "private"}});
        alice.channel.send({{"type", "reply"}, {"text", "reply"}, {"quote", "quote"}});
        alice.channel.send({{"type", "forward"}, {"to", "Bob"}, {"text", "forward"}});
        alice.channel.send({{"type", "sticker"}, {"to", "Bob"}, {"id", "nobita"}});
        QImage image(4, 4, QImage::Format_RGB32); image.fill(Qt::red);
        QByteArray jpeg; QBuffer b(&jpeg); QVERIFY(b.open(QIODevice::WriteOnly)); QVERIFY(image.save(&b, "JPEG"));
        alice.channel.send({{"type", "image"}, {"to", "Bob"}, {"data", QString::fromLatin1(jpeg.toBase64())}});
        QTRY_COMPARE_WITH_TIMEOUT(alice.sent, 1, 30000);
        QCOMPARE(bob.files.size(), 1); QCOMPARE(eve.files.size(), 0); QCOMPARE(eve.chunks, 0);
        QCOMPARE(hashFile(path), hashFile(bob.files.first()));
        QCOMPARE(QFileInfo(bob.files.first()).size(), QFileInfo(path).size());
        QVERIFY(bob.got("chat") && bob.got("private") && bob.got("reply") && bob.got("forward") && bob.got("sticker") && bob.got("image"));
        QVERIFY(eve.got("chat") && eve.got("reply"));
        QVERIFY(!eve.got("private") && !eve.got("forward") && !eve.got("sticker") && !eve.got("image"));
        QVERIFY(alice.failures.isEmpty() && bob.failures.isEmpty());
        QVERIFY(alice.maxQueue <= VideoProtocol::MaxQueued);
        for (const auto &kind : {"chat", "private", "reply", "forward", "sticker", "image"}) {
            QString senderId, receiverId;
            for (const auto &p : alice.messages) if (p.value("type") == kind) senderId = p.value("_event_id").toString();
            for (const auto &p : bob.messages) if (p.value("type") == kind) receiverId = p.value("_event_id").toString();
            QVERIFY(VideoProtocol::validId(senderId)); QCOMPARE(senderId, receiverId);
        }
    }
    void roomAndConcurrentSenders() {
        TestServer server; QVERIFY(server.start(0));
        Peer a, b, c;
        a.join(server.port(), "Alice"); b.join(server.port(), "Bob"); c.join(server.port(), "Carol");
        QTRY_VERIFY(a.joined && b.joined && c.joined);
        QVERIFY(a.video.start(fixture(a.dir.path(), 131073), ""));
        QVERIFY(b.video.start(fixture(b.dir.path(), 196611), "Carol"));
        QTRY_VERIFY(a.sent == 1 && b.sent == 1 && c.files.size() == 2);
        QCOMPARE(b.files.size(), 1); QCOMPARE(a.files.size(), 0);
        QVERIFY(a.failures.isEmpty() && b.failures.isEmpty() && c.failures.isEmpty());
    }
    void receiverBackpressure() {
        TestServer server; QVERIFY(server.start(0));
        Peer a, b; b.autoVideo = false;
        a.join(server.port(), "Alice"); b.join(server.port(), "Bob");
        QTRY_VERIFY(a.joined && b.joined);
        QString id;
        b.observe = [&](const QJsonObject &p) {
            if (p.value("type") == "video_start") {
                id = p.value("id").toString();
                b.channel.send({{"type", "video_ack"}, {"id", id}, {"phase", "ready"}, {"offset", 0}});
            }
        };
        QVERIFY(a.video.start(fixture(a.dir.path(), 4 * 1024 * 1024), "Bob"));
        QTRY_COMPARE(b.chunks, 1);
        a.channel.send({{"type", "chat"}, {"text", "still responsive"}});
        QTRY_VERIFY(b.got("chat"));
        QTest::qWait(150);
        QCOMPARE(b.chunks, 1); // no next chunk until the receiver acknowledges.
        QVERIFY(a.socket.bytesToWrite() < VideoProtocol::ChunkSize * 2);
        a.video.cancel(); QTRY_VERIFY(b.got("video_error"));
        QVERIFY(!a.video.isSending());
    }
    void cancelAndDisconnectCleanup() {
        TestServer server; QVERIFY(server.start(0));
        Peer a, b;
        a.join(server.port(), "Alice"); b.join(server.port(), "Bob");
        QTRY_VERIFY(a.joined && b.joined);
        b.observe = [&](const QJsonObject &p) {
            if (p.value("type") == "video_chunk") a.video.cancel();
        };
        QVERIFY(a.video.start(fixture(a.dir.path(), 8 * 1024 * 1024), "Bob"));
        QTRY_VERIFY(!b.failures.isEmpty()); QTRY_COMPARE(b.video.incomingCount(), 0);
        QCOMPARE(QDir(b.dir.path()).entryList(QDir::Files | QDir::Hidden).size(), 0);
        b.observe = [&](const QJsonObject &p) {
            if (p.value("type") == "video_chunk") b.socket.abort();
        };
        QVERIFY(a.video.start(fixture(a.dir.path(), 8 * 1024 * 1024), "Bob"));
        QTRY_VERIFY(!a.video.isSending());
        QTRY_COMPARE(b.video.incomingCount(), 0);
        QCOMPARE(QDir(b.dir.path()).entryList(QDir::Files | QDir::Hidden).size(), 0);
    }
    void rejectsInvalidTransfers_data() {
        QTest::addColumn<QString>("kind");
        for (const auto &s : {"oversize", "fraction", "traversal", "offline", "offset", "base64", "chunkSize", "overrun", "signature", "earlyEnd", "checksum"})
            QTest::newRow(s) << QString(s);
    }
    void rejectsInvalidTransfers() {
        QFETCH(QString, kind);
        TestServer server; QVERIFY(server.start(0));
        Peer a, b; a.autoVideo = false;
        a.join(server.port(), "Alice"); b.join(server.port(), "Bob");
        QTRY_VERIFY(a.joined && b.joined);
        const QString id = uuid();
        QJsonObject p = startPacket(id);
        if (kind == "oversize") p["size"] = double(VideoProtocol::MaxSize + 1);
        if (kind == "fraction") p["size"] = 12.5;
        if (kind == "traversal") p["name"] = "../demo.mp4";
        if (kind == "offline") p["to"] = "Offline";
        QVERIFY(a.channel.send(p));
        if (kind == "offset" || kind == "base64" || kind == "chunkSize" || kind == "overrun" || kind == "signature" || kind == "earlyEnd" || kind == "checksum") {
            QTRY_VERIFY(a.got("video_ready"));
            QByteArray data = QByteArray::fromHex("000000186674797069736F6D");
            if (kind == "overrun") data.append('x');
            if (kind == "signature") data.fill('x');
            if (kind == "chunkSize") data = QByteArray(65537, 'x');
            if (kind != "earlyEnd") {
                a.channel.send({{"type", "video_chunk"}, {"id", id}, {"offset", kind == "offset" ? 1 : 0},
                    {"data", kind == "base64" ? QString("!!!!") : QString::fromLatin1(data.toBase64())}});
            }
            if (kind == "checksum") QTRY_COMPARE(b.chunks, 1);
            if (kind == "earlyEnd" || kind == "checksum") {
                // Wait for the receiver's ACK to reach the server.
                QTest::qWait(50);
                a.channel.send({{"type", "video_end"}, {"id", id}, {"sha256", QString(64, '0')}});
            }
        }
        QTRY_VERIFY(a.got("video_error"));
        QTRY_COMPARE(b.video.incomingCount(), 0);
        QCOMPARE(b.files.size(), 0);
        QCOMPARE(QDir(b.dir.path()).entryList(QDir::Files | QDir::Hidden).size(), 0);
    }
    void framingFragmentationAndCoalescing() {
        QTcpServer listener; QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
        QTcpSocket wire; wire.connectToHost(QHostAddress::LocalHost, listener.serverPort());
        QTRY_VERIFY(listener.hasPendingConnections());
        std::unique_ptr<QTcpSocket> receiver(listener.nextPendingConnection());
        JsonChannel channel(receiver.get()); QSignalSpy packets(&channel, &JsonChannel::packetReceived);
        wire.write("{\"type\":"); wire.flush(); QTest::qWait(10); QCOMPARE(packets.size(), 0);
        wire.write("\"chat\"}\n{\"type\":\"reply\"}\n");
        QTRY_COMPARE(packets.size(), 2);
        // Unterminated oversized input must be rejected without growing memory.
        wire.write(QByteArray(VideoProtocol::MaxFrame + 2, 'x'));
        QTRY_COMPARE(receiver->state(), QAbstractSocket::UnconnectedState);
        QVERIFY(channel.bufferedBytes() <= VideoProtocol::MaxFrame + 1);
    }
    void identityAndTimeout() {
        QTcpSocket a, b, intruder;
        QHash<QTcpSocket *, QList<QJsonObject>> packets;
        auto send = [&](QTcpSocket *s, const QJsonObject &p) { packets[s].append(p); return true; };
        VideoRelay relay(send, [&](QTcpSocket *s) { return s == &a ? "Alice" : s == &b ? "Bob" : "Eve"; },
                         [&] { return QList<QTcpSocket *>{&a, &b}; }, nullptr, 50);
        const QString id = uuid();
        QVERIFY(relay.handle(&a, startPacket(id)));
        QCOMPARE(relay.activeCount(), 1);
        relay.handle(&intruder, {{"type", "video_cancel"}, {"id", id}});
        QCOMPARE(relay.activeCount(), 1); // outsider cannot cancel another user's transfer.
        QTRY_COMPARE(relay.activeCount(), 0);
        QCOMPARE(packets[&a].last().value("type").toString(), "video_error");
    }
    void receiverDiskErrorAndTimeout() {
        QSslSocket socket; JsonChannel channel(&socket);
        QTemporaryDir root(QDir::currentPath() + "/test-disk-XXXXXX");
        const QString file = fixture(root.path(), 12);
        VideoClient blocked(&channel, &socket, file + "/not-a-directory", nullptr, 50);
        QSignalSpy failures(&blocked, &VideoClient::failed);
        QJsonObject p = startPacket(uuid()); p["sender"] = "Alice";
        blocked.handle(p);
        QCOMPARE(failures.size(), 1); QCOMPARE(blocked.incomingCount(), 0);
    }
    void sqliteOverTcpAndRestart() {
        TestServer server; QVERIFY(server.start(0));
        Peer a, b, eve;
        const QString aPath = a.dir.filePath("alice.sqlite"), bPath = b.dir.filePath("bob.sqlite");
        auto aDb = std::make_unique<HistoryStore>(); auto bDb = std::make_unique<HistoryStore>();
        QString error; QVERIFY2(aDb->open(aPath, error), qPrintable(error)); QVERIFY2(bDb->open(bPath, error), qPrintable(error));
        int failures = 0;
        auto persist = [&](HistoryStore *db, QJsonObject p, const QString &owner) {
            if (p.value("type") == "video" && p.value("outgoing").toBool()) p["sender"] = owner;
            HistoryRecord record; QString err;
            if (db->append(p, record, err) == HistoryStore::SaveResult::Error) ++failures;
        };
        connect(&a.channel, &JsonChannel::packetReceived, this, [&](const QJsonObject &p) {
            if (HistoryProtocol::messageType(p.value("type").toString())) persist(aDb.get(), p, "Alice");
        });
        connect(&b.channel, &JsonChannel::packetReceived, this, [&](const QJsonObject &p) {
            if (HistoryProtocol::messageType(p.value("type").toString())) persist(bDb.get(), p, "Bob");
        });
        connect(&a.video, &VideoClient::historyReady, this, [&](const QJsonObject &p) { persist(aDb.get(), p, "Alice"); });
        connect(&b.video, &VideoClient::historyReady, this, [&](const QJsonObject &p) { persist(bDb.get(), p, "Bob"); });
        a.join(server.port(), "Alice"); b.join(server.port(), "Bob"); eve.join(server.port(), "Eve");
        QTRY_VERIFY(a.joined && b.joined && eve.joined);
        a.channel.send({{"type", "chat"}, {"text", "saved public"}});
        a.channel.send({{"type", "private"}, {"to", "Bob"}, {"text", "saved private"}});
        a.channel.send({{"type", "reply"}, {"text", "saved reply"}, {"quote", "original"}});
        a.channel.send({{"type", "forward"}, {"to", "Bob"}, {"text", "saved forward"}});
        a.channel.send({{"type", "sticker"}, {"to", "Bob"}, {"id", "nobita"}});
        QImage image(12, 8, QImage::Format_RGB32); image.fill(Qt::blue);
        QByteArray bytes; QBuffer buffer(&bytes); QVERIFY(buffer.open(QIODevice::WriteOnly)); QVERIFY(image.save(&buffer, "JPEG"));
        a.channel.send({{"type", "image"}, {"to", "Bob"}, {"data", QString::fromLatin1(bytes.toBase64())}});
        QVERIFY(a.video.start(fixture(a.dir.path(), 262181), "Bob"));
        QTRY_COMPARE(a.sent, 1); QTRY_COMPARE(b.files.size(), 1);
        QCOMPARE(failures, 0); QCOMPARE(eve.chunks, 0);
        const auto aPage = aDb->page(100, 0, error); QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(aPage.records.size(), 7);
        aDb.reset(); bDb.reset();
        HistoryStore reopened; QVERIFY(reopened.open(bPath, error));
        const auto page = reopened.page(100, 0, error); QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(page.records.size(), 7);
        QSet<QString> types;
        for (const auto &record : page.records) {
            types.insert(record.packet.value("type").toString());
            if (record.packet.value("type") == "image") QCOMPARE(record.image, bytes);
            if (record.packet.value("type") == "video") {
                QCOMPARE(record.packet.value("local_path").toString(), b.files.first());
                QVERIFY(QFile::exists(record.packet.value("local_path").toString()));
            }
        }
        QCOMPARE(types.size(), 7);
        HistoryRecord duplicate;
        QCOMPARE(reopened.append(page.records.first().packet, duplicate, error), HistoryStore::SaveResult::Duplicate);
    }
    void fullGiB() {
        if (!qEnvironmentVariableIsSet("CHAT_RUN_1GIB")) QSKIP("Set CHAT_RUN_1GIB=1 to run the full 1 GiB disk/network test");
        TestServer server; QVERIFY(server.start(0));
        Peer a, b; a.join(server.port(), "Alice"); b.join(server.port(), "Bob");
        HistoryStore aDb, bDb; QString error; int historyErrors = 0;
        QVERIFY(aDb.open(a.dir.filePath("sender.sqlite"), error));
        QVERIFY(bDb.open(b.dir.filePath("receiver.sqlite"), error));
        connect(&a.video, &VideoClient::historyReady, this, [&](QJsonObject p) {
            p["sender"] = "Alice"; HistoryRecord record; QString err;
            if (aDb.append(p, record, err) != HistoryStore::SaveResult::Saved) ++historyErrors;
        });
        connect(&b.video, &VideoClient::historyReady, this, [&](const QJsonObject &p) {
            HistoryRecord record; QString err;
            if (bDb.append(p, record, err) != HistoryStore::SaveResult::Saved) ++historyErrors;
        });
        QTRY_VERIFY(a.joined && b.joined);
        const QString source = fixture(a.dir.path(), VideoProtocol::MaxSize);
        QVERIFY(!source.isEmpty());
        QSignalSpy finished(&a.video, &VideoClient::completed);
        QVERIFY(a.video.start(source, "Bob"));
        // Use the native event loop rather than periodic QTRY polling for 16K round trips.
        QVERIFY2(finished.wait(600000), "1 GiB transfer did not complete within 10 minutes");
        QCOMPARE(a.sent, 1);
        QCOMPARE(b.files.size(), 1);
        QCOMPARE(QFileInfo(b.files.first()).size(), VideoProtocol::MaxSize);
        QCOMPARE(hashFile(source), hashFile(b.files.first()));
        QVERIFY(a.failures.isEmpty() && b.failures.isEmpty());
        QCOMPARE(historyErrors, 0);
        QCOMPARE(aDb.page(100, 0, error).records.size(), 1);
        const auto history = bDb.page(100, 0, error);
        QCOMPARE(history.records.size(), 1);
        QCOMPARE(history.records.first().packet.value("size").toDouble(), double(VideoProtocol::MaxSize));
        QCOMPARE(history.records.first().packet.value("local_path").toString(), b.files.first());
    }
};
QTEST_GUILESS_MAIN(TransferTests)
#include "transfer_tests.moc"

