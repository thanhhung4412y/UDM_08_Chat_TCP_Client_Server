#include "chathistory.h"
#include "historyprotocol.h"
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
    const auto page = store.page(100, 0, error);
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
    if (!record.packet.isEmpty()) live.append(record);
    if (result == HistoryStore::SaveResult::Error)
        emit storageStatus("Khong luu duoc lich su: " + error);
}
void ChatHistory::showDialog(QWidget *parent) {
    if (!store.isOpen()) {
        QMessageBox::warning(parent, "Lich su", "Database lich su chua mo duoc. Xem thong bao SQLite trong cua so chat.");
        return;
    }
    QDialog dialog(parent);
    dialog.setWindowTitle("Lich su — " + owner + " @ " + server + ":" + QString::number(port));
    dialog.resize(850, 600);
    auto *layout = new QVBoxLayout(&dialog);
    auto *pathLabel = new QLabel("Database: " + store.path());
    pathLabel->setTextFormat(Qt::PlainText);
    pathLabel->setWordWrap(true);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(pathLabel);
    auto *browser = new QTextBrowser();
    layout->addWidget(browser);
    HistoryView view(browser, owner);
    auto *status = new QLabel(); status->setTextFormat(Qt::PlainText); layout->addWidget(status);
    auto *buttons = new QHBoxLayout(); layout->addLayout(buttons);
    auto *older = new QPushButton("Cu hon"); auto *newer = new QPushButton("Moi hon");
    auto *latest = new QPushButton("Moi nhat / Tai lai"); auto *close = new QPushButton("Dong");
    buttons->addWidget(older); buttons->addWidget(newer); buttons->addWidget(latest); buttons->addStretch(); buttons->addWidget(close);
    QVector<qint64> cursors{0};
    qint64 oldest = 0;
    auto load = [&] {
        QString error;
        const auto page = store.page(50, cursors.last(), error);
        view.replace(page.records);
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
