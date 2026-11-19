#include "gmailsender.h"
#include "authprotocol.h"
#include "gmailconfig.h"
#include <QSslSocket>
#include <QElapsedTimer>
#include <QSslConfiguration>

namespace { thread_local QString smtpError; }
QString GmailSender::lastError() {return smtpError;}

bool GmailSender::configured() {
    QString user; QByteArray password;
    const bool stored = GmailConfig::load(user, password);
    password.fill('\0');
    if (stored) return true;
    auto legacy = qgetenv("CHATTCP_GMAIL_APP_PASSWORD"); legacy.replace(" ", "");
    const bool configured = AuthProtocol::validEmail(qEnvironmentVariable("CHATTCP_GMAIL_USER")) && legacy.size() == 16;
    legacy.fill('\0');
    return configured;
}
bool GmailSender::send(const QString &recipient,const QString &code,const QString &purpose) {
    QString user; QByteArray password;
    if (!GmailConfig::load(user, password)) {
        user = AuthProtocol::emailKey(qEnvironmentVariable("CHATTCP_GMAIL_USER"));
        password = qgetenv("CHATTCP_GMAIL_APP_PASSWORD");
        password.replace(" ", "");
    }
    const bool result = sendWithSettings(user, password, recipient, code, purpose,
                                          "smtp.gmail.com", 465, QSslConfiguration::defaultConfiguration());
    password.fill('\0');
    return result;
}
bool GmailSender::sendWithSettings(const QString &user,QByteArray password,const QString &recipient,const QString &code,const QString &purpose,const QString &host,quint16 port,const QSslConfiguration &configuration,const QString &peerName) {
    smtpError="Email hoặc Mật khẩu ứng dụng Gmail chưa đúng định dạng (cần 16 ký tự).";
    if(!AuthProtocol::validEmail(user) || !AuthProtocol::validEmail(recipient) || password.size()!=16) return false;
    QSslSocket socket; QElapsedTimer deadline;deadline.start();
    auto remaining=[&]{return qMax(0,30000-int(deadline.elapsed()));};
    auto conf=configuration;conf.setProtocol(QSsl::TlsV1_2OrLater);socket.setSslConfiguration(conf);
    socket.setPeerVerifyMode(QSslSocket::VerifyPeer);
    socket.setPeerVerifyName(peerName.isEmpty()?host:peerName);
    smtpError="Không kết nối TLS tới Gmail. Kiểm tra Internet, cổng 465, ngày giờ Windows và chứng chỉ hệ thống.";
    socket.connectToHostEncrypted(host,port);
    if(!socket.waitForEncrypted(remaining())) return false; // Never ignore certificate errors.
    QByteArray buffer;
    auto response=[&](int expected) {
        int lines=0;
        while(remaining()>0 && ++lines<=100) {
            while(!buffer.contains('\n')) {
                if(buffer.size()>16384 || (socket.bytesAvailable()==0 && !socket.waitForReadyRead(remaining()))) return false;
                buffer+=socket.readAll(); if(buffer.size()>16384) return false;
            }
            const int end=buffer.indexOf('\n');const auto line=buffer.left(end+1);buffer.remove(0,end+1);
            if(line.size()<5 || line.left(3).toInt()!=expected) return false;
            if(line[3]==' ') return true;
            if(line[3]!='-') return false;
        }
        return false;
    };
    auto command=[&](const QByteArray &data,int expected) {
        if(socket.write(data+"\r\n")<0) return false;
        while(socket.bytesToWrite()>0) if(!socket.waitForBytesWritten(remaining())) return false;
        return response(expected);
    };
    smtpError="Gmail không trả lời SMTP hoặc không chấp nhận AUTH LOGIN.";
    if(!response(220) || !command("EHLO chattcp.local",250) || !command("AUTH LOGIN",334)) {password.fill('\0');return false;}
    smtpError="Gmail từ chối xác thực hoặc hết thời gian chờ. Kiểm tra Gmail quản trị và Mật khẩu ứng dụng 16 ký tự do Google cấp.";
    if(!command(user.toUtf8().toBase64(),334) || !command(password.toBase64(),235)) {password.fill('\0');return false;}
    password.fill('\0');
    smtpError="Gmail từ chối địa chỉ gửi/nhận hoặc bước chuẩn bị thư. Kiểm tra email và giới hạn gửi Gmail.";
    if(!command("MAIL FROM:<"+user.toUtf8()+">",250) || !command("RCPT TO:<"+recipient.toUtf8()+">",250) || !command("DATA",354)) return false;
    const auto body=QString("ChatTCP verification code: %1\nPurpose: %2\nExpires in 10 minutes. Never share this code.\nIf you did not request this, ignore this email.").arg(code,purpose).toUtf8().toBase64();
    const auto message="From: ChatTCP <"+user.toUtf8()+">\r\nTo: <"+recipient.toUtf8()+">\r\nSubject: ChatTCP verification code\r\nMIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\nContent-Transfer-Encoding: base64\r\n\r\n"+body;
    // Base64 is short, line-wrapped, and cannot inject SMTP terminators.
    QByteArray wrapped;const auto split=message.indexOf("\r\n\r\n");wrapped=message.left(split+4);
    for(int i=0;i<body.size();i+=76) wrapped+=body.mid(i,76)+"\r\n";
    smtpError="Gmail chưa xác nhận nhận thư (có thể bị từ chối hoặc hết thời gian chờ).";
    const bool ok=command(wrapped+".",250);if(ok)smtpError.clear();socket.disconnectFromHost();return ok;
}
