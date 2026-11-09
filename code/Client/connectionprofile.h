#pragma once
#include <QString>
class QSettings;

struct ConnectionProfile {
    QString username;
    QString host = "127.0.0.1";
    quint16 port = 12345;
    QString password;
    bool registerAccount = false;
    QString endpoint() const;
    static bool validate(const QString &username, const QString &host, int port,
                         ConnectionProfile &profile, QString &error);
    static ConnectionProfile load(QSettings &settings);
    void save(QSettings &settings) const;
};
