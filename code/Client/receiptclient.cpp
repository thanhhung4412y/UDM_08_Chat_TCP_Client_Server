#include "receiptclient.h"
#include "sessionconnection.h"
#include "chathistory.h"
#include "historyprotocol.h"
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QWidget>
ReceiptClient::ReceiptClient(SessionConnection *s,JsonChannel *c,ChatHistory *h,QObject *parent) : QObject(parent),session(s),channel(c),history(h) {
    timer.setSingleShot(true);timer.setInterval(15000);syncTimer.setSingleShot(true);syncTimer.setInterval(30000);
    connect(&timer,&QTimer::timeout,this,[this]{pending.clear();pendingIds.clear();blocked=true;emit failed("Chưa xác nhận được trạng thái nhận tin. Kết nối lại để đồng bộ.");});
    connect(&syncTimer,&QTimer::timeout,this,[this]{syncing=false;blocked=true;emit failed("Chưa đồng bộ được trạng thái tin. Kiểm tra Server và Client đã cập nhật.");});
    connect(history,&ChatHistory::receiptStorageFailed,this,[this]{blocked=true;syncing=false;syncTimer.stop();});
    connect(history,&ChatHistory::privateReceived,this,[this]{schedule();});
    connect(session,&SessionConnection::changed,this,[this](SessionConnection::State state,const QString &){
        if(state==SessionConnection::Ready) QTimer::singleShot(0,this,[this]{if(session->state()==SessionConnection::Ready){blocked=false;cursor=0;scanCursor=0;syncing=true;syncPage();}});
        else if(state==SessionConnection::Idle){pending.clear();pendingIds.clear();timer.stop();syncTimer.stop();syncing=false;}
    });
    connect(channel,&JsonChannel::packetReceived,this,[this](const QJsonObject &p){
        if(session->state()!=SessionConnection::Ready) return;
        const auto type=p.value("type").toString();
        if(type=="message_receipt") {if(!HistoryProtocol::validReceipt(p)) return;if(blocked) history->deferReceipt(p);else history->receive(p);if(p.value("state").toInt()==1 && !syncing && !blocked) schedule(true);return;}
        if(type=="receipt_ack_result" && !pending.isEmpty() && p.value("request_id").toString()==pending) {
            timer.stop();pending.clear();
            if(!p.value("ok").toBool()) {blocked=true;pendingIds.clear();emit failed(p.value("text").toString().left(300));return;}
            const auto ignored=p.value("ignored").toArray();if(ignored.size()>50) {blocked=true;pendingIds.clear();return;}
            for(const auto &id:ignored) if(!pendingIds.contains(id.toString())) {blocked=true;pendingIds.clear();return;}
            if(!ignored.isEmpty()) history->ignoreReceipts(ignored);pendingIds.clear();schedule();return;
        }
        if(type=="receipt_sync_error" && syncing) {syncTimer.stop();syncing=false;blocked=true;emit failed(p.value("text").toString().left(300));return;}
        if(type!="receipt_sync_page" || !syncing) return;
        const auto items=p.value("items").toArray();qint64 next=0;const bool more=p.value("more").toBool();
        if(!p.value("items").isArray() || items.size()>50 || !VideoProtocol::number(p.value("cursor"),next,9007199254740991LL) || next<cursor || (more && next<=cursor)) {syncTimer.stop();syncing=false;blocked=true;emit failed("Trang trạng thái không hợp lệ.");return;}
        for(const auto &value:items) if(value.isObject() && value.toObject().value("type")=="message_receipt") {if(blocked) history->deferReceipt(value.toObject());else history->receive(value.toObject());}
        if(blocked) return;cursor=next;
        if(more) QTimer::singleShot(0,this,[this]{if(syncing) syncPage();});
        else {syncTimer.stop();syncing=false;emit synchronized();schedule(true);}
    });
}
void ReceiptClient::setViewing(const QString &value,bool visible) {if(peer==value && viewing==visible) return;peer=value;viewing=visible;schedule(true);}
void ReceiptClient::schedule(bool reset) {if(reset) scanCursor=0;if(pumpQueued) return;pumpQueued=true;QTimer::singleShot(0,this,[this]{pumpQueued=false;pump();});}
void ReceiptClient::syncPage() {syncTimer.start();if(!channel->send({{"type","receipt_sync"},{"after",double(cursor)}})){blocked=true;syncing=false;syncTimer.stop();emit failed("Không gửi được yêu cầu trạng thái tin.");}}
void ReceiptClient::pump() {
    if(session->state()!=SessionConnection::Ready || syncing || blocked || !pending.isEmpty()) return;
    QString error;const auto work=history->receiptWork(peer,viewing && (!readGuard || readGuard()),scanCursor,error);
    if(!error.isEmpty()){blocked=true;emit failed(error);return;}
    scanCursor=qint64(work.value("cursor").toDouble());const auto items=work.value("items").toArray();
    if(items.isEmpty()){if(work.value("more").toBool()) schedule();return;}
    pending=QUuid::createUuid().toString(QUuid::WithoutBraces);pendingIds.clear();for(const auto &item:items) pendingIds.insert(item.toObject().value("event_id").toString());
    timer.start();if(!channel->send({{"type","receipt_ack"},{"request_id",pending},{"items",items}})){timer.stop();pending.clear();pendingIds.clear();blocked=true;emit failed("Không gửi được xác nhận nhận tin.");}
}
bool ReceiptVisibility::canMarkSeen(QWidget *window) {return window && window->isVisible() && !window->isMinimized() && window->isActiveWindow() && QApplication::applicationState()==Qt::ApplicationActive && !QApplication::activeModalWidget();}
ReceiptVisibility::ReceiptVisibility(ReceiptClient *c,QWidget *w,QComboBox *r) : QObject(w),client(c),window(w),receiver(r) {
    window->installEventFilter(this);client->setReadGuard([w]{return canMarkSeen(w);});
    connect(receiver,&QComboBox::currentIndexChanged,this,[this]{update();});
    connect(qApp,&QGuiApplication::applicationStateChanged,this,[this]{update();});update();
}
void ReceiptVisibility::update() {client->setViewing(receiver->currentData().toString(),canMarkSeen(window));}
bool ReceiptVisibility::eventFilter(QObject *object,QEvent *event) {
    if(object==window && (event->type()==QEvent::WindowActivate || event->type()==QEvent::WindowDeactivate || event->type()==QEvent::WindowStateChange || event->type()==QEvent::Show || event->type()==QEvent::Hide)) QTimer::singleShot(0,this,[this]{update();});
    return QObject::eventFilter(object,event);
}
