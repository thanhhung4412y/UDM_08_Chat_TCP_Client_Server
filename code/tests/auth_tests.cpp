#include "accountstore.h"
#include "authprotocol.h"
#include "tlsidentity.h"
#include "sessionconnection.h"
#include "chatserver.h"
#include "testserver.h"
#include <QtTest>
#include <QDir>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSettings>
#include <QSslKey>

class AuthTests : public QObject {
    Q_OBJECT
    QTemporaryDir settingsRoot{QDir::currentPath() + "/auth-settings-XXXXXX"};
private slots:
    void initTestCase() {
        QVERIFY(settingsRoot.isValid());
        QCoreApplication::setOrganizationName("ChatTCP1-Auth-Tests"); QCoreApplication::setApplicationName("AuthTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsRoot.path());
    }
    void nativeIdentity() {
        QTemporaryDir dir(QDir::currentPath() + "/tls-test-XXXXXX"); QVERIFY(dir.isValid());
        QSslConfiguration tls, again; QString error;
        QVERIFY2(TlsIdentity::load(dir.path(), tls, error), qPrintable(error));
        QVERIFY(!tls.localCertificate().isNull()); QVERIFY(!tls.privateKey().isNull());
        QVERIFY2(TlsIdentity::load(dir.path(), again, error), qPrintable(error));
        QCOMPARE(tls.localCertificate().digest(), again.localCertificate().digest());
    }
    void passwordPersistence() {
        QTemporaryDir dir(QDir::currentPath() + "/account-test-XXXXXX"); QVERIFY(dir.isValid());
        QString id, error; const QString password = "Correct horse 123!";
        { AccountStore store; QVERIFY2(store.open(dir.filePath("accounts.sqlite"), error), qPrintable(error));
          const auto result = store.create("Alice", password); QVERIFY2(result.ok, qPrintable(result.error)); id = result.id;
          QVERIFY(!store.create("ALICE", "Another pass 123!").ok);
          QVERIFY(!store.login("Alice", "Wrong password 123!").ok); }
        AccountStore reopened; QVERIFY(reopened.open(dir.filePath("accounts.sqlite"), error));
        auto result = reopened.login("alice", password); QVERIFY2(result.ok, qPrintable(result.error)); QCOMPARE(result.id, id);
        QCOMPARE(result.name, "Alice");
    }
    void saltsAndSettings() {
        QTemporaryDir dir(QDir::currentPath() + "/hash-test-XXXXXX"); QVERIFY(dir.isValid());
        const QString file = dir.filePath("accounts.sqlite"); QString error;
        { AccountStore store; QVERIFY(store.open(file, error));
          QVERIFY(store.create("Alice", TestPassword).ok); QVERIFY(store.create("Bob", TestPassword).ok);
          QVERIFY(!store.create("Weak", "short").ok); }
        const QString name = QUuid::createUuid().toString();
        { auto db = QSqlDatabase::addDatabase("QSQLITE", name); db.setDatabaseName(file); QVERIFY(db.open());
          { QSqlQuery query(db); QVERIFY(query.exec("SELECT salt,password_hash,iterations,algorithm FROM accounts ORDER BY display_name"));
            QVERIFY(query.next()); const QByteArray firstSalt = query.value(0).toByteArray(), firstHash = query.value(1).toByteArray();
            QCOMPARE(firstSalt.size(), 16); QCOMPARE(firstHash.size(), 32); QCOMPARE(query.value(2).toInt(), 600000);
            QCOMPARE(query.value(3).toString(), "pbkdf2-hmac-sha256");
            QVERIFY(query.next()); QVERIFY(firstSalt != query.value(0).toByteArray()); QVERIFY(firstHash != query.value(1).toByteArray()); }
          db.close(); }
        QSqlDatabase::removeDatabase(name);
        ConnectionProfile profile{"Alice", "127.0.0.1", 12345, TestPassword, true};
        QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat); profile.save(settings);
        const auto restored = ConnectionProfile::load(settings); QVERIFY(restored.password.isEmpty()); QVERIFY(!restored.registerAccount);
        QFile saved(settings.fileName()); QVERIFY(saved.open(QIODevice::ReadOnly)); QVERIFY(!saved.readAll().contains(TestPassword.toUtf8()));
        QVERIFY(AuthProtocol::equalHash(QByteArray(32, 'x'), QByteArray(32, 'x')));
        QVERIFY(!AuthProtocol::equalHash(QByteArray(32, 'x'), QByteArray(32, 'y')));
    }
    void registerWrongPasswordAndLogin() {
        TestServer server; QVERIFY2(server.start(), qPrintable(server.startupError()));
        QSslSocket first; JsonChannel a(&first);
        SessionConnection registration(&first, &a, {"Alice", "127.0.0.1", server.port(), TestPassword, true}); trustFixture(registration, server);
        QString accountId;
        connect(&a, &JsonChannel::packetReceived, this, [&](const QJsonObject &p) { if (p.value("type") == "joined") accountId = p.value("account_id").toString(); });
        registration.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(registration.state(), SessionConnection::Ready, 15000);
        QVERIFY(first.isEncrypted()); QVERIFY(!accountId.isEmpty()); registration.stop(); QTest::qWait(50);
        QSslSocket second; JsonChannel b(&second);
        SessionConnection wrong(&second, &b, {"alice", "127.0.0.1", server.port(), "Wrong-password-123!", false}); trustFixture(wrong, server);
        QSignalSpy failed(&wrong, &SessionConnection::failed); wrong.connectNow(); QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty(), 15000);
        QCOMPARE(wrong.state(), SessionConnection::Idle);
        QSslSocket third; JsonChannel c(&third);
        SessionConnection login(&third, &c, {"ALICE", "127.0.0.1", server.port(), TestPassword, false}); trustFixture(login, server);
        QString loggedId;
        connect(&c, &JsonChannel::packetReceived, this, [&](const QJsonObject &p) { if (p.value("type") == "joined") loggedId = p.value("account_id").toString(); });
        login.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(login.state(), SessionConnection::Ready, 15000);
        QCOMPARE(loggedId, accountId); login.stop();
    }
    void trustRefusalAndPlaintextGuard() {
        TestServer server; QVERIFY(server.start());
        QSslSocket socket; JsonChannel channel(&socket);
        SessionConnection connection(&socket, &channel, {"Alice", "127.0.0.1", server.port(), TestPassword, true});
        connection.setTrustPrompt([](const QString &, const QString &) { return false; });
        QSignalSpy failed(&connection, &SessionConnection::failed);
        connection.connectNow(); QTRY_VERIFY(!failed.isEmpty()); QCOMPARE(connection.state(), SessionConnection::Idle);
        // A QSslSocket with only TCP connected must not send application JSON at all.
        QTcpServer raw; QVERIFY(raw.listen(QHostAddress::LocalHost, 0));
        QSslSocket unencrypted; JsonChannel guarded(&unencrypted);
        unencrypted.connectToHost("127.0.0.1", raw.serverPort()); QTRY_COMPARE(unencrypted.state(), QAbstractSocket::ConnectedState);
        QVERIFY(!guarded.send({{"type", "login"}, {"password", TestPassword}}));
        QTRY_VERIFY(raw.hasPendingConnections()); std::unique_ptr<QTcpSocket> peer(raw.nextPendingConnection()); QCOMPARE(peer->bytesAvailable(), 0);
    }
    void legacyJoinCannotAuthenticate() {
        TestServer server; QVERIFY(server.start());
        QSslSocket socket; JsonChannel channel(&socket); int errors = 0, joined = 0;
        connect(&socket, &QSslSocket::sslErrors, this, [&](const QList<QSslError> &list) {
            if (TlsIdentity::fingerprint(socket.peerCertificate()) == server.certificateFingerprint()) socket.ignoreSslErrors(list);
        });
        connect(&channel, &JsonChannel::packetReceived, this, [&](const QJsonObject &p) {
            if (p.value("type") == "error") ++errors; if (p.value("type") == "joined") ++joined;
        });
        socket.connectToHostEncrypted("127.0.0.1", server.port()); QTRY_VERIFY(socket.isEncrypted());
        QVERIFY(channel.send({{"type", "join"}, {"name", "Alice"}})); QTRY_COMPARE(errors, 1); QCOMPARE(joined, 0);
        QVERIFY(channel.send({{"type", "chat"}, {"text", "not authenticated"}})); QTRY_COMPARE(errors, 2); QCOMPARE(joined, 0);
    }
    void changedCertificateRequiresDecision() {
        TestServer original; QVERIFY(original.start()); const quint16 port = original.port();
        QSslSocket a; JsonChannel ca(&a);
        SessionConnection first(&a, &ca, {"Alice", "127.0.0.1", port, TestPassword, true}); trustFixture(first, original);
        first.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(first.state(), SessionConnection::Ready, 15000); first.stop(); original.shutdown();
        TestServer replacement; QVERIFY(replacement.start(port));
        QVERIFY(original.certificateFingerprint() != replacement.certificateFingerprint());
        QSslSocket b; JsonChannel cb(&b); SessionConnection second(&b, &cb, {"Alice", "127.0.0.1", port, TestPassword, false});
        bool checked = false;
        second.setTrustPrompt([&](const QString &current, const QString &previous) {
            checked = !previous.isEmpty() && current != previous; return false;
        });
        QSignalSpy failed(&second, &SessionConnection::failed);
        second.connectNow(); QTRY_VERIFY(!failed.isEmpty()); QVERIFY(checked); QCOMPARE(second.state(), SessionConnection::Idle);
    }
    void rateLimitAndResponsiveWorker() {
        QTemporaryDir dir(QDir::currentPath() + "/rate-test-XXXXXX"); QVERIFY(dir.isValid());
        AuthService service(dir.filePath("accounts.sqlite")); QString error; QVERIFY(service.initialize(error));
        QTcpSocket token; int count = 0, ticks = 0; QString lastError;
        connect(&service, &AuthService::finished, this, [&](QTcpSocket *, const AccountResult &result) {
            ++count; lastError = result.error;
        });
        QTimer heartbeat; heartbeat.setInterval(5); connect(&heartbeat, &QTimer::timeout, this, [&] { ++ticks; }); heartbeat.start();
        for (int i = 0; i < 10; ++i) {
            service.request(&token, "login", "NotRegistered", TestPassword);
            QTRY_COMPARE_WITH_TIMEOUT(count, i + 1, 15000);
        }
        QVERIFY(ticks > 10);
        service.request(&token, "login", "NotRegistered", TestPassword);
        QCOMPARE(count, 11); QVERIFY(lastError.contains("60"));
    }
    void simultaneousRegistration() {
        QTemporaryDir dir(QDir::currentPath() + "/race-test-XXXXXX"); QVERIFY(dir.isValid());
        AuthService service(dir.filePath("accounts.sqlite")); QString error; QVERIFY(service.initialize(error));
        QTcpSocket a, b; int count = 0, success = 0;
        connect(&service, &AuthService::finished, this, [&](QTcpSocket *, const AccountResult &result) { ++count; success += int(result.ok); });
        service.request(&a, "register", "SameName", TestPassword); service.request(&b, "register", "samename", TestPassword);
        QTRY_COMPARE_WITH_TIMEOUT(count, 2, 15000); QCOMPARE(success, 1);
    }
};
QTEST_MAIN(AuthTests)
#include "auth_tests.moc"
