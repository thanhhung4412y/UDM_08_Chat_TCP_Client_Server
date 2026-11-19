#pragma once
#include "historystore.h"
#include <QObject>
#include <QHash>
#include <QUrl>
#include <QTextCursor>
class QTextBrowser;

class HistoryView : public QObject {
    Q_OBJECT
public:
    HistoryView(QTextBrowser *browser, QString owner, QObject *parent = nullptr);
    void replace(const QVector<HistoryRecord> &records);
    void append(const HistoryRecord &record);
    bool focusRecord(qint64 sequence);
    QJsonObject packetAt(int position) const;
    void recall(const QString &id);
    void enableRecallMenu();
    void edit(const QJsonObject &notice);
    void receipt(const QJsonObject &notice);
    QString videoPath(const QUrl &link, QString &error) const;
signals:
    void editRequested(const QJsonObject &packet);
    void recallRequested(const QString &id);
    void linkError(const QString &text);
private:
    QTextBrowser *browser;
    QString owner;
    QVector<HistoryRecord> visible;
    QHash<QUrl, QString> videoLinks;
    QString lastDay;
    QHash<QString,QPair<QTextCursor,QTextCursor>> eventPositions;
    QHash<qint64,QPair<int,int>> recordPositions;
    bool placeholder = false;
    qint64 focusedSequence=0;
    void render(const HistoryRecord &record);
};
