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
                    && exec("CREATE TABLE IF NOT EXISTS recall_tombstones(event_id TEXT PRIMARY KEY,packet TEXT NOT NULL)")
                    && exec("CREATE TABLE IF NOT EXISTS message_edits(event_id TEXT PRIMARY KEY,revision INTEGER NOT NULL,packet TEXT NOT NULL)")
                    && exec("CREATE TABLE IF NOT EXISTS message_receipts(event_id TEXT PRIMARY KEY,packet TEXT NOT NULL)")
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
    if(input.value("_recalled").toBool()) { /* A tombstone intentionally has no message body. */ }
    else if (type == "chat" || type == "private" || type == "reply" || type == "forward") {
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
    record.packet = packet.value("_recalled").toBool() ? HistoryProtocol::tombstone(packet) : packet;
    return true;
}
HistoryStore::SaveResult HistoryStore::append(const QJsonObject &packet, HistoryRecord &record, QString &error) {
    if (!normalize(packet, record, error)) return SaveResult::Error;
    if (!isOpen()) { error = "Lich su chua mo duoc; tin nhan van hien thi truc tiep"; return SaveResult::Error; }
    QSqlQuery tombstone(database); tombstone.prepare("SELECT packet FROM recall_tombstones WHERE event_id=?"); tombstone.addBindValue(record.packet.value("_event_id").toString());
    if(!tombstone.exec()) { error=tombstone.lastError().text(); return SaveResult::Error; }
    if(tombstone.next()) { record.packet=HistoryProtocol::tombstone(record.packet); record.image.clear(); }
    tombstone.finish();
    if(!record.packet.value("_recalled").toBool()) {
        tombstone.prepare("SELECT packet FROM message_edits WHERE event_id=?");tombstone.addBindValue(record.packet.value("_event_id").toString());
        if(!tombstone.exec()) {error=tombstone.lastError().text();return SaveResult::Error;}
        if(tombstone.next()) {
            const auto notice=QJsonDocument::fromJson(tombstone.value(0).toString().toUtf8()).object();
            if(record.packet.value("sender").toString().compare(notice.value("sender").toString(),Qt::CaseInsensitive)==0 && record.packet.value("to").toString().compare(notice.value("to").toString(),Qt::CaseInsensitive)==0)
                record.packet=HistoryProtocol::withEdit(record.packet,notice);
        }
        tombstone.finish();
    }
    if(!record.packet.value("_recalled").toBool()) {
        tombstone.prepare("SELECT packet FROM message_receipts WHERE event_id=?");tombstone.addBindValue(record.packet.value("_event_id").toString());
        if(!tombstone.exec()) {error=tombstone.lastError().text();return SaveResult::Error;}
        if(tombstone.next()) record.packet=HistoryProtocol::withReceipt(record.packet,QJsonDocument::fromJson(tombstone.value(0).toString().toUtf8()).object());tombstone.finish();
    }
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
HistoryPage HistoryStore::page(int limit, qint64 before, QString &error, const QString &peer, bool filter) const {
    error.clear(); HistoryPage page;
    if (!isOpen()) { error = "Khong mo duoc lich su"; return page; }
    limit = std::clamp(limit, 1, 100);
    QSqlQuery query(database);
    QString sql = "SELECT sequence,packet,image FROM events WHERE 1=1";
    if (before > 0) sql += " AND sequence < :before";
    if (filter) {
        if (peer.isEmpty()) sql += " AND coalesce(json_extract(packet,'$.to'),'') = ''";
        else sql += " AND coalesce(json_extract(packet,'$.to'),'') != '' AND (json_extract(packet,'$.sender') = :peer COLLATE NOCASE OR json_extract(packet,'$.to') = :peer COLLATE NOCASE)";
    }
    sql += " ORDER BY sequence DESC LIMIT :limit";
    query.prepare(sql);
    if (filter && !peer.isEmpty()) query.bindValue(":peer", peer);
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
        if (packet.value("type") == "image" && !packet.value("_recalled").toBool()) packet["data"] = QString::fromLatin1(image.toBase64());
        HistoryRecord record;
        if (!normalize(packet, record, error)) return {};
        record.sequence = query.value(0).toLongLong();
        page.records.append(record);
    }
    if (query.lastError().isValid()) { error = query.lastError().text(); return {}; }
    std::reverse(page.records.begin(), page.records.end());
    return page;
}

QStringList HistoryStore::peers(const QString &owner, QString &error) const {
    QStringList result; error.clear();
    if (!isOpen()) return result;
    QSqlQuery query(database);
    if (!query.exec("SELECT DISTINCT json_extract(packet,'$.sender'), json_extract(packet,'$.to') FROM events WHERE coalesce(json_extract(packet,'$.to'),'') != ''")) {
        error = query.lastError().text(); return result;
    }
    while (query.next()) {
        const QString sender = query.value(0).toString(), to = query.value(1).toString();
        const QString peer = sender.compare(owner, Qt::CaseInsensitive) == 0 ? to : sender;
        if (!peer.isEmpty() && !result.contains(peer)) result.append(peer);
    }
    if (query.lastError().isValid()) error = query.lastError().text();
    return result;
}

namespace {
QString conversationPredicate(const QString &peer) {
    return peer.isEmpty() ? QString("coalesce(json_extract(packet,'$.to'),'') = ''")
        : QString("coalesce(json_extract(packet,'$.to'),'') != '' AND (json_extract(packet,'$.sender') = :peer COLLATE NOCASE OR json_extract(packet,'$.to') = :peer COLLATE NOCASE)");
}
}
bool HistoryStore::openReadOnly(const QString &path, QString &error) {
    close(); error.clear(); filePath=QFileInfo(path).absoluteFilePath();
    if (!QFileInfo::exists(filePath)) { error="Không tìm thấy database lịch sử."; return false; }
    connectionName="history-search-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    database=QSqlDatabase::addDatabase("QSQLITE",connectionName); database.setDatabaseName(filePath);
    database.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=500");
    if(!database.open()) { error=database.lastError().text(); close(); return false; }
    bool ok=false;
    {
        QSqlQuery q(database);
        ok=q.exec("PRAGMA user_version") && q.next() && q.value(0).toInt()==1;
        q.finish();
        if(ok) ok=q.exec("SELECT sequence,packet,image FROM events LIMIT 0");
        if(!ok) error="Không đọc được cấu trúc lịch sử hoặc phiên bản database không phù hợp.";
    }
    if(!ok) close();
    return ok;
}
QString HistoryStore::searchKey(const QString &text) {
    QString result;
    for(QChar c : text.normalized(QString::NormalizationForm_D).toCaseFolded()) {
        if(c.category()==QChar::Mark_NonSpacing || c.category()==QChar::Mark_SpacingCombining || c.category()==QChar::Mark_Enclosing) continue;
        result.append(c==QChar(0x0111) ? QChar('d') : c);
    }
    return result;
}
HistoryPage HistoryStore::search(const QString &text, const QString &peer, qint64 before, QString &error, int limit, const std::shared_ptr<std::atomic_bool> &cancel) const {
    error.clear(); HistoryPage page;
    if(!isOpen()) { error="Lịch sử chưa mở được."; return page; }
    const QString key=searchKey(text.trimmed());
    if(key.isEmpty()) return page;
    if(text.size()>128) { error="Từ khóa tối đa 128 ký tự."; return page; }
    limit=std::clamp(limit,1,50);
    QSqlQuery q(database); q.setForwardOnly(true);
    QString sql="SELECT sequence,packet FROM events WHERE "+conversationPredicate(peer);
    if(before>0) sql+=" AND sequence < :before";
    sql+=" ORDER BY sequence DESC";
    q.prepare(sql); if(!peer.isEmpty()) q.bindValue(":peer",peer); if(before>0) q.bindValue(":before",before);
    if(cancel && cancel->load()) return page;
    if(!q.exec()) { error=q.lastError().text(); return page; }
    while(q.next()) {
        if(cancel && cancel->load()) return {};
        QJsonParseError parse; const auto doc=QJsonDocument::fromJson(q.value(1).toString().toUtf8(),&parse);
        if(parse.error!=QJsonParseError::NoError || !doc.isObject()) { error="Một bản ghi lịch sử bị hỏng."; return {}; }
        const auto packet=doc.object(); if(packet.value("_recalled").toBool()) continue; bool matches=false;
        for(const auto &field : {"text","quote","name"})
            if(searchKey(packet.value(field).toString()).contains(key)) { matches=true; break; }
        if(!matches) continue;
        if(page.records.size()==limit) { page.hasMore=true; break; }
        page.records.append({q.value(0).toLongLong(),packet,{}});
    }
    if(q.lastError().isValid()) { error=q.lastError().text(); return {}; }
    return page; // Newest matching record first; no image BLOBs read while searching.
}
QVector<HistoryRecord> HistoryStore::context(qint64 sequence, const QString &peer, QString &error) const {
    error.clear(); QVector<HistoryRecord> records;
    if(!isOpen() || sequence<=0) { error="Không tìm thấy tin nhắn."; return records; }
    const QString predicate=conversationPredicate(peer);
    QSqlQuery q(database); q.setForwardOnly(true);
    q.prepare("SELECT sequence,packet,image FROM events WHERE sequence IN ("
        "SELECT sequence FROM (SELECT sequence FROM events WHERE "+predicate+" AND sequence <= :seq ORDER BY sequence DESC LIMIT 11) "
        "UNION ALL SELECT sequence FROM (SELECT sequence FROM events WHERE "+predicate+" AND sequence > :seq ORDER BY sequence ASC LIMIT 10)) ORDER BY sequence");
    q.bindValue(":seq",sequence); if(!peer.isEmpty()) q.bindValue(":peer",peer);
    if(!q.exec()) { error=q.lastError().text(); return records; }
    bool found=false;
    while(q.next()) {
        QJsonParseError parse; const auto doc=QJsonDocument::fromJson(q.value(1).toString().toUtf8(),&parse);
        if(parse.error!=QJsonParseError::NoError || !doc.isObject()) { error="Một bản ghi lịch sử bị hỏng."; return {}; }
        auto packet=doc.object(); const auto image=q.value(2).toByteArray();
        if(packet.value("type")=="image" && !packet.value("_recalled").toBool()) packet["data"]=QString::fromLatin1(image.toBase64());
        HistoryRecord record; if(!normalize(packet,record,error)) return {};
        record.sequence=q.value(0).toLongLong(); found |= record.sequence==sequence; records.append(record);
    }
    if(q.lastError().isValid()) { error=q.lastError().text(); return {}; }
    if(!found) { error="Tin nhắn không thuộc cuộc trò chuyện này hoặc không còn trong lịch sử."; return {}; }
    return records;
}

bool HistoryStore::applyRecall(const QJsonObject &notification,QString &error) {
    error.clear(); const QString id=notification.value("event_id").toString();
    if(!isOpen() || !VideoProtocol::validId(id)) { error="Không lưu được trạng thái thu hồi.";return false; }
    if(!database.transaction()) { error=database.lastError().text();return false; }
    auto fail=[&](const QString &text) { error=text;database.rollback();return false; };
    QSqlQuery q(database);q.prepare("SELECT packet FROM events WHERE event_id=?");q.addBindValue(id);
    if(!q.exec()) return fail(q.lastError().text());
    QJsonObject original;
    if(q.next()) {
        original=QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
        if(original.value("sender").toString().compare(notification.value("sender").toString(),Qt::CaseInsensitive)!=0 || original.value("to").toString().compare(notification.value("to").toString(),Qt::CaseInsensitive)!=0) return fail("Thông tin thu hồi không khớp tin đã lưu.");
    }
    q.finish(); q.prepare("INSERT INTO recall_tombstones(event_id,packet) VALUES(?,?) ON CONFLICT(event_id) DO NOTHING");q.addBindValue(id);q.addBindValue(QString::fromUtf8(QJsonDocument(notification).toJson(QJsonDocument::Compact)));
    if(!q.exec()) return fail(q.lastError().text());
    if(!original.isEmpty()) {
        q.prepare("UPDATE events SET packet=?,image=NULL WHERE event_id=?");q.addBindValue(QString::fromUtf8(QJsonDocument(HistoryProtocol::tombstone(original)).toJson(QJsonDocument::Compact)));q.addBindValue(id);
        if(!q.exec()) return fail(q.lastError().text());
    }
    q.prepare("DELETE FROM message_edits WHERE event_id=?");q.addBindValue(id);if(!q.exec()) return fail(q.lastError().text());
    if(!database.commit()) return fail(database.lastError().text());return true;
}

bool HistoryStore::applyEdit(const QJsonObject &p,QString &error) {
    error.clear();const QString id=p.value("event_id").toString(),text=p.value("text").toString();qint64 revision=0;
    if(!isOpen() || !VideoProtocol::validId(id) || !HistoryProtocol::editableType(p.value("kind").toString()) || !VideoProtocol::number(p.value("revision"),revision,9007199254740991LL) || revision<=0 || text.trimmed().isEmpty() || text.toUtf8().size()>4000 || text.contains(QChar(0))) {error="Thông tin chỉnh sửa không hợp lệ.";return false;}
    if(!database.transaction()) {error=database.lastError().text();return false;}
    auto fail=[&](const QString &message) {error=message;database.rollback();return false;};
    QSqlQuery q(database);q.prepare("SELECT 1 FROM recall_tombstones WHERE event_id=?");q.addBindValue(id);
    if(!q.exec()) return fail(q.lastError().text());
    if(q.next()) {q.finish();database.rollback();return true;}q.finish();
    q.prepare("SELECT packet FROM events WHERE event_id=?");q.addBindValue(id);if(!q.exec()) return fail(q.lastError().text());
    QJsonObject original;if(q.next()) original=QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();q.finish();
    if(!original.isEmpty()) {
        if(original.value("sender").toString().compare(p.value("sender").toString(),Qt::CaseInsensitive)!=0 || original.value("to").toString().compare(p.value("to").toString(),Qt::CaseInsensitive)!=0 || !HistoryProtocol::editableType(original.value("type").toString())) return fail("Thông tin chỉnh sửa không khớp tin đã lưu.");
        if(original.value("_recalled").toBool() || original.value("_edit_revision").toDouble()>=revision) {database.rollback();return true;}
    }
    q.prepare("SELECT revision FROM message_edits WHERE event_id=?");q.addBindValue(id);if(!q.exec()) return fail(q.lastError().text());
    if(q.next() && q.value(0).toLongLong()>=revision) {q.finish();database.rollback();return true;}q.finish();
    q.prepare("INSERT INTO message_edits(event_id,revision,packet) VALUES(?,?,?) ON CONFLICT(event_id) DO UPDATE SET revision=excluded.revision,packet=excluded.packet");q.addBindValue(id);q.addBindValue(revision);q.addBindValue(QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact)));if(!q.exec()) return fail(q.lastError().text());
    if(!original.isEmpty()) {
        q.prepare("UPDATE events SET packet=? WHERE event_id=?");q.addBindValue(QString::fromUtf8(QJsonDocument(HistoryProtocol::withEdit(original,p)).toJson(QJsonDocument::Compact)));q.addBindValue(id);if(!q.exec()) return fail(q.lastError().text());
    }
    if(!database.commit()) return fail(database.lastError().text());return true;
}

bool HistoryStore::applyReceipt(const QJsonObject &p,QString &error) {
    error.clear();if(!isOpen() || !HistoryProtocol::validReceipt(p)) {error="Trạng thái tin không hợp lệ.";return false;}
    if(!database.transaction()) {error=database.lastError().text();return false;}
    auto fail=[&] {error="Không lưu được trạng thái tin: "+database.lastError().text();database.rollback();return false;};
    const QString id=p.value("event_id").toString();QSqlQuery q(database);q.prepare("SELECT 1 FROM recall_tombstones WHERE event_id=?");q.addBindValue(id);if(!q.exec()) return fail();if(q.next()) {q.finish();database.rollback();return true;}q.finish();
    q.prepare("SELECT packet FROM events WHERE event_id=?");q.addBindValue(id);if(!q.exec()) return fail();QJsonObject original;if(q.next()) original=QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();q.finish();
    if(!original.isEmpty()) {
        if(original.value("sender").toString().compare(p.value("sender").toString(),Qt::CaseInsensitive)!=0 || original.value("to").toString().compare(p.value("to").toString(),Qt::CaseInsensitive)!=0 || original.value("to").toString().isEmpty()) return fail();
        if(original.value("_recalled").toBool() || original.value("_receipt_revision").toDouble()>=p.value("revision").toDouble() || original.value("_receipt_state").toInt()>p.value("state").toInt()) {database.rollback();return true;}
    }
    q.prepare("SELECT packet FROM message_receipts WHERE event_id=?");q.addBindValue(id);if(!q.exec()) return fail();if(q.next()) {const auto old=QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();if(old.value("revision").toDouble()>=p.value("revision").toDouble() || old.value("state").toInt()>p.value("state").toInt()) {q.finish();database.rollback();return true;}}q.finish();
    q.prepare("INSERT INTO message_receipts(event_id,packet) VALUES(?,?) ON CONFLICT(event_id) DO UPDATE SET packet=excluded.packet");q.addBindValue(id);q.addBindValue(QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact)));if(!q.exec()) return fail();
    if(!original.isEmpty()) {q.prepare("UPDATE events SET packet=? WHERE event_id=?");q.addBindValue(QString::fromUtf8(QJsonDocument(HistoryProtocol::withReceipt(original,p)).toJson(QJsonDocument::Compact)));q.addBindValue(id);if(!q.exec()) return fail();}
    if(!database.commit()) return fail();return true;
}
bool HistoryStore::ignoreReceipts(const QJsonArray &ids,QString &error) {
    error.clear();if(!isOpen() || ids.size()>50 || !database.transaction()) {error="Không lưu được trạng thái không hỗ trợ.";return false;}
    QSqlQuery q(database);
    for(const auto &id:ids) {
        q.prepare("SELECT packet FROM events WHERE event_id=?");q.addBindValue(id.toString());if(!q.exec()) {database.rollback();error=q.lastError().text();return false;}
        QJsonObject p;if(q.next()) p=QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();q.finish();if(p.isEmpty()) continue;
        p["_receipt_untracked"]=true;q.prepare("UPDATE events SET packet=? WHERE event_id=?");q.addBindValue(QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact)));q.addBindValue(id.toString());if(!q.exec()) {database.rollback();error=q.lastError().text();return false;}
    }
    if(!database.commit()) {error=database.lastError().text();database.rollback();return false;}return true;
}
QJsonObject HistoryStore::scanReceipts(const QString &owner,const QString &peer,bool viewing,qint64 after,QString &error) const {
    error.clear();QJsonArray items;qint64 cursor=after;int count=0;
    if(!isOpen()) {error="Lịch sử chưa mở.";return {};}
    QSqlQuery q(database);q.setForwardOnly(true);q.prepare("SELECT sequence,packet FROM events WHERE sequence>? AND coalesce(json_extract(packet,'$.to'),'')!='' AND coalesce(json_extract(packet,'$._recalled'),0)=0 AND coalesce(json_extract(packet,'$._receipt_untracked'),0)=0 AND coalesce(json_extract(packet,'$._receipt_state'),0)<3 ORDER BY sequence LIMIT 200");q.addBindValue(after);
    if(!q.exec()) {error=q.lastError().text();return {};}
    while(q.next()) {
        ++count;cursor=q.value(0).toLongLong();const auto p=QJsonDocument::fromJson(q.value(1).toString().toUtf8()).object();
        if(p.value("to").toString().compare(owner,Qt::CaseInsensitive)!=0 || p.value("sender").toString().compare(owner,Qt::CaseInsensitive)==0) continue;
        const int desired=viewing && !peer.isEmpty() && peer.compare(p.value("sender").toString(),Qt::CaseInsensitive)==0?3:2;
        if(p.value("_receipt_state").toInt()<desired) items.append(QJsonObject{{"event_id",p.value("_event_id")},{"state",desired}});
        if(items.size()==50) break;
    }
    if(q.lastError().isValid()) {error=q.lastError().text();return {};}
    return {{"items",items},{"cursor",double(cursor)},{"more",count==200 || items.size()==50}};
}
