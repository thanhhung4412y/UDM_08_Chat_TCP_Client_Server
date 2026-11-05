#pragma once
#include "videoprotocol.h"
#include <QJsonDocument>
#include <QJsonParseError>

// Bounded incremental newline framing, independent of the transport.
class JsonFramer {
public:
    enum Result { NeedData, Packet, Error };
    qint64 size() const { return buffer.size(); }
    qint64 room() const { return VideoProtocol::MaxFrame + 1 - buffer.size(); }
    void clear() { buffer.clear(); }
    bool append(const QByteArray &data) {
        if (data.size() > room()) return false;
        buffer.append(data); return true;
    }
    Result next(QJsonObject &packet) {
        const qsizetype pos = buffer.indexOf('\n');
        if (pos < 0) return buffer.size() > VideoProtocol::MaxFrame ? Error : NeedData;
        if (pos > VideoProtocol::MaxFrame) return Error;
        const QByteArray line = buffer.left(pos);
        buffer.remove(0, pos + 1);
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) return Error;
        packet = doc.object(); return Packet;
    }
private:
    QByteArray buffer;
};
