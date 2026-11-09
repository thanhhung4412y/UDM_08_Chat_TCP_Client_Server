#pragma once
#include "historystore.h"
#include "historyview.h"
#include <QObject>
class QTextBrowser;
class QWidget;

class ChatHistory : public QObject {
    Q_OBJECT
public:
    ChatHistory(QTextBrowser *browser, QString owner, QString server, quint16 port, QObject *parent = nullptr);
    bool initialize(const QString &root);
    bool receive(const QJsonObject &packet);
    void completedVideo(QJsonObject packet);
    void showDialog(QWidget *parent);
    QString path() const { return store.path(); }
signals:
    void storageStatus(const QString &text);
    void linkError(const QString &text);
private:
    HistoryStore store;
    HistoryView live;
    QString owner, server;
    quint16 port;
    void save(const QJsonObject &packet);
    bool belongs(const QJsonObject &packet) const;
};
