#include "accountstore.h"
#include "authprotocol.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

AccountStore::~AccountStore() {
    if (db.isValid()) db.close();
    db = QSqlDatabase();
    if (!connection.isEmpty()) QSqlDatabase::removeDatabase(connection);
}
QByteArray AccountStore::salt() {
    QByteArray value(16, Qt::Uninitialized);
    for (int i = 0; i < 16; i += 4) {
        const quint32 random = QRandomGenerator::system()->generate();
        for (int n = 0; n < 4; ++n) value[i+n] = char(random >> (n * 8));
    }
    return value;
}
QByteArray AccountStore::derive(const QString &password, const QByteArray &salt, int iterations) {
    if (salt.size() != 16 || iterations < AuthProtocol::Iterations || iterations > 2000000) return {};
    QByteArray bytes = password.toUtf8();
    auto hash = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, bytes, salt, iterations, 32);
    bytes.fill('\0'); return hash;
}
bool AccountStore::open(const QString &path, QString &error) {
    error.clear();
    if (db.isValid()) { error = "AccountStore da mo."; return false; }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { error = "Khong tao duoc thu muc tai khoan."; return false; }
    connection = "accounts-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    db = QSqlDatabase::addDatabase("QSQLITE", connection); db.setDatabaseName(path); db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=3000");
    if (!db.open()) { error = db.lastError().text(); return false; }
    QSqlQuery q(db);
    if (!q.exec("PRAGMA user_version") || !q.next()) { error = q.lastError().text(); return false; }
    const int version = q.value(0).toInt(); q.finish();
    if (version > 1) { error = "Schema tai khoan moi hon ung dung; khong tu ghi de."; return false; }
    for (const auto &sql : {"PRAGMA journal_mode=WAL", "PRAGMA synchronous=FULL",
        "CREATE TABLE IF NOT EXISTS accounts(id TEXT PRIMARY KEY, username_key TEXT NOT NULL UNIQUE,"
        "display_name TEXT NOT NULL, salt BLOB NOT NULL CHECK(length(salt)=16),"
        "password_hash BLOB NOT NULL CHECK(length(password_hash)=32), iterations INTEGER NOT NULL,"
        "algorithm TEXT NOT NULL, created_utc TEXT NOT NULL)", "CREATE TABLE IF NOT EXISTS recovery(account_id TEXT PRIMARY KEY, code_hash BLOB NOT NULL CHECK(length(code_hash)=32), credential BLOB NOT NULL CHECK(length(credential)=32))",
        "CREATE TABLE IF NOT EXISTS avatars(account_id TEXT PRIMARY KEY, jpeg BLOB NOT NULL CHECK(length(jpeg)<=16384))", "CREATE TABLE IF NOT EXISTS account_emails(account_id TEXT PRIMARY KEY, email TEXT NOT NULL UNIQUE)", "PRAGMA user_version=1"}) {
        if (!q.exec(sql)) { error = q.lastError().text(); return false; }
    }
    return true;
}
AccountResult AccountStore::create(const QString &name, const QString &password, const QString &email) {
    if (!email.isEmpty() && !AuthProtocol::validEmail(email)) return {false, {}, {}, "Email không hợp lệ."};
    if (!AuthProtocol::validName(name) || !AuthProtocol::validPassword(password)) return {false, {}, {}, "Ten 1-20 ky tu; mat khau 12-128 ky tu."};
    const QByteArray random = salt(), hash = derive(password, random);
    if (hash.size() != 32) return {false, {}, {}, "Khong tao duoc bam mat khau."};
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString display = name.trimmed().normalized(QString::NormalizationForm_C);
    if (!db.transaction()) return {false, {}, {}, "Database đang bận."};
    QSqlQuery q(db);
    q.prepare("INSERT INTO accounts(id,username_key,display_name,salt,password_hash,iterations,algorithm,created_utc) VALUES(?,?,?,?,?,?,?,?)");
    q.addBindValue(id); q.addBindValue(AuthProtocol::canonical(display)); q.addBindValue(display);
    q.addBindValue(random); q.addBindValue(hash); q.addBindValue(AuthProtocol::Iterations);
    q.addBindValue("pbkdf2-hmac-sha256"); q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!q.exec()) {
        db.rollback();
        if (q.lastError().nativeErrorCode() == "19" || q.lastError().nativeErrorCode() == "2067") return {false, {}, {}, "Ten tai khoan da duoc dang ky."};
        return {false, {}, {}, "Khong luu duoc tai khoan. Kiem tra database Server."};
    }
    if (!email.isEmpty()) {
        q.prepare("INSERT INTO account_emails(account_id,email) VALUES(?,?)"); q.addBindValue(id); q.addBindValue(AuthProtocol::emailKey(email));
        if (!q.exec()) { db.rollback(); return {false, {}, {}, "Không tạo được tài khoản. Email có thể đã được sử dụng."}; }
    }
    if (!db.commit()) { db.rollback(); return {false, {}, {}, "Không lưu được tài khoản."}; }
    return {true, display, id, {}, hash};
}
AccountResult AccountStore::login(const QString &name, const QString &password) {
    if (!AuthProtocol::validName(name) || !AuthProtocol::validPassword(password)) return {false, {}, {}, "Tai khoan hoac mat khau khong dung."};
    QSqlQuery q(db); q.prepare("SELECT id,display_name,salt,password_hash,iterations,algorithm FROM accounts WHERE username_key=?");
    q.addBindValue(AuthProtocol::canonical(name));
    if (!q.exec()) return {false, {}, {}, "Khong doc duoc database tai khoan."};
    const bool exists = q.next();
    // A missing account still performs the same expensive KDF.
    const QByteArray random = exists ? q.value(2).toByteArray() : QByteArray(16, 'x');
    const QByteArray stored = exists ? q.value(3).toByteArray() : QByteArray(32, '\0');
    const int iterations = exists ? q.value(4).toInt() : AuthProtocol::Iterations;
    if (exists && q.value(5).toString() != "pbkdf2-hmac-sha256") return {false, {}, {}, "Dinh dang tai khoan khong duoc ho tro."};
    const auto computed = derive(password, random, iterations);
    const bool equal = AuthProtocol::equalHash(computed, stored);
    if (!exists || !equal) return {false, {}, {}, "Tai khoan hoac mat khau khong dung."};
    AccountResult result{true, q.value(1).toString(), q.value(0).toString(), {}, stored};
    if (!isCurrent(result)) return {false, {}, {}, "Mat khau da thay doi. Hay dang nhap lai."};
    return result;
}
bool AccountStore::isCurrent(const AccountResult &account) {
    if (!account.ok || account.id.isEmpty() || account.credentialToken.size() != 32) return false;
    QSqlQuery q(db); q.prepare("SELECT password_hash FROM accounts WHERE id=?"); q.addBindValue(account.id);
    return q.exec() && q.next() && AuthProtocol::equalHash(q.value(0).toByteArray(), account.credentialToken);
}
AccountResult AccountStore::changePassword(const QString &name, const QString &currentPassword, const QString &newPassword) {
    if (!AuthProtocol::validPassword(newPassword)) return {false, {}, {}, "Mat khau moi phai co 12-128 ky tu."};
    if (currentPassword == newPassword) return {false, {}, {}, "Mat khau moi phai khac mat khau hien tai."};
    const auto account = login(name, currentPassword);
    if (!account.ok) return {false, {}, {}, "Mat khau hien tai khong dung hoac tai khoan da thay doi."};
    const auto random = salt(), hash = derive(newPassword, random);
    if (hash.size() != 32) return {false, {}, {}, "Khong tao duoc bam mat khau moi."};
    QSqlQuery q(db);
    // One atomic compare-and-swap: two simultaneous changes cannot both use the old hash.
    q.prepare("UPDATE accounts SET salt=?,password_hash=?,iterations=?,algorithm=? WHERE id=? AND password_hash=?");
    q.addBindValue(random); q.addBindValue(hash); q.addBindValue(AuthProtocol::Iterations);
    q.addBindValue("pbkdf2-hmac-sha256"); q.addBindValue(account.id); q.addBindValue(account.credentialToken);
    if (!q.exec()) return {false, {}, {}, "Khong cap nhat duoc mat khau. Kiem tra database Server va dang nhap lai."};
    if (q.numRowsAffected() != 1) return {false, {}, {}, "Tai khoan vua thay doi o yeu cau khac. Hay dang nhap lai."};
    return {true, account.name, account.id, {}, hash};
}

AccountResult AccountStore::createRecovery(const QString &name, const QString &password) {
    const auto account = login(name, password);
    if (!account.ok) return {false, {}, {}, "Mật khẩu hiện tại không đúng hoặc tài khoản đã thay đổi."};
    const QString code = QString::fromLatin1((salt() + salt()).toHex());
    const auto hash = QCryptographicHash::hash(code.toLatin1(), QCryptographicHash::Sha256);
    QSqlQuery q(db);
    q.prepare("INSERT INTO recovery(account_id,code_hash,credential) SELECT id,?,password_hash FROM accounts WHERE id=? AND password_hash=? ON CONFLICT(account_id) DO UPDATE SET code_hash=excluded.code_hash,credential=excluded.credential");
    q.addBindValue(hash); q.addBindValue(account.id); q.addBindValue(account.credentialToken);
    if (!q.exec() || q.numRowsAffected() != 1) return {false, {}, {}, "Không tạo được mã. Hãy đăng nhập lại và thử lại."};
    auto result = account; result.recoveryCode = code; return result;
}
AccountResult AccountStore::resetPassword(const QString &name, const QString &code, const QString &password) {
    const QString failure = "Tài khoản hoặc mã khôi phục không đúng, đã dùng hoặc đã hết hiệu lực.";
    if (!AuthProtocol::validName(name) || !AuthProtocol::validRecovery(code) || !AuthProtocol::validPassword(password)) return {false, {}, {}, failure};
    const auto codeHash = QCryptographicHash::hash(code.toLower().toLatin1(), QCryptographicHash::Sha256);
    // Compute the new KDF before taking the write lock.
    const auto random = salt(), hash = derive(password, random);
    if (hash.size() != 32) return {false, {}, {}, "Không tạo được mật khẩu mới."};
    QSqlQuery begin(db);
    if (!begin.exec("BEGIN IMMEDIATE")) return {false, {}, {}, "Database đang bận. Thử lại sau."};
    auto fail = [&](const QString &text) { db.rollback(); return AccountResult{false, {}, {}, text}; };
    QSqlQuery q(db); q.prepare("SELECT a.id,a.display_name,a.password_hash,r.code_hash,r.credential FROM accounts a JOIN recovery r ON r.account_id=a.id WHERE a.username_key=?");
    q.addBindValue(AuthProtocol::canonical(name));
    if (!q.exec()) return fail("Không đọc được database tài khoản.");
    if (!q.next() || !AuthProtocol::equalHash(codeHash,q.value(3).toByteArray()) || !AuthProtocol::equalHash(q.value(2).toByteArray(),q.value(4).toByteArray())) return fail(failure);
    const QString id = q.value(0).toString(), display = q.value(1).toString(); q.finish();
    q.prepare("UPDATE accounts SET salt=?,password_hash=?,iterations=?,algorithm=? WHERE id=?");
    q.addBindValue(random); q.addBindValue(hash); q.addBindValue(AuthProtocol::Iterations); q.addBindValue("pbkdf2-hmac-sha256"); q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected()!=1) return fail("Không cập nhật được mật khẩu.");
    q.prepare("DELETE FROM recovery WHERE account_id=?"); q.addBindValue(id);
    if (!q.exec() || !db.commit()) return fail("Không hoàn tất được khôi phục.");
    return {true,display,id,{},hash};
}
bool AccountStore::setAvatar(const QString &name, const QByteArray &bytes) {
    QSqlQuery q(db);
    if (bytes.isEmpty()) {
        q.prepare("DELETE FROM avatars WHERE account_id IN (SELECT id FROM accounts WHERE username_key=?)");
        q.addBindValue(AuthProtocol::canonical(name)); return q.exec();
    }
    q.prepare("INSERT INTO avatars(account_id,jpeg) SELECT id,? FROM accounts WHERE username_key=? ON CONFLICT(account_id) DO UPDATE SET jpeg=excluded.jpeg");
    q.addBindValue(bytes); q.addBindValue(AuthProtocol::canonical(name));
    return q.exec() && q.numRowsAffected()==1;
}
QByteArray AccountStore::avatar(const QString &name) {
    QSqlQuery q(db); q.prepare("SELECT jpeg FROM avatars JOIN accounts ON accounts.id=avatars.account_id WHERE username_key=?"); q.addBindValue(AuthProtocol::canonical(name));
    return q.exec() && q.next() ? q.value(0).toByteArray() : QByteArray();
}

QByteArray AccountStore::emailCredential(const QString &name, const QString &email) {
    QSqlQuery q(db); q.prepare("SELECT password_hash FROM accounts a JOIN account_emails e ON e.account_id=a.id WHERE username_key=? AND email=?");
    q.addBindValue(AuthProtocol::canonical(name)); q.addBindValue(AuthProtocol::emailKey(email));
    return q.exec() && q.next() ? q.value(0).toByteArray() : QByteArray();
}
AccountResult AccountStore::bindEmail(const QString &name, const QString &email, const QString &password) {
    if (!AuthProtocol::validEmail(email)) return {false, {}, {}, "Email không hợp lệ."};
    const auto account=login(name,password); if(!account.ok) return account;
    // Existing verified email cannot be replaced through this recovery onboarding flow.
    QSqlQuery q(db); q.prepare("INSERT INTO account_emails(account_id,email) SELECT id,? FROM accounts WHERE id=? AND password_hash=?");
    q.addBindValue(AuthProtocol::emailKey(email)); q.addBindValue(account.id); q.addBindValue(account.credentialToken);
    if(!q.exec() || q.numRowsAffected()!=1) return {false, {}, {}, "Không liên kết được email. Tài khoản hoặc email đã được liên kết, hoặc mật khẩu vừa thay đổi."};
    return account;
}
AccountResult AccountStore::resetEmail(const QString &name, const QString &email, const QString &password, const QByteArray &credential) {
    const QString error="Yêu cầu hết hiệu lực. Hãy xin mã mới.";
    if(!AuthProtocol::validPassword(password) || credential.size()!=32) return {false, {}, {}, error};
    const auto random=salt(), hash=derive(password,random);
    if(hash.size()!=32 || !db.transaction()) return {false, {}, {}, error};
    QSqlQuery q(db); q.prepare("SELECT a.id,a.display_name FROM accounts a JOIN account_emails e ON e.account_id=a.id WHERE username_key=? AND email=? AND password_hash=?");
    q.addBindValue(AuthProtocol::canonical(name));q.addBindValue(AuthProtocol::emailKey(email));q.addBindValue(credential);
    if(!q.exec() || !q.next()) {db.rollback();return {false, {}, {}, error};}
    const auto id=q.value(0).toString(),display=q.value(1).toString();q.finish();
    q.prepare("UPDATE accounts SET salt=?,password_hash=?,iterations=?,algorithm=? WHERE id=? AND password_hash=?");
    q.addBindValue(random);q.addBindValue(hash);q.addBindValue(AuthProtocol::Iterations);q.addBindValue("pbkdf2-hmac-sha256");q.addBindValue(id);q.addBindValue(credential);
    if(!q.exec() || q.numRowsAffected()!=1) {db.rollback();return {false, {}, {}, error};}
    q.prepare("DELETE FROM recovery WHERE account_id=?");q.addBindValue(id);
    if(!q.exec() || !db.commit()) {db.rollback();return {false, {}, {}, error};}
    return {true,display,id,{},hash};
}
