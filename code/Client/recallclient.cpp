#include "recallclient.h"
#include "sessionconnection.h"
#include "chathistory.h"
#include "videoprotocol.h"
#include <QJsonArray>
RecallClient::RecallClient(SessionConnection *s,JsonChannel *c,ChatHistory *h,QObject *parent) : QObject(parent),session(s),channel(c),history(h) {
    editTimer.setSingleShot(true);editTimer.setInterval(30000);editSyncTimer.setSingleShot(true);editSyncTimer.setInterval(30000);
    connect(&editTimer,&QTimer::timeout,this,[this]{editPending.clear();emit editFailed("Chưa xác nhận được chỉnh sửa. Kết nối lại để đồng bộ, không tự gửi lại nội dung cũ.");});
    connect(&editSyncTimer,&QTimer::timeout,this,[this]{editSyncing=false;emit failed("Chưa đồng bộ được tin chỉnh sửa. Kiểm tra Server và Client đã cập nhật.");});
    timer.setSingleShot(true);timer.setInterval(30000);syncTimer.setSingleShot(true);syncTimer.setInterval(30000);
    connect(&timer,&QTimer::timeout,this,[this]{pending.clear();emit failed("Chưa nhận được xác nhận thu hồi. Kết nối lại để đồng bộ trước khi thử lại.");});
    connect(&syncTimer,&QTimer::timeout,this,[this]{syncing=false;emit failed("Chưa đồng bộ được trạng thái thu hồi. Kiểm tra cả Server và Client đã cập nhật.");});
    connect(session,&SessionConnection::changed,this,[this](SessionConnection::State state,const QString &) {
        if(state==SessionConnection::Ready) QTimer::singleShot(0,this,[this]{if(session->state()==SessionConnection::Ready) {cursor=0;syncing=true;syncPage();editCursor=0;editSyncing=true;editSyncPage();}});
        else if(state==SessionConnection::Idle) {editSyncTimer.stop();editSyncing=false;if(!editPending.isEmpty()){editPending.clear();editTimer.stop();emit editFailed("Mất kết nối khi sửa tin. Đăng nhập lại để đồng bộ trước khi thử lại.");}syncTimer.stop();syncing=false; if(!pending.isEmpty()) {pending.clear();timer.stop();emit failed("Mất kết nối khi thu hồi. Server có thể đã xử lý; đăng nhập lại để đồng bộ.");}}
    });
    connect(channel,&JsonChannel::packetReceived,this,[this](const QJsonObject &p) {
        if(session->state()!=SessionConnection::Ready) return;
        const QString type=p.value("type").toString();
        if(type=="message_recalled" || type=="message_edited") {history->receive(p);return;}
        if(type=="edit_result" && !editPending.isEmpty() && p.value("request_id").toString()==editPending) {
            editPending.clear();editTimer.stop();if(p.value("ok").toBool()) emit editCompleted();else emit editFailed(p.value("text").toString().left(300));return;
        }
        if(type=="edit_sync_error" && editSyncing) {editSyncing=false;editSyncTimer.stop();emit failed(p.value("text").toString().left(300));return;}
        if(type=="edit_sync_page" && editSyncing) {
            qint64 next=0;const auto items=p.value("items").toArray();const bool more=p.value("more").toBool();
            if(!p.value("items").isArray() || items.size()>50 || !VideoProtocol::number(p.value("cursor"),next,9007199254740991LL) || next<editCursor || (more && next<=editCursor)) {editSyncing=false;editSyncTimer.stop();emit failed("Trang chỉnh sửa không hợp lệ.");return;}
            for(const auto &item:items) if(item.isObject() && item.toObject().value("type")=="message_edited") history->receive(item.toObject());
            editCursor=next;if(more) QTimer::singleShot(0,this,[this]{if(editSyncing) editSyncPage();});else {editSyncing=false;editSyncTimer.stop();emit editsSynchronized();}return;
        }
        if(type=="recall_result" && !pending.isEmpty() && p.value("request_id").toString()==pending) {
            pending.clear();timer.stop();if(p.value("ok").toBool()) emit completed();else emit failed(p.value("text").toString().left(300));return;
        }
        if(type=="recall_sync_error" && syncing) {syncing=false;syncTimer.stop();emit failed(p.value("text").toString().left(300));return;}
        if(type!="recall_sync_page" || !syncing) return;
        qint64 next=0;const auto items=p.value("items").toArray();const bool more=p.value("more").toBool();
        if(!p.value("items").isArray() || items.size()>50 || !VideoProtocol::number(p.value("cursor"),next,9007199254740991LL) || next<cursor || (more && next<=cursor)) {
            syncing=false;syncTimer.stop();emit failed("Trang đồng bộ thu hồi không hợp lệ.");return;
        }
        for(const auto &item:items) if(item.isObject() && item.toObject().value("type")=="message_recalled") history->receive(item.toObject());
        cursor=next;
        if(more) QTimer::singleShot(0,this,[this]{if(syncing) syncPage();});
        else {syncing=false;syncTimer.stop();emit synchronized();}
    });
}
void RecallClient::syncPage() {
    syncTimer.start();if(!channel->send({{"type","recall_sync"},{"after",double(cursor)}})) {syncTimer.stop();syncing=false;emit failed("Không gửi được yêu cầu đồng bộ thu hồi.");}
}
bool RecallClient::request(const QString &eventId) {
    if(session->state()!=SessionConnection::Ready || !pending.isEmpty() || !editPending.isEmpty() || !VideoProtocol::validId(eventId)) {emit failed("Cần đăng nhập và chờ yêu cầu thu hồi trước hoàn tất.");return false;}
    pending=QUuid::createUuid().toString(QUuid::WithoutBraces);timer.start();
    if(!channel->send({{"type","recall_message"},{"event_id",eventId},{"request_id",pending}})) {pending.clear();timer.stop();emit failed("Không gửi được yêu cầu thu hồi.");return false;}return true;
}

void RecallClient::editSyncPage() {
    editSyncTimer.start();if(!channel->send({{"type","edit_sync"},{"after",double(editCursor)}})) {editSyncTimer.stop();editSyncing=false;emit failed("Không gửi được yêu cầu đồng bộ chỉnh sửa.");}
}
bool RecallClient::edit(const QString &eventId,const QString &input,qint64 base) {
    const QString text=input.trimmed();
    if(session->state()!=SessionConnection::Ready || !pending.isEmpty() || !editPending.isEmpty() || !VideoProtocol::validId(eventId) || text.isEmpty() || text.toUtf8().size()>4000 || text.contains(QChar(0)) || base<0 || base>9007199254740991LL) {emit editFailed("Cần đăng nhập, nội dung hợp lệ và chờ thao tác trước hoàn tất.");return false;}
    editPending=QUuid::createUuid().toString(QUuid::WithoutBraces);editTimer.start();
    if(!channel->send({{"type","edit_message"},{"event_id",eventId},{"text",text},{"base_revision",double(base)},{"request_id",editPending}})) {editPending.clear();editTimer.stop();emit editFailed("Không gửi được chỉnh sửa.");return false;}return true;
}
