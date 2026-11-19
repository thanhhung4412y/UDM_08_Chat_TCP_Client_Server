#pragma once
#include "connectionprofile.h"
#include "jsonchannel.h"
#include <QObject>
#include <QTimer>
#include <QSslSocket>
#include <functional>

class SessionConnection : public QObject {
    Q_OBJECT
public:
    enum State { Idle, Connecting, Registering, Ready };
    Q_ENUM(State)
    SessionConnection(QSslSocket *socket, JsonChannel *channel, ConnectionProfile profile,
                      QObject *parent = nullptr, int timeoutMs = 30000);
    void setTrustPrompt(std::function<bool(const QString &, const QString &)> prompt) { trustPrompt = std::move(prompt); }
    void setEmailCodePrompt(std::function<QString()> prompt) {emailCodePrompt=std::move(prompt);}
    ConnectionProfile connectionProfile() const {return profile;}
    void setProfile(ConnectionProfile value) {if(current==Idle)profile=std::move(value);}
    State state() const { return current; }
    void connectNow();
    void stop();
    bool createRecovery(const QString &currentPassword);
    bool changePassword(const QString &currentPassword, const QString &newPassword);
signals:
    void changed(SessionConnection::State state, const QString &text);
    void failed(const QString &text);
    void passwordChanged();
    void passwordReset();
    void accountSetupFinished(const QString &text);
    void recoveryCreated(const QString &code);
    void recoveryFailed(const QString &text);
    void passwordChangeFailed(const QString &text);
private:
    QSslSocket *socket;
    JsonChannel *channel;
    ConnectionProfile profile;
    QTimer timer;
    QTimer passwordTimer, recoveryTimer;
    QString recoveryRequest, resetRequest;
    QString passwordRequest;
    State current = Idle;
    std::function<bool(const QString &, const QString &)> trustPrompt;
    std::function<QString()> emailCodePrompt;
    int authTimeoutMs=30000;
    QString verifiedFingerprint;
    QString pinKey() const;
    bool verifyPeer();
    void fail(const QString &reason);
};
