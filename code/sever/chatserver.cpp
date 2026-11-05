
#include "chatserver.h"
#include <QImage>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QDebug>
#include <algorithm>

ChatServer::ChatServer(QObject *parent)
    : QObject(parent)
{
    server = new QTcpServer(this);

    connect(server, &QTcpServer::newConnection,
            this, [this]() {

                while (server->hasPendingConnections()) {

                    QTcpSocket *client =
                        server->nextPendingConnection();

                    buffers.insert(client, QByteArray());

                    qDebug() << "Co ket noi TCP moi";

                    connect(client, &QTcpSocket::readyRead,
                            this, [this, client]() {

                                buffers[client].append(client->readAll());

                                while (buffers.contains(client)) {

                                    QByteArray &buffer = buffers[client];
                                    int pos = buffer.indexOf('\n');

                                    if (pos < 0)
                                        break;

                                    if (pos > 512 * 1024) {
                                        client->disconnectFromHost();
                                        return;
                                    }

                                    QByteArray line = buffer.left(pos);
                                    buffer.remove(0, pos + 1);

                                    QJsonParseError error;
                                    QJsonDocument doc =
                                        QJsonDocument::fromJson(
                                            line, &error);

                                    if (error.error !=
                                            QJsonParseError::NoError ||
                                        !doc.isObject()) {
                                        sendPacket(client, {
                                                               {"type", "error"},
                                                               {"text", "Du lieu JSON khong hop le"}
                                                           });
                                        continue;
                                    }

                                    processPacket(client, doc.object());
                                }

                                if (buffers.contains(client) &&
                                    buffers[client].size() > 512 * 1024) {
                                    client->disconnectFromHost();
                                }
                            });

                    connect(client, &QTcpSocket::disconnected,
                            this, [this, client]() {

                                bool registered = names.contains(client);
                                QString name = names.take(client);

                                buffers.remove(client);
                                client->deleteLater();

                                if (registered) {
                                    qDebug() << name << "da thoat";

                                    broadcast({
                                        {"type", "system"},
                                        {"text", name + " da roi phong chat"}
                                    });

                                    updateOnlineUsers();
                                }
                            });
                }
            });
}

bool ChatServer::start(quint16 port)
{
    if (!server->listen(QHostAddress::Any, port)) {
        qWarning() << "Khong mo duoc Server:"
                   << server->errorString();
        return false;
    }

    qDebug() << "Server dang lang nghe cong:"
             << server->serverPort();

    return true;
}

void ChatServer::sendPacket(
    QTcpSocket *client,
    const QJsonObject &packet)
{
    if (client->state() !=
        QAbstractSocket::ConnectedState)
        return;

    QByteArray data =
        QJsonDocument(packet)
            .toJson(QJsonDocument::Compact);

    client->write(data + "\n");
}

void ChatServer::broadcast(
    const QJsonObject &packet)
{
    for (QTcpSocket *client : names.keys()) {
        sendPacket(client, packet);
    }
}

void ChatServer::updateOnlineUsers()
{
    QStringList users;

    for (const QString &name : names.values()) {
        users.append(name);
    }

    // Sap xep danh sach theo ten
    std::sort(users.begin(), users.end());

    QJsonArray array;

    for (const QString &name : users) {
        array.append(name);
    }

    broadcast({
        {"type", "users"},
        {"names", array}
    });
}

void ChatServer::processPacket(
    QTcpSocket *client,
    const QJsonObject &packet)
{
    QString type = packet.value("type").toString();
qDebug() << "SERVER NHAN GOI:" << type;
    // Gui tin nhan rieng
    if (type == "private") {

        // Chi nguoi da dang nhap moi duoc gui
        if (!names.contains(client))
            return;

        QString receiverName =
            packet.value("to").toString().trimmed();

        QString message =
            packet.value("text").toString().trimmed();

        if (receiverName.isEmpty() ||
            message.isEmpty() ||
            message.toUtf8().size() > 4000) {
            return;
        }

        QTcpSocket *receiver = nullptr;

        // Tim Client theo ten nguoi nhan
        for (auto it = names.cbegin();
             it != names.cend(); ++it) {

            if (it.value() == receiverName) {
                receiver = it.key();
                break;
            }
        }

        if (receiver == nullptr) {

            sendPacket(client, {
                                   {"type", "error"},
                                   {"text", "Nguoi nhan khong online"}
                               });

            return;
        }

        QString senderName = names.value(client);

        QJsonObject privateMessage {
            {"type", "private"},
            {"sender", senderName},
            {"to", receiverName},
            {"text", message}
        };




        // Gui cho nguoi nhan
        sendPacket(receiver, privateMessage);

        // Hien thi lai cho nguoi gui
        if (receiver != client) {
            sendPacket(client, privateMessage);
        }

        qDebug() << "Tin rieng:"
                 << senderName
                 << "->"
                 << receiverName;

        return;
    }
    // =====================================
    // XU LY TIN NHAN ANH
    // =====================================
    if (type == "image") {

        if (!names.contains(client))
            return;

        QString receiverName =
            packet.value("to").toString().trimmed();

        QString encoded =
            packet.value("data").toString();

        // Kiem tra dung luong Base64
        if (encoded.isEmpty() ||
            encoded.size() > 350000) {

            sendPacket(client, {
                                   {"type", "error"},
                                   {"text", "Du lieu anh khong hop le"}
                               });
            return;
        }

        QByteArray bytes =
            QByteArray::fromBase64(encoded.toLatin1());

        if (bytes.isEmpty() ||
            bytes.size() > 256 * 1024) {

            sendPacket(client, {
                                   {"type", "error"},
                                   {"text", "Anh qua lon hoac bi loi"}
                               });
            return;
        }

        // Chi chap nhan anh JPEG hop le
        QImage image;
        if (!image.loadFromData(bytes, "JPEG")) {

            sendPacket(client, {
                                   {"type", "error"},
                                   {"text", "Anh JPEG khong hop le"}
                               });
            return;
        }

        QString senderName = names.value(client);

        QJsonObject imagePacket {
            {"type", "image"},
            {"sender", senderName},
            {"to", receiverName},
            {"data", encoded}
        };

        // Gui vao phong chung
        if (receiverName.isEmpty()) {

            broadcast(imagePacket);
            qDebug() << senderName
                     << "gui anh vao phong chung";
            return;
        }

        // Tim nguoi nhan
        QTcpSocket *receiver = nullptr;

        for (auto it = names.cbegin();
             it != names.cend(); ++it) {

            if (it.value() == receiverName) {
                receiver = it.key();
                break;
            }
        }

        if (receiver == nullptr) {
            sendPacket(client, {
                                   {"type", "error"},
                                   {"text", "Nguoi nhan khong online"}
                               });
            return;
        }

        sendPacket(receiver, imagePacket);

        // Hien thi anh cho nguoi gui
        if (receiver != client)
            sendPacket(client, imagePacket);

        qDebug() << senderName
                 << "gui anh den"
                 << receiverName;
        return;
    } // Ket thuc xu ly image

    // Dang ky ten
    if (type == "join") {
qDebug() << "DANG XU LY DANG NHAP";
        if (names.contains(client))
            return;

        QString name =
            packet.value("name").toString().trimmed();

        if (name.isEmpty() || name.size() > 20 ||
            name.contains('|') ||
            name.contains('\n') ||
            name.contains('\r')) {

            sendPacket(client, {
                                   {"type", "error"},
                                   {"text", "Ten khong hop le (1-20 ky tu)"}
                               });
            return;
        }

        // Khong cho phep trung ten
        for (const QString &existing : names.values()) {
            if (existing.compare(name,
                                 Qt::CaseInsensitive) == 0) {

                sendPacket(client, {
                                       {"type", "error"},
                                       {"text", "Ten da duoc su dung"}
                                   });
                return;
            }
        }

        names.insert(client, name);

        sendPacket(client, {
                               {"type", "joined"},
                               {"name", name}
                           });

        broadcast({
            {"type", "system"},
            {"text", name + " da tham gia phong chat"}
        });

        updateOnlineUsers();

        qDebug() << name << "da dang nhap";
        return;
    }

    // Gui tin nhan
    if (type == "chat") {

        if (!names.contains(client))
            return;

        QString text =
            packet.value("text").toString().trimmed();

        if (text.isEmpty() ||
            text.toUtf8().size() > 4000)
            return;

        QString sender = names.value(client);

        broadcast({
            {"type", "chat"},
            {"sender", sender},
            {"text", text}
        });


        qDebug() << sender << ":" << text;
        return;
    }

    // Xu ly Reply trong phong chung

    // ==================================
    // XU LY REPLY TRONG PHONG CHUNG
    // ==================================
    if (type == "reply") {

        if (!names.contains(client))
            return;

        QString text =
            packet.value("text").toString().trimmed();

        QString quote =
            packet.value("quote").toString().trimmed();

        if (text.isEmpty() || quote.isEmpty() ||
            text.toUtf8().size() > 4000 ||
            quote.toUtf8().size() > 1000) {
            return;
        }

        QString sender = names.value(client);

        broadcast({
            {"type", "reply"},
            {"sender", sender},
            {"text", text},
            {"quote", quote}
        });

        qDebug() << sender << "reply:" << text;
        return;
    }

    // ==================================
    // XU LY FORWARD
    // ==================================
    if (type == "forward") {

        // Kiem tra Client da dang nhap
        if (!names.contains(client))
            return;

        QString text =
            packet.value("text").toString().trimmed();

        QString receiverName =
            packet.value("to").toString().trimmed();

        // Kiem tra noi dung
        if (text.isEmpty() ||
            text.toUtf8().size() > 4000) {
            return;
        }

        QString senderName = names.value(client);

        QJsonObject forwardPacket{
            {"type", "forward"},
            {"sender", senderName},
            {"to", receiverName},
            {"text", text}
        };

        // TRUONG HOP 1: Forward vao phong chung
        if (receiverName.isEmpty()) {

            broadcast(forwardPacket);

            qDebug() << senderName
                     << "forward vao phong chung";

            return;
        }

        // TRUONG HOP 2: Forward cho mot nguoi
        QTcpSocket *receiver = nullptr;

        for (auto it = names.cbegin();
             it != names.cend(); ++it) {

            if (it.value() == receiverName) {
                receiver = it.key();
                break;
            }
        }

        // Neu nguoi nhan khong online
        if (receiver == nullptr) {

            sendPacket(client, {
                                   {"type", "error"},
                                   {"text", "Nguoi nhan khong online"}
                               });

            return;
        }

        // Gui tin nhan cho nguoi nhan
        sendPacket(receiver, forwardPacket);

        // Gui ban sao ve nguoi Forward
        if (receiver != client) {
            sendPacket(client, forwardPacket);
        }

        qDebug() << senderName
                 << "forward den"
                 << receiverName;

        return;
    }

    // ==================================
    // XU LY STICKER
    // ==================================
    if (type == "sticker") {

        // Kiem tra Client da dang nhap
        if (!names.contains(client))
            return;

        // Lay ma Sticker
        QString stickerId =
            packet.value("id").toString();

        // Lay nguoi nhan
        QString receiverName =
            packet.value("to").toString().trimmed();

        // Danh sach Sticker hop le
        QStringList allowedStickers = {
            "nobita",
            "doremon"
        };

        // Kiem tra ma Sticker
        if (!allowedStickers.contains(stickerId)) {

            sendPacket(client, {
                                   {"type", "error"},
                                   {"text", "Sticker khong hop le"}
                               });

            return;
        }

        // Lay ten nguoi gui tu Server
        QString senderName = names.value(client);

        // Tao goi tin Sticker
        QJsonObject stickerPacket{
            {"type", "sticker"},
            {"sender", senderName},
            {"to", receiverName},
            {"id", stickerId}
        };

        // Gui vao phong chung
        if (receiverName.isEmpty()) {

            broadcast(stickerPacket);

            qDebug() << senderName
                     << "gui sticker vao phong chung";

            return;
        }

        // Gui rieng cho mot Client
        QTcpSocket *receiver = nullptr;

        for (auto it = names.cbegin();
             it != names.cend(); ++it) {

            if (it.value() == receiverName) {
                receiver = it.key();
                break;
            }
        }

        // Neu nguoi nhan khong online
        if (receiver == nullptr) {

            sendPacket(client, {
                                   {"type", "error"},
                                   {"text", "Nguoi nhan khong online"}
                               });

            return;
        }

        // Gui Sticker cho nguoi nhan
        sendPacket(receiver, stickerPacket);

        // Gui ban sao ve nguoi gui
        if (receiver != client) {
            sendPacket(client, stickerPacket);
        }

        qDebug() << senderName
                 << "gui sticker cho"
                 << receiverName;

        return;
    }

}    // Ket thuc ham ChatServer::processPacket()

