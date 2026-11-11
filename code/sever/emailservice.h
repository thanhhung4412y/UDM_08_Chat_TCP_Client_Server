#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QHash>
#include <QThreadPool>
#include <QPointer>
#include <functional>
class EmailService : public QObject {
    Q_OBJECT
public:
    explicit EmailService(QString database,QObject *parent=nullptr);
    ~EmailService() override;
    void begin(QTcpSocket *client,const QJsonObject &packet);
    void confirm(QTcpSocket *client,const QJsonObject &packet);
    void disconnected(QTcpSocket *client);
    // Dependency injection for local tests; production uses certificate-verified Gmail TLS.
    using Sender=std::function<bool(const QString &,const QString &,const QString &)>;
    void setClockForTests(std::function<qint64()> value) {clock=std::move(value);}
    void setSenderForTests(Sender value) { sender=std::move(value); }
signals:
    void reply(QTcpSocket *client,const QJsonObject &packet);
    void verified(QTcpSocket *client,const QString &purpose,const QString &name,const QString &email,const QString &password,const QByteArray &credential);
private:
    struct Challenge {QString id,name,email,purpose;QByteArray digest,credential; qint64 expires=0;int attempts=0;bool ready=false,eligible=true;};
    struct Limit {qint64 start=0,last=0;int count=0;};
    QHash<QTcpSocket*,Challenge> challenges;
    QHash<QString,Limit> limits;
    QThreadPool pool;int active=0,total=0;qint64 window=0;
    QString path;Sender sender;std::function<qint64()> clock;
    bool allow(const QString &key,int maximum,qint64 now);
};
