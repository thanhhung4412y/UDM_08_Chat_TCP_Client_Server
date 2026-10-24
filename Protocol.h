#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>

class Protocol {
public:
    enum MessageType {
        ChatText = 1,   
        ReplyMessage = 2, 
        ForwardMessage = 3 
    };

    static QByteArray packMessage(int type, const QString &sender, const QString &content, const QString &extra = "");

    static bool unpackMessage(const QByteArray &data, int &type, QString &sender, QString &content, QString &extra);
};

#endif 