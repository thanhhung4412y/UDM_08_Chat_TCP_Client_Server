#pragma once
#include <QString>
#include <QSslConfiguration>
// Runs only on a bounded worker pool. No SMTP payloads or credentials are logged.
namespace GmailSender {
bool send(const QString &recipient,const QString &code,const QString &purpose);
// Transport seam for a certificate-verified local SMTP test, never plaintext or ignoreSslErrors.
bool sendWithSettings(const QString &user,QByteArray password,const QString &recipient,const QString &code,const QString &purpose,const QString &host,quint16 port,const QSslConfiguration &configuration,const QString &peerName = {});
}
