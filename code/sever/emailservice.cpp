#include "emailservice.h"
#include "gmailsender.h"
#include "accountstore.h"
#include "authprotocol.h"
#include <QDateTime>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QUuid>
#include <QRunnable>
EmailService::EmailService(QString file,QObject *parent):QObject(parent),path(std::move(file)),sender(GmailSender::send),clock([]{return QDateTime::currentMSecsSinceEpoch();}) {pool.setMaxThreadCount(2);}
EmailService::~EmailService(){pool.waitForDone();}
void EmailService::disconnected(QTcpSocket *client){challenges.remove(client);}
bool EmailService::allow(const QString &key,int maximum,qint64 now) {
    auto &l=limits[key];if(now-l.start>=3600000) l={now,0,0};
    if(l.count>=maximum || (l.last && now-l.last<60000)) return false;
    l.last=now;++l.count;return true;
}
void EmailService::begin(QTcpSocket *client,const QJsonObject &packet) {
    auto reject=[&](const QString &text){emit reply(client,{{"type","error"},{"text",text}});};
    const auto name=packet.value("name").toString(),email=AuthProtocol::emailKey(packet.value("email").toString()),purpose=packet.value("purpose").toString();
    if(!AuthProtocol::validName(name)||!AuthProtocol::validEmail(email)||(purpose!="register"&&purpose!="reset"&&purpose!="bind")) {reject("Thông tin email không hợp lệ.");return;}
    const auto now=clock();
    for(auto i=limits.begin();i!=limits.end();) if(now-i->start>=3600000) i=limits.erase(i);else ++i;
    if(now-window>=3600000){window=now;total=0;}
    // Hard cap includes pending SMTP tasks; clients cannot build an unbounded queue.
    if(active>=8 || total>=100 || limits.size()>=4094 || challenges.contains(client)) {reject("Yêu cầu đang xử lý hoặc quá nhiều yêu cầu. Hãy thử lại sau.");return;}
    if(!allow("ip:"+client->peerAddress().toString(),20,now)||!allow("email:"+email,5,now)) {reject("Vui lòng chờ ít nhất 60 giây; tối đa 5 mã/email mỗi giờ.");return;}
    if(sender.target<bool(*)(const QString&,const QString&,const QString&)>() && (qEnvironmentVariableIsEmpty("CHATTCP_GMAIL_USER")||qEnvironmentVariableIsEmpty("CHATTCP_GMAIL_APP_PASSWORD"))) {reject("Server chưa cấu hình Gmail. Hãy liên hệ quản trị viên.");return;}
    Challenge c;c.name=name;c.email=email;c.purpose=purpose;c.id=QUuid::createUuid().toString(QUuid::WithoutBraces);c.expires=now+600000;
    // 8 digits generated with rejection sampling by Qt's bounded CSPRNG.
    const auto code=QString::number(QRandomGenerator::system()->bounded(100000000u)).rightJustified(8,'0');
    c.digest=QCryptographicHash::hash((c.id+code).toUtf8(),QCryptographicHash::Sha256);
    challenges.insert(client,c);++active;++total;QPointer<QTcpSocket> guard(client);auto send=sender;
    pool.start(QRunnable::create([this,guard,c,code,send]() mutable {
        bool eligible=true;QByteArray credential;
        if(c.purpose=="reset") {AccountStore store;QString error;eligible=store.open(path,error);if(eligible){credential=store.emailCredential(c.name,c.email);eligible=credential.size()==32;}}
        const bool sent=eligible && send(c.email,code,c.purpose);
        if(eligible && !sent) qWarning("ChatTCP: Gmail delivery failed. Check Internet, TLS, Gmail user and App Password. No code or credentials logged.");
        QMetaObject::invokeMethod(this,[this,guard,c,credential,eligible,sent]{
            --active;if(!guard || !challenges.contains(guard.data()) || challenges.value(guard.data()).id!=c.id)return;
            auto &current=challenges[guard.data()];current.credential=credential;current.eligible=eligible&&sent;current.ready=true;
            // Same payload for nonexistent account/email and SMTP failure; do not disclose account existence explicitly.
            emit reply(guard.data(),{{"type","email_challenge"},{"challenge_id",c.id},{"text","Nếu thông tin hợp lệ, mã đã được gửi. Kiểm tra cả thư rác. Mã có hiệu lực 10 phút."}});
        },Qt::QueuedConnection);
    }));
}
void EmailService::confirm(QTcpSocket *client,const QJsonObject &packet) {
    auto fail=[&]{emit reply(client,{{"type","error"},{"text","Mã không đúng, hết hạn hoặc đã dùng. Hãy xin mã mới nếu cần."}});};
    auto i=challenges.find(client);
    if(i==challenges.end() || !i->ready){fail();return;}
    if(clock()>=i->expires || i->attempts>=5) {challenges.erase(i);fail();return;}
    ++i->attempts;const auto code=packet.value("code").toString();
    bool digits=code.size()==8;for(auto ch:code) digits=digits&&ch>='0'&&ch<='9';
    const auto digest=QCryptographicHash::hash((i->id+code).toUtf8(),QCryptographicHash::Sha256);
    if(packet.value("challenge_id").toString()!=i->id || !digits || !i->eligible || !AuthProtocol::equalHash(digest,i->digest)) {if(i->attempts>=5)challenges.erase(i);fail();return;}
    const auto c=*i;challenges.erase(i); // Consume before expensive KDF, never replay or retry automatically.
    const auto password=packet.value("password").toString();if(!AuthProtocol::validPassword(password)){fail();return;}
    emit verified(client,c.purpose,c.name,c.email,password,c.credential);
}
