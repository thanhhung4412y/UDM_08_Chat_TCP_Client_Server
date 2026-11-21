#include "testserver.h"
#include "passworddialog.h"
#include "authprotocol.h"
#include "historystore.h"
#include <QtTest>
#include <QSettings>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSqlQuery>

inline const QString NewPassword = "New-only-test-password-456!";
class PasswordTests : public QObject {
    Q_OBJECT
    QTemporaryDir settingsRoot{QDir::currentPath() + "/password-settings-XXXXXX"};
private slots:
    void initTestCase() {
        QVERIFY(settingsRoot.isValid()); QCoreApplication::setOrganizationName("ChatTCP1-Password-Tests");
        QCoreApplication::setApplicationName("PasswordTests"); QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsRoot.path());
    }
    void eightCharacterPasswords() {
        QVERIFY(!AuthProtocol::validPassword("1234567"));
        QVERIFY(AuthProtocol::validPassword("12345678"));
        QVERIFY(AuthProtocol::validPassword(QString(128,'x')));
        QVERIFY(!AuthProtocol::validPassword(QString(129,'x')));
        QString nul(8,'x');nul[3]=QChar(0);QVERIFY(!AuthProtocol::validPassword(nul));
        QTemporaryDir dir(QDir::currentPath()+"/password-eight-XXXXXX");QString error;
        const auto file=dir.filePath("accounts.sqlite");
        {
            AccountStore store;QVERIFY(store.open(file,error));
            QVERIFY(!store.create("TooShort","1234567").ok);
            QVERIFY(store.create("Eight","12345678","eight@example.com").ok);
            QVERIFY(store.login("Eight","12345678").ok);
            QVERIFY(!store.changePassword("Eight","12345678","1234567").ok);
            QVERIFY(store.changePassword("Eight","12345678","abcdefgh").ok);
            QVERIFY(store.login("Eight","abcdefgh").ok);
            const auto backup=store.createRecovery("Eight","abcdefgh");QVERIFY(backup.ok);
            QVERIFY(store.resetPassword("Eight",backup.recoveryCode,"87654321").ok);
            auto credential=store.emailCredential("Eight","eight@example.com");
            QVERIFY(!store.resetEmail("Eight","eight@example.com","1234567",credential).ok);
            QVERIFY(store.resetEmail("Eight","eight@example.com","hgfedcba",credential).ok);
        }
        AccountStore reopened;QVERIFY(reopened.open(file,error));QVERIFY(reopened.login("Eight","hgfedcba").ok);
    }
    void atomicChangeAndHistoryPreserved() {
        QTemporaryDir dir(QDir::currentPath() + "/password-db-XXXXXX"); QString error;
        const QString file = dir.filePath("accounts.sqlite");
        AccountResult oldSnapshot, changed;
        {
            AccountStore store; QVERIFY(store.open(file, error));
            oldSnapshot = store.create("Alice", TestPassword); QVERIFY(oldSnapshot.ok);
            QVERIFY(!store.changePassword("Alice", "Wrong-password-123!", NewPassword).ok);
            QVERIFY(!store.changePassword("Alice", TestPassword, TestPassword).ok);
            QVERIFY(!store.changePassword("Alice", TestPassword, "short").ok);
            QVERIFY(store.login("Alice", TestPassword).ok);
            HistoryStore history; QVERIFY(history.open(HistoryStore::accountPath(dir.path(), "127.0.0.1", 12345, "Alice"), error));
            HistoryRecord record; QCOMPARE(history.append({{"type", "chat"}, {"sender", "Alice"}, {"text", "keep-history"}}, record, error), HistoryStore::SaveResult::Saved);
            changed = store.changePassword("alice", TestPassword, NewPassword); QVERIFY2(changed.ok, qPrintable(changed.error));
            QCOMPARE(changed.id, oldSnapshot.id); QCOMPARE(changed.name, oldSnapshot.name);
            QVERIFY(changed.credentialToken != oldSnapshot.credentialToken);
            QVERIFY(!store.isCurrent(oldSnapshot)); QVERIFY(store.isCurrent(changed));
            QVERIFY(!store.login("Alice", TestPassword).ok); QVERIFY(store.login("Alice", NewPassword).ok);
            QCOMPARE(history.page(100, 0, error).records.first().packet.value("text"), "keep-history");
        }
        AccountStore reopened; QVERIFY(reopened.open(file, error));
        QCOMPARE(reopened.login("ALICE", NewPassword).id, oldSnapshot.id);
        QVERIFY(!reopened.login("Alice", TestPassword).ok);
    }
    void tlsChangeLogsOutAndRequiresNewPassword() {
        TestServer server; QVERIFY(server.start());
        QSslSocket socket; JsonChannel channel(&socket);
        SessionConnection connection(&socket, &channel, {"Alice", "127.0.0.1", server.port(), TestPassword, true}); trustFixture(connection, server);
        connection.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(connection.state(), SessionConnection::Ready, 15000);
        QSignalSpy failure(&connection, &SessionConnection::passwordChangeFailed);
        QSignalSpy success(&connection, &SessionConnection::passwordChanged);
        QVERIFY(connection.changePassword("Wrong-password-123!", NewPassword));
        QTRY_COMPARE_WITH_TIMEOUT(failure.size(), 1, 15000); QCOMPARE(connection.state(), SessionConnection::Ready);
        QVERIFY(connection.changePassword(TestPassword, NewPassword));
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 15000); QCOMPARE(connection.state(), SessionConnection::Idle);
        connection.connectNow(); QCOMPARE(connection.state(), SessionConnection::Idle); // old password cleared in memory
        QSslSocket a; JsonChannel ca(&a); SessionConnection oldLogin(&a, &ca, {"Alice", "127.0.0.1", server.port(), TestPassword, false}); trustFixture(oldLogin, server);
        QSignalSpy oldFailed(&oldLogin, &SessionConnection::failed); oldLogin.connectNow(); QTRY_VERIFY_WITH_TIMEOUT(!oldFailed.isEmpty(), 15000);
        QSslSocket b; JsonChannel cb(&b); SessionConnection newLogin(&b, &cb, {"Alice", "127.0.0.1", server.port(), NewPassword, false}); trustFixture(newLogin, server);
        newLogin.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(newLogin.state(), SessionConnection::Ready, 15000); newLogin.stop();
    }
    void cannotSelectAnotherAccount() {
        TestServer server; QVERIFY(server.start()); QString error;
        { AccountStore store; QVERIFY(store.open(QDir(server.data.path()).filePath("accounts.sqlite"), error)); QVERIFY(store.create("Bob", TestPassword).ok); }
        QSslSocket socket; JsonChannel channel(&socket); SessionConnection connection(&socket, &channel, {"Alice", "127.0.0.1", server.port(), TestPassword, true}); trustFixture(connection, server);
        connection.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(connection.state(), SessionConnection::Ready, 15000);
        bool changed = false;
        connect(&channel, &JsonChannel::packetReceived, this, [&](const QJsonObject &p) { if (p.value("type") == "password_changed") changed = true; });
        QVERIFY(channel.send({{"type", "change_password"}, {"request_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"name", "Bob"}, {"current_password", TestPassword}, {"new_password", NewPassword}}));
        QTRY_VERIFY_WITH_TIMEOUT(changed, 15000);
        AccountStore store; QVERIFY(store.open(QDir(server.data.path()).filePath("accounts.sqlite"), error));
        QVERIFY(store.login("Alice", NewPassword).ok); QVERIFY(store.login("Bob", TestPassword).ok);
        QVERIFY(!store.login("Bob", NewPassword).ok); connection.stop();
    }
    void simultaneousChangesOnlyOneWins() {
        QTemporaryDir dir(QDir::currentPath() + "/password-race-XXXXXX"); QString error;
        const QString file = dir.filePath("accounts.sqlite");
        { AccountStore store; QVERIFY(store.open(file, error)); QVERIFY(store.create("Alice", TestPassword).ok); }
        AuthService service(file); QVERIFY(service.initialize(error));
        QTcpSocket a, b; int count = 0, successes = 0;
        connect(&service, &AuthService::passwordFinished, this, [&](QTcpSocket *, const AccountResult &result) { ++count; successes += int(result.ok); });
        service.request(&a, "change_password", "Alice", TestPassword, NewPassword);
        service.request(&b, "change_password", "Alice", TestPassword, "Other-new-password-789!");
        QTRY_COMPARE_WITH_TIMEOUT(count, 2, 20000); QCOMPARE(successes, 1);
        AccountStore check; QVERIFY(check.open(file, error)); QVERIFY(!check.login("Alice", TestPassword).ok);
        const bool first = check.login("Alice", NewPassword).ok;
        const bool second = check.login("Alice", "Other-new-password-789!").ok; QVERIFY(first != second);
    }
    void disconnectDoesNotLoseCommittedResult() {
        QTemporaryDir dir(QDir::currentPath() + "/password-disconnect-XXXXXX"); QString error;
        const QString file = dir.filePath("accounts.sqlite");
        { AccountStore store; QVERIFY(store.open(file, error)); QVERIFY(store.create("Alice", TestPassword).ok); }
        AuthService service(file); QVERIFY(service.initialize(error));
        QTcpSocket socket; int success = 0;
        connect(&service, &AuthService::passwordFinished, this, [&](QTcpSocket *, const AccountResult &result) { success += int(result.ok); });
        service.request(&socket, "change_password", "Alice", TestPassword, NewPassword);
        service.disconnected(&socket); QTRY_COMPARE_WITH_TIMEOUT(success, 1, 15000);
        AccountStore check; QVERIFY(check.open(file, error)); QVERIFY(check.login("Alice", NewPassword).ok);
    }
    void formValidationAndNoNetwork() {
        QSslSocket socket; JsonChannel channel(&socket); SessionConnection connection(&socket, &channel, {"Alice", "127.0.0.1", 12345, TestPassword, false});
        PasswordDialog dialog(&connection, [] { return true; });
        auto *old = dialog.findChild<QLineEdit *>("currentPassword"); auto *next = dialog.findChild<QLineEdit *>("newPassword");
        auto *confirm = dialog.findChild<QLineEdit *>("confirmNewPassword"); auto *submit = dialog.findChild<QPushButton *>("submitPasswordChange");
        QVERIFY(old && next && confirm && submit); QCOMPARE(old->echoMode(), QLineEdit::Password);
        old->setText(TestPassword); next->setText(NewPassword); confirm->setText(TestPassword); submit->click();
        QVERIFY(dialog.findChild<QLabel *>("passwordStatus")->text().contains("chua khop"));
        confirm->setText(NewPassword); submit->click();
        QVERIFY(dialog.findChild<QLabel *>("passwordStatus")->text().contains("dang nhap")); QVERIFY(submit->isEnabled());
    }
    void unauthenticatedRequestDenied() {
        TestServer server; QVERIFY(server.start()); QString error;
        { AccountStore store; QVERIFY(store.open(QDir(server.data.path()).filePath("accounts.sqlite"), error)); QVERIFY(store.create("Alice", TestPassword).ok); }
        QSslSocket socket; JsonChannel channel(&socket); bool denied = false;
        connect(&socket, &QSslSocket::sslErrors, this, [&](const QList<QSslError> &list) {
            if (TlsIdentity::fingerprint(socket.peerCertificate()) == server.certificateFingerprint()) socket.ignoreSslErrors(list);
        });
        connect(&channel, &JsonChannel::packetReceived, this, [&](const QJsonObject &p) { if (p.value("type") == "password_change_error") denied = true; });
        socket.connectToHostEncrypted("127.0.0.1", server.port()); QTRY_VERIFY(socket.isEncrypted());
        QVERIFY(channel.send({{"type", "change_password"}, {"request_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"name", "Alice"}, {"current_password", TestPassword}, {"new_password", NewPassword}}));
        QTRY_VERIFY(denied);
        AccountStore check; QVERIFY(check.open(QDir(server.data.path()).filePath("accounts.sqlite"), error));
        QVERIFY(check.login("Alice", TestPassword).ok); QVERIFY(!check.login("Alice", NewPassword).ok);
    }
    void lostConnectionRequiresManualLogin() {
        TestServer server; QVERIFY(server.start());
        QSslSocket socket; JsonChannel channel(&socket); SessionConnection connection(&socket, &channel, {"Alice", "127.0.0.1", server.port(), TestPassword, true}); trustFixture(connection, server);
        connection.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(connection.state(), SessionConnection::Ready, 15000);
        QSignalSpy failed(&connection, &SessionConnection::passwordChangeFailed), success(&connection, &SessionConnection::passwordChanged);
        QVERIFY(connection.changePassword(TestPassword, NewPassword)); socket.abort();
        QTRY_COMPARE(failed.size(), 1); QCOMPARE(success.size(), 0); QCOMPARE(connection.state(), SessionConnection::Idle);
        connection.connectNow(); QCOMPARE(connection.state(), SessionConnection::Idle); // no cached old credential replay
    }
};
QTEST_MAIN(PasswordTests)
#include "password_tests.moc"
