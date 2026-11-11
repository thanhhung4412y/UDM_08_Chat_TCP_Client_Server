#include "tlsidentity.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSslKey>
#include <QSslSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <ncrypt.h>
#endif

QString TlsIdentity::fingerprint(const QSslCertificate &certificate) {
    return QString::fromLatin1(certificate.digest(QCryptographicHash::Sha256).toHex(':')).toUpper();
}
static bool generate(QByteArray &certificate, QByteArray &privateKey, QString &error) {
#ifdef Q_OS_WIN
    NCRYPT_PROV_HANDLE provider = 0; NCRYPT_KEY_HANDLE key = 0; PCCERT_CONTEXT cert = nullptr;
    bool ok = false;
    do {
        if (NCryptOpenStorageProvider(&provider, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS
            || NCryptCreatePersistedKey(provider, &key, NCRYPT_RSA_ALGORITHM, nullptr, 0, 0) != ERROR_SUCCESS) break;
        DWORD bits = 2048, policy = NCRYPT_ALLOW_EXPORT_FLAG | NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG;
        if (NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY, reinterpret_cast<PBYTE>(&bits), sizeof(bits), 0) != ERROR_SUCCESS
            || NCryptSetProperty(key, NCRYPT_EXPORT_POLICY_PROPERTY, reinterpret_cast<PBYTE>(&policy), sizeof(policy), 0) != ERROR_SUCCESS
            || NCryptFinalizeKey(key, 0) != ERROR_SUCCESS) break;
        DWORD size = 0;
        if (!CertStrToNameW(X509_ASN_ENCODING, L"CN=ChatTCP1 Server", CERT_X500_NAME_STR, nullptr, nullptr, &size, nullptr)) break;
        QByteArray subject(int(size), Qt::Uninitialized);
        if (!CertStrToNameW(X509_ASN_ENCODING, L"CN=ChatTCP1 Server", CERT_X500_NAME_STR, nullptr,
                            reinterpret_cast<BYTE *>(subject.data()), &size, nullptr)) break;
        CERT_NAME_BLOB blob{size, reinterpret_cast<BYTE *>(subject.data())};
        CRYPT_ALGORITHM_IDENTIFIER algorithm{}; algorithm.pszObjId = const_cast<char *>(szOID_RSA_SHA256RSA);
        SYSTEMTIME start{}, end{};
        const auto from = QDateTime::currentDateTimeUtc().addDays(-1), to = from.addYears(5);
        auto systemTime = [](QDateTime t) {
            SYSTEMTIME s{}; s.wYear = WORD(t.date().year()); s.wMonth = WORD(t.date().month()); s.wDay = WORD(t.date().day());
            s.wHour = WORD(t.time().hour()); s.wMinute = WORD(t.time().minute()); s.wSecond = WORD(t.time().second()); return s;
        };
        start = systemTime(from); end = systemTime(to);
        cert = CertCreateSelfSignCertificate(key, &blob, CERT_CREATE_SELFSIGN_NO_KEY_INFO, nullptr, &algorithm, &start, &end, nullptr);
        if (!cert) break;
        certificate = QByteArray(reinterpret_cast<const char *>(cert->pbCertEncoded), int(cert->cbCertEncoded));
        DWORD length = 0;
        if (NCryptExportKey(key, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, nullptr, nullptr, 0, &length, 0) != ERROR_SUCCESS) break;
        privateKey.resize(int(length));
        if (NCryptExportKey(key, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, nullptr,
                            reinterpret_cast<PBYTE>(privateKey.data()), length, &length, 0) != ERROR_SUCCESS) break;
        privateKey.resize(int(length)); ok = true;
    } while (false);
    if (cert) CertFreeCertificateContext(cert);
    if (key) NCryptFreeObject(key);
    if (provider) NCryptFreeObject(provider);
    if (!ok) error = "Khong tao duoc chung chi TLS bang Windows CNG.";
    return ok;
#else
    Q_UNUSED(certificate); Q_UNUSED(privateKey);
    error = "Tu tao chung chi hien ho tro Windows. Can cung cap identity.json tren he dieu hanh khac.";
    return false;
#endif
}
bool TlsIdentity::load(const QString &directory, QSslConfiguration &configuration, QString &error) {
    error.clear();
    if (!QSslSocket::supportsSsl()) { error = "Qt chua co TLS backend (tls/qschannelbackend.dll)."; return false; }
    if (!QDir().mkpath(directory)) { error = "Khong tao duoc thu muc TLS."; return false; }
    QLockFile lock(QDir(directory).filePath("identity.lock"));
    if (!lock.tryLock(5000)) { error = "Danh tinh TLS dang duoc Server khac khoi tao. Thu lai sau."; return false; }
    const QString path = QDir(directory).filePath("identity.json");
    QByteArray certificate, key;
    if (QFile::exists(path)) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024) { error = "Khong doc duoc TLS identity."; return false; }
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        certificate = QByteArray::fromBase64(object.value("certificate").toString().toLatin1());
        key = QByteArray::fromBase64(object.value("key").toString().toLatin1());
    } else {
        if (!generate(certificate, key, error)) return false;
        QSaveFile file(path); file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly)) { error = file.errorString(); return false; }
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        const auto data = QJsonDocument(QJsonObject{{"certificate", QString::fromLatin1(certificate.toBase64())},
            {"key", QString::fromLatin1(key.toBase64())}}).toJson(QJsonDocument::Compact);
        if (file.write(data) != data.size() || !file.commit()) { error = "Khong luu duoc TLS identity."; return false; }
    }
    const QSslCertificate cert(certificate, QSsl::Der);
    const QSslKey privateKey(key, QSsl::Rsa, QSsl::Der, QSsl::PrivateKey);
    key.fill('\0');
    if (cert.isNull() || privateKey.isNull() || cert.expiryDate() <= QDateTime::currentDateTimeUtc()) {
        error = "TLS identity bi loi/het han; khong tu ghi de danh tinh Server."; return false;
    }
    configuration = QSslConfiguration::defaultConfiguration();
    configuration.setProtocol(QSsl::TlsV1_2OrLater);
    configuration.setLocalCertificate(cert); configuration.setPrivateKey(privateKey);
    configuration.setPeerVerifyMode(QSslSocket::VerifyNone); // Clients authenticate using accounts inside TLS.
    return true;
}
