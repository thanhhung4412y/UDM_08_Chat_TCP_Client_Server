#include "connectiondialog.h"
#include "authprotocol.h"
#include "passwordvisibility.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QColor>

ConnectionDialog::ConnectionDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Tài khoản · ChatTCP");
    resize(510, 575);
    setMinimumWidth(450);
    setObjectName("LoginDialog");
    setStyleSheet(R"(
        QDialog#LoginDialog { background:#eff6f7; }
        QFrame#LoginCard { background:#ffffff; border:1px solid #dce8eb; border-radius:18px; }
        QLabel#LoginBrand { color:#08786d; font-size:29px; font-weight:800; }
        QLabel#LoginTitle { color:#153e47; font-size:17px; font-weight:700; }
        QLabel#LoginHint { color:#6b8992; font-size:11px; }
        QFrame#LoginCard QLineEdit,QFrame#LoginCard QComboBox,QFrame#LoginCard QSpinBox {
            background:white; border:1px solid #cfdfe3; border-radius:9px; padding:10px;min-height:19px;
        }
        QFrame#LoginCard QLineEdit:focus {border:2px solid #0d9488;}
        QPushButton#LoginContinue {background:#0f766e;color:white;border:0;border-radius:9px;
            padding:12px 18px;font-weight:700;min-height:20px;}
        QPushButton#LoginContinue:hover {background:#09645c;}
        QPushButton#LoginCancel {border:1px solid #d6e6e8;border-radius:9px;padding:12px 18px;}
    )");
    QSettings settings;
    const auto previous = ConnectionProfile::load(settings);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(25, 25, 25, 25); layout->setSpacing(0);
    auto *card=new QFrame(this);card->setObjectName("LoginCard");layout->addWidget(card);
    auto *cardLayout=new QVBoxLayout(card);
    cardLayout->setContentsMargins(30,26,30,25);cardLayout->setSpacing(13);
    auto *brand=new QLabel("✦  ChatTCP",card);brand->setObjectName("LoginBrand");cardLayout->addWidget(brand);
    auto *sub=new QLabel("Đăng nhập an toàn · Nhắn tin riêng tư",card);sub->setObjectName("LoginHint");cardLayout->addWidget(sub);
    auto *shadow=new QGraphicsDropShadowEffect(card);shadow->setBlurRadius(30);shadow->setOffset(0,7);shadow->setColor(QColor(30,65,70,25));card->setGraphicsEffect(shadow);
    auto *welcome = new QLabel("Chào mừng trở lại",card); welcome->setObjectName("LoginTitle"); cardLayout->addWidget(welcome);
    auto *hint = new QLabel("Cùng máy: 127.0.0.1. Máy khác: nhập IP hiển thị trên cửa sổ Server.");
    hint->setWordWrap(true);hint->setObjectName("LoginHint"); cardLayout->addWidget(hint);
    auto *form = new QFormLayout();form->setSpacing(11);cardLayout->addLayout(form);
    mode = new QComboBox(); mode->setObjectName("authMode");
    mode->addItem("Đăng nhập"); mode->addItem("Đăng ký tài khoản mới"); mode->addItem("Quên mật khẩu (dùng mã dự phòng)"); mode->addItem("Quên mật khẩu qua email"); mode->addItem("Liên kết email cho tài khoản cũ");
    form->addRow("Thao tác", mode);
    username = new QLineEdit(previous.username); username->setObjectName("usernameInput");
    host = new QLineEdit(previous.host); host->setObjectName("hostInput");
    host->setPlaceholderText("192.168.1.10 hoac ten may Server");
    port = new QSpinBox(); port->setObjectName("portInput"); port->setRange(1, 65535); port->setValue(previous.port);
    form->addRow("Tên tài khoản", username); form->addRow("Máy chủ", host); form->addRow("Cổng", port);
    email=new QLineEdit();email->setObjectName("emailInput");email->setMaxLength(254);email->setPlaceholderText("Email bạn đang sử dụng");
    form->addRow("Email",email);email->hide();auto *emailLabel=form->labelForField(email);emailLabel->hide();
    password = new QLineEdit(); password->setObjectName("passwordInput"); password->setEchoMode(QLineEdit::Password);
    password->setMaxLength(128); password->setPlaceholderText("8–128 ký tự, không lưu trên máy");
    confirmation = new QLineEdit(); confirmation->setObjectName("confirmPasswordInput");
    confirmation->setEchoMode(QLineEdit::Password); confirmation->setMaxLength(128);
    form->addRow("Mật khẩu", password); form->addRow("Nhập lại mật khẩu", confirmation);
    addPasswordVisibility(password); addPasswordVisibility(confirmation);
    recoveryCode = new QLineEdit(); recoveryCode->setObjectName("recoveryCodeInput"); recoveryCode->setMaxLength(64); recoveryCode->setEchoMode(QLineEdit::Password); addPasswordVisibility(recoveryCode);
    recoveryCode->setPlaceholderText("Mã dự phòng 64 ký tự đã lưu trước đó");
    form->addRow("Mã dự phòng",recoveryCode); recoveryCode->hide();
    auto *recoveryLabel = form->labelForField(recoveryCode); recoveryLabel->hide();
    auto *confirmationLabel = form->labelForField(confirmation);
    confirmation->hide(); confirmationLabel->hide();
    connect(mode, &QComboBox::currentIndexChanged, this, [=](int index) {
        confirmation->setVisible(index != 0 && index!=4); confirmationLabel->setVisible(index != 0 && index!=4);
        email->setVisible(index==1||index==3||index==4);emailLabel->setVisible(email->isVisible());
        recoveryCode->setVisible(index==2); recoveryLabel->setVisible(index==2);
        welcome->setText(index==0?"Chào mừng trở lại":index==1?"Tạo tài khoản ChatTCP":index==2?"Khôi phục bằng mã dự phòng":index==3?"Đặt lại mật khẩu qua Email":"Liên kết Email");
        password->clear(); confirmation->clear(); recoveryCode->clear();
        for (auto *field : {password,confirmation,recoveryCode})
            if(auto *toggle=field->findChild<QAction *>("toggle_"+field->objectName())) toggle->setChecked(false);
        password->setPlaceholderText((index==2||index==3) ? "Mật khẩu mới: 8–128 ký tự" : "8–128 ký tự, không lưu trên máy");
    });
    error = new QLabel(); error->setObjectName("connectionError");
    error->setTextFormat(Qt::PlainText); error->setWordWrap(true); error->setStyleSheet("color:#b91c1c"); cardLayout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Tiếp tục  →");buttons->button(QDialogButtonBox::Ok)->setObjectName("LoginContinue");buttons->button(QDialogButtonBox::Cancel)->setObjectName("LoginCancel");
    buttons->button(QDialogButtonBox::Cancel)->setText("Thoát"); cardLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        QString text;
        if (!ConnectionProfile::validate(username->text(), host->text(), port->value(), selected, text)) {
            error->setText(text); return;
        }
        if (!AuthProtocol::validPassword(password->text())) { error->setText("Mat khau phai co 8-128 ky tu."); return; }
        if (mode->currentIndex() != 0 && mode->currentIndex()!=4 && password->text() != confirmation->text()) {
            error->setText("Hai lan nhap mat khau chua khop."); return;
        }
        if (mode->currentIndex()==2 && !AuthProtocol::validRecovery(recoveryCode->text().trimmed())) { error->setText("Cần mã dự phòng 64 ký tự. Phải tạo mã khi còn đăng nhập; không thể đặt lại chỉ bằng tên tài khoản."); return; }
        if((mode->currentIndex()==1||mode->currentIndex()>=3) && !AuthProtocol::validEmail(AuthProtocol::emailKey(email->text()))) {error->setText("Nhập email hợp lệ để nhận mã xác minh.");return;}
        selected.email=AuthProtocol::emailKey(email->text());selected.emailReset=mode->currentIndex()==3;selected.emailBind=mode->currentIndex()==4;
        selected.resetPassword = mode->currentIndex()==2; selected.recoveryCode = recoveryCode->text().trimmed();
        selected.password = password->text(); selected.registerAccount = mode->currentIndex() == 1;
        QSettings settings; selected.save(settings); accept();
    });
    username->setFocus();
}
