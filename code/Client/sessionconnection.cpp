#include "sessionconnection.h"
#include "authprotocol.h"
#include "videoprotocol.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QSettings>
#include <QSslConfiguration>

SessionConnection::SessionConnection(QSslSocket *s, JsonChannel *c, ConnectionProfile p, QObject *parent, int timeoutMs)
    : QObject(parent), socket(s), channel(c), profile(std::move(p)) {
    timer.setSingleShot(true); timer.setInterval(timeoutMs);
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
        if (!channel->send({{"type", profile.registerAccount ? "register" : "login"},
                            {"name", profile.username}, {"password", profile.password}})) fail("Khong gui duoc yeu cau dang nhap.");
    });
    connect(channel, &JsonChannel::packetReceived, this, [this](const QJsonObject &p) {
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
    if (!AuthProtocol::validPassword(profile.password)) { fail("Can mat khau 12-128 ky tu. Bam Doi ket noi de nhap lai."); return; }
    if (!QSslSocket::supportsSsl()) { fail("Qt chua co TLS backend. Kiem tra thu muc tls."); return; }
    socket->abort(); verifiedFingerprint.clear(); current = Connecting;
    auto configuration = socket->sslConfiguration(); configuration.setProtocol(QSsl::TlsV1_2OrLater);
    configuration.setPeerVerifyMode(QSslSocket::VerifyPeer); socket->setSslConfiguration(configuration);
    emit changed(current, "Dang ket noi TLS " + profile.endpoint() + "...");
    timer.start(); socket->connectToHostEncrypted(profile.host, profile.port);
}
void SessionConnection::stop() { timer.stop(); current = Idle; socket->abort(); }
void SessionConnection::fail(const QString &reason) {
    timer.stop(); current = Idle; socket->abort();
    emit changed(current, reason + " Bam Ket noi lai hoac Doi ket noi."); emit failed(reason);
}
