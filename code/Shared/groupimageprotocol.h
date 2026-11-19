#pragma once

#include <QBuffer>
#include <QImageReader>

namespace GroupImageProtocol {
inline constexpr int MaxBytes = 96 * 1024;
inline constexpr int MaxSide = 960;

inline QImage decode(const QString &encoded, int previewSide = MaxSide) {
    if (encoded.isEmpty() || encoded.size() > (MaxBytes + 2) / 3 * 4) return {};
    QByteArray bytes = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (bytes.isEmpty() || bytes.size() > MaxBytes) return {};
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::ReadOnly)) return {};
    QImageReader reader(&buffer, "JPEG");
    const QSize size = reader.size();
    if (!size.isValid() || size.width() > MaxSide || size.height() > MaxSide) return {};
    if (size.width() > previewSide || size.height() > previewSide)
        reader.setScaledSize(size.scaled(previewSide, previewSide, Qt::KeepAspectRatio));
    return reader.read();
}
}
