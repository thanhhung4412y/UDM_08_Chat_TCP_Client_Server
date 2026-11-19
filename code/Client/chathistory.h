#pragma once
#include "historystore.h"
#include "historyview.h"
#include <QObject>
#include <QSet>
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
    void showSearch(QWidget *parent);
    void selectConversation(const QString &peer);
    QJsonObject recallCandidate(int position) const;
    QJsonObject receiptWork(const QString &peer,bool viewing,qint64 after,QString &error) const;
    void ignoreReceipts(const QJsonArray &ids);
    void deferReceipt(const QJsonObject &notice);
    QString path() const { return store.path(); }
signals:
    void privateReceived();
    void receiptChanged(const QJsonObject &notice);
    void receiptStorageFailed();
    void editRequested(const QJsonObject &packet);
    void messageEdited(const QJsonObject &notice);
    void recallRequested(const QString &id);
    void messageRecalled(const QString &id);
    void conversationMessage(const QString &peer, bool incoming);
    void storageStatus(const QString &text);
    void linkError(const QString &text);
private:
    HistoryStore store;
    HistoryView live;
    QString owner, server, activePeer;
    QVector<HistoryRecord> fallback;
    QSet<QString> failedRecalls;
    QHash<QString,QJsonObject> failedEdits,failedReceipts;
    QString peerFor(const QJsonObject &packet) const;
    quint16 port;
    void save(const QJsonObject &packet);
    void updateReceipt(const QJsonObject &notice,bool persist);
    bool belongs(const QJsonObject &packet) const;
};
