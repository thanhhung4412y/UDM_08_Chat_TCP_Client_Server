#include "authprotocol.h"
#include "avatarprotocol.h"
#include <QDateTime>

#include "chatserver.h"
#include "historyprotocol.h"
#include "tlsidentity.h"
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
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
    server = new QSslServer(this);
    server->setHandshakeTimeout(120000); // Allow time to compare the fingerprint on first connection.
    server->setMaxPendingConnections(32);
    videos = new VideoRelay(
        [this](QTcpSocket *s, const QJsonObject &p) {
            auto *c = channels.value(s, nullptr);
            return c && c->send(p);
        },
        [this](QTcpSocket *s) { return names.value(s); },
        [this] { return names.keys(); }, this);

    videos->completed=[this](const QJsonObject &packet) {
        if(!recalls.remember(packet)) qWarning()<<"Không lưu được thông tin thu hồi video.";
        else if(!packet.value("to").toString().isEmpty()) {
            const auto state=recalls.receipt(packet.value("_event_id").toString());
            for(auto *peer:names.keys()) if(AuthProtocol::canonical(names.value(peer))==AuthProtocol::canonical(packet.value("sender").toString()) || AuthProtocol::canonical(names.value(peer))==AuthProtocol::canonical(packet.value("to").toString())) sendPacket(peer,state);
        }
    };
    connect(server, &QTcpServer::pendingConnectionAvailable,
            this, [this]() {

                while (server->hasPendingConnections()) {

                    QTcpSocket *client =
                        server->nextPendingConnection();
                    if (channels.size() >= 64) { client->abort(); client->deleteLater(); continue; }
                    QTimer::singleShot(660000, client, [this, client] { if (!names.contains(client)) client->abort(); });

                    auto *channel = new JsonChannel(client, client);
                    channels.insert(client, channel);
                    connect(channel, &JsonChannel::packetReceived, this,
                            [this, client](const QJsonObject &p) { processPacket(client, p); });
                    connect(channel, &JsonChannel::transportError, this,
                            [](const QString &text) { qWarning() << text; });
                    connect(client, &QTcpSocket::disconnected,
                            this, [this, client]() {

                                bool registered = names.contains(client);
                                QString name = names.take(client);

                                videos->disconnected(client);
                                if (authentication) authentication->disconnected(client);
                                if(emails) emails->disconnected(client);
                                passwordRequests.remove(client); recoveryRequests.remove(client); avatarTimes.remove(client); recallSync.remove(client); recallLimits.remove(client); editSync.remove(client);editLimits.remove(client);receiptSync.remove(client);
                                channels.remove(client);
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

QString ChatServer::certificateFingerprint() const {
    return TlsIdentity::fingerprint(server->sslConfiguration().localCertificate());
}
ChatServer::~ChatServer() { shutdown(); }
void ChatServer::shutdown() {
    server->close();
    for (auto *client : channels.keys()) client->abort();
    delete emails;emails=nullptr;
    delete authentication; authentication = nullptr; recalls.close();
}
bool ChatServer::start(quint16 port, const QString &dataDirectory)
{
    if (!authentication) {
        directory = dataDirectory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) : dataDirectory;
        if(!recalls.open(QDir(directory).filePath("recalls.sqlite"),lastError)) return false;
        QSslConfiguration tls;
        if (!TlsIdentity::load(QDir(directory).filePath("tls"), tls, lastError)) return false;
        server->setSslConfiguration(tls);
        authentication = new AuthService(QDir(directory).filePath("accounts.sqlite"), this);
        if (!authentication->initialize(lastError)) { delete authentication; authentication = nullptr; return false; }
        emails=new EmailService(QDir(directory).filePath("accounts.sqlite"),this);
        connect(emails,&EmailService::reply,this,[this](QTcpSocket *c,const QJsonObject &p){if(channels.contains(c))sendPacket(c,p);});
        connect(emails,&EmailService::verified,this,[this](QTcpSocket *c,const QString &purpose,const QString &name,const QString &email,const QString &password,const QByteArray &credential){
            if(!channels.contains(c)||names.contains(c))return;
            if(purpose=="reset") {
                recoveryRequests.insert(c,"email-reset");
                authentication->request(c,"email_reset",name,{},password,email,credential);
            } else authentication->request(c,purpose=="bind"?"email_bind":"register",name,password,{},email);
        });
        connect(authentication,&AuthService::emailBound,this,[this](QTcpSocket *c,const AccountResult &r){
            if(channels.contains(c))sendPacket(c,{{"type",r.ok?"email_bound":"error"},{"text",r.ok?"Đã xác minh và liên kết email. Hãy đăng nhập.":r.error}});
        });
        connect(authentication, &AuthService::finished, this, [this](QTcpSocket *client, const AccountResult &result) {
            if (!channels.contains(client) || client->state() != QAbstractSocket::ConnectedState || names.contains(client)) return;
            if (!result.ok) { sendPacket(client, {{"type", "error"}, {"text", result.error}}); return; }
            if (!authentication->isCurrent(result)) {
                sendPacket(client, {{"type", "error"}, {"text", "Mat khau da thay doi. Hay dang nhap lai."}}); return;
            }
            for (const auto &existing : names) {
                if (AuthProtocol::canonical(existing) == AuthProtocol::canonical(result.name)) {
                    sendPacket(client, {{"type", "error"}, {"text", "Tai khoan dang dang nhap o Client khac."}}); return;
                }
            }
            names.insert(client, result.name); recallSync.insert(client,0);editSync.insert(client,0);receiptSync.insert(client,0);
            // Send avatars as individual bounded frames, never a large users packet.
            for (const auto &name : names) sendPacket(client, {{"type","avatar"},{"name",name},{"data",QString::fromLatin1(authentication->avatar(name).toBase64())}});
            broadcast({{"type","avatar"},{"name",result.name},{"data",QString::fromLatin1(authentication->avatar(result.name).toBase64())}});
            sendPacket(client, {{"type", "joined"}, {"name", result.name}, {"account_id", result.id}});
            broadcast({{"type", "system"}, {"text", result.name + " da tham gia phong chat"}});
            updateOnlineUsers();
        });
        connect(authentication, &AuthService::recoveryFinished, this, [this](QTcpSocket *client, const AccountResult &result, bool reset) {
            const QString request = recoveryRequests.take(client);
            if (channels.contains(client) && !request.isEmpty()) {
                sendPacket(client, {{"type",result.ok ? (reset ? "recovery_reset_ok" : "recovery_code") : "recovery_error"},
                    {"request_id",request},{"text",result.error},{"code",result.ok && !reset ? result.recoveryCode : QString()}});
            }
            if (!result.ok || !reset) return;
            for (auto *peer : names.keys()) {
                if (AuthProtocol::canonical(names.value(peer)) != AuthProtocol::canonical(result.name)) continue;
                names.remove(peer); videos->disconnected(peer);
                sendPacket(peer, {{"type","session_revoked"},{"text","Mật khẩu đã được khôi phục. Đăng nhập lại."}});
                peer->disconnectFromHost();
                QTimer::singleShot(5000,peer,[peer]{ if(peer->state()!=QAbstractSocket::UnconnectedState) peer->abort(); });
            }
            updateOnlineUsers();
        });
        connect(authentication, &AuthService::passwordFinished, this, [this](QTcpSocket *client, const AccountResult &result) {
            const QString request = passwordRequests.take(client);
            if (!result.ok) {
                if (channels.contains(client) && !request.isEmpty())
                    sendPacket(client, {{"type", "password_change_error"}, {"request_id", request}, {"text", result.error}});
                return;
            }
            // Revoke by authenticated identity even when the requesting socket disconnected while hashing.
            for (auto *peer : names.keys()) {
                if (AuthProtocol::canonical(names.value(peer)) != AuthProtocol::canonical(result.name)) continue;
                names.remove(peer);
                videos->disconnected(peer);
                if (peer == client && !request.isEmpty())
                    sendPacket(peer, {{"type", "password_changed"}, {"request_id", request}, {"text", "Da doi mat khau. Hay dang nhap lai."}});
                else sendPacket(peer, {{"type", "session_revoked"}, {"text", "Mat khau da thay doi. Hay dang nhap lai."}});
                peer->disconnectFromHost();
                QTimer::singleShot(5000, peer, [peer] { if (peer->state() != QAbstractSocket::UnconnectedState) peer->abort(); });
            }
            updateOnlineUsers();
        });
    }
    if (!server->listen(QHostAddress::Any, port)) {
        lastError = server->errorString();
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
    auto message=HistoryProtocol::stamp(packet);
    if(HistoryProtocol::messageType(message.value("type").toString()) && !recalls.remember(message)) {
        if(auto *c=channels.value(client,nullptr)) c->send({{"type","error"},{"text","Không lưu được thông tin tin nhắn trên Server; chưa gửi tin."}});
        return;
    }
    if(HistoryProtocol::messageType(message.value("type").toString()) && !message.value("to").toString().isEmpty()) {
        const auto state=recalls.receipt(message.value("_event_id").toString());message=HistoryProtocol::withReceipt(message,state);
    }
    if (auto *channel = channels.value(client, nullptr)) channel->send(message);
}

void ChatServer::broadcast(
    const QJsonObject &packet)
{
    const auto message = HistoryProtocol::stamp(packet);
    if(HistoryProtocol::messageType(message.value("type").toString()) && !recalls.remember(message)) {
        for(auto *s:names.keys()) if(names.value(s)==message.value("sender").toString()) sendPacket(s,{{"type","error"},{"text","Không lưu được thông tin tin nhắn trên Server; chưa gửi tin."}});
        return;
    }
    for (QTcpSocket *client : names.keys()) {
        if(auto *c=channels.value(client,nullptr)) c->send(message);
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
    if (type == "recovery_setup" || type == "recovery_reset") {
        const QString request = packet.value("request_id").toString();
        if (!VideoProtocol::validId(request) || recoveryRequests.contains(client) || passwordRequests.contains(client)) return;
        if ((type == "recovery_setup") != names.contains(client)) {
            sendPacket(client, {{"type","recovery_error"},{"request_id",request},{"text","Thao tác không phù hợp với phiên hiện tại."}}); return;
        }
        recoveryRequests.insert(client,request);
        authentication->request(client,type,type=="recovery_setup" ? names.value(client) : packet.value("name").toString(),
            packet.value(type=="recovery_setup" ? "current_password" : "code").toString(),packet.value("new_password").toString());
        return;
    }
    if (recoveryRequests.contains(client)) return;
    if (type == "avatar_set") {
        const QString request = packet.value("request_id").toString();
        if (!names.contains(client) || !VideoProtocol::validId(request)) return;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const QString encoded = packet.value("data").toString();
        const auto bytes = AvatarProtocol::fromBase64(encoded);
        QString error;
        if (avatarTimes.contains(client) && now-avatarTimes.value(client)<2000) error="Chờ 2 giây trước khi đổi ảnh tiếp.";
        else if ((!encoded.isEmpty() && bytes.isEmpty()) || !packet.value("data").isString()) error="Ảnh đại diện không hợp lệ (JPEG tối đa 128×128, 16 KiB).";
        else if (!authentication->setAvatar(names.value(client),bytes)) error="Không lưu được ảnh trên Server.";
        avatarTimes[client]=now;
        sendPacket(client,{{"type","avatar_result"},{"request_id",request},{"ok",error.isEmpty()},{"text",error}});
        if (error.isEmpty()) broadcast({{"type","avatar"},{"name",names.value(client)},{"data",encoded}});
        return;
    }
    if (type == "change_password") {
        const QString request = packet.value("request_id").toString();
        if (!names.contains(client) || !VideoProtocol::validId(request)) {
            sendPacket(client, {{"type", "password_change_error"}, {"request_id", request.left(36)}, {"text", "Can dang nhap truoc khi doi mat khau."}}); return;
        }
        if (passwordRequests.contains(client)) {
            sendPacket(client, {{"type", "password_change_error"}, {"request_id", request}, {"text", "Dang xu ly yeu cau doi mat khau."}}); return;
        }
        passwordRequests.insert(client, request);
        authentication->request(client, type, names.value(client), packet.value("current_password").toString(), packet.value("new_password").toString());
        return;
    }
    if (passwordRequests.contains(client)) return;
    if(type=="video_start" && names.contains(client) && recalls.contains(packet.value("id").toString())) {
        sendPacket(client,{{"type","video_error"},{"id",packet.value("id")},{"text","Mã video đã dùng."}});return;
    }
    if (videos->handle(client, packet)) return;
    if(type=="email_begin" || type=="email_confirm") {
        if(names.contains(client))return;
        if(type=="email_begin")emails->begin(client,packet);else emails->confirm(client,packet);
        return;
    }
    if (type == "register") {sendPacket(client,{{"type","error"},{"text","Đăng ký cần xác minh email. Hãy cập nhật Client."}});return;}
    if (type == "login") {
        if (names.contains(client)) { sendPacket(client, {{"type", "error"}, {"text", "Ban da dang nhap."}}); return; }
        authentication->request(client, type, packet.value("name").toString(), packet.value("password").toString());
        return;
    }
    if (!names.contains(client)) {
        sendPacket(client, {{"type", "error"}, {"text", "Can dang nhap bang tai khoan va mat khau."}}); return;
    }
    if(type=="receipt_sync") {
        qint64 after=0;if(!VideoProtocol::number(packet.value("after"),after,9007199254740991LL) || !receiptSync.contains(client) || receiptSync.value(client)<0 || after!=receiptSync.value(client)) return;
        QString error;qint64 cursor=after;bool more=false;const auto items=recalls.receiptPage(names.value(client),after,cursor,more,error);
        if(!error.isEmpty()) {sendPacket(client,{{"type","receipt_sync_error"},{"text","Không đọc được trạng thái tin trên Server."}});return;}
        receiptSync[client]=more?cursor:-1;sendPacket(client,{{"type","receipt_sync_page"},{"items",items},{"cursor",double(cursor)},{"more",more}});return;
    }
    if(type=="receipt_ack") {
        const auto request=packet.value("request_id").toString();if(!VideoProtocol::validId(request)) return;
        QString error;QJsonArray ignored;const auto notices=recalls.acknowledge(names.value(client),packet.value("items").toArray(),ignored,error);
        if(error.isEmpty()) for(const auto &value:notices) {
            const auto notice=value.toObject();for(auto *peer:names.keys()) if(AuthProtocol::canonical(names.value(peer))==AuthProtocol::canonical(notice.value("sender").toString()) || AuthProtocol::canonical(names.value(peer))==AuthProtocol::canonical(notice.value("to").toString())) sendPacket(peer,notice);
        }
        sendPacket(client,{{"type","receipt_ack_result"},{"request_id",request},{"ok",error.isEmpty()},{"ignored",ignored},{"text",error}});return;
    }
    if(type=="edit_sync") {
        qint64 after=0;
        if(!VideoProtocol::number(packet.value("after"),after,9007199254740991LL) || !editSync.contains(client) || editSync.value(client)<0 || after!=editSync.value(client)) return;
        QString error;qint64 cursor=after;bool more=false;const auto items=recalls.editPage(names.value(client),after,cursor,more,error);
        if(!error.isEmpty()) {sendPacket(client,{{"type","edit_sync_error"},{"text","Không đọc được trạng thái chỉnh sửa trên Server."}});return;}
        editSync[client]=more?cursor:-1;sendPacket(client,{{"type","edit_sync_page"},{"items",items},{"cursor",double(cursor)},{"more",more}});return;
    }
    if(type=="edit_message") {
        const QString request=packet.value("request_id").toString(),id=packet.value("event_id").toString();qint64 base=0;
        if(!VideoProtocol::validId(request)||!VideoProtocol::validId(id)) return;
        auto &limit=editLimits[client];const qint64 now=QDateTime::currentMSecsSinceEpoch();if(now-limit.first>60000) limit={now,0};
        QString error;QJsonObject notification;
        if(++limit.second>30) error="Tối đa 30 yêu cầu sửa tin mỗi phút.";
        else if(!VideoProtocol::number(packet.value("base_revision"),base,9007199254740991LL) || !packet.value("text").isString()) error="Yêu cầu chỉnh sửa không hợp lệ.";
        else notification=recalls.edit(id,names.value(client),packet.value("text").toString(),base,error);
        if(error.isEmpty()) for(auto *peer:names.keys()) {
            const QString to=notification.value("to").toString();
            if(to.isEmpty() || AuthProtocol::canonical(names.value(peer))==AuthProtocol::canonical(notification.value("sender").toString()) || AuthProtocol::canonical(names.value(peer))==AuthProtocol::canonical(to)) sendPacket(peer,notification);
        }
        sendPacket(client,{{"type","edit_result"},{"request_id",request},{"ok",error.isEmpty()},{"text",error}});return;
    }
    if(type=="recall_sync") {
        qint64 after=0;
        if(!VideoProtocol::number(packet.value("after"),after,9007199254740991LL) || !recallSync.contains(client) || recallSync.value(client)<0 || after!=recallSync.value(client)) return;
        QString error; qint64 cursor=after; bool more=false; const auto items=recalls.page(names.value(client),after,cursor,more,error);
        if(!error.isEmpty()) { sendPacket(client,{{"type","recall_sync_error"},{"text","Không đọc được trạng thái thu hồi trên Server."}});return; }
        recallSync[client]=more?cursor:-1;
        sendPacket(client,{{"type","recall_sync_page"},{"items",items},{"cursor",double(cursor)},{"more",more}});return;
    }
    if(type=="recall_message") {
        const QString request=packet.value("request_id").toString(),id=packet.value("event_id").toString();
        if(!VideoProtocol::validId(request)||!VideoProtocol::validId(id)) return;
        auto &limit=recallLimits[client]; const qint64 now=QDateTime::currentMSecsSinceEpoch();
        if(now-limit.first>60000) limit={now,0};
        QString error; QJsonObject notification;
        if(++limit.second>30) error="Tối đa 30 yêu cầu thu hồi mỗi phút.";
        else notification=recalls.recall(id,names.value(client),error);
        if(error.isEmpty()) for(auto *peer:names.keys()) {
            const auto to=notification.value("to").toString();
            if(to.isEmpty() || AuthProtocol::canonical(names.value(peer))==AuthProtocol::canonical(notification.value("sender").toString()) || AuthProtocol::canonical(names.value(peer))==AuthProtocol::canonical(to)) sendPacket(peer,notification);
        }
        sendPacket(client,{{"type","recall_result"},{"request_id",request},{"ok",error.isEmpty()},{"text",error}});return;
    }
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
        privateMessage = HistoryProtocol::stamp(privateMessage);




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
        imagePacket = HistoryProtocol::stamp(imagePacket);

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
        forwardPacket = HistoryProtocol::stamp(forwardPacket);

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
        stickerPacket = HistoryProtocol::stamp(stickerPacket);

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



