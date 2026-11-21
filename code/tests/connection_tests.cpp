#include "testserver.h"
#include "connectiondialog.h"
#include "sessionconnection.h"
#include "chatserver.h"
#include "historystore.h"
#include <QtTest>
#include <QDialogButtonBox>
#include <QDir>
#include <QLineEdit>
#include <QLabel>
#include <QSettings>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QTemporaryDir>

class ConnectionTests : public QObject {
    Q_OBJECT
    QTemporaryDir settingsRoot{QDir::currentPath() + "/connection-settings-XXXXXX"};
private slots:
    void initTestCase() {
        QVERIFY(settingsRoot.isValid());
        QCoreApplication::setOrganizationName("ChatTCP1-Tests");
        QCoreApplication::setApplicationName("ConnectionTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsRoot.path());
    }
    void validation_data() {
        QTest::addColumn<QString>("name"); QTest::addColumn<QString>("host");
        QTest::addColumn<int>("port"); QTest::addColumn<bool>("valid");
        QTest::newRow("ipv4") << "Alice" << "192.168.1.10" << 12345 << true;
        QTest::newRow("localhost") << "Alice" << "localhost" << 1 << true;
        QTest::newRow("hostname") << "Alice" << "CHAT-PC.local" << 65535 << true;
        QTest::newRow("ipv6") << "Alice" << "[::1]" << 23456 << true;
        QTest::newRow("blank-name") << " " << "127.0.0.1" << 12345 << false;
        QTest::newRow("long-name") << QString(21, 'x') << "127.0.0.1" << 12345 << false;
        QTest::newRow("bad-name") << "A|B" << "127.0.0.1" << 12345 << false;
        QTest::newRow("blank-host") << "Alice" << " " << 12345 << false;
        QTest::newRow("url") << "Alice" << "http://127.0.0.1" << 12345 << false;
        QTest::newRow("combined-port") << "Alice" << "127.0.0.1:12345" << 12345 << false;
        QTest::newRow("bad-ip") << "Alice" << "999.999.1.1" << 12345 << false;
        QTest::newRow("listen-address") << "Alice" << "0.0.0.0" << 12345 << false;
        QTest::newRow("listen-v6") << "Alice" << "::" << 12345 << false;
        QTest::newRow("multicast") << "Alice" << "224.0.0.1" << 12345 << false;
        QTest::newRow("zero-port") << "Alice" << "127.0.0.1" << 0 << false;
        QTest::newRow("large-port") << "Alice" << "127.0.0.1" << 65536 << false;
    }
    void validation() {
        QFETCH(QString, name); QFETCH(QString, host); QFETCH(int, port); QFETCH(bool, valid);
        ConnectionProfile p; QString error;
        QCOMPARE(ConnectionProfile::validate(name, host, port, p, error), valid);
        QCOMPARE(error.isEmpty(), valid);
        if (valid) QVERIFY(p.port == port && !p.host.isEmpty());
    }
    void settingsAndHistoryIdentity() {
        QSettings settings(settingsRoot.filePath("last.ini"), QSettings::IniFormat);
        ConnectionProfile p; QString error;
        QVERIFY(ConnectionProfile::validate(" Alice ", "CHAT-PC.local.", 23456, p, error));
        QCOMPARE(p.host, "chat-pc.local"); QCOMPARE(p.username, "Alice");
        p.save(settings); const auto restored = ConnectionProfile::load(settings);
        QCOMPARE(restored.host, p.host); QCOMPARE(restored.port, p.port); QCOMPARE(restored.username, p.username);
        const QString path = HistoryStore::accountPath(settingsRoot.path(), p.host, p.port, p.username);
        QVERIFY(path != HistoryStore::accountPath(settingsRoot.path(), p.host, 12345, p.username));
        settings.setValue("connection/port", -1);
        QCOMPARE(ConnectionProfile::load(settings).port, quint16(12345));
        QVERIFY(ConnectionProfile::validate("Alice", "[::1]", 12345, p, error)); QCOMPARE(p.endpoint(), "[::1]:12345");
    }
    void dialogValidationAndRemember() {
        ConnectionDialog dialog;
        auto *name = dialog.findChild<QLineEdit *>("usernameInput"); auto *host = dialog.findChild<QLineEdit *>("hostInput");
        auto *port = dialog.findChild<QSpinBox *>("portInput");
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        QVERIFY(name && host && port && buttons);
        dialog.findChild<QLineEdit *>("passwordInput")->setText(TestPassword);
        name->setText("Alice"); host->setText("http://bad"); port->setValue(23456);
        buttons->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(dialog.result(), 0); QVERIFY(!dialog.findChild<QLabel *>("connectionError")->text().isEmpty());
        host->setText("127.0.0.1"); buttons->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted)); QCOMPARE(dialog.profile().port, quint16(23456));
        ConnectionDialog next; QCOMPARE(next.findChild<QSpinBox *>("portInput")->value(), 23456);
        QCOMPARE(next.findChild<QLineEdit *>("usernameInput")->text(), "Alice");
        QVERIFY(next.findChild<QLineEdit *>("passwordInput")->text().isEmpty());
        QCOMPARE(next.findChild<QLineEdit *>("passwordInput")->echoMode(), QLineEdit::Password);
    }
    void customPortAndReconnect() {
        TestServer server; QVERIFY(server.start(0));
        QSslSocket socket; JsonChannel channel(&socket);
        SessionConnection connection(&socket, &channel, {"Alice", "127.0.0.1", server.port(), TestPassword, true});
        trustFixture(connection, server);
        QSignalSpy failures(&connection, &SessionConnection::failed);
        connection.connectNow(); QTRY_COMPARE(connection.state(), SessionConnection::Ready);
        connection.connectNow(); QCOMPARE(connection.state(), SessionConnection::Ready); // no duplicate JOIN
        socket.abort(); QTRY_COMPARE(connection.state(), SessionConnection::Idle);
        QTest::qWait(50); connection.connectNow(); QTRY_COMPARE(connection.state(), SessionConnection::Ready);
        QCOMPARE(failures.size(), 0); connection.stop();
    }
    void registrationConfirmation() {
        ConnectionDialog dialog;
        dialog.findChild<QComboBox *>("authMode")->setCurrentIndex(1);
        dialog.findChild<QLineEdit *>("usernameInput")->setText("NewAccount");
        dialog.findChild<QLineEdit *>("emailInput")->setText("new@example.com");
        dialog.findChild<QLineEdit *>("hostInput")->setText("127.0.0.1");
        dialog.findChild<QLineEdit *>("passwordInput")->setText(TestPassword);
        dialog.findChild<QLineEdit *>("confirmPasswordInput")->setText("Different-password-123!");
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        buttons->button(QDialogButtonBox::Ok)->click();
        QVERIFY(dialog.result() != QDialog::Accepted);
        QVERIFY(dialog.findChild<QLabel *>("connectionError")->text().contains("chua khop"));
        dialog.findChild<QLineEdit *>("confirmPasswordInput")->setText(TestPassword);
        buttons->button(QDialogButtonBox::Ok)->click(); QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(dialog.profile().registerAccount); QCOMPARE(dialog.profile().password, TestPassword);
        ConnectionDialog next;
        QCOMPARE(next.findChild<QComboBox *>("authMode")->currentIndex(), 0);
        QVERIFY(next.findChild<QLineEdit *>("passwordInput")->text().isEmpty());
    }
    void refusedThenRetry() {
        QTcpServer reserve; QVERIFY(reserve.listen(QHostAddress::LocalHost, 0)); const quint16 port = reserve.serverPort(); reserve.close();
        QSslSocket socket; JsonChannel channel(&socket); SessionConnection connection(&socket, &channel, {"Alice", "127.0.0.1", port, TestPassword, true}, nullptr, 10000);
        QSignalSpy errors(&connection, &SessionConnection::failed);
        connection.connectNow(); QTRY_VERIFY(!errors.isEmpty()); QCOMPARE(connection.state(), SessionConnection::Idle);
        TestServer server; QVERIFY(server.start(port));
        trustFixture(connection, server);
        connection.connectNow(); QTRY_COMPARE(connection.state(), SessionConnection::Ready); connection.stop();
    }
    void hostnameConnection() {
        TestServer server; QVERIFY(server.start(0));
        QSslSocket socket; JsonChannel channel(&socket);
        SessionConnection connection(&socket, &channel, {"Alice", "localhost", server.port(), TestPassword, true});
        trustFixture(connection, server);
        connection.connectNow(); QTRY_COMPARE(connection.state(), SessionConnection::Ready); connection.stop();
    }
    void duplicateNameAndRetry() {
        TestServer server; QVERIFY(server.start(0));
        QSslSocket a, b; JsonChannel ca(&a), cb(&b);
        SessionConnection first(&a, &ca, {"Alice", "127.0.0.1", server.port(), TestPassword, true});
        SessionConnection second(&b, &cb, {"Alice", "127.0.0.1", server.port(), TestPassword, false});
        trustFixture(first, server); trustFixture(second, server);
        QSignalSpy errors(&second, &SessionConnection::failed);
        first.connectNow(); QTRY_COMPARE(first.state(), SessionConnection::Ready);
        second.connectNow(); QTRY_VERIFY(!errors.isEmpty()); QCOMPARE(second.state(), SessionConnection::Idle);
        first.stop(); QTest::qWait(50);
        second.connectNow(); QTRY_COMPARE(second.state(), SessionConnection::Ready); second.stop();
    }
    void handshakeTimeoutAndStop() {
        QTcpServer silent; QVERIFY(silent.listen(QHostAddress::LocalHost, 0));
        QSslSocket socket; JsonChannel channel(&socket);
        SessionConnection connection(&socket, &channel, {"Alice", "127.0.0.1", silent.serverPort(), TestPassword, true}, nullptr, 100);
        QSignalSpy errors(&connection, &SessionConnection::failed);
        connection.connectNow(); QTRY_VERIFY(!errors.isEmpty()); QCOMPARE(connection.state(), SessionConnection::Idle);
        errors.clear(); connection.connectNow(); connection.stop(); QTest::qWait(200);
        QCOMPARE(connection.state(), SessionConnection::Idle); QCOMPARE(errors.size(), 0);
    }
};
QTEST_MAIN(ConnectionTests)
#include "connection_tests.moc"

