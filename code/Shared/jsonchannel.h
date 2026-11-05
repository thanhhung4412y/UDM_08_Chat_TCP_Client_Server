#pragma once
#include "videoprotocol.h"
#include "jsonframer.h"
#include <QObject>
#include <QTcpSocket>

// The only reader/writer of a connection. Keeps legacy newline JSON compatible.
class JsonChannel : public QObject {
    Q_OBJECT
public:
    explicit JsonChannel(QTcpSocket *socket, QObject *parent = nullptr);
    virtual bool send(const QJsonObject &packet);
    qint64 bufferedBytes() const { return decoder.size(); }
signals:
    void packetReceived(const QJsonObject &packet);
    void transportError(const QString &text);
private:
    QTcpSocket *socket;
    JsonFramer decoder;
    bool scheduled = false;
    void drain();
    void fail(const QString &text);
};
