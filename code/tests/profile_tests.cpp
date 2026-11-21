#include "testserver.h"
#include "authprotocol.h"
#include "avatarprotocol.h"
#include "connectiondialog.h"
#include "passworddialog.h"
#include "chatshell.h"
#include <QtTest>
#include <QAction>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSqlQuery>
#include <QListWidget>

static const QString NewPassword="New-recovered-password-456!";
static QByteArray jpeg() {
    QImage image(128,128,QImage::Format_RGB32); image.fill(Qt::green);
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"JPEG",80); return bytes;
}
class ProfileTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings{QDir::currentPath()+"/profile-settings-XXXXXX"};
private slots:
    void initTestCase() {
        QVERIFY(settings.isValid()); QCoreApplication::setOrganizationName("ChatTCP1-Profile-Tests"); QCoreApplication::setApplicationName("ProfileTests");
        QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    }
    void recoveryRotationConsumptionAndPersistence() {
        QTemporaryDir dir(QDir::currentPath()+"/profile-db-XXXXXX"); QVERIFY(dir.isValid()); QString error;
        QString id,code; const auto file=dir.filePath("accounts.sqlite");
        {
            AccountStore store; QVERIFY(store.open(file,error)); const auto created=store.create("Alice",TestPassword); QVERIFY(created.ok); id=created.id;
            QVERIFY(!store.createRecovery("Alice","Wrong-password-123!").ok);
            const auto first=store.createRecovery("Alice",TestPassword); QVERIFY(first.ok); QVERIFY(AuthProtocol::validRecovery(first.recoveryCode));
            const auto next=store.createRecovery("Alice",TestPassword); QVERIFY(next.ok); code=next.recoveryCode; QVERIFY(code!=first.recoveryCode);
            QVERIFY(!store.resetPassword("Alice",first.recoveryCode,NewPassword).ok);
            QVERIFY(!store.resetPassword("Alice",QString(64,'0'),NewPassword).ok);
            QVERIFY(!store.resetPassword("Bob",code,NewPassword).ok);
            QVERIFY(!store.resetPassword("Alice",code,"short").ok);
            QVERIFY(store.login("Alice",TestPassword).ok);
            QVERIFY(store.setAvatar("Alice",jpeg()));
        }
        AccountStore store; QVERIFY(store.open(file,error)); const auto result=store.resetPassword("ALICE",code.toUpper(),NewPassword);
        QVERIFY2(result.ok,qPrintable(result.error)); QCOMPARE(result.id,id); QVERIFY(!store.login("Alice",TestPassword).ok); QVERIFY(store.login("Alice",NewPassword).ok);
        QVERIFY(!store.resetPassword("Alice",code,"Other-new-password-789!").ok); QVERIFY(!store.avatar("Alice").isEmpty());
        auto fresh=store.createRecovery("Alice",NewPassword); QVERIFY(fresh.ok);
        QVERIFY(store.changePassword("Alice",NewPassword,TestPassword).ok);
        QVERIFY(!store.resetPassword("Alice",fresh.recoveryCode,NewPassword).ok);
        QVERIFY(store.setAvatar("Alice",{})); QVERIFY(store.avatar("Alice").isEmpty());
    }
    void simultaneousResetsConsumeOnce() {
        QTemporaryDir dir(QDir::currentPath()+"/profile-race-XXXXXX"); QString error,code; const auto file=dir.filePath("accounts.sqlite");
        { AccountStore store; QVERIFY(store.open(file,error)); QVERIFY(store.create("Alice",TestPassword).ok); code=store.createRecovery("Alice",TestPassword).recoveryCode; }
        AuthService service(file); QVERIFY(service.initialize(error)); QTcpSocket a,b; int count=0,success=0;
        connect(&service,&AuthService::recoveryFinished,this,[&](QTcpSocket *,const AccountResult &result,bool reset) { QVERIFY(reset); ++count; success+=int(result.ok); });
        service.request(&a,"recovery_reset","Alice",code,NewPassword); service.request(&b,"recovery_reset","Alice",code,"Other-new-password-789!");
        QTRY_COMPARE_WITH_TIMEOUT(count,2,20000); QCOMPARE(success,1);
    }
    void passwordVisibilityAndResetForm() {
        ConnectionDialog dialog;
        for(const auto &name : {"passwordInput","confirmPasswordInput","recoveryCodeInput"}) {
            auto *field=dialog.findChild<QLineEdit *>(name); QVERIFY(field); QCOMPARE(field->echoMode(),QLineEdit::Password);
            field->setText("example"); auto *action=field->findChild<QAction *>("toggle_"+field->objectName()); QVERIFY(action);
            action->trigger(); QCOMPARE(field->echoMode(),QLineEdit::Normal); QCOMPARE(field->text(),"example"); action->trigger(); QCOMPARE(field->echoMode(),QLineEdit::Password);
        }
        dialog.findChild<QComboBox *>("authMode")->setCurrentIndex(2);
        QVERIFY(!dialog.findChild<QLineEdit *>("recoveryCodeInput")->isHidden()); QVERIFY(!dialog.findChild<QLineEdit *>("confirmPasswordInput")->isHidden());
        QSslSocket socket; JsonChannel channel(&socket); SessionConnection connection(&socket,&channel,{"Alice","localhost",12345,TestPassword,false});
        PasswordDialog change(&connection,[]{return true;});
        for(const auto &name : {"currentPassword","newPassword","confirmNewPassword"}) {
            auto *field=change.findChild<QLineEdit *>(name); QVERIFY(field); auto *action=field->findChild<QAction *>("toggle_"+field->objectName()); QVERIFY(action); action->trigger(); QCOMPARE(field->echoMode(),QLineEdit::Normal);
        }
    }
    void avatarValidation() {
        QVERIFY(!AvatarProtocol::decode(jpeg()).isNull()); QVERIFY(AvatarProtocol::decode(QByteArray(16385,'x')).isNull());
        QVERIFY(AvatarProtocol::decode("not-jpeg").isNull()); QVERIFY(AvatarProtocol::fromBase64("!!!!").isEmpty());
        QImage large(129,128,QImage::Format_RGB32); large.fill(Qt::red); QByteArray data; QBuffer b(&data); b.open(QIODevice::WriteOnly); large.save(&b,"JPEG"); QVERIFY(AvatarProtocol::decode(data).isNull());
        ChatShell window("Alice","localhost"); window.setUsers({"Alice","Bob"}); window.setAvatar("Bob",jpeg()); window.setAvatar("Alice",jpeg());
        QVERIFY(!window.onlineList->item(0)->icon().isNull()); window.setAvatar("Bob",{});
    }
    void tlsRecoveryRevokesOldSession() {
        TestServer server; QVERIFY(server.start());
        QSslSocket socket; JsonChannel channel(&socket); SessionConnection connection(&socket,&channel,{"Alice","127.0.0.1",server.port(),TestPassword,true}); trustFixture(connection,server);
        connection.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(connection.state(),SessionConnection::Ready,15000);
        QSignalSpy codes(&connection,&SessionConnection::recoveryCreated), errors(&connection,&SessionConnection::recoveryFailed);
        QVERIFY(connection.createRecovery("Wrong-password-123!")); QTRY_COMPARE_WITH_TIMEOUT(errors.size(),1,15000); QCOMPARE(connection.state(),SessionConnection::Ready);
        QVERIFY(connection.createRecovery(TestPassword)); QTRY_COMPARE_WITH_TIMEOUT(codes.size(),1,15000); const QString code=codes.first().first().toString();
        QSslSocket resetSocket; JsonChannel resetChannel(&resetSocket); ConnectionProfile profile{"Alice","127.0.0.1",server.port(),NewPassword,false}; profile.resetPassword=true; profile.recoveryCode=code;
        SessionConnection reset(&resetSocket,&resetChannel,profile); trustFixture(reset,server); QSignalSpy success(&reset,&SessionConnection::passwordReset);
        reset.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(success.size(),1,15000); QCOMPARE(reset.state(),SessionConnection::Idle); QTRY_COMPARE_WITH_TIMEOUT(connection.state(),SessionConnection::Idle,5000);
        QSslSocket newSocket; JsonChannel newChannel(&newSocket); SessionConnection login(&newSocket,&newChannel,{"Alice","127.0.0.1",server.port(),NewPassword,false}); trustFixture(login,server);
        login.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(login.state(),SessionConnection::Ready,15000); login.stop();
    }
    void tlsAvatarBoundToSessionAndPersisted() {
        TestServer server; QVERIFY(server.start()); QSslSocket socket; JsonChannel channel(&socket);
        SessionConnection connection(&socket,&channel,{"Alice","127.0.0.1",server.port(),TestPassword,true}); trustFixture(connection,server);
        QList<QJsonObject> packets; connect(&channel,&JsonChannel::packetReceived,this,[&](const QJsonObject &p){packets.append(p);});
        connection.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(connection.state(),SessionConnection::Ready,15000);
        auto result=[&](const QString &request) { for(const auto &p:packets) if(p.value("type")=="avatar_result" && p.value("request_id")==request) return p; return QJsonObject(); };
        QString request=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(channel.send({{"type","avatar_set"},{"request_id",request},{"data","invalid"}})); QTRY_VERIFY_WITH_TIMEOUT(!result(request).isEmpty(),5000); QVERIFY(!result(request).value("ok").toBool());
        QTest::qWait(2100); request=QUuid::createUuid().toString(QUuid::WithoutBraces); const auto bytes=jpeg();
        QVERIFY(channel.send({{"type","avatar_set"},{"request_id",request},{"name","Bob"},{"data",QString::fromLatin1(bytes.toBase64())}}));
        QTRY_VERIFY_WITH_TIMEOUT(!result(request).isEmpty(),5000); QVERIFY(result(request).value("ok").toBool());
        { AccountStore store; QString error; QVERIFY(store.open(QDir(server.data.path()).filePath("accounts.sqlite"),error)); QCOMPARE(store.avatar("Alice"),bytes); QVERIFY(store.avatar("Bob").isEmpty()); }
        connection.stop(); QTRY_COMPARE(socket.state(),QAbstractSocket::UnconnectedState);
        QSslSocket other; JsonChannel otherChannel(&other); SessionConnection login(&other,&otherChannel,{"Alice","127.0.0.1",server.port(),TestPassword,false}); trustFixture(login,server);
        bool received=false; connect(&otherChannel,&JsonChannel::packetReceived,this,[&](const QJsonObject &p) { if(p.value("type")=="avatar" && p.value("name")=="Alice" && p.value("data")==QString::fromLatin1(bytes.toBase64())) received=true; });
        login.connectNow(); QTRY_COMPARE_WITH_TIMEOUT(login.state(),SessionConnection::Ready,15000); QTRY_VERIFY_WITH_TIMEOUT(received,5000); login.stop();
    }
};
QTEST_MAIN(ProfileTests)
#include "profile_tests.moc"
