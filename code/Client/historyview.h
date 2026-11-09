#pragma once
#include "historystore.h"
#include <QObject>
#include <QHash>
#include <QUrl>
class QTextBrowser;

class HistoryView : public QObject {
    Q_OBJECT
public:
    HistoryView(QTextBrowser *browser, QString owner, QObject *parent = nullptr);
    void replace(const QVector<HistoryRecord> &records);
    void append(const HistoryRecord &record);
    QString videoPath(const QUrl &link, QString &error) const;
signals:
    void linkError(const QString &text);
private:
    QTextBrowser *browser;
    QString owner;
    QVector<HistoryRecord> visible;
    QHash<QUrl, QString> videoLinks;
    void render(const HistoryRecord &record);
};
