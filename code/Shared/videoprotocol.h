#pragma once
#include <QByteArray>
#include <QFileInfo>
#include <QJsonObject>
#include <QUuid>
#include <cmath>

namespace VideoProtocol {
inline constexpr qint64 MaxSize = 1024LL * 1024 * 1024;
inline constexpr qint64 ChunkSize = 64 * 1024;
inline constexpr qint64 MaxFrame = 512 * 1024;
inline constexpr qint64 MaxQueued = 2 * 1024 * 1024;
inline constexpr int MaxTransfers = 8;
inline constexpr int TimeoutMs = 60000;
inline bool number(const QJsonValue &v, qint64 &n, qint64 max = MaxSize) {
    if (!v.isDouble()) return false;
    const double d = v.toDouble();
    if (!std::isfinite(d) || d < 0 || d > double(max) || std::floor(d) != d) return false;
    n = qint64(d);
    return true;
}
inline bool validId(const QString &id) {
    return !QUuid(id).isNull() && QUuid(id).toString(QUuid::WithoutBraces) == id;
}
inline bool validName(const QString &name) {
    if (name.isEmpty() || name.size() > 160 || name != name.trimmed()
        || name.contains('/') || name.contains('\\') || name.contains(':')
        || QFileInfo(name).suffix().compare("mp4", Qt::CaseInsensitive) != 0) return false;
    for (const QChar ch : name) if (ch.unicode() < 32 || ch.unicode() == 127) return false;
    return true;
}
// Lightweight ISO BMFF signature check; does not validate codecs/playability.
inline bool isMp4(const QByteArray &head) {
    return head.size() >= 12 && head.mid(4, 4) == "ftyp";
}
inline bool decode(const QJsonObject &p, QByteArray &bytes) {
    const QString text = p.value("data").toString();
    if (text.isEmpty() || text.size() > ((ChunkSize + 2) / 3) * 4) return false;
    const QByteArray encoded = text.toLatin1();
    auto decoded = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.isEmpty() || decoded.decoded.size() > ChunkSize
        || decoded.decoded.toBase64() != encoded) return false;
    bytes = decoded.decoded;
    return true;
}
inline bool digest(const QString &s) {
    if (s.size() != 64) return false;
    for (QChar c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
}
