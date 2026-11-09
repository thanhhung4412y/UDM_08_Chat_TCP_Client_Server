#pragma once
#include <QBuffer>
#include <QImageReader>

// Decode a bounded preview, never keep a full-size JPEG for every history row.
inline QImage historyImagePreview(const QByteArray &bytes) {
    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly)) return {};
    QImageReader reader(&buffer, "JPEG");
    const QSize size = reader.size();
    if (!size.isValid()) return {};
    if (size.width() > 240 || size.height() > 240)
        reader.setScaledSize(size.scaled(QSize(240, 240), Qt::KeepAspectRatio));
    return reader.read();
}
