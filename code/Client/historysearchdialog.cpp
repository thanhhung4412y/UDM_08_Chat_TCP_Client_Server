#include "historysearchdialog.h"
#include "historyview.h"
#include "historyprotocol.h"
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QPushButton>
#include <QRunnable>
#include <QShortcut>
#include <QSplitter>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

HistorySearchDialog::HistorySearchDialog(QString file, QString user, QString recipient, QWidget *parent)
    : QDialog(parent), path(std::move(file)),owner(std::move(user)),peer(std::move(recipient)) {
    setWindowTitle("Tìm tin nhắn · "+(peer.isEmpty() ? QString("Phòng chung") : peer)); resize(980,650); setMinimumSize(740,520);
    pool.setMaxThreadCount(1);
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(20,20,20,20); layout->setSpacing(12);
    auto *scope=new QLabel("Tìm trong: "+(peer.isEmpty() ? QString("Phòng chung") : peer)+" · Lịch sử đã lưu trên máy này");
    scope->setTextFormat(Qt::PlainText); scope->setWordWrap(true); layout->addWidget(scope);
    auto *row=new QHBoxLayout(); input=new QLineEdit(); input->setObjectName("historySearchInput"); input->setMaxLength(128); input->setClearButtonEnabled(true);
    input->setPlaceholderText("Tìm nội dung, câu trích dẫn hoặc tên video… (có / không dấu)"); row->addWidget(input,1);
    auto *find=new QPushButton("Tìm kiếm"); find->setObjectName("runHistorySearch"); row->addWidget(find); layout->addLayout(row);
    status=new QLabel("Nhập từ khóa để tìm tin cũ trong cuộc trò chuyện này."); status->setObjectName("searchStatus"); status->setTextFormat(Qt::PlainText); status->setWordWrap(true); layout->addWidget(status);
    auto *split=new QSplitter(Qt::Horizontal); split->setChildrenCollapsible(false); layout->addWidget(split,1);
    results=new QListWidget(); results->setObjectName("searchResults"); results->setWordWrap(true); results->setSpacing(4);
    results->setStyleSheet("QListWidget::item { padding:10px; border-bottom:1px solid #e5edf0; } QListWidget::item:selected { background:#e3f1ed; color:#155c53; }"); results->setMinimumWidth(220); split->addWidget(results);
    auto *preview=new QWidget(); auto *previewLayout=new QVBoxLayout(preview); previewLayout->setContentsMargins(10,0,0,0);
    contextStatus=new QLabel("Chọn kết quả để xem tin và đoạn lịch sử xung quanh."); contextStatus->setObjectName("searchContextStatus"); contextStatus->setWordWrap(true); contextStatus->setTextFormat(Qt::PlainText); previewLayout->addWidget(contextStatus);
    browser=new QTextBrowser(); browser->setObjectName("searchContext"); previewLayout->addWidget(browser,1); split->addWidget(preview); split->setSizes({330,610});
    view=new HistoryView(browser,owner,this); view->replace({}); view->enableRecallMenu();
    connect(view,&HistoryView::recallRequested,this,&HistorySearchDialog::recallRequested);
    connect(view,&HistoryView::editRequested,this,&HistorySearchDialog::editRequested);
    connect(view,&HistoryView::linkError,this,[this](const QString &text){ contextStatus->setText(text); });
    auto *buttons=new QHBoxLayout(); newer=new QPushButton("Kết quả mới hơn"); older=new QPushButton("Kết quả cũ hơn");
    newer->setObjectName("searchNewer"); older->setObjectName("searchOlder"); newer->setEnabled(false); older->setEnabled(false);
    buttons->addWidget(newer); buttons->addWidget(older); buttons->addStretch(); auto *close=new QPushButton("Đóng"); buttons->addWidget(close); layout->addLayout(buttons);
    debounce=new QTimer(this); debounce->setSingleShot(true); debounce->setInterval(300);
    connect(input,&QLineEdit::textChanged,this,[this] {
        if(searchCancel) searchCancel->store(true); if(contextCancel) contextCancel->store(true);
        ++searchVersion; ++contextVersion; older->setEnabled(false); newer->setEnabled(false);
        hits.clear(); results->clear(); view->replace({});
        status->setText(input->text().trimmed().isEmpty() ? "Nhập từ khóa để tìm tin cũ trong cuộc trò chuyện này." : "Đang chờ tìm…"); debounce->start();
    });
    connect(debounce,&QTimer::timeout,this,[this]{startSearch(true);});
    connect(find,&QPushButton::clicked,this,[this]{startSearch(true);});
    connect(input,&QLineEdit::returnPressed,this,[this]{startSearch(true);});
    connect(older,&QPushButton::clicked,this,[this]{ if(!hits.isEmpty()) { cursors.append(hits.last().sequence); startSearch(false); } });
    connect(newer,&QPushButton::clicked,this,[this]{ if(cursors.size()>1) { cursors.removeLast(); startSearch(false); } });
    connect(results,&QListWidget::currentRowChanged,this,&HistorySearchDialog::openResult);
    connect(close,&QPushButton::clicked,this,&QDialog::reject);
    auto *shortcut=new QShortcut(QKeySequence::Find,this); connect(shortcut,&QShortcut::activated,input,[this]{input->setFocus();input->selectAll();});
    for(auto *button : {find,older,newer,close}) button->setAutoDefault(false);
    input->setFocus();
}
HistorySearchDialog::~HistorySearchDialog() {
    if(searchCancel) searchCancel->store(true); if(contextCancel) contextCancel->store(true);
    pool.clear(); pool.waitForDone(); // Worker owns/closes its SQLite connection before this object is destroyed.
}
void HistorySearchDialog::startSearch(bool reset) {
    debounce->stop(); if(searchCancel) searchCancel->store(true); if(contextCancel) contextCancel->store(true);
    ++searchVersion; ++contextVersion; pool.clear();
    if(reset) { cursors={0}; submitted=input->text().trimmed(); }
    hits.clear(); results->clear(); view->replace({}); older->setEnabled(false); newer->setEnabled(false);
    contextStatus->setText("Chọn kết quả để xem tin và đoạn lịch sử xung quanh.");
    if(submitted.isEmpty()) { status->setText("Nhập từ khóa để tìm tin cũ trong cuộc trò chuyện này."); return; }
    status->setText("Đang tìm trong lịch sử… Bạn vẫn có thể đổi từ khóa hoặc đóng cửa sổ.");
    searchCancel=std::make_shared<std::atomic_bool>(false);
    const auto cancel=searchCancel; const auto version=searchVersion; const auto before=cursors.last();
    const QString file=path, recipient=peer, text=submitted;
    pool.start(QRunnable::create([this,cancel,version,before,file,recipient,text] {
        HistoryPage page; QString error;
        { HistoryStore store; if(store.openReadOnly(file,error)) page=store.search(text,recipient,before,error,50,cancel); }
        if(cancel->load()) return;
        QMetaObject::invokeMethod(this,[this,cancel,version,page,error] {
            if(cancel->load() || version!=searchVersion) return;
            if(!error.isEmpty()) { status->setText("Không tìm được tin: "+error); return; }
            hits.clear();for(auto record:page.records) {
                const auto id=record.packet.value("_event_id").toString();if(recalledIds.contains(id)) continue;
                if(editOverlay.contains(id)) record.packet=HistoryProtocol::withEdit(record.packet,editOverlay.value(id));
                if(receiptOverlay.contains(id)) record.packet=HistoryProtocol::withReceipt(record.packet,receiptOverlay.value(id));
                bool matches=false;for(const auto &field:{"text","quote","name"}) if(HistoryStore::searchKey(record.packet.value(field).toString()).contains(HistoryStore::searchKey(submitted))) matches=true;
                if(matches) hits.append(record);
            }
            for(const auto &hit : hits) {
                const auto &p=hit.packet;
                const auto time=QDateTime::fromString(p.value("_utc").toString(),Qt::ISODateWithMs).toLocalTime();
                QString snippet=p.value("text").toString();
                if(!p.value("quote").toString().isEmpty()) snippet+="  · Trích dẫn: "+p.value("quote").toString();
                if(p.value("type")=="video") snippet="Video: "+p.value("name").toString();
                auto *item=new QListWidgetItem(p.value("sender").toString()+" · "+time.toString("dd/MM/yyyy HH:mm")+"\n"+snippet.simplified().left(220),results);
                item->setData(Qt::UserRole,hit.sequence); item->setToolTip(snippet.left(1000));
            }
            older->setEnabled(page.hasMore); newer->setEnabled(cursors.size()>1);
            status->setText(hits.isEmpty() ? "Không tìm thấy tin phù hợp trong lịch sử đã lưu." : QString("Trang %1 · %2 kết quả (mới → cũ). Tin đang chọn được tô vàng.").arg(cursors.size()).arg(hits.size()));
            if(!hits.isEmpty()) results->setCurrentRow(0);
        },Qt::QueuedConnection);
    }));
}
void HistorySearchDialog::openResult(int row) {
    if(contextCancel) contextCancel->store(true); ++contextVersion;
    if(row<0 || row>=hits.size()) return;
    const auto sequence=hits[row].sequence; const auto version=contextVersion;
    contextCancel=std::make_shared<std::atomic_bool>(false); const auto cancel=contextCancel;
    const QString file=path,recipient=peer;
    view->replace({}); contextStatus->setText("Đang mở đoạn lịch sử…");
    pool.start(QRunnable::create([this,cancel,version,sequence,file,recipient] {
        if(cancel->load()) return;
        QVector<HistoryRecord> records; QString error;
        { HistoryStore store; if(store.openReadOnly(file,error)) records=store.context(sequence,recipient,error); }
        if(cancel->load()) return;
        QMetaObject::invokeMethod(this,[this,cancel,version,sequence,records,error] {
            if(cancel->load() || version!=contextVersion) return;
            if(!error.isEmpty()) { contextStatus->setText(error); return; }
            auto sanitized=records;
            for(auto &record:sanitized) {const auto id=record.packet.value("_event_id").toString();if(editOverlay.contains(id)) record.packet=HistoryProtocol::withEdit(record.packet,editOverlay.value(id));}
            for(auto &record:sanitized) {const auto id=record.packet.value("_event_id").toString();if(receiptOverlay.contains(id)) record.packet=HistoryProtocol::withReceipt(record.packet,receiptOverlay.value(id));}
            for(auto &record:sanitized) if(recalledIds.contains(record.packet.value("_event_id").toString())) {record.packet=HistoryProtocol::tombstone(record.packet);record.image.clear();}
            view->replace(sanitized); view->focusRecord(sequence);
            contextStatus->setText("Tin được chọn (tô vàng) và tối đa 10 tin trước / sau. Câu trích dẫn hiển thị trong tin trả lời.");
        },Qt::QueuedConnection);
    }));
}


void HistorySearchDialog::invalidateRecall(const QString &id) { recalledIds.insert(id); startSearch(true); }

void HistorySearchDialog::invalidateEdit(const QJsonObject &notice) {
    const QString id=notice.value("event_id").toString();
    if(recalledIds.contains(id) || notice.value("revision").toDouble()<=editOverlay.value(id).value("revision").toDouble()) return;
    editOverlay.insert(id,notice);startSearch(true);
}

void HistorySearchDialog::updateReceipt(const QJsonObject &notice) {
    const auto partner=notice.value("sender").toString().compare(owner,Qt::CaseInsensitive)==0?notice.value("to").toString():notice.value("sender").toString();
    if(peer.isEmpty() || peer.compare(partner,Qt::CaseInsensitive)!=0) return;
    const auto id=notice.value("event_id").toString();if(notice.value("revision").toDouble()<=receiptOverlay.value(id).value("revision").toDouble()) return;
    receiptOverlay.insert(id,notice);for(auto &hit:hits) if(hit.packet.value("_event_id").toString()==id) hit.packet=HistoryProtocol::withReceipt(hit.packet,notice);
    view->receipt(notice);
}
