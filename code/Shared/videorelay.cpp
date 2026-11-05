#include "videorelay.h"
#include <QTimer>

VideoRelay::VideoRelay(Send s, Name n, Peers p, QObject *parent, int timeoutMs)
    : QObject(parent), send(std::move(s)), name(std::move(n)), peers(std::move(p)) {
    auto *timer = new QTimer(this);
    timer->setInterval(qMin(1000, timeoutMs));
    connect(timer, &QTimer::timeout, this, [this, timeoutMs] {
        for (const auto &t : transfers.values())
            if (t->activity.hasExpired(timeoutMs)) abort(t->id, "Video het thoi gian cho (timeout)");
    });
    timer->start();
}
void VideoRelay::abort(const QString &id, const QString &reason) {
    auto t = transfers.take(id);
    if (!t) return;
    const QJsonObject p{{"type", "video_error"}, {"id", id}, {"text", reason}};
    send(t->sender, p);
    for (auto *r : t->receivers) send(r, p);
}
void VideoRelay::disconnected(QTcpSocket *client) {
    for (const auto &t : transfers.values())
        if (t->sender == client || t->receivers.contains(client))
            abort(t->id, "Mot ben truyen video da ngat ket noi");
}
void VideoRelay::forward(const std::shared_ptr<Transfer> &t, const QJsonObject &p) {
    t->pending = t->receivers;
    t->activity.restart();
    for (auto *r : t->receivers) {
        if (!transfers.contains(t->id)) return;
        if (!send(r, p)) { abort(t->id, "Khong gui duoc video den nguoi nhan"); return; }
    }
    if (t->pending.isEmpty()) advance(t);
}
void VideoRelay::advance(const std::shared_ptr<Transfer> &t) {
    const QString phase = t->phase;
    t->activity.restart();
    if (phase == "done") {
        transfers.remove(t->id);
        send(t->sender, {{"type", "video_done"}, {"id", t->id}, {"offset", double(t->offset)}});
    } else {
        // Exactly one chunk in flight, acknowledged by every recipient's disk writer.
        t->phase = "send";
        if (!send(t->sender, {{"type", phase == "ready" ? "video_ready" : "video_ack"},
                              {"id", t->id}, {"offset", double(t->offset)}}))
            abort(t->id, "Khong gui duoc xac nhan video");
    }
}
bool VideoRelay::handle(QTcpSocket *client, const QJsonObject &p) {
    const QString type = p.value("type").toString();
    if (!type.startsWith("video_")) return false;
    const QString id = p.value("id").toString();
    auto reject = [&](const QString &text) {
        send(client, {{"type", "video_error"}, {"id", id.left(36)}, {"text", text}});
    };
    if (name(client).isEmpty()) { reject("Can dang nhap truoc khi gui video"); return true; }
    if (!VideoProtocol::validId(id)) { reject("Ma video khong hop le"); return true; }
    if (type == "video_start") {
        if (transfers.contains(id)) { reject("Ma video da duoc su dung"); return true; }
        for (const auto &t : transfers)
            if (t->sender == client) { reject("Chi gui mot video moi lan"); return true; }
        qint64 size = 0;
        const QString filename = p.value("name").toString();
        if (transfers.size() >= VideoProtocol::MaxTransfers
            || !VideoProtocol::number(p.value("size"), size) || size < 12
            || !VideoProtocol::validName(filename)) { reject("MP4 khong hop le hoac Server dang ban (toi da 1 GiB)"); return true; }
        const QString to = p.value("to").toString();
        auto t = std::make_shared<Transfer>();
        t->id = id; t->sender = client; t->size = size; t->phase = "ready";
        bool found = to.isEmpty();
        for (auto *peer : peers()) {
            if (to.isEmpty() || name(peer) == to) {
                found = true;
                if (peer != client) t->receivers.insert(peer);
            }
        }
        if (!found) { reject("Nguoi nhan video khong online"); return true; }
        transfers.insert(id, t);
        forward(t, {{"type", "video_start"}, {"id", id}, {"name", filename},
                    {"size", double(size)}, {"sender", name(client)}, {"to", to}});
        return true;
    }
    auto t = transfers.value(id);
    if (!t) { reject("Video khong con hoat dong"); return true; }
    const bool sender = t->sender == client;
    const bool receiver = t->receivers.contains(client);
    if (!sender && !receiver) { reject("Khong co quyen voi video nay"); return true; }
    if (type == "video_cancel" || type == "video_error") {
        abort(id, "Video bi huy: " + p.value("text").toString().left(200)); return true;
    }
    if (type == "video_ack" && receiver) {
        qint64 offset;
        if (!t->pending.contains(client) || p.value("phase").toString() != t->phase
            || !VideoProtocol::number(p.value("offset"), offset) || offset != t->offset) {
            abort(id, "Xac nhan video sai thu tu"); return true;
        }
        t->pending.remove(client);
        if (t->pending.isEmpty()) advance(t);
        return true;
    }
    if (sender && type == "video_chunk") {
        qint64 offset;
        QByteArray bytes;
        if (t->phase != "send" || !VideoProtocol::number(p.value("offset"), offset)
            || offset != t->offset || !VideoProtocol::decode(p, bytes)
            || bytes.size() > t->size - t->offset
            || (t->offset == 0 && !VideoProtocol::isMp4(bytes))) {
            abort(id, "Khoi video sai thu tu, sai kich thuoc hoac khong phai MP4"); return true;
        }
        t->hash.addData(bytes);
        t->offset += bytes.size();
        t->phase = "chunk";
        forward(t, {{"type", "video_chunk"}, {"id", id}, {"offset", double(offset)},
                    {"data", p.value("data")}});
        return true;
    }
    if (sender && type == "video_end") {
        const QString sha = p.value("sha256").toString();
        if (t->phase != "send" || t->offset != t->size || !VideoProtocol::digest(sha)
            || sha.toLatin1() != t->hash.result().toHex()) {
            abort(id, "Video chua du du lieu hoac SHA-256 khong khop"); return true;
        }
        t->phase = "done";
        forward(t, {{"type", "video_end"}, {"id", id}, {"sha256", sha}});
        return true;
    }
    abort(id, "Goi video khong dung trang thai");
    return true;
}
