#pragma once
#include <QObject>
#include <QTimer>
#include <QSet>
#include <functional>
class SessionConnection;
class JsonChannel;
class ChatHistory;
class QWidget;
class QComboBox;
class ReceiptClient : public QObject {
    Q_OBJECT
public:
    ReceiptClient(SessionConnection *session,JsonChannel *channel,ChatHistory *history,QObject *parent=nullptr);
    void setViewing(const QString &peer,bool visible);
    void setReadGuard(std::function<bool()> guard) {readGuard=std::move(guard);schedule(true);}
signals:
    void failed(const QString &text);
    void synchronized();
private:
    SessionConnection *session;JsonChannel *channel;ChatHistory *history;
    QTimer timer,syncTimer;QString pending,peer;QSet<QString> pendingIds;
    bool viewing=false,syncing=false,blocked=false,pumpQueued=false;qint64 cursor=0,scanCursor=0;
    std::function<bool()> readGuard;
    void syncPage();void schedule(bool reset=false);void pump();
};
class ReceiptVisibility : public QObject {
public:
    ReceiptVisibility(ReceiptClient *client,QWidget *window,QComboBox *receiver);
    static bool canMarkSeen(QWidget *window);
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    ReceiptClient *client;QWidget *window;QComboBox *receiver;
    void update();
};
