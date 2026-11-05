
#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <QJsonObject>

class ChatServer : public QObject
{
    Q_OBJECT

public:
    explicit ChatServer(QObject *parent = nullptr);

    bool start(quint16 port = 12345);

private:
    // TCP Server
    QTcpServer *server;

    // Luu du lieu nhan cua tung Client
    QHash<QTcpSocket*, QByteArray> buffers;

    // Luu ten nguoi dung da dang ky
    QHash<QTcpSocket*, QString> names;

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
