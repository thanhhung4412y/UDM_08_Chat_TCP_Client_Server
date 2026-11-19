#include "sessionconnection.h"
#include "authprotocol.h"
#include "videoprotocol.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QSettings>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSslConfiguration>

SessionConnection::SessionConnection(QSslSocket *s, JsonChannel *c, ConnectionProfile p, QObject *parent, int timeoutMs)
    : QObject(parent), socket(s), channel(c), profile(std::move(p)) {
    authTimeoutMs=timeoutMs;
    recoveryTimer.setSingleShot(true); recoveryTimer.setInterval(30000);
    connect(&recoveryTimer,&QTimer::timeout,this,[this] {
        recoveryRequest.clear(); emit recoveryFailed("Chưa xác nhận được mã mới. Tạo lại để thay thế mã chưa nhận được.");
    });
    timer.setSingleShot(true); timer.setInterval(timeoutMs);
    passwordTimer.setSingleShot(true); passwordTimer.setInterval(30000);
    connect(&passwordTimer, &QTimer::timeout, this, [this] {
        fail("Chua xac nhan duoc viec doi mat khau. Dang nhap lai bang mat khau moi; neu khong duoc, thu mat khau cu. Khong tu gui lai yeu cau.");
    });
    connect(&timer, &QTimer::timeout, this, [this] { fail("Het thoi gian cho ket noi/dang nhap. Kiem tra Server va thu lai."); });
    connect(socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
        for (const auto &error : errors) {
            switch (error.error()) {
            case QSslError::SelfSignedCertificate: case QSslError::SelfSignedCertificateInChain:
            case QSslError::HostNameMismatch: case QSslError::CertificateUntrusted:
            case QSslError::UnableToGetLocalIssuerCertificate: case QSslError::UnableToVerifyFirstCertificate: break;
            default: fail("Loi chung chi TLS: " + error.errorString()); return;
            }
        }
        if (verifyPeer() && current == Connecting && socket->state() == QAbstractSocket::ConnectedState)
            socket->ignoreSslErrors(errors); // Only the inspected errors, for the explicitly pinned leaf.
    });
    connect(socket, &QSslSocket::encrypted, this, [this] {
        if (current != Connecting || !verifyPeer()) return;
        QSettings settings; settings.setValue(pinKey(), verifiedFingerprint); settings.sync();
        current = Registering;
        emit changed(current, "TLS da ket noi " + profile.endpoint() + " - Dang xac thuc tai khoan...");
        if(profile.registerAccount || profile.emailReset || profile.emailBind) {
            timer.start(60000);
            if(!channel->send({{"type","email_begin"},{"name",profile.username},{"email",profile.email},{"purpose",profile.registerAccount?"register":profile.emailReset?"reset":"bind"}}))fail("Không gửi được yêu cầu xác minh email.");
        } else if (profile.resetPassword) {
            resetRequest=QUuid::createUuid().toString(QUuid::WithoutBraces);
            if (!channel->send({{"type","recovery_reset"},{"request_id",resetRequest},
                {"name",profile.username},{"code",profile.recoveryCode},{"new_password",profile.password}})) fail("Không gửi được yêu cầu khôi phục.");
        } else if (!channel->send({{"type", profile.registerAccount ? "register" : "login"},
                            {"name", profile.username}, {"password", profile.password}})) fail("Khong gui duoc yeu cau dang nhap.");
    });
    connect(channel, &JsonChannel::packetReceived, this, [this](const QJsonObject &p) {
        const QString type = p.value("type").toString();
        if(current==Registering && (profile.registerAccount||profile.emailReset||profile.emailBind)) {
            if(type=="email_challenge") {
                timer.stop();bool ok=false;
                const auto id=p.value("challenge_id").toString();
                if(!VideoProtocol::validId(id)){fail("Phản hồi email không hợp lệ.");return;}
                const auto code=emailCodePrompt ? emailCodePrompt() : QInputDialog::getText(nullptr,"Xác minh email","Nếu thông tin hợp lệ, mã được gửi đến email đã nhập.\nKiểm tra thư rác. Nhập mã 8 chữ số (hiệu lực 10 phút):",QLineEdit::Normal,{},&ok).trimmed();
                if((!emailCodePrompt && !ok) || current!=Registering) {if(current==Registering)fail("Đã hủy xác minh email.");return;}
                bool digits=code.size()==8;for(auto ch:code)digits=digits&&ch>='0'&&ch<='9';
                if(!digits){fail("Mã xác minh cần đúng 8 chữ số. Hãy thực hiện lại.");return;}
                timer.start(30000);
                if(!channel->send({{"type","email_confirm"},{"challenge_id",id},{"code",code},{"password",profile.password}}))fail("Không gửi được mã.");
                return;
            }
            if(type=="email_bound" || (profile.emailReset && type=="recovery_reset_ok")) {
                timer.stop();current=Idle;profile.password.fill(QChar(0));profile.password.clear();socket->abort();
                const QString message=type=="email_bound"?"Đã xác minh và liên kết email. Hãy đăng nhập.":"Đã đặt mật khẩu mới. Đăng nhập lại và tạo mã dự phòng mới nếu cần.";
                emit changed(current,message);emit accountSetupFinished(message);return;
            }
            if(type=="recovery_error") {fail(p.value("text").toString());return;}
        }
        if (current==Registering && profile.resetPassword) {
            if (type=="recovery_reset_ok" && p.value("request_id").toString()==resetRequest) {
                timer.stop(); current=Idle; profile.password.fill(QChar(0)); profile.password.clear();
                profile.recoveryCode.fill(QChar(0)); profile.recoveryCode.clear(); socket->abort();
                emit changed(current,"Đã đặt mật khẩu mới. Hãy đăng nhập lại."); emit passwordReset();
            } else if (type=="recovery_error" || type=="error") fail(p.value("text").toString().left(300));
            return;
        }
        if (current==Ready && !recoveryRequest.isEmpty() && p.value("request_id").toString()==recoveryRequest) {
            if (type=="recovery_code" || type=="recovery_error") {
                recoveryRequest.clear(); recoveryTimer.stop();
                if (type=="recovery_code" && AuthProtocol::validRecovery(p.value("code").toString())) emit recoveryCreated(p.value("code").toString());
                else emit recoveryFailed(p.value("text").toString().left(300));
                return;
            }
        }
        if (type == "session_revoked" && current == Ready) {
            profile.password.fill(QChar(0)); profile.password.clear();
            fail("Phien dang nhap da bi thu hoi. Hay dang nhap lai."); return;
        }
        if (current == Ready && !passwordRequest.isEmpty() && p.value("request_id").toString() == passwordRequest) {
            if (type == "password_changed") {
                passwordTimer.stop(); passwordRequest.clear();
                profile.password.fill(QChar(0)); profile.password.clear();
                current = Idle; socket->abort();
                emit changed(current, "Da doi mat khau. Hay dang nhap lai bang mat khau moi.");
                emit passwordChanged(); return;
            }
            if (type == "password_change_error") {
                passwordTimer.stop(); passwordRequest.clear();
                emit passwordChangeFailed(p.value("text").toString().left(300)); return;
            }
        }
        if (current != Registering) return;
        if (p.value("type") == "joined") {
            if (AuthProtocol::canonical(profile.username) != AuthProtocol::canonical(p.value("name").toString())
                || !VideoProtocol::validId(p.value("account_id").toString())) {
                fail("Server xac nhan sai tai khoan."); return;
            }
            timer.stop(); current = Ready; profile.registerAccount = false;
            emit changed(current, "Da dang nhap: " + profile.username + " @ " + profile.endpoint() + " (TLS)");
        } else if (p.value("type") == "error") fail(p.value("text").toString().left(300));
    });
    connect(socket, &QTcpSocket::disconnected, this, [this] {
        timer.stop();
        if (!recoveryRequest.isEmpty()) { recoveryTimer.stop(); recoveryRequest.clear(); emit recoveryFailed("Mất kết nối. Mã có thể đã được tạo; đăng nhập lại và tạo mã khác."); }
        if (!passwordRequest.isEmpty()) {
            passwordTimer.stop(); passwordRequest.clear(); profile.password.fill(QChar(0)); profile.password.clear();
            emit passwordChangeFailed("Mat ket noi khi doi mat khau. Viec doi co the da hoan tat; dang nhap lai bang mat khau moi truoc, khong tu gui lai.");
        }
        if (current != Idle) { current = Idle; emit changed(current, "Mat ket noi " + profile.endpoint() + ". Bam Ket noi lai hoac Doi ket noi."); }
    });
    connect(socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (current != Idle) fail("Khong ket noi duoc " + profile.endpoint() + ": " + socket->errorString());
    });
    connect(channel, &JsonChannel::transportError, this, [this](const QString &text) { if (current != Idle) fail(text); });
}
QString SessionConnection::pinKey() const {
    return "tlsPins/" + QString::fromLatin1(QCryptographicHash::hash(profile.endpoint().toUtf8(), QCryptographicHash::Sha256).toHex());
}
bool SessionConnection::verifyPeer() {
    const auto certificate = socket->peerCertificate();
    if (certificate.isNull() || certificate.effectiveDate() > QDateTime::currentDateTimeUtc()
        || certificate.expiryDate() <= QDateTime::currentDateTimeUtc()) { fail("Chung chi TLS trong/het han/chua co hieu luc."); return false; }
    const QString fingerprint = QString::fromLatin1(certificate.digest(QCryptographicHash::Sha256).toHex(':')).toUpper();
    if (fingerprint == verifiedFingerprint) return true;
    QSettings settings; const QString previous = settings.value(pinKey()).toString();
    if (previous == fingerprint) { verifiedFingerprint = fingerprint; return true; }
    timer.stop();
    const bool accepted = trustPrompt && trustPrompt(fingerprint, previous);
    if (!accepted || current != Connecting) { if (current != Idle) fail("Chua xac minh danh tinh Server; khong gui mat khau."); return false; }
    verifiedFingerprint = fingerprint; timer.start(); return true;
}
void SessionConnection::connectNow() {
    if (current != Idle) return;
    if (profile.resetPassword && !AuthProtocol::validRecovery(profile.recoveryCode)) { fail("Nhập lại mã dự phòng và mật khẩu mới ở màn hình tài khoản."); return; }
    if (!AuthProtocol::validPassword(profile.password)) { fail("Can mat khau 8-128 ky tu. Bam Doi ket noi de nhap lai."); return; }
    if (!QSslSocket::supportsSsl()) { fail("Qt chua co TLS backend. Kiem tra thu muc tls."); return; }
    socket->abort(); verifiedFingerprint.clear(); current = Connecting;
    auto configuration = socket->sslConfiguration(); configuration.setProtocol(QSsl::TlsV1_2OrLater);
    configuration.setPeerVerifyMode(QSslSocket::VerifyPeer); socket->setSslConfiguration(configuration);
    emit changed(current, "Dang ket noi TLS " + profile.endpoint() + "...");
    timer.start(authTimeoutMs); socket->connectToHostEncrypted(profile.host, profile.port);
}
bool SessionConnection::changePassword(const QString &currentPassword, const QString &newPassword) {
    if (current != Ready || !socket->isEncrypted()) { emit passwordChangeFailed("Can dang nhap truoc khi doi mat khau."); return false; }
    if (!passwordRequest.isEmpty() || !recoveryRequest.isEmpty()) { emit passwordChangeFailed("Dang xu ly yeu cau doi mat khau."); return false; }
    if (!AuthProtocol::validPassword(currentPassword) || !AuthProtocol::validPassword(newPassword) || currentPassword == newPassword) {
        emit passwordChangeFailed("Mat khau moi phai co 8-128 ky tu va khac mat khau hien tai."); return false;
    }
    passwordRequest = QUuid::createUuid().toString(QUuid::WithoutBraces);
    passwordTimer.start();
    if (!channel->send({{"type", "change_password"}, {"request_id", passwordRequest},
                        {"current_password", currentPassword}, {"new_password", newPassword}})) {
        if (!passwordRequest.isEmpty()) fail("Khong gui duoc yeu cau doi mat khau. Hay dang nhap lai.");
        return false;
    }
    return true;
}
bool SessionConnection::createRecovery(const QString &password) {
    if (current!=Ready || !socket->isEncrypted() || !passwordRequest.isEmpty() || !recoveryRequest.isEmpty() || !AuthProtocol::validPassword(password)) {
        emit recoveryFailed("Cần đăng nhập, mật khẩu hiện tại 8–128 ký tự và chờ yêu cầu trước hoàn tất."); return false;
    }
    recoveryRequest=QUuid::createUuid().toString(QUuid::WithoutBraces); recoveryTimer.start();
    if (!channel->send({{"type","recovery_setup"},{"request_id",recoveryRequest},{"current_password",password}})) {
        recoveryTimer.stop(); recoveryRequest.clear(); emit recoveryFailed("Không gửi được yêu cầu tạo mã."); return false;
    }
    return true;
}
void SessionConnection::stop() { recoveryTimer.stop(); recoveryRequest.clear(); timer.stop(); passwordTimer.stop(); current = Idle; socket->abort(); }
void SessionConnection::fail(const QString &reason) {
    if (profile.resetPassword) { profile.recoveryCode.fill(QChar(0)); profile.recoveryCode.clear(); }
    if (!recoveryRequest.isEmpty()) { recoveryTimer.stop(); recoveryRequest.clear(); emit recoveryFailed(reason); }
    if (!passwordRequest.isEmpty()) {
        passwordTimer.stop(); passwordRequest.clear(); profile.password.fill(QChar(0)); profile.password.clear();
        emit passwordChangeFailed(reason);
    }
    timer.stop(); current = Idle; socket->abort();
    const QString message = profile.resetPassword ? reason + " Nếu đã gửi yêu cầu, Server có thể đã đổi xong: thử đăng nhập bằng mật khẩu mới. Mã có thể đã dùng; không tự gửi lại. Bấm Đổi kết nối để nhập lại." : reason + " Bam Ket noi lai hoac Doi ket noi.";
    emit changed(current, message); emit failed(message);
}
