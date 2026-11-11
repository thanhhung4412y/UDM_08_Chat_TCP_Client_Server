#pragma once
#include <QSqlDatabase>
#include <QJsonObject>
#include <QJsonArray>
class RecallStore {
public:
    ~RecallStore();
    void close();
    bool open(const QString &path, QString &error);
    bool remember(const QJsonObject &packet);
    bool contains(const QString &id);
    QJsonObject recall(const QString &id,const QString &actor,QString &error);
    QJsonObject receipt(const QString &id);
    QJsonArray acknowledge(const QString &actor,const QJsonArray &items,QJsonArray &ignored,QString &error);
    QJsonArray receiptPage(const QString &actor,qint64 after,qint64 &cursor,bool &more,QString &error);
    QJsonObject edit(const QString &id,const QString &actor,const QString &text,qint64 base,QString &error);
    QJsonArray editPage(const QString &actor,qint64 after,qint64 &cursor,bool &more,QString &error);
    QJsonArray page(const QString &actor,qint64 after,qint64 &cursor,bool &more,QString &error);
private:
    QSqlDatabase db; QString connection;
};
