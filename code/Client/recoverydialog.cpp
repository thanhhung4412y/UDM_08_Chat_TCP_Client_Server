#include "recoverydialog.h"
#include "sessionconnection.h"
#include "passwordvisibility.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
RecoveryDialog::RecoveryDialog(SessionConnection *connection, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Mã dự phòng · ChatTCP"); resize(550,340);
    auto *layout=new QVBoxLayout(this);
    auto *hint=new QLabel("Tạo mã khi còn đăng nhập để dùng lúc quên mật khẩu. Mỗi lần tạo sẽ thay mã cũ. Mã dùng một lần, không gửi cho người khác; cất riêng ngoài máy chat. Đổi mật khẩu cũng làm mã cũ hết hiệu lực.");
    hint->setWordWrap(true); layout->addWidget(hint);
    auto *password=new QLineEdit(); password->setObjectName("recoveryCurrentPassword"); password->setEchoMode(QLineEdit::Password); password->setMaxLength(128); password->setPlaceholderText("Mật khẩu hiện tại"); addPasswordVisibility(password); layout->addWidget(password);
    auto *status=new QLabel(); status->setWordWrap(true); status->setTextFormat(Qt::PlainText); layout->addWidget(status);
    auto *code=new QLineEdit(); code->setObjectName("generatedRecoveryCode"); code->setReadOnly(true); code->hide(); layout->addWidget(code);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Close); layout->addWidget(buttons);
    auto *submit=buttons->button(QDialogButtonBox::Ok); submit->setText("Tạo mã mới"); submit->setObjectName("generateRecovery");
    auto *close=buttons->button(QDialogButtonBox::Close); close->setText("Đóng");
    connect(buttons,&QDialogButtonBox::rejected,this,&RecoveryDialog::reject);
    connect(submit,&QPushButton::clicked,this,[=] {
        busy=true; submit->setEnabled(false); close->setEnabled(false); password->setEnabled(false);
        status->setText("Đang tạo mã…"); code->clear(); code->hide(); connection->createRecovery(password->text());
    });
    connect(connection,&SessionConnection::recoveryFailed,this,[=](const QString &text) {
        busy=false; submit->setEnabled(true); close->setEnabled(true); password->setEnabled(true); status->setText(text);
    });
    connect(connection,&SessionConnection::recoveryCreated,this,[=](const QString &value) {
        busy=false; close->setEnabled(true); password->clear(); code->setText(value); code->show(); code->selectAll();
        status->setText("Đã tạo mã. Sao chép và cất ở nơi riêng trước khi đóng. Server chỉ giữ bản băm, không xem lại được mã này.");
    });
}
void RecoveryDialog::reject() { if (!busy) QDialog::reject(); }
