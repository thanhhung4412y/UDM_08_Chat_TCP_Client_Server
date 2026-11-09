#include "connectionprofile.h"
#include <QHostAddress>
#include <QRegularExpression>
#include <QSettings>

QString ConnectionProfile::endpoint() const {
    return (host.contains(':') ? "[" + host + "]" : host) + ":" + QString::number(port);
}
bool ConnectionProfile::validate(const QString &name, const QString &address, int number,
                                 ConnectionProfile &profile, QString &error) {
    error.clear();
    const QString username = name.trimmed().normalized(QString::NormalizationForm_C);
    if (username.isEmpty() || username.size() > 20) { error = "Ten phai co tu 1 den 20 ky tu."; return false; }
    for (QChar ch : username)
        if (ch == '|' || ch.unicode() < 32 || ch.unicode() == 127) {
            error = "Ten khong duoc chua dau | hoac ky tu dieu khien."; return false;
        }
    if (number < 1 || number > 65535) { error = "Cong phai tu 1 den 65535."; return false; }
    QString host = address.trimmed();
    if (host.startsWith('[') && host.endsWith(']')) host = host.mid(1, host.size() - 2);
    QHostAddress ip;
    if (ip.setAddress(host)) {
        if (ip == QHostAddress::AnyIPv4 || ip == QHostAddress::AnyIPv6 || ip == QHostAddress::Broadcast || ip.isMulticast()) {
            error = "Hay nhap IP cua may Server, khong dung dia chi lang nghe 0.0.0.0/:: hoac broadcast."; return false;
        }
        host = ip.toString().toLower();
    } else {
        static const QRegularExpression dns(
            R"(\A(?=.{1,253}\z)[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?(?:\.[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?)*\.?\z)");
        static const QRegularExpression numeric(R"(\A[0-9.]+\z)");
        if (!dns.match(host).hasMatch() || numeric.match(host).hasMatch()) {
            error = "IP/ten may khong hop le. Nhap rieng dia chi va cong; khong them http:// hay duong dan."; return false;
        }
        host = host.toLower();
        if (host.endsWith('.')) host.chop(1);
    }
    profile = {username, host, quint16(number)};
    return true;
}
ConnectionProfile ConnectionProfile::load(QSettings &settings) {
    ConnectionProfile profile;
    QString error;
    if (!validate(settings.value("connection/username").toString(),
                  settings.value("connection/host", "127.0.0.1").toString(),
                  settings.value("connection/port", 12345).toInt(), profile, error)) {
        profile.username = settings.value("connection/username").toString().trimmed().left(20);
        if (!validate(profile.username.isEmpty() ? "User" : profile.username, "127.0.0.1", 12345, profile, error)) profile = {};
        if (settings.value("connection/username").toString().isEmpty()) profile.username.clear();
    }
    return profile;
}
void ConnectionProfile::save(QSettings &settings) const {
    settings.setValue("connection/username", username);
    settings.setValue("connection/host", host);
    settings.setValue("connection/port", int(port));
    settings.sync();
}
