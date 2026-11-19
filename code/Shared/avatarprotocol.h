#pragma once
#include <QBuffer>
#include <QImage>
#include <QImageReader>
namespace AvatarProtocol {
inline constexpr int MaxBytes = 16384;
inline QImage decode(const QByteArray &bytes) {
    if (bytes.isEmpty() || bytes.size() > MaxBytes) return {};
    QBuffer buffer; buffer.setData(bytes); if (!buffer.open(QIODevice::ReadOnly)) return {};
    QImageReader reader(&buffer, "JPEG"); const auto size = reader.size();
    if (!size.isValid() || size.width() > 128 || size.height() > 128) return {};
    return reader.read();
}
inline QByteArray fromBase64(const QString &text) {
    if (text.size() > (MaxBytes + 2) / 3 * 4) return {};
    const auto bytes = QByteArray::fromBase64(text.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    return decode(bytes).isNull() ? QByteArray() : bytes;
}
}
