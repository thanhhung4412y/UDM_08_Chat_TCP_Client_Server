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
    State state() const { return current; }
    void connectNow();
    void stop();
signals:
    void changed(SessionConnection::State state, const QString &text);
    void failed(const QString &text);
private:
    QSslSocket *socket;
    JsonChannel *channel;
    ConnectionProfile profile;
    QTimer timer;
    State current = Idle;
    std::function<bool(const QString &, const QString &)> trustPrompt;
    QString verifiedFingerprint;
    QString pinKey() const;
    bool verifyPeer();
    void fail(const QString &reason);
};
