#pragma once
#include <QJsonObject>
#include <QSqlDatabase>
#include <QVector>
#include <QStringList>
#include <QJsonArray>
#include <atomic>
#include <memory>

struct HistoryRecord {
    qint64 sequence = 0;
    QJsonObject packet;
    QByteArray image;
};
struct HistoryPage {
    QVector<HistoryRecord> records; // oldest first, bounded page size
    bool hasMore = false;
};
class HistoryStore {
public:
    enum class SaveResult { Saved, Duplicate, Error };
    HistoryStore() = default;
    ~HistoryStore();
    HistoryStore(const HistoryStore &) = delete;
    HistoryStore &operator=(const HistoryStore &) = delete;
    bool open(const QString &path, QString &error);
    bool openReadOnly(const QString &path, QString &error);
    HistoryPage search(const QString &text, const QString &peer, qint64 before, QString &error, int limit = 50, const std::shared_ptr<std::atomic_bool> &cancel = {}) const;
    QVector<HistoryRecord> context(qint64 sequence, const QString &peer, QString &error) const;
    static QString searchKey(const QString &text);
    bool isOpen() const { return database.isValid() && database.isOpen(); }
    QString path() const { return filePath; }
    SaveResult append(const QJsonObject &packet, HistoryRecord &record, QString &error);
    bool applyReceipt(const QJsonObject &notice,QString &error);
    bool ignoreReceipts(const QJsonArray &ids,QString &error);
    QJsonObject scanReceipts(const QString &owner,const QString &peer,bool viewing,qint64 after,QString &error) const;
    bool applyEdit(const QJsonObject &notification,QString &error);
    bool applyRecall(const QJsonObject &notification, QString &error);
    HistoryPage page(int limit, qint64 beforeSequence, QString &error, const QString &peer = QString(), bool filter = false) const;
    QStringList peers(const QString &owner, QString &error) const;
    static bool normalize(const QJsonObject &packet, HistoryRecord &record, QString &error);
    static QString accountPath(const QString &root, const QString &server, quint16 port, const QString &username);
private:
    QSqlDatabase database;
    QString connectionName, filePath;
    void close();
};
