#include "gmailconfig.h"
#include "authprotocol.h"
#include <QSettings>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {
const char *emailKey = "gmail/user";
const char *secretKey = "gmail/dpapi_password";
}

bool GmailConfig::load(QString &email, QByteArray &appPassword, QString *error) {
    email.clear();
    appPassword.clear();
    QSettings store;
    const auto storedEmail = AuthProtocol::emailKey(store.value(emailKey).toString());
    const QByteArray cipher = store.value(secretKey).toByteArray();
    if (storedEmail.isEmpty() || cipher.isEmpty()) {
        if (error) *error = "Chưa có cấu hình Gmail được lưu.";
        return false;
    }
#ifdef Q_OS_WIN
    DATA_BLOB input{DWORD(cipher.size()), reinterpret_cast<BYTE *>(const_cast<char *>(cipher.constData()))};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        if (error) *error = "Không thể giải mã mật khẩu ứng dụng với tài khoản Windows hiện tại.";
        return false;
    }
    appPassword = QByteArray(reinterpret_cast<const char *>(output.pbData), int(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    if (!AuthProtocol::validEmail(storedEmail) || appPassword.size() != 16) {
        appPassword.fill('\0'); appPassword.clear();
        if (error) *error = "Cấu hình Gmail đã lưu không hợp lệ.";
        return false;
    }
    email = storedEmail;
    return true;
#else
    if (error) *error = "Chỉ hỗ trợ lưu mật khẩu Gmail an toàn bằng Windows DPAPI.";
    return false;
#endif
}

bool GmailConfig::save(const QString &email, const QByteArray &appPassword, QString *error) {
    const auto normalized = AuthProtocol::emailKey(email);
    QByteArray secret = appPassword;
    secret.replace(" ", "");
    if (!AuthProtocol::validEmail(normalized) || secret.size() != 16) {
        secret.fill('\0');
        if (error) *error = "Nhập Gmail hợp lệ và Mật khẩu ứng dụng Google đủ 16 ký tự.";
        return false;
    }
#ifdef Q_OS_WIN
    DATA_BLOB input{DWORD(secret.size()), reinterpret_cast<BYTE *>(secret.data())};
    DATA_BLOB output{};
    const bool ok = CryptProtectData(&input, L"ChatTCP1 Gmail App Password", nullptr,
                                      nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
    secret.fill('\0');
    if (!ok) {
        if (error) *error = "Windows không bảo vệ được mật khẩu bằng DPAPI.";
        return false;
    }
    QByteArray encrypted(reinterpret_cast<const char *>(output.pbData), int(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    QSettings store;
    store.setValue(emailKey, normalized);
    store.setValue(secretKey, encrypted);
    store.sync();
    if (store.status() != QSettings::NoError) {
        if (error) *error = "Không lưu được cấu hình vào Windows Settings.";
        return false;
    }
    return true;
#else
    secret.fill('\0');
    if (error) *error = "Phiên bản giao diện này lưu mật khẩu an toàn trên Windows. Không lưu plaintext trên hệ khác.";
    return false;
#endif
}

bool GmailConfig::clear(QString *error) {
    QSettings store;
    store.remove(emailKey);
    store.remove(secretKey);
    store.sync();
    if (store.status() != QSettings::NoError) {
        if (error) *error = "Không xóa được cấu hình Gmail đã lưu.";
        return false;
    }
    return true;
}
