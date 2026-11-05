#include "jsonchannel.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTimer>
#include <QSslSocket>
#include <algorithm>

JsonChannel::JsonChannel(QTcpSocket *s, QObject *parent) : QObject(parent), socket(s) {
    socket->setReadBufferSize(VideoProtocol::MaxFrame + 1);
    connect(socket, &QTcpSocket::readyRead, this, &JsonChannel::drain);
    connect(socket, &QTcpSocket::disconnected, this, [this] { decoder.clear(); });
    connect(socket, &QTcpSocket::connected, this, [this] {
        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    });
    if (socket->state() == QAbstractSocket::ConnectedState)
        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
}
void JsonChannel::fail(const QString &text) {
    decoder.clear();
    emit transportError(text);
    socket->abort();
}
bool JsonChannel::send(const QJsonObject &packet) {
    if (socket->state() != QAbstractSocket::ConnectedState) return false;
    auto *tls = qobject_cast<QSslSocket *>(socket);
    if (tls && !tls->isEncrypted()) return false;
    QByteArray frame = QJsonDocument(packet).toJson(QJsonDocument::Compact);
    if (frame.size() > VideoProtocol::MaxFrame) { fail("JSON frame qua lon"); return false; }
    frame.append('\n');
    const qint64 queued = socket->bytesToWrite() + (tls ? tls->encryptedBytesToWrite() : 0);
    if (queued + frame.size() > VideoProtocol::MaxQueued) {
        fail("Ket noi qua cham: hang doi gui da day"); return false;
    }
    if (socket->write(frame) != frame.size()) { fail("Khong ghi duoc socket"); return false; }
    return true;
}
void JsonChannel::drain() {
    if (scheduled) return;
    int frames = 0;
    while (socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject packet;
        const auto result = decoder.next(packet);
        if (result == JsonFramer::Error) { fail("JSON khong hop le hoac frame qua lon"); return; }
        if (result == JsonFramer::Packet) {
            emit packetReceived(packet);
            if (++frames == 32) {
                scheduled = true;
                QTimer::singleShot(0, this, [this] { scheduled = false; drain(); });
                return;
            }
            continue;
        }
        if (!socket->bytesAvailable()) return;
        if (!decoder.append(socket->read(std::min<qint64>(64 * 1024, decoder.room())))) {
            fail("JSON frame qua lon"); return;
        }
    }
}
