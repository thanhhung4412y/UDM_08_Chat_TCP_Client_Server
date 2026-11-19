
#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <QJsonObject>
#include <QStringList>
#include "jsonchannel.h"
#include "videorelay.h"
#include "authservice.h"
#include "emailservice.h"
#include <QSslServer>
#include "recallstore.h"

class ChatServer : public QObject
{
    Q_OBJECT

public:
    explicit ChatServer(QObject *parent = nullptr);
    ~ChatServer() override;
    void shutdown();

    bool start(quint16 port = 12345, const QString &dataDirectory = {});
    quint16 port() const { return server->serverPort(); }
    QString certificateFingerprint() const;
    QString dataPath() const { return directory; }
    QString startupError() const { return lastError; }

signals:
    void mailStatus(const QString &text);
private:
    // TCP Server
    QSslServer *server;
    AuthService *authentication = nullptr;
    EmailService *emails = nullptr;
    QString directory, lastError;
    RecallStore recalls;
    QHash<QTcpSocket*,qint64> recallSync,editSync,receiptSync;
    QHash<QTcpSocket*,QPair<qint64,int>> recallLimits,editLimits;

    // Luu du lieu nhan cua tung Client
    QHash<QTcpSocket*, JsonChannel*> channels;
    VideoRelay *videos;

    // Luu ten nguoi dung da dang ky
    QHash<QTcpSocket*, QString> names;
    struct GroupRoom { QString name; QStringList members; };
    QHash<QString, GroupRoom> groups;
    void sendGroupsFor(QTcpSocket *client);
    bool readGroupHistory(const QString &id, QJsonArray &messages) const;
    bool saveGroupMessage(const QString &id, const QJsonObject &message) const;
    bool writeGroupHistory(const QString &id, QJsonArray messages) const;
    void loadGroups();
    bool saveGroups();

    QHash<QTcpSocket*, QString> passwordRequests;
    QHash<QTcpSocket*, QString> recoveryRequests;
    QHash<QTcpSocket*, qint64> avatarTimes;

    // Gui JSON cho mot Client
    void sendPacket(
        QTcpSocket *client,
        const QJsonObject &packet
        );

    // Gui JSON cho tat ca Client
    void broadcast(
        const QJsonObject &packet
        );

    // Cap nhat danh sach Online
    void updateOnlineUsers();

    // Xu ly thong diep JSON
    void processPacket(
        QTcpSocket *client,
        const QJsonObject &packet
        );
};

#endif
