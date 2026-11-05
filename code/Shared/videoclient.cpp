#include "videoclient.h"
#include <QDir>
#include <QStorageInfo>
#include <QTimer>
#include <algorithm>

VideoClient::VideoClient(JsonChannel *c, QTcpSocket *s, QString dir, QObject *parent, int timeoutMs)
    : QObject(parent), channel(c), directory(std::move(dir)) {
    connect(s, &QTcpSocket::disconnected, this, [this] {
        const bool active = out || !incoming.isEmpty();
        out.reset(); incoming.clear();
        if (active) emit failed("Mat ket noi: da huy video dang truyen va xoa tep tam");
    });
    auto *timer = new QTimer(this);
    timer->setInterval(qMin(1000, timeoutMs));
    connect(timer, &QTimer::timeout, this, [this, timeoutMs] {
        if (out && out->activity.hasExpired(timeoutMs)) fail(out->id, "Gui video het thoi gian cho");
        for (const auto &t : incoming.values())
            if (t->activity.hasExpired(timeoutMs)) fail(t->id, "Nhan video het thoi gian cho");
    });
    timer->start();
}
void VideoClient::fail(QString id, const QString &text, bool notify) {
    if (out && out->id == id) out.reset();
    incoming.remove(id); // QSaveFile destructor discards unfinished data.
    if (notify) channel->send({{"type", "video_cancel"}, {"id", id}, {"text", text.left(200)}});
    emit failed(text);
}
void VideoClient::cancel() {
    if (out) fail(out->id, "Ban da huy gui video");
}
bool VideoClient::start(const QString &path, const QString &to) {
    if (out) { emit failed("Dang gui video, hay cho hoan tat hoac bam Huy video"); return false; }
    auto t = std::make_shared<Outgoing>();
    t->name = QFileInfo(path).fileName(); t->path = QFileInfo(path).absoluteFilePath(); t->to = to;
    t->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t->file.setFileName(path);
    if (!VideoProtocol::validName(t->name) || !t->file.open(QIODevice::ReadOnly)) {
        emit failed("Khong mo duoc tep MP4 (ten toi da 160 ky tu)"); return false;
    }
    t->size = t->file.size();
    if (t->size < 12 || t->size > VideoProtocol::MaxSize || !VideoProtocol::isMp4(t->file.peek(12))) {
        emit failed("Can tep MP4 co chu ky ftyp hop le, toi da 1 GiB (1.073.741.824 byte)"); return false;
    }
    t->activity.start();
    out = t;
    if (!channel->send({{"type", "video_start"}, {"id", t->id}, {"to", to},
                        {"name", t->name}, {"size", double(t->size)}})) {
        out.reset(); emit failed("Khong gui duoc yeu cau video"); return false;
    }
    emit progress("Dang cho nguoi nhan san sang: " + t->name);
    return true;
}
void VideoClient::pump() {
    auto t = out;
    if (!t || t->phase != "send") return;
    t->activity.restart();
    if (t->offset == t->size) {
        if (t->file.size() != t->size || !t->file.read(1).isEmpty()) {
            fail(t->id, "Tep nguon da thay doi khi dang gui"); return;
        }
        t->phase = "done";
        if (!channel->send({{"type", "video_end"}, {"id", t->id},
                            {"sha256", QString::fromLatin1(t->hash.result().toHex())}}))
            fail(t->id, "Khong gui duoc ket thuc video", false);
        return;
    }
    const qint64 want = std::min(VideoProtocol::ChunkSize, t->size - t->offset);
    const QByteArray bytes = t->file.read(want);
    if (bytes.size() != want) { fail(t->id, "Khong doc duoc tep nguon hoac tep da bi cat ngan"); return; }
    const qint64 offset = t->offset;
    t->hash.addData(bytes); t->offset += bytes.size(); t->phase = "chunk";
    if (!channel->send({{"type", "video_chunk"}, {"id", t->id}, {"offset", double(offset)},
                        {"data", QString::fromLatin1(bytes.toBase64())}}))
        fail(t->id, "Khong gui duoc khoi video", false);
}
bool VideoClient::ack(const std::shared_ptr<Incoming> &t, const QString &phase) {
    if (channel->send({{"type", "video_ack"}, {"id", t->id},
                       {"phase", phase}, {"offset", double(t->offset)}})) return true;
    fail(t->id, "Khong gui duoc xac nhan video", false);
    return false;
}
bool VideoClient::handle(const QJsonObject &p) {
    const QString type = p.value("type").toString();
    if (!type.startsWith("video_")) return false;
    const QString id = p.value("id").toString();
    if (!VideoProtocol::validId(id)) { emit failed("Server gui ma video khong hop le"); return true; }
    if (type == "video_error") {
        fail(id, p.value("text").toString().left(300), false); return true;
    }
    if (out && out->id == id) {
        auto t = out;
        qint64 offset;
        if (!VideoProtocol::number(p.value("offset"), offset) || offset != t->offset
            || !((type == "video_ready" && t->phase == "ready")
                 || (type == "video_ack" && t->phase == "chunk")
                 || (type == "video_done" && t->phase == "done"))) {
            fail(id, "Xac nhan gui video khong hop le"); return true;
        }
        t->activity.restart();
        if (type == "video_done") {
            out.reset();
            emit historyReady({{"type", "video"}, {"_event_id", t->id}, {"id", t->id},
                {"sender", QString()}, {"to", t->to}, {"name", t->name}, {"local_path", t->path},
                {"size", double(t->size)}, {"sha256", QString::fromLatin1(t->hash.result().toHex())}, {"outgoing", true}});
            emit completed(t->path, QString(), t->name, true);
        } else {
            emit progress(QString("Gui %1: %2% (%3 / %4 byte da xac nhan)")
                          .arg(t->name).arg(100 * offset / t->size).arg(offset).arg(t->size));
            t->phase = "send";
            QTimer::singleShot(0, this, &VideoClient::pump);
        }
        return true;
    }
    if (type == "video_start") {
        qint64 size;
        const QString filename = p.value("name").toString();
        if (incoming.contains(id) || incoming.size() >= VideoProtocol::MaxTransfers
            || !VideoProtocol::validName(filename) || !VideoProtocol::number(p.value("size"), size)
            || size < 12) { fail(id, "Thong tin video nhan khong hop le"); return true; }
        auto t = std::make_shared<Incoming>();
        t->id = id; t->size = size; t->name = filename; t->sender = p.value("sender").toString().left(20);
        t->to = p.value("to").toString();
        // Never use a network supplied name as a local filesystem path.
        t->path = QDir(directory).absoluteFilePath(id + ".mp4");
        if (!QDir().mkpath(directory)) { fail(id, "Khong tao duoc thu muc video"); return true; }
        const QStorageInfo storage(directory);
        if (storage.isValid() && storage.isReady() && storage.bytesAvailable() >= 0
            && storage.bytesAvailable() < size) { fail(id, "Khong du dung luong dia de nhan video"); return true; }
        t->file.setFileName(t->path);
        t->file.setDirectWriteFallback(false);
        if (QFile::exists(t->path) || !t->file.open(QIODevice::WriteOnly)) {
            fail(id, "Khong tao duoc tep video tam"); return true;
        }
        t->activity.start(); incoming.insert(id, t);
        ack(t, "ready");
        emit progress("Dang nhan video tu " + t->sender + ": " + filename);
        return true;
    }
    auto t = incoming.value(id);
    if (!t) { fail(id, "Khong tim thay video dang nhan"); return true; }
    t->activity.restart();
    if (type == "video_chunk") {
        qint64 offset;
        QByteArray bytes;
        if (!VideoProtocol::number(p.value("offset"), offset) || offset != t->offset
            || !VideoProtocol::decode(p, bytes) || bytes.size() > t->size - t->offset
            || (offset == 0 && !VideoProtocol::isMp4(bytes))) {
            fail(id, "Khoi video nhan bi loi"); return true;
        }
        if (t->file.write(bytes) != bytes.size()) { fail(id, "Loi ghi dia khi nhan video"); return true; }
        t->hash.addData(bytes); t->offset += bytes.size();
        ack(t, "chunk");
        emit progress(QString("Nhan %1: %2%").arg(t->name).arg(100 * t->offset / t->size));
    } else if (type == "video_end") {
        const QString sha = p.value("sha256").toString();
        if (t->offset != t->size || !VideoProtocol::digest(sha)
            || t->hash.result().toHex() != sha.toLatin1()) {
            fail(id, "Video thieu du lieu hoac SHA-256 khong khop"); return true;
        }
        if (!t->file.commit()) { fail(id, "Khong luu duoc video: " + t->file.errorString()); return true; }
        incoming.remove(id);
        ack(t, "done");
        emit historyReady({{"type", "video"}, {"_event_id", t->id}, {"id", t->id},
            {"sender", t->sender}, {"to", t->to}, {"name", t->name}, {"local_path", t->path},
            {"size", double(t->size)}, {"sha256", sha}, {"outgoing", false}});
        emit completed(t->path, t->sender, t->name, false);
    } else { fail(id, "Goi video nhan sai trang thai"); }
    return true;
}
