#include "recallstore.h"
#include "authprotocol.h"
#include "historyprotocol.h"
#include <QJsonDocument>
#include "videoprotocol.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
RecallStore::~RecallStore() { close(); }
void RecallStore::close() { if(db.isValid()) db.close(); db=QSqlDatabase(); if(!connection.isEmpty()) QSqlDatabase::removeDatabase(connection); connection.clear(); }
bool RecallStore::open(const QString &path,QString &error) {
    error.clear();
    close();
    if(!QDir().mkpath(QFileInfo(path).absolutePath())) { error="Không tạo được thư mục thu hồi."; return false; }
    connection="recall-"+QUuid::createUuid().toString(QUuid::WithoutBraces); db=QSqlDatabase::addDatabase("QSQLITE",connection); db.setDatabaseName(path); db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=3000");
    if(!db.open()) { error=db.lastError().text(); return false; }
    QSqlQuery q(db); if(!q.exec("PRAGMA user_version") || !q.next() || q.value(0).toInt()>1) { error="Database thu hồi không phù hợp."; return false; } q.finish();
    for(const auto &sql : {"PRAGMA journal_mode=WAL","PRAGMA synchronous=FULL",
        "CREATE TABLE IF NOT EXISTS messages(id TEXT PRIMARY KEY,sender TEXT NOT NULL,recipient TEXT NOT NULL,kind TEXT NOT NULL,utc TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS recalls(sequence INTEGER PRIMARY KEY AUTOINCREMENT,event_id TEXT NOT NULL UNIQUE,utc TEXT NOT NULL)","CREATE TABLE IF NOT EXISTS edits(sequence INTEGER PRIMARY KEY AUTOINCREMENT,event_id TEXT NOT NULL UNIQUE,text TEXT NOT NULL,utc TEXT NOT NULL)","CREATE TABLE IF NOT EXISTS receipts(sequence INTEGER PRIMARY KEY AUTOINCREMENT,event_id TEXT NOT NULL UNIQUE,state INTEGER NOT NULL,received_utc TEXT NOT NULL,seen_utc TEXT NOT NULL)",
        "INSERT INTO receipts(event_id,state,received_utc,seen_utc) SELECT id,1,'','' FROM messages WHERE recipient!='' AND NOT EXISTS(SELECT 1 FROM receipts r WHERE r.event_id=messages.id)","PRAGMA user_version=1"}) {
        if(!q.exec(sql)) { error=q.lastError().text(); return false; }
    }
    return true;
}
bool RecallStore::contains(const QString &id) { QSqlQuery q(db); q.prepare("SELECT 1 FROM messages WHERE id=?");q.addBindValue(id);return !q.exec() || q.next(); }
bool RecallStore::remember(const QJsonObject &p) {
    const QString id=p.value("_event_id").toString(); if(!VideoProtocol::validId(id)) return false;
    QSqlQuery q(db); q.prepare("INSERT INTO messages(id,sender,recipient,kind,utc) VALUES(?,?,?,?,?) ON CONFLICT(id) DO NOTHING");
    q.addBindValue(id); q.addBindValue(p.value("sender").toString()); q.addBindValue(p.value("to").toString().isEmpty()?QStringLiteral(""):p.value("to").toString()); q.addBindValue(p.value("type").toString());q.addBindValue(p.value("_utc").toString());if(!q.exec()) return false;
    if(!p.value("to").toString().isEmpty()) {q.prepare("INSERT INTO receipts(event_id,state,received_utc,seen_utc) VALUES(?,1,'','') ON CONFLICT(event_id) DO NOTHING");q.addBindValue(id);return q.exec();}return true;
}
static QJsonObject notice(QSqlQuery &q) {
    return {{"type","message_recalled"},{"event_id",q.value(0).toString()},{"sender",q.value(1).toString()},{"to",q.value(2).toString()},
        {"kind",q.value(3).toString()},{"_utc",q.value(4).toString()},{"revoked_utc",q.value(5).toString()}};
}
QJsonObject RecallStore::recall(const QString &id,const QString &actor,QString &error) {
    error.clear(); QSqlQuery q(db); q.prepare("SELECT sender FROM messages WHERE id=?");q.addBindValue(id);
    if(!q.exec()) { error="Không đọc được thông tin tin nhắn.";return {}; }
    if(!q.next() || AuthProtocol::canonical(q.value(0).toString())!=AuthProtocol::canonical(actor)) { error="Chỉ thu hồi được tin do bạn gửi từ bản mới.";return {}; } q.finish();
    if(!db.transaction()) {error="Không bắt đầu được thu hồi.";return {};}
    q.prepare("INSERT INTO recalls(event_id,utc) VALUES(?,?) ON CONFLICT(event_id) DO NOTHING");q.addBindValue(id);q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if(!q.exec()) { db.rollback();error="Không lưu được trạng thái thu hồi.";return {}; }
    q.prepare("DELETE FROM edits WHERE event_id=?");q.addBindValue(id);
    if(!q.exec() || !db.commit()) {db.rollback();error="Không hoàn tất được thu hồi.";return {};}
    q.prepare("SELECT m.id,m.sender,m.recipient,m.kind,m.utc,r.utc FROM messages m JOIN recalls r ON r.event_id=m.id WHERE m.id=?");q.addBindValue(id);
    if(!q.exec() || !q.next()) { error="Không đọc được trạng thái thu hồi.";return {}; }return notice(q);
}
QJsonArray RecallStore::page(const QString &actor,qint64 after,qint64 &cursor,bool &more,QString &error) {
    error.clear(); cursor=after;more=false; QJsonArray items;QSqlQuery q(db);q.setForwardOnly(true);
    // Actor is compared with Qt Unicode case-folding, matching account identity rules.
    q.prepare("SELECT m.id,m.sender,m.recipient,m.kind,m.utc,r.utc,r.sequence FROM recalls r JOIN messages m ON m.id=r.event_id WHERE r.sequence>? ORDER BY r.sequence LIMIT 50");q.addBindValue(after);
    if(!q.exec()) { error=q.lastError().text();return {}; }
    int scanned=0; while(q.next()) { ++scanned;cursor=q.value(6).toLongLong();const QString to=q.value(2).toString();
        if(to.isEmpty() || AuthProtocol::canonical(q.value(1).toString())==AuthProtocol::canonical(actor) || AuthProtocol::canonical(to)==AuthProtocol::canonical(actor)) items.append(notice(q));
    }
    if(q.lastError().isValid()) { error=q.lastError().text();return {}; }
    more=scanned==50;return items; // Bounded scan even if every private notice belongs to another account.
}

static QJsonObject editedNotice(QSqlQuery &q) {
    return {{"type","message_edited"},{"event_id",q.value(0).toString()},{"sender",q.value(1).toString()},{"to",q.value(2).toString()},
        {"kind",q.value(3).toString()},{"_utc",q.value(4).toString()},{"text",q.value(5).toString()},{"edited_utc",q.value(6).toString()},{"revision",double(q.value(7).toLongLong())}};
}
QJsonObject RecallStore::edit(const QString &id,const QString &actor,const QString &input,qint64 base,QString &error) {
    error.clear();const QString text=input.trimmed();
    if(text.isEmpty() || text.toUtf8().size()>4000 || text.contains(QChar(0)) || base<0 || base>9007199254740991LL) {error="Nội dung trống hoặc quá dài. Hãy rút ngắn tin nhắn.";return {};}
    QSqlQuery begin(db);if(!begin.exec("BEGIN IMMEDIATE")) {error="Database đang bận, thử lại sau.";return {};}
    auto fail=[&](const QString &message) {error=message;db.rollback();return QJsonObject();};
    QSqlQuery q(db);q.prepare("SELECT m.sender,m.kind,coalesce(e.sequence,0),EXISTS(SELECT 1 FROM recalls r WHERE r.event_id=m.id) FROM messages m LEFT JOIN edits e ON e.event_id=m.id WHERE m.id=?");q.addBindValue(id);
    if(!q.exec()) return fail("Không đọc được tin nhắn.");
    if(!q.next() || AuthProtocol::canonical(q.value(0).toString())!=AuthProtocol::canonical(actor)) return fail("Chỉ chỉnh sửa được tin chữ do bạn gửi và có thông tin trên Server.");
    if(!HistoryProtocol::editableType(q.value(1).toString()) || q.value(3).toBool()) return fail("Không sửa được ảnh, sticker, video hoặc tin đã thu hồi.");
    if(q.value(2).toLongLong()!=base) return fail("Tin đã thay đổi. Mở lại tin để sửa nội dung mới nhất.");
    q.finish();q.prepare("DELETE FROM edits WHERE event_id=?");q.addBindValue(id);if(!q.exec()) return fail("Không cập nhật được tin.");
    q.prepare("INSERT INTO edits(event_id,text,utc) VALUES(?,?,?)");q.addBindValue(id);q.addBindValue(text);q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if(!q.exec() || !db.commit()) return fail("Không hoàn tất được chỉnh sửa.");
    q.prepare("SELECT m.id,m.sender,m.recipient,m.kind,m.utc,e.text,e.utc,e.sequence FROM messages m JOIN edits e ON e.event_id=m.id WHERE m.id=?");q.addBindValue(id);
    if(!q.exec() || !q.next()) {error="Đã ghi nhưng chưa đọc được xác nhận. Kết nối lại để đồng bộ.";return {};}
    return editedNotice(q);
}
QJsonArray RecallStore::editPage(const QString &actor,qint64 after,qint64 &cursor,bool &more,QString &error) {
    error.clear();cursor=after;more=false;QJsonArray items;QSqlQuery q(db);q.setForwardOnly(true);
    q.prepare("SELECT m.id,m.sender,m.recipient,m.kind,m.utc,e.text,e.utc,e.sequence FROM edits e JOIN messages m ON m.id=e.event_id WHERE e.sequence>? AND NOT EXISTS(SELECT 1 FROM recalls r WHERE r.event_id=m.id) ORDER BY e.sequence LIMIT 50");q.addBindValue(after);
    if(!q.exec()) {error=q.lastError().text();return {};}
    int scanned=0,bytes=0;
    while(q.next()) {
        const QString to=q.value(2).toString();const bool visible=to.isEmpty() || AuthProtocol::canonical(to)==AuthProtocol::canonical(actor) || AuthProtocol::canonical(q.value(1).toString())==AuthProtocol::canonical(actor);
        if(visible) {
            const auto item=editedNotice(q);const int size=QJsonDocument(item).toJson(QJsonDocument::Compact).size()+1;
            if(bytes+size>200*1024) {more=true;return items;}
            bytes+=size;items.append(item);
        }
        ++scanned;cursor=q.value(7).toLongLong();
    }
    if(q.lastError().isValid()) {error=q.lastError().text();return {};}
    more=scanned==50;return items;
}

static QJsonObject receiptNotice(QSqlQuery &q) {
    return {{"type","message_receipt"},{"event_id",q.value(0).toString()},{"sender",q.value(1).toString()},{"to",q.value(2).toString()},
        {"state",q.value(3).toInt()},{"revision",double(q.value(4).toLongLong())},{"received_utc",q.value(5).toString()},{"seen_utc",q.value(6).toString()}};
}
QJsonObject RecallStore::receipt(const QString &id) {
    QSqlQuery q(db);q.prepare("SELECT m.id,m.sender,m.recipient,r.state,r.sequence,r.received_utc,r.seen_utc FROM receipts r JOIN messages m ON m.id=r.event_id WHERE m.id=?");q.addBindValue(id);
    return q.exec()&&q.next()?receiptNotice(q):QJsonObject();
}
QJsonArray RecallStore::acknowledge(const QString &actor,const QJsonArray &items,QJsonArray &ignored,QString &error) {
    ignored={};error.clear();QJsonArray notices;
    if(items.isEmpty() || items.size()>50) {error="Gói xác nhận tối đa 50 tin.";return {};}
    QSqlQuery begin(db);if(!begin.exec("BEGIN IMMEDIATE")) {error="Không lưu được xác nhận.";return {};}
    auto fail=[&] {error="Không cập nhật được trạng thái tin.";db.rollback();return QJsonArray();};
    for(const auto &value:items) {
        const auto item=value.toObject();const QString id=item.value("event_id").toString();const int state=item.value("state").toInt();
        if(!VideoProtocol::validId(id) || !item.value("state").isDouble() || (item.value("state").toDouble()!=2 && item.value("state").toDouble()!=3)) return fail();
        QSqlQuery q(db);q.prepare("SELECT m.recipient,r.state,r.received_utc,r.seen_utc,EXISTS(SELECT 1 FROM recalls c WHERE c.event_id=m.id) FROM messages m LEFT JOIN receipts r ON r.event_id=m.id WHERE m.id=?");q.addBindValue(id);
        if(!q.exec()) return fail();
        if(!q.next() || q.value(0).toString().isEmpty() || AuthProtocol::canonical(q.value(0).toString())!=AuthProtocol::canonical(actor) || q.value(4).toBool()) {ignored.append(id);continue;}
        const int old=q.value(1).toInt();QString received=q.value(2).toString(),seen=q.value(3).toString();q.finish();
        if(state>old) {
            const QString now=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);if(received.isEmpty()) received=now;if(state==3 && seen.isEmpty()) seen=now;
            q.prepare("DELETE FROM receipts WHERE event_id=?");q.addBindValue(id);if(!q.exec()) return fail();
            q.prepare("INSERT INTO receipts(event_id,state,received_utc,seen_utc) VALUES(?,?,?,?)");q.addBindValue(id);q.addBindValue(state);q.addBindValue(received);q.addBindValue(seen.isNull()?QStringLiteral(""):seen);if(!q.exec()) return fail();
        }
        const auto notification=receipt(id);if(notification.isEmpty()) return fail();notices.append(notification);
    }
    if(!db.commit()) return fail();return notices;
}
QJsonArray RecallStore::receiptPage(const QString &actor,qint64 after,qint64 &cursor,bool &more,QString &error) {
    error.clear();cursor=after;more=false;QJsonArray items;QSqlQuery q(db);q.setForwardOnly(true);
    q.prepare("SELECT m.id,m.sender,m.recipient,r.state,r.sequence,r.received_utc,r.seen_utc FROM receipts r JOIN messages m ON m.id=r.event_id WHERE r.sequence>? AND NOT EXISTS(SELECT 1 FROM recalls c WHERE c.event_id=m.id) ORDER BY r.sequence LIMIT 50");q.addBindValue(after);
    if(!q.exec()) {error=q.lastError().text();return {};}
    int count=0;while(q.next()) {++count;cursor=q.value(4).toLongLong();if(AuthProtocol::canonical(actor)==AuthProtocol::canonical(q.value(1).toString()) || AuthProtocol::canonical(actor)==AuthProtocol::canonical(q.value(2).toString())) items.append(receiptNotice(q));}
    more=count==50;if(q.lastError().isValid()) error=q.lastError().text();return items;
}
