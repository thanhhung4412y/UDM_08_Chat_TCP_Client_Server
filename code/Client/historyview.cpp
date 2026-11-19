#include "historyview.h"
#include "historyimage.h"
#include "historyprotocol.h"
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QImage>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextOption>
#include <QTextCursor>
#include <QColor>
#include <QMenu>
#include <QAction>
#include <QScrollBar>
#include <QMessageBox>

HistoryView::HistoryView(QTextBrowser *b, QString name, QObject *parent)
    : QObject(parent), browser(b), owner(std::move(name)) {
    browser->setOpenLinks(false);
    browser->setOpenExternalLinks(false);
    connect(browser, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        if (url.scheme() == "group-video" || url.scheme() == "group-message") return;
        QString error;
        const QString path = videoPath(url, error);
        if (!path.isEmpty() && !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            error = "Khong mo duoc video bang trinh phat mac dinh";
        if (!error.isEmpty()) emit linkError(error);
    });
}
QString HistoryView::videoPath(const QUrl &link, QString &error) const {
    error.clear();
    if (!videoLinks.contains(link)) { error = "Lien ket khong thuoc video trong lich su"; return {}; }
    const QString path = videoLinks.value(link);
    if (!QFileInfo(path).isFile()) {
        error = "Video da bi xoa hoac di chuyen. Lich su chi luu thong tin va duong dan tep.";
        return {};
    }
    return path;
}
void HistoryView::replace(const QVector<HistoryRecord> &records) {
    focusedSequence=0;
    visible = records;
    if (visible.size() > 150) visible = visible.sliced(visible.size() - 150);
    videoLinks.clear(); eventPositions.clear(); recordPositions.clear(); browser->setExtraSelections({});
    lastDay.clear(); placeholder = visible.isEmpty();
    // Replacing the owned document also releases cached image resources.
    browser->setDocument(new QTextDocument(browser));
    browser->document()->setMaximumBlockCount(1000);
    auto option = browser->document()->defaultTextOption(); option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); browser->document()->setDefaultTextOption(option);
    if (placeholder) browser->setHtml("<p align='center' style='color:#81949f;margin-top:70px'>Chưa có tin nhắn.<br>Gửi lời chào để bắt đầu cuộc trò chuyện.</p>");
    for (const auto &record : visible) render(record);
}
void HistoryView::append(const HistoryRecord &record) {
    if (placeholder) { replace({record}); return; }
    visible.append(record);
    if (visible.size() > 150) {
        const auto keep = visible.sliced(visible.size() - 100);
        replace(keep);
    } else render(record);
}
void HistoryView::render(const HistoryRecord &record) {
    QTextCursor startCursor(browser->document()); startCursor.movePosition(QTextCursor::End); startCursor.setKeepPositionOnInsert(true);
    const int start=startCursor.position();
    const auto &p = record.packet;
    const QString type = p.value("type").toString();
    const QString sender = p.value("sender").toString(), receiver = p.value("to").toString();
    const bool mine = sender.compare(owner, Qt::CaseInsensitive) == 0;
    const auto time = QDateTime::fromString(p.value("_utc").toString(), Qt::ISODateWithMs).toLocalTime();
    const QString day = time.date().toString(Qt::ISODate);
    if (lastDay != day) {
        lastDay = day;
        const QString date = time.date() == QDate::currentDate() ? "Hôm nay" : time.date().toString("dd/MM/yyyy");
        browser->append("<p align='center' style='font-size:11px;color:#7c909d;margin:14px 0'>" + date + "</p>");
    }
    QString body;
    const QString text = p.value("text").toString().toHtmlEscaped().replace("\n", "<br>");
    if(p.value("_recalled").toBool()) body="<i style='color:#7c909a'>Tin nhắn đã được thu hồi</i>";
    else if (type == "chat" || type == "private") body = text;
    else if (type == "reply") {
        body = "<table width='100%' cellpadding='8' cellspacing='0' bgcolor='#eef4f6'><tr><td>"
            "<span style='font-size:11px;color:#527987'>TRẢ LỜI</span><br>"
            + p.value("quote").toString().toHtmlEscaped().replace("\n", "<br>") + "</td></tr></table><br>" + text;
    } else if (type == "forward") body = "<span style='color:#6e8590;font-size:11px'>↪ Đã chuyển tiếp</span><br>" + text;
    else if (type == "sticker") {
        const QString path = ":/stickers/" + p.value("id").toString() + ".jpg";
        QImage image(path);
        if (image.isNull()) body = "[Sticker: " + p.value("id").toString().toHtmlEscaped() + "]";
        else {
            browser->document()->addResource(QTextDocument::ImageResource, QUrl(path), image);
            body = "<img src='" + path + "' width='120'>";
        }
    } else if (type == "image") {
        const QImage image = historyImagePreview(record.image);
        const QString url = "history-image:/" + p.value("_event_id").toString();
        if (image.isNull()) body = "[Không đọc được ảnh trong lịch sử]";
        else {
            browser->document()->addResource(QTextDocument::ImageResource, QUrl(url), image);
            body = "<img src='" + url + "' width='" + QString::number(image.width()) + "'>";
        }
    } else if (type == "video") {
        const QUrl url("chatvideo:/" + p.value("_event_id").toString());
        videoLinks.insert(url, p.value("local_path").toString());
        body = "<span style='font-size:11px;color:#547c80'>VIDEO MP4</span><br><b>"
            + p.value("name").toString().toHtmlEscaped() + "</b><br>"
            + QString::number(p.value("size").toDouble() / (1024 * 1024), 'f', 1) + " MiB<br>"
            + "<a style='color:#0f766e' href=\"" + url.toString(QUrl::FullyEncoded).toHtmlEscaped() + "\">▶ Mở video</a>";
    }
    const QString route = receiver.isEmpty() ? "Phòng chung" : "Riêng: " + sender + " → " + receiver;
    const QString author = mine ? "Bạn" : sender;
    QString delivery;
    if(mine && !receiver.isEmpty() && !p.value("_recalled").toBool()) {
        const int state=p.value("_receipt_state").toInt();delivery=state==3?" · Đã xem":state==2?" · Đã nhận":state==1?" · Đã gửi":"";
    }
    const QString card = "<table width='78%' align='" + QString(mine ? "right" : "left")
        + "' cellpadding='12' cellspacing='0' bgcolor='" + QString(mine ? "#def3eb" : "#ffffff")
        + "'><tr><td><span style='font-size:11px;font-weight:600;color:#14786c'>" + author.toHtmlEscaped()
        + "</span><p style='font-size:14px;color:#23424f;margin:6px 0'>" + body
        + "</p><span style='font-size:10px;color:#7c909a'>" + time.toString("HH:mm") + (p.value("_edit_revision").toDouble()>0 ? QString(" · Đã chỉnh sửa") : QString()) + " · " + route.toHtmlEscaped() + delivery
        + "</span></td></tr></table><p style='margin:0;font-size:5px'>&nbsp;</p>";
    browser->append(card);
    QTextCursor endCursor(browser->document()); endCursor.movePosition(QTextCursor::End); endCursor.setKeepPositionOnInsert(true);
    eventPositions.insert(p.value("_event_id").toString(),{startCursor,endCursor});
    if(record.sequence>0) recordPositions.insert(record.sequence,{start,browser->document()->characterCount()-1});
}

bool HistoryView::focusRecord(qint64 sequence) {
    if(!recordPositions.contains(sequence)) return false;
    focusedSequence=sequence;
    const auto range=recordPositions.value(sequence);
    QTextCursor cursor(browser->document()); cursor.setPosition(range.first); cursor.setPosition(range.second,QTextCursor::KeepAnchor);
    QTextEdit::ExtraSelection selection; selection.cursor=cursor; selection.format.setBackground(QColor("#fff1b8"));
    browser->setExtraSelections({selection}); cursor.clearSelection(); cursor.setPosition(range.first);
    browser->setTextCursor(cursor); browser->ensureCursorVisible(); return true;
}

QJsonObject HistoryView::packetAt(int position) const {
    for(auto i=visible.crbegin();i!=visible.crend();++i) {
        const auto found=eventPositions.constFind(i->packet.value("_event_id").toString());
        if(found==eventPositions.cend()) continue;
        const auto &range=found.value();
        if(position>=range.first.position() && position<range.second.position()) return i->packet;
    }
    return {};
}
void HistoryView::recall(const QString &id) {
    auto records=visible; bool changed=false;
    for(auto &record:records) if(record.packet.value("_event_id").toString()==id) { record.packet=HistoryProtocol::tombstone(record.packet);record.image.clear();changed=true; }
    if(changed) replace(records);
}

void HistoryView::enableRecallMenu() {
    browser->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(browser,&QTextBrowser::customContextMenuRequested,this,[this](const QPoint &point) {
        auto *menu=browser->createStandardContextMenu(); menu->addSeparator();
        const auto packet=packetAt(browser->cursorForPosition(point).position());
        auto *action=menu->addAction("Thu hồi tin nhắn của tôi");
        action->setEnabled(!packet.isEmpty() && !packet.value("_recalled").toBool() && owner.compare(packet.value("sender").toString(),Qt::CaseInsensitive)==0);
        connect(action,&QAction::triggered,menu,[this,packet] {
            if(QMessageBox::question(browser,"Thu hồi tin nhắn","Thu hồi tin này? Bản sao hoặc tệp đã tải riêng có thể vẫn còn.",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes)
                emit recallRequested(packet.value("_event_id").toString());
        });
        auto *editAction=menu->addAction("Chỉnh sửa tin nhắn của tôi");
        editAction->setEnabled(action->isEnabled() && HistoryProtocol::editableType(packet.value("type").toString()));
        connect(editAction,&QAction::triggered,menu,[this,packet]{emit editRequested(packet);});
        menu->exec(browser->mapToGlobal(point));delete menu;
    });
}

void HistoryView::edit(const QJsonObject &notice) {
    auto records=visible;bool changed=false;
    for(auto &record:records) if(record.packet.value("_event_id")==notice.value("event_id")) {
        const auto packet=HistoryProtocol::withEdit(record.packet,notice);if(packet!=record.packet) {record.packet=packet;changed=true;}
    }
    if(changed) replace(records);
}

void HistoryView::receipt(const QJsonObject &notice) {
    auto records=visible;bool changed=false,redraw=false;
    for(auto &record:records) if(record.packet.value("_event_id")==notice.value("event_id")) {
        const auto p=HistoryProtocol::withReceipt(record.packet,notice);if(p!=record.packet) {record.packet=p;changed=true;redraw|=owner.compare(p.value("sender").toString(),Qt::CaseInsensitive)==0;}
    }
    if(!changed) return;
    if(!redraw) {visible=records;return;}
    const int scroll=browser->verticalScrollBar()->value();const bool bottom=scroll>=browser->verticalScrollBar()->maximum()-2;const auto focus=focusedSequence;
    replace(records);if(focus>0) focusRecord(focus);browser->verticalScrollBar()->setValue(bottom?browser->verticalScrollBar()->maximum():scroll);
}
