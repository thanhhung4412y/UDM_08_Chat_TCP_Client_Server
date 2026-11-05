#pragma once
#include "videoprotocol.h"
#include <QObject>
#include <QTcpSocket>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <functional>
#include <memory>

class VideoRelay : public QObject {
public:
    using Send = std::function<bool(QTcpSocket *, const QJsonObject &)>;
    using Name = std::function<QString(QTcpSocket *)>;
    using Peers = std::function<QList<QTcpSocket *>()>;
    VideoRelay(Send send, Name name, Peers peers, QObject *parent = nullptr,
               int timeoutMs = VideoProtocol::TimeoutMs);
    bool handle(QTcpSocket *client, const QJsonObject &packet);
    void disconnected(QTcpSocket *client);
    int activeCount() const { return transfers.size(); }
private:
    struct Transfer {
        QString id;
        QTcpSocket *sender = nullptr;
        QSet<QTcpSocket *> receivers, pending;
        qint64 size = 0, offset = 0;
        QString phase;
        QCryptographicHash hash{QCryptographicHash::Sha256};
        QElapsedTimer activity;
    };
    QHash<QString, std::shared_ptr<Transfer>> transfers;
    Send send;
    Name name;
    Peers peers;
    void abort(const QString &id, const QString &reason);
    void forward(const std::shared_ptr<Transfer> &t, const QJsonObject &p);
    void advance(const std::shared_ptr<Transfer> &t);
};
