#pragma once
#include <QString>
#include <QSqlDatabase>
#include <QByteArray>

struct AccountResult {
    bool ok = false;
    QString name, id, error;
    QByteArray credentialToken; // Internal snapshot, never sent on the wire.
    QString recoveryCode; // Only a newly generated code, delivered once to its requester.
};
class AccountStore {
public:
    ~AccountStore();
    bool open(const QString &path, QString &error);
    AccountResult create(const QString &name, const QString &password, const QString &email = {});
    AccountResult login(const QString &name, const QString &password);
    AccountResult changePassword(const QString &name, const QString &currentPassword, const QString &newPassword);
    AccountResult createRecovery(const QString &name, const QString &password);
    AccountResult resetPassword(const QString &name, const QString &code, const QString &password);
    QByteArray emailCredential(const QString &name, const QString &email);
    AccountResult resetEmail(const QString &name, const QString &email, const QString &password, const QByteArray &credential);
    AccountResult bindEmail(const QString &name, const QString &email, const QString &password);
    bool setAvatar(const QString &name, const QByteArray &avatar);
    QByteArray avatar(const QString &name);
    bool isCurrent(const AccountResult &account);
    static QByteArray salt();
    static QByteArray derive(const QString &password, const QByteArray &salt, int iterations = 600000);
private:
    QSqlDatabase db;
    QString connection;
};
