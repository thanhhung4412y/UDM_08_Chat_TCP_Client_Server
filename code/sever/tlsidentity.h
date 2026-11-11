#pragma once
#include <QSslConfiguration>

class TlsIdentity {
public:
    static bool load(const QString &directory, QSslConfiguration &configuration, QString &error);
    static QString fingerprint(const QSslCertificate &certificate);
};
