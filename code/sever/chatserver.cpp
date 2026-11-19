#include "authprotocol.h"
#include "avatarprotocol.h"
#include "groupimageprotocol.h"
#include <QDateTime>
#include <QUuid>
#include <QSet>
#include <QSaveFile>
#include <QFile>

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

namespace {
constexpr qsizetype GroupHistoryCount = 300;
constexpr qsizetype GroupHistoryBytes = 384 * 1024;
}

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
                                        {"text", name + " đã ngoại tuyến"}
                                    });

                                    updateOnlineUsers();
                                }
                            });
                }
            });
}

void ChatServer::loadGroups() {
    groups.clear();
    QFile file(QDir(directory).filePath("groups.json"));
    if(!file.open(QIODevice::ReadOnly)) return;
    if(file.size()>1024*1024) {qWarning()<<"groups.json quá lớn";return;}
    QJsonParseError error;
    const QJsonDocument doc=QJsonDocument::fromJson(file.readAll(),&error);
    if(error.error!=QJsonParseError::NoError || !doc.isArray()) return;
    for(const auto &value:doc.array()) {
        if(!value.isObject() || groups.size()>=1000) continue;
        const auto obj=value.toObject();
        const QString id=obj.value("id").toString(), name=obj.value("name").toString();
        const auto membersValue=obj.value("members").toArray();
        if(QUuid(id).isNull() || name.isEmpty() || name.size()>48 || membersValue.size()<2 || membersValue.size()>16) continue;
        QStringList members;QSet<QString> unique;
        for(const auto &v:membersValue) {
            const QString person=v.toString();const QString key=AuthProtocol::canonical(person);
            if(!AuthProtocol::validName(person) || unique.contains(key)) {members.clear();break;}
            members.append(person);unique.insert(key);
        }
        if(members.size()>=2) groups.insert(id,{name,members});
    }
}
bool ChatServer::saveGroups() {
    QJsonArray data;
    for(auto it=groups.cbegin();it!=groups.cend();++it) {
        QJsonArray members;for(const auto &name:it.value().members) members.append(name);
        data.append(QJsonObject{{"id",it.key()},{"name",it.value().name},{"members",members}});
    }
    QSaveFile file(QDir(directory).filePath("groups.json"));
    if(!file.open(QIODevice::WriteOnly)) return false;
    if(file.write(QJsonDocument(data).toJson(QJsonDocument::Compact))<0) return false;
    return file.commit();
}
void ChatServer::sendGroupsFor(QTcpSocket *client) {
    const QString account=names.value(client);
    if(account.isEmpty()) return;
    for(auto it=groups.cbegin();it!=groups.cend();++it) {
        bool member=false;
        for(const auto &name:it.value().members)
            if(AuthProtocol::canonical(name)==AuthProtocol::canonical(account)) { member=true; break; }
        if(!member) continue;
        QJsonArray users;
        for(const auto &name:it.value().members) users.append(name);
        sendPacket(client,{{"type","group_created"},{"id",it.key()},
            {"name",it.value().name},{"members",users},{"existing",true}});
    }
}

bool ChatServer::readGroupHistory(const QString &id, QJsonArray &messages) const {
    QFile file(QDir(directory).filePath("group-history/" + id + ".json"));
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly) || file.size() > GroupHistoryBytes) return false;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) return false;
    messages = document.array();
    return true;
}

bool ChatServer::saveGroupMessage(const QString &id, const QJsonObject &message) const {
    QJsonArray messages;
    if (!readGroupHistory(id, messages)) return false;
    messages.append(message);
    return writeGroupHistory(id, messages);
}

bool ChatServer::writeGroupHistory(const QString &id, QJsonArray messages) const {
    while (messages.size() > GroupHistoryCount) messages.removeFirst();
    QByteArray data = QJsonDocument(messages).toJson(QJsonDocument::Compact);
    while (data.size() > GroupHistoryBytes) {
        messages.removeFirst();
        data = QJsonDocument(messages).toJson(QJsonDocument::Compact);
    }
    const QDir root(directory);
    if (!root.mkpath("group-history")) return false;
    QSaveFile file(root.filePath("group-history/" + id + ".json"));
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
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
        if(!QDir().mkpath(directory)) {lastError="Không tạo được thư mục dữ liệu Server.";return false;}
        loadGroups();
        if(!recalls.open(QDir(directory).filePath("recalls.sqlite"),lastError)) return false;
        QSslConfiguration tls;
        if (!TlsIdentity::load(QDir(directory).filePath("tls"), tls, lastError)) return false;
        server->setSslConfiguration(tls);
        authentication = new AuthService(QDir(directory).filePath("accounts.sqlite"), this);
        if (!authentication->initialize(lastError)) { delete authentication; authentication = nullptr; return false; }
        emails=new EmailService(QDir(directory).filePath("accounts.sqlite"),this);
        connect(emails,&EmailService::deliveryStatus,this,&ChatServer::mailStatus);
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
            sendGroupsFor(client);
            broadcast({{"type", "system"}, {"text", result.name + " đã trực tuyến"}});
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
    // Server owns membership. Clients cannot supply an arbitrary sender or room audience.
    if(type=="group_create") {
        if(!names.contains(client)) return;
        const QString groupName=packet.value("name").toString().trimmed();
        const auto list=packet.value("members").toArray();
        bool invalidName=groupName.isEmpty() || groupName.size()>48;
        for(QChar ch:groupName) if(ch.unicode()<32 || ch.unicode()==127) invalidName=true;
        if(invalidName || list.isEmpty() || list.size()>15 || groups.size()>=1000) {
            sendPacket(client,{{"type","error"},{"text","Tên nhóm hoặc số thành viên không hợp lệ (2–16 người tính cả bạn)."}});return;
        }
        QStringList members{names.value(client)};
        QSet<QString> unique{AuthProtocol::canonical(names.value(client))};
        for(const auto &value:list) {
            if(!value.isString()) {sendPacket(client,{{"type","error"},{"text","Danh sách thành viên không hợp lệ."}});return;}
            const QString invited=value.toString();
            QString onlineName;
            for(const auto &actual:names)
                if(AuthProtocol::canonical(actual)==AuthProtocol::canonical(invited)) {onlineName=actual;break;}
            if(onlineName.isEmpty() || unique.contains(AuthProtocol::canonical(onlineName))) {
                sendPacket(client,{{"type","error"},{"text","Chọn các tài khoản đang online, không trùng nhau và không chọn chính mình."}});return;
            }
            unique.insert(AuthProtocol::canonical(onlineName)); members.append(onlineName);
        }
        int ownGroups=0;
        for(const auto &room:groups) if(room.members.contains(names.value(client),Qt::CaseInsensitive)) ++ownGroups;
        if(ownGroups>=32) {sendPacket(client,{{"type","error"},{"text","Bạn đã đạt giới hạn 32 nhóm trên Server."}});return;}
        const QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        groups.insert(id,{groupName,members});
        if(!saveGroups()) {
            groups.remove(id);
            sendPacket(client,{{"type","error"},{"text","Không lưu được nhóm trên Server."}});return;
        }
        QJsonArray audience;for(const auto &member:members) audience.append(member);
        const QJsonObject created{{"type","group_created"},{"id",id},{"name",groupName},{"members",audience}};
        for(auto *socket:names.keys()) if(members.contains(names.value(socket),Qt::CaseInsensitive)) sendPacket(socket,created);
        return;
    }
    if (type == "group_rename" || type == "group_invite") {
        if (!names.contains(client)) return;
        const QString id = packet.value("id").toString();
        auto group = groups.find(id);
        if (group == groups.end() || group->members.isEmpty()
            || AuthProtocol::canonical(group->members.first()) != AuthProtocol::canonical(names.value(client))) {
            sendPacket(client, {{"type","error"},{"text","Chỉ người tạo nhóm mới có thể thực hiện thao tác này."}});
            return;
        }
        const auto previous = *group;
        QString invited;
        if (type == "group_invite") {
            const QString target = AuthProtocol::canonical(packet.value("name").toString());
            for (const auto &account : names)
                if (AuthProtocol::canonical(account) == target) { invited = account; break; }
            if (invited.isEmpty() || group->members.contains(invited, Qt::CaseInsensitive)
                || group->members.size() >= 16) {
                sendPacket(client, {{"type","error"},{"text","Hãy chọn người đang online, chưa thuộc nhóm. Nhóm tối đa 16 thành viên."}});
                return;
            }
            group->members.append(invited);
        } else {
            const QString name = packet.value("name").toString().trimmed();
            const bool valid = !name.isEmpty() && name.size() <= 48
                && std::none_of(name.cbegin(), name.cend(), [](QChar ch) { return ch.unicode() < 32 || ch.unicode() == 127; });
            if (!valid) {
                sendPacket(client, {{"type","error"},{"text","Tên nhóm phải dài từ 1 đến 48 ký tự hợp lệ."}});
                return;
            }
            group->name = name;
        }
        if (!saveGroups()) {
            *group = previous;
            sendPacket(client, {{"type","error"},{"text","Không lưu được thay đổi của nhóm."}});
            return;
        }
        const auto members = group->members;
        QJsonObject update{{"type","group_created"},{"id",id},{"name",group->name},
                                 {"members",QJsonArray::fromStringList(members)},{"existing",true}};
        for (auto *peer : names.keys()) {
            const QString account = names.value(peer);
            if (!members.contains(account, Qt::CaseInsensitive)) continue;
            update["existing"] = account != invited;
            sendPacket(peer, update);
        }
        return;
    }
    if (type == "group_chat" || type == "group_image" || type == "group_sticker"
        || type == "group_history_request" || type == "group_message_update") {
        if(!names.contains(client)) return;
        const QString id=packet.value("id").toString();
        const auto it=groups.constFind(id);
        const bool member = it != groups.cend() && it.value().members.contains(names.value(client), Qt::CaseInsensitive);
        if (type == "group_history_request") {
            QJsonObject response{{"type","group_history"},{"id",id}};
            QJsonArray messages;
            if (!member) response["error"] = "Bạn không phải thành viên của nhóm này.";
            else if (!readGroupHistory(id, messages)) response["error"] = "Không đọc được lịch sử nhóm.";
            else response["messages"] = messages;
            sendPacket(client, response);
            return;
        }
        if (!member) {
            sendPacket(client, {{"type","error"},{"text","Bạn không phải thành viên của nhóm này."}});
            return;
        }
        if (type == "group_message_update") {
            const QString messageId = packet.value("message_id").toString();
            const QString operation = packet.value("operation").toString();
            const QString account = AuthProtocol::canonical(names.value(client));
            QJsonArray messages;
            if (!VideoProtocol::validId(messageId) || !readGroupHistory(id, messages)) return;
            for (qsizetype index = 0; index < messages.size(); ++index) {
                auto message = messages.at(index).toObject();
                if (message.value("message_id") != messageId || message.value("recalled").toBool()) continue;
                const bool own = AuthProtocol::canonical(message.value("sender").toString()) == account;
                if (operation == "react") {
                    const QString emoji = packet.value("emoji").toString();
                    if (emoji != "👍" && emoji != "❤️" && emoji != "😂") return;
                    auto reactions = message.value("reactions").toObject();
                    if (reactions.value(account) == emoji) reactions.remove(account);
                    else reactions[account] = emoji;
                    message["reactions"] = reactions;
                } else if (!own) {
                    sendPacket(client, {{"type","error"},{"text","Bạn chỉ có thể sửa hoặc thu hồi tin của mình."}});
                    return;
                } else if (operation == "edit" && message.contains("text")) {
                    const QString text = packet.value("text").toString().trimmed();
                    if (text.isEmpty() || text.toUtf8().size() > 4000) return;
                    message["text"] = text;
                    message["edited"] = true;
                } else if (operation == "recall") {
                    for (const auto *field : {"text", "image", "sticker", "quote", "reactions", "edited"})
                        message.remove(field);
                    message["recalled"] = true;
                } else return;
                messages[index] = message;
                if (!writeGroupHistory(id, messages)) {
                    sendPacket(client, {{"type","error"},{"text","Không lưu được thay đổi của tin nhắn."}});
                    return;
                }
                const auto members = it->members;
                for (auto *peer : names.keys())
                    if (members.contains(names.value(peer), Qt::CaseInsensitive))
                        sendPacket(peer, {{"type","group_history_changed"},{"id",id}});
                return;
            }
            sendPacket(client, {{"type","error"},{"text","Tin nhắn không còn trong lịch sử nhóm."}});
            return;
        }
        QJsonObject message{{"sender",names.value(client)},
            {"message_id",QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        if (type == "group_image") {
            const QString image = packet.value("data").toString();
            if (GroupImageProtocol::decode(image).isNull()) {
                sendPacket(client, {{"type","error"},{"text","Ảnh nhóm phải là JPEG hợp lệ, tối đa 96 KiB và 960 px mỗi cạnh."}});
                return;
            }
            message["image"] = image;
        } else if (type == "group_sticker") {
            const QString sticker = packet.value("sticker").toString();
            if (sticker != "nobita" && sticker != "doremon") return;
            message["sticker"] = sticker;
        } else {
            const QString text = packet.value("text").toString().trimmed();
            if (text.isEmpty() || text.toUtf8().size() > 4000) return;
            message["text"] = text;
            const QString quote = packet.value("quote").toString().trimmed().left(500);
            if (!quote.isEmpty()) message["quote"] = quote;
        }
        if (!saveGroupMessage(id, message)) {
            sendPacket(client, {{"type","error"},{"text","Không lưu được tin nhắn nhóm. Tin chưa được gửi."}});
            return;
        }
        auto msg = message;
        msg["type"] = "group_chat";
        msg["id"] = id;
        for(auto *socket:names.keys()) if(it.value().members.contains(names.value(socket),Qt::CaseInsensitive)) sendPacket(socket,msg);
        return;
    }
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
    if(type=="video_start" && names.contains(client) && packet.value("to").toString().trimmed().isEmpty()) {
        sendPacket(client,{{"type","video_error"},{"id",packet.value("id")},{"text","Hãy chọn người nhận video."}});return;
    }
    if(type=="video_start" && names.contains(client) && recalls.contains(packet.value("id").toString())) {
        sendPacket(client,{{"type","video_error"},{"id",packet.value("id")},{"text","Mã video đã dùng."}});return;
    }
    if (type == "video_start" && names.contains(client)) {
        const QString target = packet.value("to").toString();
        if (VideoProtocol::groupTarget(target)) {
            const auto group = groups.constFind(target.mid(6));
            if (group == groups.cend() || !group->members.contains(names.value(client), Qt::CaseInsensitive)) {
                sendPacket(client, {{"type","video_error"},{"id",packet.value("id").toString().left(36)},
                                    {"text","Bạn không phải thành viên của nhóm này."}});
                return;
            }
            QList<QTcpSocket *> recipients;
            for (auto peer = names.cbegin(); peer != names.cend(); ++peer)
                if (peer.key() != client && group->members.contains(peer.value(), Qt::CaseInsensitive))
                    recipients.append(peer.key());
            videos->handle(client, packet, recipients);
            return;
        }
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

        // No global room: never broadcast media to all connected users.
        if (receiverName.isEmpty()) {
            sendPacket(client,{{"type","error"},{"text","Hãy chọn người nhận ảnh."}});
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

    // Public-room messages are no longer accepted.
    if(type=="chat") {
        sendPacket(client,{{"type","error"},{"text","Phòng chung đã được gỡ bỏ. Hãy tạo nhóm hoặc nhắn riêng."}});
        return;
    }

    // Replies are private after removing the public room.
    if(type=="reply") {
        const QString to=packet.value("to").toString().trimmed();
        const QString text=packet.value("text").toString().trimmed();
        const QString quote=packet.value("quote").toString().trimmed();
        if(to.isEmpty() || text.isEmpty() || quote.isEmpty() || text.toUtf8().size()>4000 || quote.toUtf8().size()>1000) return;
        QTcpSocket *receiver=nullptr;
        for(auto *peer:names.keys()) if(AuthProtocol::canonical(names.value(peer))==AuthProtocol::canonical(to)) {receiver=peer;break;}
        if(!receiver) {sendPacket(client,{{"type","error"},{"text","Người nhận hiện không online."}});return;}
        const QJsonObject outgoing=HistoryProtocol::stamp({{"type","reply"},{"sender",names.value(client)},
            {"to",names.value(receiver)},{"text",text},{"quote",quote}});
        sendPacket(receiver,outgoing);
        if(receiver!=client) sendPacket(client,outgoing);
        return;
    }

    // ==================================
    // XU LY FORWARD
    // ==================================
    if(type=="forward" && packet.value("to").toString().trimmed().isEmpty()) {
        sendPacket(client,{{"type","error"},{"text","Hãy chọn người nhận tin chuyển tiếp."}});return;
    }
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
    if(type=="sticker" && packet.value("to").toString().trimmed().isEmpty()) {
        sendPacket(client,{{"type","error"},{"text","Hãy chọn người nhận Sticker."}});return;
    }
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



