#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QUuid>
#include "videoprotocol.h"

namespace HistoryProtocol {
inline bool messageType(const QString &type) {
    return type == "chat" || type == "private" || type == "reply" || type == "forward"
        || type == "image" || type == "sticker";
}
inline bool editableType(const QString &type) {return type=="chat" || type=="private" || type=="reply" || type=="forward";}
inline bool validEdit(const QJsonObject &p) {
    qint64 revision=0;const QString text=p.value("text").toString();
    return VideoProtocol::validId(p.value("event_id").toString()) && editableType(p.value("kind").toString())
        && VideoProtocol::number(p.value("revision"),revision,9007199254740991LL) && revision>0
        && !text.trimmed().isEmpty() && text.toUtf8().size()<=4000 && !text.contains(QChar(0));
}
inline QJsonObject withEdit(QJsonObject packet,const QJsonObject &notice) {
    if(packet.value("_recalled").toBool() || !editableType(packet.value("type").toString()) || packet.value("type")!=notice.value("kind")
        || packet.value("sender").toString().compare(notice.value("sender").toString(),Qt::CaseInsensitive)!=0
        || packet.value("to").toString().compare(notice.value("to").toString(),Qt::CaseInsensitive)!=0
        || notice.value("revision").toDouble()<=packet.value("_edit_revision").toDouble()) return packet;
    packet["text"]=notice.value("text");packet["_edit_revision"]=notice.value("revision");packet["_edited_utc"]=notice.value("edited_utc");return packet;
}
inline bool validReceipt(const QJsonObject &p) {
    qint64 state=0,revision=0;
    return VideoProtocol::validId(p.value("event_id").toString()) && !p.value("sender").toString().isEmpty() && !p.value("to").toString().isEmpty()
        && VideoProtocol::number(p.value("state"),state,3) && state>=1 && VideoProtocol::number(p.value("revision"),revision,9007199254740991LL) && revision>0;
}
inline QJsonObject withReceipt(QJsonObject packet,const QJsonObject &notice) {
    if(packet.value("_recalled").toBool() || !validReceipt(notice) || packet.value("sender").toString().compare(notice.value("sender").toString(),Qt::CaseInsensitive)!=0
        || packet.value("to").toString().compare(notice.value("to").toString(),Qt::CaseInsensitive)!=0 || packet.value("to").toString().isEmpty()
        || notice.value("revision").toDouble()<=packet.value("_receipt_revision").toDouble() || notice.value("state").toInt()<packet.value("_receipt_state").toInt()) return packet;
    packet["_receipt_state"]=notice.value("state");packet["_receipt_revision"]=notice.value("revision");
    packet.remove("_receipt_untracked");return packet;
}
inline QJsonObject tombstone(const QJsonObject &p) {
    return {{"type",p.value("type")},{"sender",p.value("sender")},{"to",p.value("to").toString()},
        {"_event_id",p.value("_event_id")},{"_utc",p.value("_utc")},{"_recalled",true}};
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
