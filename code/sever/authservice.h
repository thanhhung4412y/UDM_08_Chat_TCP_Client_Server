#pragma once
#include "accountstore.h"
#include <QObject>
#include <QTcpSocket>
#include <QPointer>
#include <QHash>
#include <QElapsedTimer>
#include <QThreadPool>
#include <QSet>
#include <memory>

class AuthService : public QObject {
    Q_OBJECT
public:
    explicit AuthService(QString path, QObject *parent = nullptr);
    ~AuthService() override;
    bool initialize(QString &error);
    void request(QTcpSocket *client, const QString &action, const QString &name, QString password, QString newPassword = {}, QString email = {}, QByteArray credential = {});
    bool isCurrent(const AccountResult &account) const;
    bool setAvatar(const QString &name, const QByteArray &bytes);
    QByteArray avatar(const QString &name);
    void disconnected(QTcpSocket *client);
signals:
    void emailBound(QTcpSocket *client, const AccountResult &result);
    void finished(QTcpSocket *client, const AccountResult &result);
    void recoveryFinished(QTcpSocket *client, const AccountResult &result, bool reset);
    void passwordFinished(QTcpSocket *client, const AccountResult &result);
private:
    QString path;
    std::unique_ptr<AccountStore> verifier;
    QThreadPool pool;
    QSet<QTcpSocket *> pending;
    struct Limit { QElapsedTimer window; int attempts = 0; };
    QHash<QString, Limit> limits;
    int active = 0;
};
