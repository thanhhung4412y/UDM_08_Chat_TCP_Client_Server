#pragma once
#include "chatserver.h"
#include "sessionconnection.h"
#include "tlsidentity.h"
#include <QTemporaryDir>
#include <QDir>
#include <QSslSocket>

inline const QString TestPassword = "Only-test-password-123!";
inline QHash<quint16, QString> FixtureFingerprints;
inline QHash<quint16, QString> FixtureDatabases;
class TestServer : public ChatServer {
public:
    QTemporaryDir data{QDir::currentPath() + "/server-test-XXXXXX"};
    bool start(quint16 port = 0) {
        const bool ok = data.isValid() && ChatServer::start(port, data.path());
        if (ok) {FixtureFingerprints.insert(this->port(), certificateFingerprint());FixtureDatabases.insert(this->port(),QDir(data.path()).filePath("accounts.sqlite"));}
        return ok;
    }
    ~TestServer() override { shutdown(); }
};
inline void trustFixture(SessionConnection &connection, const ChatServer &server) {
    // Non-email regression suites provision local accounts directly; production registration is tested in email_tests.
    auto profile=connection.connectionProfile();
    if(profile.registerAccount) {
        AccountStore store;QString error;
        if(store.open(QDir(server.dataPath()).filePath("accounts.sqlite"),error))store.create(profile.username,profile.password);
        profile.registerAccount=false;connection.setProfile(profile);
    }
    const QString expected = server.certificateFingerprint();
    connection.setTrustPrompt([expected](const QString &actual, const QString &) { return actual == expected; });
}
