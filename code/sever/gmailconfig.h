#pragma once
#include <QByteArray>
#include <QString>

// Gmail credentials are user-scoped, DPAPI-encrypted on Windows.
// No secrets are saved as cleartext or checked into the project.
namespace GmailConfig {
bool load(QString &email, QByteArray &appPassword, QString *error = nullptr);
bool save(const QString &email, const QByteArray &appPassword, QString *error = nullptr);
bool clear(QString *error = nullptr);
}
