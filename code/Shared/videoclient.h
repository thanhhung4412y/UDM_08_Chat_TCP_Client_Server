#pragma once
#include "jsonchannel.h"
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QSaveFile>
#include <memory>

class VideoClient : public QObject {
    Q_OBJECT
public:
    explicit VideoClient(JsonChannel *channel, QTcpSocket *socket, QString directory,
                         QObject *parent = nullptr, int timeoutMs = VideoProtocol::TimeoutMs);
    bool start(const QString &path, const QString &to);
    bool handle(const QJsonObject &packet);
    bool isSending() const { return bool(out); }
    int incomingCount() const { return incoming.size(); }
    void cancel();
signals:
    void progress(const QString &text);
    void failed(const QString &text);
    void completed(const QString &path, const QString &sender, const QString &name, bool outgoing);
    void historyReady(const QJsonObject &packet);
private:
    struct Outgoing {
        QString id, name, path, to, phase = "ready";
        QFile file;
        qint64 size = 0, offset = 0;
        QCryptographicHash hash{QCryptographicHash::Sha256};
        QElapsedTimer activity;
    };
    struct Incoming {
        QString id, name, sender, path, to;
        QSaveFile file;
        qint64 size = 0, offset = 0;
        QCryptographicHash hash{QCryptographicHash::Sha256};
        QElapsedTimer activity;
    };
    JsonChannel *channel;
    QString directory;
    std::shared_ptr<Outgoing> out;
    QHash<QString, std::shared_ptr<Incoming>> incoming;
    void fail(QString id, const QString &text, bool notify = true);
    void pump();
    bool ack(const std::shared_ptr<Incoming> &t, const QString &phase);
};
