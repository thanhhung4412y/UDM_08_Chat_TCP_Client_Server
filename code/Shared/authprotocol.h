#pragma once
#include <QString>
#include <QByteArray>
#include <QRegularExpression>

namespace AuthProtocol {
inline constexpr int Iterations = 600000;
inline QString canonical(const QString &name) { return name.trimmed().normalized(QString::NormalizationForm_C).toCaseFolded(); }
inline bool validName(const QString &value) {
    const QString name = value.trimmed().normalized(QString::NormalizationForm_C);
    if (name.isEmpty() || name.size() > 20) return false;
    for (QChar c : name) if (c == '|' || c.unicode() < 32 || c.unicode() == 127) return false;
    return true;
}
inline bool validPassword(const QString &password) {
    return password.size() >= 8 && password.size() <= 128 && password.toUtf8().size() <= 512
        && !password.contains(QChar(0));
}
inline QString emailKey(const QString &email) { return email.trimmed().toLower(); }
inline bool validEmail(const QString &email) {
    static const QRegularExpression re(R"(\A[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]{1,64}@[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?(?:\.[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?)+\z)");
    return email.size() <= 254 && re.match(email).hasMatch() && !email.startsWith('.') && !email.contains("..") && !email.contains(".@");
}
inline bool validRecovery(const QString &code) {
    if (code.size() != 64) return false;
    for (QChar c : code.toLower()) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
inline bool equalHash(const QByteArray &a, const QByteArray &b) {
    if (a.size() != 32 || b.size() != 32) return false;
    volatile unsigned char difference = 0;
    for (int i = 0; i < 32; ++i) difference = difference | (static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]));
    return difference == 0;
}
}
