#pragma once
#include "historystore.h"
#include <QDialog>
#include <QThreadPool>
#include <QSet>
class QLineEdit;
class QListWidget;
class QLabel;
class QPushButton;
class QTextBrowser;
class QTimer;
class HistoryView;
class HistorySearchDialog : public QDialog {
    Q_OBJECT
public:
    HistorySearchDialog(QString path, QString owner, QString peer, QWidget *parent=nullptr);
    ~HistorySearchDialog() override;
    void invalidateRecall(const QString &id);
    void invalidateEdit(const QJsonObject &notice);
    void updateReceipt(const QJsonObject &notice);
signals:
    void editRequested(const QJsonObject &packet);
    void recallRequested(const QString &id);
private:
    QString path, owner, peer, submitted;
    QSet<QString> recalledIds;
    QHash<QString,QJsonObject> editOverlay,receiptOverlay;
    QLineEdit *input;
    QListWidget *results;
    QLabel *status, *contextStatus;
    QPushButton *older, *newer;
    QTextBrowser *browser;
    HistoryView *view;
    QTimer *debounce;
    QThreadPool pool;
    std::shared_ptr<std::atomic_bool> searchCancel, contextCancel;
    quint64 searchVersion=0, contextVersion=0;
    QVector<HistoryRecord> hits;
    QVector<qint64> cursors{0};
    void startSearch(bool reset);
    void openResult(int row);
};
