#include "testserver.h"
#include "groupimageprotocol.h"
#include <QJsonArray>
#include <QSettings>
#include <QtTest>

namespace {
struct Peer {
    QSslSocket socket;
    JsonChannel channel{&socket};
    SessionConnection session;
    QList<QJsonObject> packets;

    Peer(const QString &name, const TestServer &server)
        : session(&socket, &channel, {name, "127.0.0.1", server.port(), TestPassword, true}) {
        trustFixture(session, server);
        QObject::connect(&channel, &JsonChannel::packetReceived, &session,
                         [this](const QJsonObject &packet) { packets.append(packet); });
        QObject::connect(&session, &SessionConnection::failed, &session,
                         [name](const QString &error) { qWarning() << name << error; });
    }

    QJsonObject latest(const QString &type) const {
        for (auto it = packets.crbegin(); it != packets.crend(); ++it)
            if (it->value("type") == type) return *it;
        return {};
    }
};
}

class GroupHistoryTests : public QObject {
    Q_OBJECT

private slots:
    void persistsOfflineMessagesAndRejectsOutsiders() {
        QTemporaryDir settings(QDir::currentPath() + "/group-settings-XXXXXX");
        QVERIFY(settings.isValid());
        QCoreApplication::setOrganizationName("ChatTCP1-Group-Tests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());

        TestServer server;
        QVERIFY2(server.start(), qPrintable(server.startupError()));
        Peer alice("Alice", server), bob("Bob", server), carol("Carol", server);
        alice.session.connectNow();
        bob.session.connectNow();
        carol.session.connectNow();
        QTRY_COMPARE_WITH_TIMEOUT(alice.session.state(), SessionConnection::Ready, 30000);
        QTRY_COMPARE_WITH_TIMEOUT(bob.session.state(), SessionConnection::Ready, 30000);
        QTRY_COMPARE_WITH_TIMEOUT(carol.session.state(), SessionConnection::Ready, 30000);

        QVERIFY(alice.channel.send({{"type","group_create"},{"name","Group"},{"members",QJsonArray{"Bob"}}}));
        QTRY_VERIFY(!bob.latest("group_created").isEmpty());
        const QString id = bob.latest("group_created").value("id").toString();
        const QJsonObject request{{"type","group_history_request"},{"id",id}};
        QVERIFY(bob.channel.send(request));
        QTRY_VERIFY(bob.latest("group_history").value("messages").isArray());
        QVERIFY(bob.latest("group_history").value("messages").toArray().isEmpty());

        bob.session.stop();
        QTRY_COMPARE(bob.socket.state(), QAbstractSocket::UnconnectedState);
        const QString text = QStringLiteral("Tin nhắn khi offline 😃");
        QVERIFY(alice.channel.send({{"type","group_chat"},{"id",id},{"text",text}}));
        QTRY_COMPARE(alice.latest("group_chat").value("text").toString(), text);
        QVERIFY(carol.channel.send(request));
        QTRY_VERIFY(!carol.latest("group_history").value("error").toString().isEmpty());
        QVERIFY(!carol.latest("group_history").contains("messages"));
        QVERIFY(carol.latest("group_chat").isEmpty());

        QImage image(64, 48, QImage::Format_RGB32);
        image.fill(Qt::green);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "JPEG"));
        const QString encoded = QString::fromLatin1(bytes.toBase64());
        const QJsonObject photo{{"type","group_image"},{"id",id},{"data",encoded}};
        QVERIFY(carol.channel.send(photo));
        QTRY_VERIFY(!carol.latest("error").isEmpty());
        QVERIFY(alice.channel.send(photo));
        QTRY_COMPARE(alice.latest("group_chat").value("image").toString(), encoded);

        const quint16 port = server.port();
        server.shutdown();
        QTRY_COMPARE(alice.session.state(), SessionConnection::Idle);
        QTRY_COMPARE(carol.session.state(), SessionConnection::Idle);
        QVERIFY2(server.start(port), qPrintable(server.startupError()));
        bob.packets.clear();
        bob.session.connectNow();
        QTRY_COMPARE_WITH_TIMEOUT(bob.session.state(), SessionConnection::Ready, 30000);
        QTRY_COMPARE(bob.latest("group_created").value("id").toString(), id);
        QVERIFY(bob.latest("group_created").value("existing").toBool());
        QVERIFY(bob.channel.send(request));
        QTRY_COMPARE(bob.latest("group_history").value("messages").toArray().size(), 2);
        const auto messages = bob.latest("group_history").value("messages").toArray();
        const auto message = messages.first().toObject();
        QCOMPARE(message.value("text").toString(), text);
        QCOMPARE(message.value("sender").toString(), QString("Alice"));
        QCOMPARE(messages.last().toObject().value("image").toString(), encoded);
    }
};

QTEST_MAIN(GroupHistoryTests)
#include "group_history_tests.moc"
