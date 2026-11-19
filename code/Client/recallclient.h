#pragma once
#include <QObject>
#include <QTimer>
class SessionConnection;
class JsonChannel;
class ChatHistory;
class RecallClient : public QObject {
    Q_OBJECT
public:
    RecallClient(SessionConnection *session,JsonChannel *channel,ChatHistory *history,QObject *parent=nullptr);
    bool request(const QString &eventId);
    bool edit(const QString &eventId,const QString &text,qint64 base);
signals:
    void failed(const QString &text);
    void completed();
    void editCompleted();
    void editFailed(const QString &text);
    void editsSynchronized();
    void synchronized();
private:
    SessionConnection *session; JsonChannel *channel;ChatHistory *history;
    QTimer timer,syncTimer,editTimer,editSyncTimer;
    QString editPending;bool editSyncing=false;qint64 editCursor=0;QString pending;bool syncing=false;qint64 cursor=0;
    void syncPage();
    void editSyncPage();
};
