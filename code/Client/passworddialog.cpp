#include "passworddialog.h"
#include "sessionconnection.h"
#include "authprotocol.h"
#include "passwordvisibility.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

PasswordDialog::PasswordDialog(SessionConnection *connection, std::function<bool()> canSubmit, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Đổi mật khẩu · ChatTCP"); resize(500, 360);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 22); layout->setSpacing(14);
    auto *hint = new QLabel("Đổi thành công sẽ đăng xuất. Hãy đăng nhập lại bằng mật khẩu mới. Lịch sử vẫn được giữ nguyên.");
    hint->setWordWrap(true); layout->addWidget(hint);
    auto *form = new QFormLayout(); layout->addLayout(form);
    currentPassword = new QLineEdit(); newPassword = new QLineEdit(); confirmation = new QLineEdit();
    currentPassword->setObjectName("currentPassword"); newPassword->setObjectName("newPassword"); confirmation->setObjectName("confirmNewPassword");
    for (auto *field : {currentPassword, newPassword, confirmation}) { field->setEchoMode(QLineEdit::Password); field->setMaxLength(128); addPasswordVisibility(field); }
    newPassword->setPlaceholderText("8-128 ky tu; khac mat khau cu");
    form->addRow("Mật khẩu hiện tại", currentPassword); form->addRow("Mật khẩu mới", newPassword); form->addRow("Nhập lại mật khẩu mới", confirmation);
    status = new QLabel(); status->setObjectName("passwordStatus"); status->setWordWrap(true); status->setTextFormat(Qt::PlainText); layout->addWidget(status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout->addWidget(buttons);
    submit = buttons->button(QDialogButtonBox::Ok); submit->setText("Đổi mật khẩu"); submit->setObjectName("submitPasswordChange");
    cancel = buttons->button(QDialogButtonBox::Cancel); cancel->setText("Hủy");
    connect(buttons, &QDialogButtonBox::rejected, this, &PasswordDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this, connection, canSubmit] {
        if (busy) return;
        if (!canSubmit()) { status->setText("Can dang nhap va cho video hoan tat/huy video truoc khi doi mat khau."); return; }
        if (!AuthProtocol::validPassword(currentPassword->text()) || !AuthProtocol::validPassword(newPassword->text())) {
            status->setText("Mat khau phai co 8-128 ky tu."); return;
        }
        if (newPassword->text() != confirmation->text()) { status->setText("Hai lan nhap mat khau moi chua khop."); return; }
        if (currentPassword->text() == newPassword->text()) { status->setText("Mat khau moi phai khac mat khau hien tai."); return; }
        setBusy(true); status->setText("Dang doi mat khau... Khong dong ung dung trong luc cho.");
        if (!connection->changePassword(currentPassword->text(), newPassword->text())) setBusy(false);
    });
    connect(connection, &SessionConnection::passwordChangeFailed, this, [this](const QString &text) { setBusy(false); status->setText(text); });
    connect(connection, &SessionConnection::passwordChanged, this, [this] {
        busy = false;
        for (auto *field : {currentPassword, newPassword, confirmation}) field->clear();
        accept();
    });
    currentPassword->setFocus();
}
void PasswordDialog::setBusy(bool value) {
    busy = value;
    for (auto *field : {currentPassword, newPassword, confirmation}) field->setEnabled(!value);
    submit->setEnabled(!value); cancel->setEnabled(!value);
}
void PasswordDialog::reject() { if (!busy) QDialog::reject(); }
