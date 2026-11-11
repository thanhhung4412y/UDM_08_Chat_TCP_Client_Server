#include "authservice.h"
#include "authprotocol.h"
#include <QRunnable>
#include <QMetaObject>

AuthService::AuthService(QString file, QObject *parent) : QObject(parent), path(std::move(file)) { pool.setMaxThreadCount(2); }
AuthService::~AuthService() { pool.waitForDone(); }
bool AuthService::initialize(QString &error) {
    verifier = std::make_unique<AccountStore>(); return verifier->open(path, error);
}
bool AuthService::isCurrent(const AccountResult &account) const { return verifier && verifier->isCurrent(account); }
void AuthService::disconnected(QTcpSocket *client) { pending.remove(client); }
void AuthService::request(QTcpSocket *client, const QString &action, const QString &name, QString password, QString newPassword, QString email, QByteArray credential) {
    const bool changing = action == "change_password";
    const bool setup = action == "recovery_setup", reset = action == "recovery_reset" || action == "email_reset";
    const bool emailReset=action=="email_reset", bind=action=="email_bind";
    auto reject = [&](const QString &text) {
        if (setup || reset) emit recoveryFinished(client, {false, {}, {}, text}, reset);
        else if (changing) emit passwordFinished(client, {false, {}, {}, text});
        else if(bind) emit emailBound(client,{false, {}, {}, text});
        else emit finished(client, {false, {}, {}, text});
    };
    if (pending.contains(client)) return;
    if (action != "login" && action != "register" && !changing && !setup && !reset && !bind) { reject("Thao tac tai khoan khong hop le."); return; }
    if (!AuthProtocol::validName(name) || !(emailReset ? credential.size()==32 : (reset ? AuthProtocol::validRecovery(password) : AuthProtocol::validPassword(password)))) { reject("Ten 1-20 ky tu; mat khau 12-128 ky tu."); return; }
    if ((changing || reset) && (!AuthProtocol::validPassword(newPassword) || newPassword == password)) {
        reject("Mat khau moi phai co 12-128 ky tu va khac mat khau cu."); return;
    }
    const QString address = client->peerAddress().toString();
    for (auto i = limits.begin(); i != limits.end();) { if (i->window.hasExpired(60000)) i = limits.erase(i); else ++i; }
    if (!limits.contains(address) && limits.size() >= 4096) { reject("Server dang ban, thu lai sau."); return; }
    auto &limit = limits[address]; if (!limit.window.isValid()) limit.window.start();
    if (++limit.attempts > 10) { reject("Qua nhieu lan dang nhap/dang ky. Hay doi 60 giay."); return; }
    if (active >= 8) { reject("Server dang xu ly tai khoan khac. Hay thu lai sau."); return; }
    ++active; pending.insert(client); QPointer<QTcpSocket> guard(client);
    pool.start(QRunnable::create([this, guard, action, name, changing, setup, reset, emailReset, bind, email, credential, password = std::move(password), newPassword = std::move(newPassword)]() mutable {
        AccountResult result; QString error;
        { AccountStore store;
          if (!store.open(path, error)) result.error = "Khong mo duoc database tai khoan Server.";
          else if (emailReset) result = store.resetEmail(name,email,newPassword,credential);
          else if (bind) result = store.bindEmail(name,email,password);
          else if (setup) result = store.createRecovery(name, password);
          else if (reset) result = store.resetPassword(name, password, newPassword);
          else if (changing) result = store.changePassword(name, password, newPassword);
          else result = action == "register" ? store.create(name, password, email) : store.login(name, password); }
        password.fill(QChar(0)); password.clear();
        newPassword.fill(QChar(0)); newPassword.clear();
        QMetaObject::invokeMethod(this, [this, guard, result, changing, setup, reset, bind] {
            --active;
            const bool live = guard && pending.remove(guard.data());
            if ((setup || reset) && (live || (reset && result.ok))) emit recoveryFinished(guard.data(), result, reset);
            else if (changing && (result.ok || live)) emit passwordFinished(guard.data(), result);
            else if (bind && live) emit emailBound(guard.data(),result);
            else if (live) emit finished(guard.data(), result);
        }, Qt::QueuedConnection);
    }));
}

bool AuthService::setAvatar(const QString &name, const QByteArray &bytes) { return verifier && verifier->setAvatar(name, bytes); }
QByteArray AuthService::avatar(const QString &name) { return verifier ? verifier->avatar(name) : QByteArray(); }
