#pragma once
#include <QJsonObject>
#include <QSqlDatabase>
#include <QVector>

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
    bool isOpen() const { return database.isValid() && database.isOpen(); }
    QString path() const { return filePath; }
    SaveResult append(const QJsonObject &packet, HistoryRecord &record, QString &error);
    HistoryPage page(int limit, qint64 beforeSequence, QString &error) const;
    static bool normalize(const QJsonObject &packet, HistoryRecord &record, QString &error);
    static QString accountPath(const QString &root, const QString &server, quint16 port, const QString &username);
private:
    QSqlDatabase database;
    QString connectionName, filePath;
    void close();
};
