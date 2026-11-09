#include "historyview.h"
#include "historyimage.h"
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QImage>
#include <QTextBrowser>
#include <QTextDocument>

HistoryView::HistoryView(QTextBrowser *b, QString name, QObject *parent)
    : QObject(parent), browser(b), owner(std::move(name)) {
    browser->setOpenLinks(false);
    browser->setOpenExternalLinks(false);
    connect(browser, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
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
    visible = records;
    if (visible.size() > 150) visible = visible.sliced(visible.size() - 150);
    videoLinks.clear();
    // Replacing the owned document also releases cached image resources.
    browser->setDocument(new QTextDocument(browser));
    browser->document()->setMaximumBlockCount(1000);
    for (const auto &record : visible) render(record);
}
void HistoryView::append(const HistoryRecord &record) {
    visible.append(record);
    if (visible.size() > 150) {
        const auto keep = visible.sliced(visible.size() - 100);
        replace(keep);
    } else render(record);
}
void HistoryView::render(const HistoryRecord &record) {
    const auto &p = record.packet;
    const QString type = p.value("type").toString();
    const QString sender = p.value("sender").toString();
    const QString receiver = p.value("to").toString();
    const QString when = QDateTime::fromString(p.value("_utc").toString(), Qt::ISODateWithMs)
        .toLocalTime().toString("dd/MM/yyyy HH:mm:ss");
    const QString time = "<span style='color:#777'>[" + when.toHtmlEscaped() + "]</span> ";
    const QString route = receiver.isEmpty() ? "[Phong chung] " : "[Rieng " + sender + " → " + receiver + "] ";
    const QString displaySender = sender.compare(owner, Qt::CaseInsensitive) == 0 ? "Ban (" + sender + ")" : sender;
    const QString title = "<b>" + route.toHtmlEscaped() + displaySender.toHtmlEscaped() + ":</b> ";
    const QString text = p.value("text").toString().toHtmlEscaped();
    if (type == "chat" || type == "private") {
        browser->append(time + title + text);
    } else if (type == "reply") {
        browser->append(time + title + "<span style='color:#777'>↪ Reply: "
                        + p.value("quote").toString().toHtmlEscaped() + "</span><br>" + text);
    } else if (type == "forward") {
        browser->append(time + title + "<span style='color:#7c3aed'>[Forward]</span> " + text);
    } else if (type == "sticker") {
        const QString path = ":/stickers/" + p.value("id").toString() + ".jpg";
        QImage image(path);
        if (image.isNull()) browser->append(time + title + "[Sticker: " + p.value("id").toString().toHtmlEscaped() + "]");
        else {
            browser->document()->addResource(QTextDocument::ImageResource, QUrl(path), image);
            browser->append(time + title + "Sticker<div><img src='" + path + "' width='120'></div>");
        }
    } else if (type == "image") {
        const QImage image = historyImagePreview(record.image);
        const QString url = "history-image:/" + p.value("_event_id").toString();
        if (image.isNull()) browser->append(time + title + "[Anh lich su bi loi]");
        else {
            browser->document()->addResource(QTextDocument::ImageResource, QUrl(url), image);
            browser->append(time + title + "Anh<div><img src='" + url + "' width='" + QString::number(image.width()) + "'></div>");
        }
    } else if (type == "video") {
        const QUrl url("chatvideo:/" + p.value("_event_id").toString());
        videoLinks.insert(url, p.value("local_path").toString());
        const QString label = p.value("name").toString() + " - Mo video";
        browser->append(time + title + "Video <a href=\"" + url.toString(QUrl::FullyEncoded).toHtmlEscaped()
                        + "\">" + label.toHtmlEscaped() + "</a>");
    }
}
