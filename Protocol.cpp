#include "Protocol.h"

QByteArray Protocol::packMessage(int type, const QString &sender, const QString &content, const QString &extra) {
    QJsonObject jsonObj;
    jsonObj["type"] = type;
    jsonObj["sender"] = sender;
    jsonObj["content"] = content;
    jsonObj["extra"] = extra;

    QJsonDocument doc(jsonObj);
    return doc.toJson(QJsonDocument::Compact); 
}

bool Protocol::unpackMessage(const QByteArray &data, int &type, QString &sender, QString &content, QString &extra) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (doc.isNull() || !doc.isObject()) {
        return false; 
    }

    QJsonObject jsonObj = doc.object();
    type = jsonObj["type"].toInt();
    sender = jsonObj["sender"].toString();
    content = jsonObj["content"].toString();
    extra = jsonObj["extra"].toString();

    return true;
}