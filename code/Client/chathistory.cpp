#include "chathistory.h"
#include "historysearchdialog.h"
#include "historyprotocol.h"
#include "videoprotocol.h"
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

ChatHistory::ChatHistory(QTextBrowser *browser, QString user, QString host, quint16 serverPort, QObject *parent)
    : QObject(parent), live(browser, user), owner(std::move(user)), server(std::move(host)), port(serverPort) {
    connect(&live, &HistoryView::linkError, this, &ChatHistory::linkError);
}
bool ChatHistory::initialize(const QString &root) {
    QString error;
    if (!store.open(HistoryStore::accountPath(root, server, port, owner), error)) {
        emit storageStatus("Khong mo duoc lich su: " + error + ". Chat truc tiep van hoat dong.");
        return false;
    }
    for (const auto &peer : store.peers(owner, error)) emit conversationMessage(peer, false);
    const auto page = store.page(100, 0, error, activePeer, true);
    if (!error.isEmpty()) { emit storageStatus("Khong tai duoc lich su: " + error); return false; }
    live.replace(page.records);
    emit storageStatus(QString("SQLite: da tai %1 muc gan nhat. Bam Lich su de xem muc cu.").arg(page.records.size()));
    return true;
}
bool ChatHistory::belongs(const QJsonObject &p) const {
    const QString to = p.value("to").toString();
    if (to.isEmpty()) return p.value("type") != "private";
    return owner.compare(p.value("sender").toString(), Qt::CaseInsensitive) == 0
        || owner.compare(to, Qt::CaseInsensitive) == 0;
}
bool ChatHistory::receive(const QJsonObject &p) {
    if(p.value("type")=="message_receipt") {updateReceipt(p,true);return true;}
    if(p.value("type")=="message_edited") {
        if(!HistoryProtocol::validEdit(p) || !belongs(p) || p.value("sender").toString().isEmpty()) return true;
        QString error;const QString id=p.value("event_id").toString();
        if(!store.applyEdit(p,error)) {
            if(p.value("revision").toDouble()>failedEdits.value(id).value("revision").toDouble()) failedEdits.insert(id,p);
            emit storageStatus("Không lưu được chỉnh sửa: "+error);
        } else if(p.value("revision").toDouble()>=failedEdits.value(id).value("revision").toDouble()) failedEdits.remove(id);
        if(!failedRecalls.contains(id)) {
            live.edit(p);for(auto &record:fallback) if(record.packet.value("_event_id").toString()==id) record.packet=HistoryProtocol::withEdit(record.packet,p);
            emit messageEdited(p);
        }
        return true;
    }
    if(p.value("type")=="message_recalled") {
        if(!VideoProtocol::validId(p.value("event_id").toString()) || !belongs(p) || p.value("sender").toString().isEmpty()) return true;
        const QString id=p.value("event_id").toString(); QString error;
        if(!store.applyRecall(p,error)) { failedRecalls.insert(id); emit storageStatus("Không lưu được thu hồi: "+error); }
        else failedRecalls.remove(id);
        failedEdits.remove(id);failedReceipts.remove(id);live.recall(id);
        for(auto &record:fallback) if(record.packet.value("_event_id").toString()==id) {record.packet=HistoryProtocol::tombstone(record.packet);record.image.clear();}
        emit messageRecalled(id);return true;
    }
    if (!HistoryProtocol::messageType(p.value("type").toString())) return false;
    if (belongs(p)) save(p);
    return true;
}
void ChatHistory::completedVideo(QJsonObject p) {
    if (p.value("type") != "video") return;
    if (p.value("outgoing").toBool()) p["sender"] = owner;
    if (belongs(p)) save(p);
}
void ChatHistory::save(const QJsonObject &p) {
    HistoryRecord record;
    QString error;
    const auto result = store.append(p, record, error);
    if (result == HistoryStore::SaveResult::Duplicate) return;
    if (!record.packet.isEmpty()) {
        if(failedEdits.contains(record.packet.value("_event_id").toString())) record.packet=HistoryProtocol::withEdit(record.packet,failedEdits.value(record.packet.value("_event_id").toString()));
        if(failedRecalls.contains(record.packet.value("_event_id").toString())) {record.packet=HistoryProtocol::tombstone(record.packet);record.image.clear();}
        if(failedReceipts.contains(record.packet.value("_event_id").toString())) record.packet=HistoryProtocol::withReceipt(record.packet,failedReceipts.value(record.packet.value("_event_id").toString()));
        const QString peer = peerFor(record.packet);
        emit conversationMessage(peer, owner.compare(record.packet.value("sender").toString(), Qt::CaseInsensitive) != 0);
        if (peer.compare(activePeer, Qt::CaseInsensitive) == 0) live.append(record);
        if (result == HistoryStore::SaveResult::Error) {
            fallback.append(record);
            if (fallback.size() > 100) fallback.removeFirst();
        }
    }
    if(!record.packet.isEmpty() && !record.packet.value("to").toString().isEmpty() && owner.compare(record.packet.value("sender").toString(),Qt::CaseInsensitive)!=0 && !record.packet.value("_recalled").toBool()) emit privateReceived();
    if (result == HistoryStore::SaveResult::Error)
        emit storageStatus("Khong luu duoc lich su: " + error);
}
void ChatHistory::showDialog(QWidget *parent) {
    if (!store.isOpen()) {
        QMessageBox::warning(parent, "Lich su", "Database lich su chua mo duoc. Xem thong bao SQLite trong cua so chat.");
        return;
    }
    QDialog dialog(parent);
    dialog.setWindowTitle("Lịch sử: " + (activePeer.isEmpty() ? QString("Chưa chọn cuộc trò chuyện") : activePeer) + " — " + owner + " @ " + server + ":" + QString::number(port));
    dialog.resize(850, 600);
    auto *layout = new QVBoxLayout(&dialog);
    auto *pathLabel = new QLabel("Database: " + store.path());
    pathLabel->setTextFormat(Qt::PlainText);
    pathLabel->setWordWrap(true);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(pathLabel);
    auto *browser = new QTextBrowser();
    layout->addWidget(browser);
    HistoryView view(browser, owner); view.enableRecallMenu();
    connect(&view,&HistoryView::recallRequested,this,&ChatHistory::recallRequested);
    connect(&view,&HistoryView::editRequested,this,&ChatHistory::editRequested);
    connect(this,&ChatHistory::messageEdited,&view,&HistoryView::edit);
    connect(this,&ChatHistory::receiptChanged,&view,&HistoryView::receipt);
    connect(this,&ChatHistory::messageRecalled,&dialog,[&view](const QString &id){view.recall(id);});
    auto *status = new QLabel(); status->setTextFormat(Qt::PlainText); layout->addWidget(status);
    auto *buttons = new QHBoxLayout(); layout->addLayout(buttons);
    auto *older = new QPushButton("Cu hon"); auto *newer = new QPushButton("Moi hon");
    auto *latest = new QPushButton("Moi nhat / Tai lai"); auto *close = new QPushButton("Dong");
    buttons->addWidget(older); buttons->addWidget(newer); buttons->addWidget(latest); buttons->addStretch(); buttons->addWidget(close);
    QVector<qint64> cursors{0};
    qint64 oldest = 0;
    auto load = [&] {
        QString error;
        const auto page = store.page(50, cursors.last(), error, activePeer, true);
        auto records=page.records;
        for(auto &record:records) if(failedReceipts.contains(record.packet.value("_event_id").toString())) record.packet=HistoryProtocol::withReceipt(record.packet,failedReceipts.value(record.packet.value("_event_id").toString()));
        for(auto &record:records) {if(failedEdits.contains(record.packet.value("_event_id").toString())) record.packet=HistoryProtocol::withEdit(record.packet,failedEdits.value(record.packet.value("_event_id").toString()));}
        for(auto &record:records) if(failedRecalls.contains(record.packet.value("_event_id").toString())) {record.packet=HistoryProtocol::tombstone(record.packet);record.image.clear();}
        view.replace(records);
        oldest = page.records.isEmpty() ? 0 : page.records.first().sequence;
        older->setEnabled(error.isEmpty() && page.hasMore);
        newer->setEnabled(cursors.size() > 1);
        status->setText(error.isEmpty() ? QString("Trang %1 — %2 muc (cu → moi)").arg(cursors.size()).arg(page.records.size()) : error);
    };
    connect(older, &QPushButton::clicked, &dialog, [&] { if (oldest > 0) { cursors.append(oldest); load(); } });
    connect(newer, &QPushButton::clicked, &dialog, [&] { if (cursors.size() > 1) { cursors.removeLast(); load(); } });
    connect(latest, &QPushButton::clicked, &dialog, [&] { cursors = {0}; load(); });
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(&view, &HistoryView::linkError, &dialog, [&](const QString &text) { QMessageBox::warning(&dialog, "Video", text); });
    load(); dialog.exec();
}

QString ChatHistory::peerFor(const QJsonObject &packet) const {
    const QString to = packet.value("to").toString();
    if (to.isEmpty()) return {};
    const QString sender = packet.value("sender").toString();
    return owner.compare(sender, Qt::CaseInsensitive) == 0 ? to : sender;
}
void ChatHistory::selectConversation(const QString &peer) {
    activePeer = peer;
    QString error;
    auto page = store.page(100, 0, error, activePeer, true);
    for (const auto &record : fallback)
        if (peerFor(record.packet).compare(peer, Qt::CaseInsensitive) == 0) page.records.append(record);
    for(auto &record:page.records) if(failedReceipts.contains(record.packet.value("_event_id").toString())) record.packet=HistoryProtocol::withReceipt(record.packet,failedReceipts.value(record.packet.value("_event_id").toString()));
    for(auto &record:page.records) {if(failedEdits.contains(record.packet.value("_event_id").toString())) record.packet=HistoryProtocol::withEdit(record.packet,failedEdits.value(record.packet.value("_event_id").toString()));}
    for(auto &record:page.records) if(failedRecalls.contains(record.packet.value("_event_id").toString())) {record.packet=HistoryProtocol::tombstone(record.packet);record.image.clear();}
    live.replace(page.records);
    if (!error.isEmpty()) emit storageStatus(error);
}

void ChatHistory::showSearch(QWidget *parent) {
    if(!store.isOpen()) { QMessageBox::warning(parent,"Tìm tin nhắn","Lịch sử chưa mở được; không thể tìm các tin chưa lưu. Chat trực tiếp vẫn dùng được."); return; }
    HistorySearchDialog dialog(store.path(),owner,activePeer,parent);
    for(const auto &id:failedRecalls) dialog.invalidateRecall(id);
    for(const auto &notice:failedEdits) dialog.invalidateEdit(notice);
    for(const auto &notice:failedReceipts) dialog.updateReceipt(notice);
    connect(this,&ChatHistory::messageRecalled,&dialog,&HistorySearchDialog::invalidateRecall);
    connect(&dialog,&HistorySearchDialog::recallRequested,this,&ChatHistory::recallRequested);
    connect(this,&ChatHistory::messageEdited,&dialog,&HistorySearchDialog::invalidateEdit);
    connect(this,&ChatHistory::receiptChanged,&dialog,&HistorySearchDialog::updateReceipt);
    connect(&dialog,&HistorySearchDialog::editRequested,this,&ChatHistory::editRequested); dialog.exec();
}

QJsonObject ChatHistory::recallCandidate(int position) const {
    const auto packet=live.packetAt(position);
    if(packet.value("_recalled").toBool() || owner.compare(packet.value("sender").toString(),Qt::CaseInsensitive)!=0) return {};
    return packet;
}

QJsonObject ChatHistory::receiptWork(const QString &peer,bool viewing,qint64 after,QString &error) const {return store.scanReceipts(owner,peer,viewing,after,error);}
void ChatHistory::ignoreReceipts(const QJsonArray &ids) {QString error;if(!store.ignoreReceipts(ids,error)){emit storageStatus(error);emit receiptStorageFailed();}}

void ChatHistory::deferReceipt(const QJsonObject &notice) {updateReceipt(notice,false);}
void ChatHistory::updateReceipt(const QJsonObject &p,bool persist) {
    if(!HistoryProtocol::validReceipt(p) || !belongs(p)) return;
    QString error;const QString id=p.value("event_id").toString();
    if(!persist || !store.applyReceipt(p,error)) {
        const auto old=failedReceipts.value(id);if(p.value("revision").toDouble()>old.value("revision").toDouble() && p.value("state").toInt()>=old.value("state").toInt()) failedReceipts[id]=p;
        if(persist){emit storageStatus(error);emit receiptStorageFailed();}
    } else if(p.value("revision").toDouble()>=failedReceipts.value(id).value("revision").toDouble() && p.value("state").toInt()>=failedReceipts.value(id).value("state").toInt()) failedReceipts.remove(id);
    live.receipt(p);for(auto &record:fallback) if(record.packet.value("_event_id").toString()==id) record.packet=HistoryProtocol::withReceipt(record.packet,p);
    emit receiptChanged(p);
}
