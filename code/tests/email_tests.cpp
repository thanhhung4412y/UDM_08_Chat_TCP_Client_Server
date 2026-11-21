#include "accountstore.h"
#include "authprotocol.h"
#include "emailservice.h"
#include "gmailsender.h"
#include <atomic>
#include <QRunnable>
#include "connectiondialog.h"
#include "testserver.h"
#include <QtTest>
#include <QMutex>
#include <QMutexLocker>
#include <QComboBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>

struct Mailbox {
    QMutex mutex;QHash<QString,QString> codes;
    bool send(const QString &email,const QString &code,const QString &){QMutexLocker lock(&mutex);codes[email]=code;return true;}
    QString code(const QString &email){QMutexLocker lock(&mutex);return codes.value(email);}
};
struct MailPeer {
    QSslSocket socket;JsonChannel channel{&socket};QList<QJsonObject> packets;
    MailPeer(const ChatServer &server){
        QObject::connect(&socket,&QSslSocket::sslErrors,&socket,[&,expected=server.certificateFingerprint()](const QList<QSslError>&e){if(TlsIdentity::fingerprint(socket.peerCertificate())==expected)socket.ignoreSslErrors(e);});
        QObject::connect(&channel,&JsonChannel::packetReceived,&socket,[this](const QJsonObject&p){packets.append(p);});
        socket.connectToHostEncrypted("127.0.0.1",server.port());
    }
    QJsonObject last(const QString &type) const {for(auto i=packets.crbegin();i!=packets.crend();++i)if(i->value("type")==type)return *i;return {};}
};
class EmailTests:public QObject {
    Q_OBJECT
private slots:
    void validationAndDialog(){
        QVERIFY(AuthProtocol::validEmail("user+chat@example.com"));
        for(const auto &s:{"a@localhost","a\r\nBcc:x@y.com","a..b@example.com",".a@example.com","a.@example.com","a@-bad.com"})QVERIFY(!AuthProtocol::validEmail(s));
        ConnectionDialog dialog;dialog.findChild<QComboBox*>("authMode")->setCurrentIndex(1);
        dialog.findChild<QLineEdit*>("usernameInput")->setText("Alice");
        dialog.findChild<QLineEdit*>("passwordInput")->setText(TestPassword);dialog.findChild<QLineEdit*>("confirmPasswordInput")->setText(TestPassword);
        auto *buttons=dialog.findChild<QDialogButtonBox*>();buttons->button(QDialogButtonBox::Ok)->click();QCOMPARE(dialog.result(),0);
        dialog.findChild<QLineEdit*>("emailInput")->setText(" Alice@Example.com ");buttons->button(QDialogButtonBox::Ok)->click();QCOMPARE(dialog.result(),int(QDialog::Accepted));QCOMPARE(dialog.profile().email,"alice@example.com");
    }
    void databaseMigrationAndCAS(){
        QTemporaryDir dir(QDir::currentPath()+"/email-case-XXXXXX");QString error;const auto file=dir.filePath("accounts.sqlite");QByteArray credential;
        {AccountStore s;QVERIFY(s.open(file,error));QVERIFY(s.create("Alice",TestPassword).ok);QVERIFY(s.emailCredential("Alice","alice@example.com").isEmpty());
        QVERIFY(!s.bindEmail("Alice","alice@example.com","Incorrect-password-123").ok);
        QVERIFY(s.bindEmail("Alice","alice@example.com",TestPassword).ok);
        QVERIFY(!s.bindEmail("Alice","other@example.com",TestPassword).ok);
        QVERIFY(!s.create("Bob",TestPassword,"alice@example.com").ok);QVERIFY(!s.login("Bob",TestPassword).ok);
        credential=s.emailCredential("ALICE","ALICE@EXAMPLE.COM");QCOMPARE(credential.size(),32);
        QVERIFY(!s.resetEmail("Alice","other@example.com","Changed-password-123",credential).ok);
        QVERIFY(s.changePassword("Alice",TestPassword,"Changed-password-123").ok);
        QVERIFY(!s.resetEmail("Alice","alice@example.com","Another-password-123",credential).ok);
        credential=s.emailCredential("Alice","alice@example.com");QVERIFY(s.resetEmail("Alice","alice@example.com","Email-reset-password-123",credential).ok);
        QVERIFY(!s.resetEmail("Alice","alice@example.com","Replay-password-123",credential).ok);}
        AccountStore again;QVERIFY(again.open(file,error));QVERIFY(again.login("Alice","Email-reset-password-123").ok);QCOMPARE(again.emailCredential("Alice","alice@example.com").size(),32);
    }
    void failedDeliveryStopsRegistrationAndBinding(){
        QTemporaryDir dir(QDir::currentPath()+"/mail-failure-XXXXXX");EmailService service(dir.filePath("accounts.sqlite"));qint64 now=100000000;
        service.setClockForTests([&]{return now;});service.setSenderForTests([](auto,auto,auto){return false;});
        QTcpSocket socket;QList<QJsonObject> replies;int diagnosticCount=0;
        connect(&service,&EmailService::reply,this,[&](QTcpSocket*,const QJsonObject&p){replies.append(p);});
        connect(&service,&EmailService::deliveryStatus,this,[&]{++diagnosticCount;});
        for(const auto &purpose:{"register","bind"}) {
            replies.clear();service.begin(&socket,{{"name","Alice"},{"email","alice@example.com"},{"purpose",purpose}});
            QTRY_VERIFY(!replies.isEmpty());QCOMPARE(replies.last().value("type").toString(),"error");QVERIFY(!replies.last().contains("challenge_id"));now+=61000;
        }
        QCOMPARE(diagnosticCount,2);
        replies.clear();service.begin(&socket,{{"name","Unknown"},{"email","unknown@example.com"},{"purpose","reset"}});
        QTRY_VERIFY(!replies.isEmpty());QCOMPARE(replies.last().value("type").toString(),"email_challenge");
    }
    void challengeBoundsReplayExpiry(){
        QTemporaryDir dir(QDir::currentPath()+"/email-case-XXXXXX");EmailService service(dir.filePath("accounts.sqlite"));Mailbox box;qint64 now=100000000;
        service.setClockForTests([&]{return now;});service.setSenderForTests([&](auto a,auto b,auto c){return box.send(a,b,c);});
        QTcpSocket a,b;QList<QJsonObject> replies;int verified=0;
        connect(&service,&EmailService::reply,this,[&](QTcpSocket*,const QJsonObject&p){replies.append(p);});
        connect(&service,&EmailService::verified,this,[&]{++verified;});
        const QJsonObject begin{{"name","Alice"},{"email","alice@example.com"},{"purpose","register"}};
        service.begin(&a,begin);QTRY_VERIFY(!replies.isEmpty());auto id=replies.last().value("challenge_id").toString();QVERIFY(!id.isEmpty());
        service.confirm(&b,{{"challenge_id",id},{"code",box.code("alice@example.com")},{"password",TestPassword}});QCOMPARE(verified,0);
        for(int i=0;i<5;++i)service.confirm(&a,{{"challenge_id",id},{"code","bad"},{"password",TestPassword}});
        service.confirm(&a,{{"challenge_id",id},{"code",box.code("alice@example.com")},{"password",TestPassword}});QCOMPARE(verified,0);
        now+=61000;replies.clear();service.begin(&a,begin);QTRY_VERIFY(!replies.isEmpty());id=replies.last().value("challenge_id").toString();
        now+=600001;service.confirm(&a,{{"challenge_id",id},{"code",box.code("alice@example.com")},{"password",TestPassword}});QCOMPARE(verified,0);
        replies.clear();service.begin(&a,begin);QTRY_VERIFY(!replies.isEmpty());id=replies.last().value("challenge_id").toString();
        const QJsonObject confirm{{"challenge_id",id},{"code",box.code("alice@example.com")},{"password",TestPassword}};
        service.confirm(&a,confirm);QCOMPARE(verified,1);service.confirm(&a,confirm);QCOMPARE(verified,1);
        service.begin(&b,begin);QCOMPARE(replies.last().value("type").toString(),"error");
    }
    void smtpTransportTLS(){
        QTemporaryDir dir(QDir::currentPath()+"/smtp-fixture-XXXXXX");QString error;QSslConfiguration identity;
        QVERIFY(TlsIdentity::load(dir.path(),identity,error));QSslServer smtp;smtp.setSslConfiguration(identity);QVERIFY(smtp.listen(QHostAddress::LocalHost,0));
        QByteArray delivered;bool rejectAuth=false;int authenticated=0;
        connect(&smtp,&QSslServer::pendingConnectionAvailable,this,[&]{
            auto *peer=smtp.nextPendingConnection();auto buffer=std::make_shared<QByteArray>();auto state=std::make_shared<int>(0);
            peer->write("220 fixture SMTP ready\r\n");
            connect(peer,&QTcpSocket::readyRead,peer,[&,peer,buffer,state]{
                *buffer+=peer->readAll();
                if(*state==7){const int end=buffer->indexOf("\r\n.\r\n");if(end>=0){delivered=buffer->left(end);buffer->clear();*state=8;peer->write("250 queued\r\n");}return;}
                while(buffer->contains('\n')){
                    const int end=buffer->indexOf('\n');auto line=buffer->left(end+1).trimmed();buffer->remove(0,end+1);
                    if(*state==0){if(line!="EHLO chattcp.local"){peer->abort();return;}peer->write("250-fixture\r\n250 AUTH LOGIN\r\n");}
                    else if(*state==1){if(line!="AUTH LOGIN"){peer->abort();return;}peer->write("334 user\r\n");}
                    else if(*state==2){if(QByteArray::fromBase64(line)!="admin@example.com"){peer->abort();return;}peer->write("334 pass\r\n");}
                    else if(*state==3){if(QByteArray::fromBase64(line)!="abcdefghijklmnop"){peer->abort();return;}++authenticated;peer->write(rejectAuth?"535 denied\r\n":"235 accepted\r\n");}
                    else if(*state==4){if(line!="MAIL FROM:<admin@example.com>"){peer->abort();return;}peer->write("250 sender\r\n");}
                    else if(*state==5){if(line!="RCPT TO:<alice@example.com>"){peer->abort();return;}peer->write("250 recipient\r\n");}
                    else if(*state==6){if(line!="DATA"){peer->abort();return;}peer->write("354 send data\r\n");}
                    ++*state;
                }
            });connect(peer,&QTcpSocket::disconnected,peer,&QObject::deleteLater);
        });
        auto trusted=QSslConfiguration::defaultConfiguration();trusted.addCaCertificate(identity.localCertificate());
        auto run=[&](const QSslConfiguration &tls){
            std::atomic_bool done=false,result=false;QThreadPool pool;
            pool.start(QRunnable::create([&]{result=GmailSender::sendWithSettings("admin@example.com","abcdefghijklmnop","alice@example.com","12345678","register","localhost",smtp.serverPort(),tls,"ChatTCP1 Server");done=true;}));
            QElapsedTimer deadline;deadline.start();while(!done && deadline.elapsed()<35000){QCoreApplication::processEvents();QTest::qWait(5);}pool.waitForDone();return result.load();
        };
        QVERIFY(run(trusted));QCOMPARE(authenticated,1);QVERIFY(delivered.contains("Subject: ChatTCP verification code"));QVERIFY(!delivered.contains("abcdefghijklmnop"));
        const auto body=delivered.mid(delivered.indexOf("\r\n\r\n")+4);QVERIFY(QByteArray::fromBase64(body).contains("12345678"));
        delivered.clear();rejectAuth=true;QVERIFY(!run(trusted));QVERIFY(delivered.isEmpty());QCOMPARE(authenticated,2);
        // Default trust must reject our self-signed fixture before sending authentication.
        QVERIFY(!run(QSslConfiguration::defaultConfiguration()));QCOMPARE(authenticated,2);
    }
    void sessionEmailFlow(){
        TestServer server;QVERIFY(server.start());auto *mail=server.findChild<EmailService*>();Mailbox box;qint64 now=QDateTime::currentMSecsSinceEpoch();
        mail->setClockForTests([&]{return now;});mail->setSenderForTests([&](auto a,auto b,auto c){return box.send(a,b,c);});
        ConnectionProfile profile{"SessionUser","127.0.0.1",server.port(),"Pass1234",true};profile.email="session@example.com";
        QSslSocket socket;JsonChannel channel(&socket);SessionConnection registration(&socket,&channel,profile);
        registration.setTrustPrompt([&](auto actual,auto){return actual==server.certificateFingerprint();});
        registration.setEmailCodePrompt([&]{return box.code(profile.email);});registration.connectNow();
        QTRY_COMPARE_WITH_TIMEOUT(registration.state(),SessionConnection::Ready,15000);registration.stop();QTest::qWait(50);
        now+=61000;profile.registerAccount=false;profile.emailReset=true;profile.password="Reset123";
        QSslSocket resetSocket;JsonChannel resetChannel(&resetSocket);SessionConnection reset(&resetSocket,&resetChannel,profile);
        reset.setTrustPrompt([&](auto actual,auto){return actual==server.certificateFingerprint();});reset.setEmailCodePrompt([&]{return box.code(profile.email);});
        QSignalSpy done(&reset,&SessionConnection::accountSetupFinished);reset.connectNow();QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,15000);QCOMPARE(reset.state(),SessionConnection::Idle);
    }
    void tlsRegisterBindResetAndRevocation(){
        TestServer server;QVERIFY(server.start());auto *service=server.findChild<EmailService*>();QVERIFY(service);
        Mailbox box;qint64 now=QDateTime::currentMSecsSinceEpoch();service->setClockForTests([&]{return now;});service->setSenderForTests([&](auto a,auto b,auto c){return box.send(a,b,c);});
        MailPeer alice(server);QTRY_VERIFY(alice.socket.isEncrypted());
        alice.channel.send({{"type","register"},{"name","Alice"},{"password",TestPassword}});QTRY_VERIFY(!alice.last("error").isEmpty());QVERIFY(alice.last("joined").isEmpty());
        alice.channel.send({{"type","email_begin"},{"name","Alice"},{"email","alice@example.com"},{"purpose","register"}});QTRY_VERIFY(!alice.last("email_challenge").isEmpty());
        alice.channel.send({{"type","email_confirm"},{"challenge_id",alice.last("email_challenge").value("challenge_id")},{"code",box.code("alice@example.com")},{"password",TestPassword}});
        QTRY_VERIFY_WITH_TIMEOUT(!alice.last("joined").isEmpty(),15000);
        {AccountStore s;QString e;QVERIFY(s.open(QDir(server.dataPath()).filePath("accounts.sqlite"),e));QVERIFY(s.create("Legacy",TestPassword).ok);}
        now+=61000;MailPeer legacy(server);QTRY_VERIFY(legacy.socket.isEncrypted());legacy.channel.send({{"type","email_begin"},{"name","Legacy"},{"email","legacy@example.com"},{"purpose","bind"}});QTRY_VERIFY(!legacy.last("email_challenge").isEmpty());
        legacy.channel.send({{"type","email_confirm"},{"challenge_id",legacy.last("email_challenge").value("challenge_id")},{"code",box.code("legacy@example.com")},{"password",TestPassword}});QTRY_VERIFY_WITH_TIMEOUT(!legacy.last("email_bound").isEmpty(),15000);QVERIFY(legacy.last("joined").isEmpty());
        now+=61000;MailPeer unknown(server);QTRY_VERIFY(unknown.socket.isEncrypted());unknown.channel.send({{"type","email_begin"},{"name","Missing"},{"email","missing@example.com"},{"purpose","reset"}});QTRY_VERIFY(!unknown.last("email_challenge").isEmpty());QVERIFY(box.code("missing@example.com").isEmpty());
        now+=61000;MailPeer reset(server);QTRY_VERIFY(reset.socket.isEncrypted());reset.channel.send({{"type","email_begin"},{"name","Alice"},{"email","alice@example.com"},{"purpose","reset"}});QTRY_VERIFY(!reset.last("email_challenge").isEmpty());
        reset.channel.send({{"type","email_confirm"},{"challenge_id",reset.last("email_challenge").value("challenge_id")},{"code",box.code("alice@example.com")},{"password","New-email-password-123"}});
        QTRY_VERIFY_WITH_TIMEOUT(!reset.last("recovery_reset_ok").isEmpty(),15000);QTRY_VERIFY(!alice.last("session_revoked").isEmpty());
        {AccountStore s;QString e;QVERIFY(s.open(QDir(server.dataPath()).filePath("accounts.sqlite"),e));QVERIFY(!s.login("Alice",TestPassword).ok);QVERIFY(s.login("Alice","New-email-password-123").ok);}
    }
};
QTEST_MAIN(EmailTests)
#include "email_tests.moc"
