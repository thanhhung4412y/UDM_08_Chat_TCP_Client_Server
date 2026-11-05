#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QUuid>

namespace HistoryProtocol {
inline bool messageType(const QString &type) {
    return type == "chat" || type == "private" || type == "reply" || type == "forward"
        || type == "image" || type == "sticker";
}
inline QJsonObject stamp(QJsonObject packet) {
    if (!messageType(packet.value("type").toString())) return packet;
    if (!packet.contains("_event_id"))
        packet["_event_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!packet.contains("_utc"))
        packet["_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    return packet;
}
}
