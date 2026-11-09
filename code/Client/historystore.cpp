#include "historystore.h"
#include "historyimage.h"
#include "historyprotocol.h"
#include "videoprotocol.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

HistoryStore::~HistoryStore() { close(); }
void HistoryStore::close() {
    if (database.isValid()) database.close();
    database = QSqlDatabase();
    if (!connectionName.isEmpty()) QSqlDatabase::removeDatabase(connectionName);
    connectionName.clear();
}
QString HistoryStore::accountPath(const QString &root, const QString &server, quint16 port, const QString &username) {
    const QJsonArray identity{server.trimmed().toCaseFolded(), int(port), username.trimmed().toCaseFolded()};
    const auto key = QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex();
    return QDir(root).filePath(QString::fromLatin1(key) + ".sqlite");
}
bool HistoryStore::open(const QString &path, QString &error) {
    close(); error.clear(); filePath = QFileInfo(path).absoluteFilePath();
    if (!QDir().mkpath(QFileInfo(filePath).absolutePath())) {
        error = "Khong tao duoc thu muc lich su"; return false;
    }
    if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
        error = "Thieu Qt SQLite driver (sqldrivers/qsqlite.dll)"; return false;
    }
    connectionName = "chat-history-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    database.setDatabaseName(filePath);
    database.setConnectOptions("QSQLITE_BUSY_TIMEOUT=3000");
    if (!database.open()) { error = database.lastError().text(); close(); return false; }
    bool ok = true;
    {
        QSqlQuery query(database);
        auto exec = [&](const QString &sql) {
            if (query.exec(sql)) return true;
            error = query.lastError().text(); return false;
        };
        ok = exec("PRAGMA user_version") && query.next();
        const int version = ok ? query.value(0).toInt() : -1;
        query.finish();
        if (ok && version > 1) { error = "Database lich su moi hon phien ban ung dung"; ok = false; }
        if (ok) ok = exec("PRAGMA journal_mode=WAL") && exec("PRAGMA synchronous=FULL");
        if (ok) {
            ok = database.transaction();
            if (!ok) error = database.lastError().text();
            if (ok) {
                ok = exec("CREATE TABLE IF NOT EXISTS events ("
                          "sequence INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "event_id TEXT NOT NULL UNIQUE,"
                          "utc TEXT NOT NULL, kind TEXT NOT NULL,"
                          "packet TEXT NOT NULL, image BLOB)")
                    && exec("PRAGMA user_version=1");
                if (ok) {
                    ok = database.commit();
                    if (!ok) error = database.lastError().text();
                }
                if (!ok) database.rollback();
            }
        }
    }
    if (!ok) { if (error.isEmpty()) error = "Khong doc duoc cau truc database"; close(); }
    return ok;
}
bool HistoryStore::normalize(const QJsonObject &input, HistoryRecord &record, QString &error) {
    error.clear(); record = {};
    const QString type = input.value("type").toString();
    if (!HistoryProtocol::messageType(type) && type != "video") {
        error = "Loai su kien khong luu vao lich su"; return false;
    }
    if (QJsonDocument(input).toJson(QJsonDocument::Compact).size() > VideoProtocol::MaxFrame) {
        error = "Ban ghi lich su qua lon"; return false;
    }
    if (input.value("sender").toString().isEmpty() || input.value("sender").toString().size() > 20
        || input.value("to").toString().size() > 20) {
        error = "Nguoi gui/nhan lich su khong hop le"; return false;
    }
    if (type == "chat" || type == "private" || type == "reply" || type == "forward") {
        if (input.value("text").toString().trimmed().isEmpty()
            || input.value("text").toString().toUtf8().size() > 4000
            || (type == "reply" && (input.value("quote").toString().isEmpty()
                                    || input.value("quote").toString().toUtf8().size() > 1000))) {
            error = "Noi dung tin nhan khong hop le"; return false;
        }
    } else if (type == "sticker") {
        if (input.value("id") != "nobita" && input.value("id") != "doremon") {
            error = "Sticker khong hop le"; return false;
        }
    } else if (type == "image") {
        const QByteArray encoded = input.value("data").toString().toLatin1();
        auto decoded = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded || decoded.decoded.isEmpty() || decoded.decoded.size() > 256 * 1024
            || decoded.decoded.toBase64() != encoded || historyImagePreview(decoded.decoded).isNull()) {
            error = "Anh JPEG lich su khong hop le"; return false;
        }
        record.image = decoded.decoded;
    } else if (type == "video") {
        qint64 size;
        const QString path = input.value("local_path").toString();
        if (!VideoProtocol::validId(input.value("id").toString())
            || !VideoProtocol::validName(input.value("name").toString())
            || !VideoProtocol::number(input.value("size"), size) || size < 12
            || path.isEmpty() || !QFileInfo(path).isAbsolute()
            || QFileInfo(path).suffix().compare("mp4", Qt::CaseInsensitive) != 0
            || !VideoProtocol::digest(input.value("sha256").toString())) {
            error = "Thong tin video lich su khong hop le"; return false;
        }
    }
    QJsonObject packet = input;
    QString event = packet.value("_event_id").toString();
    if (event.isEmpty()) event = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!VideoProtocol::validId(event)) { error = "Ma lich su khong hop le"; return false; }
    packet["_event_id"] = event;
    auto utc = QDateTime::fromString(packet.value("_utc").toString(), Qt::ISODateWithMs);
    if (!utc.isValid()) utc = QDateTime::currentDateTimeUtc();
    packet["_utc"] = utc.toUTC().toString(Qt::ISODateWithMs);
    packet.remove("data"); // JPEG is stored as BLOB, no repeated Base64 in the JSON column.
    record.packet = packet;
    return true;
}
HistoryStore::SaveResult HistoryStore::append(const QJsonObject &packet, HistoryRecord &record, QString &error) {
    if (!normalize(packet, record, error)) return SaveResult::Error;
    if (!isOpen()) { error = "Lich su chua mo duoc; tin nhan van hien thi truc tiep"; return SaveResult::Error; }
    QSqlQuery query(database);
    query.prepare("INSERT INTO events(event_id,utc,kind,packet,image) VALUES(:id,:utc,:kind,:packet,:image) "
                  "ON CONFLICT(event_id) DO NOTHING");
    query.bindValue(":id", record.packet.value("_event_id").toString());
    query.bindValue(":utc", record.packet.value("_utc").toString());
    query.bindValue(":kind", record.packet.value("type").toString());
    query.bindValue(":packet", QString::fromUtf8(QJsonDocument(record.packet).toJson(QJsonDocument::Compact)));
    query.bindValue(":image", record.image);
    if (!query.exec()) { error = query.lastError().text(); return SaveResult::Error; }
    if (query.numRowsAffected() == 0) return SaveResult::Duplicate;
    record.sequence = query.lastInsertId().toLongLong();
    return SaveResult::Saved;
}
HistoryPage HistoryStore::page(int limit, qint64 before, QString &error) const {
    error.clear(); HistoryPage page;
    if (!isOpen()) { error = "Khong mo duoc lich su"; return page; }
    limit = std::clamp(limit, 1, 100);
    QSqlQuery query(database);
    query.prepare(before > 0
        ? "SELECT sequence,packet,image FROM events WHERE sequence < :before ORDER BY sequence DESC LIMIT :limit"
        : "SELECT sequence,packet,image FROM events ORDER BY sequence DESC LIMIT :limit");
    if (before > 0) query.bindValue(":before", before);
    query.bindValue(":limit", limit + 1);
    if (!query.exec()) { error = query.lastError().text(); return page; }
    while (query.next()) {
        if (page.records.size() == limit) { page.hasMore = true; break; }
        QJsonParseError parse;
        const auto doc = QJsonDocument::fromJson(query.value(1).toString().toUtf8(), &parse);
        if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
            error = "Ban ghi lich su bi hong: " + query.value(0).toString(); return {};
        }
        QJsonObject packet = doc.object();
        const QByteArray image = query.value(2).toByteArray();
        if (packet.value("type") == "image") packet["data"] = QString::fromLatin1(image.toBase64());
        HistoryRecord record;
        if (!normalize(packet, record, error)) return {};
        record.sequence = query.value(0).toLongLong();
        page.records.append(record);
    }
    if (query.lastError().isValid()) { error = query.lastError().text(); return {}; }
    std::reverse(page.records.begin(), page.records.end());
    return page;
}
