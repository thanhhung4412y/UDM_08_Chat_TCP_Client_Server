#pragma once
#include <QString>
#include <QByteArray>

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
    return password.size() >= 12 && password.size() <= 128 && password.toUtf8().size() <= 512
        && !password.contains(QChar(0));
}
inline bool equalHash(const QByteArray &a, const QByteArray &b) {
    if (a.size() != 32 || b.size() != 32) return false;
    volatile unsigned char difference = 0;
    for (int i = 0; i < 32; ++i) difference = difference | (static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]));
    return difference == 0;
}
}
