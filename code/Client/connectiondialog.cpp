#include "connectiondialog.h"
#include "authprotocol.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

ConnectionDialog::ConnectionDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Tai khoan Chat TCP");
    resize(490, 410);
    QSettings settings;
    const auto previous = ConnectionProfile::load(settings);
    auto *layout = new QVBoxLayout(this);
    auto *hint = new QLabel("Cung may: 127.0.0.1. Hai may LAN: nhap IP hien tren cua so Server.");
    hint->setWordWrap(true); layout->addWidget(hint);
    auto *form = new QFormLayout(); layout->addLayout(form);
    mode = new QComboBox(); mode->setObjectName("authMode");
    mode->addItem("Dang nhap"); mode->addItem("Dang ky tai khoan moi");
    form->addRow("Thao tac", mode);
    username = new QLineEdit(previous.username); username->setObjectName("usernameInput");
    host = new QLineEdit(previous.host); host->setObjectName("hostInput");
    host->setPlaceholderText("192.168.1.10 hoac ten may Server");
    port = new QSpinBox(); port->setObjectName("portInput"); port->setRange(1, 65535); port->setValue(previous.port);
    form->addRow("Ten cua ban", username); form->addRow("IP / ten may Server", host); form->addRow("Cong Server", port);
    password = new QLineEdit(); password->setObjectName("passwordInput"); password->setEchoMode(QLineEdit::Password);
    password->setMaxLength(128); password->setPlaceholderText("12-128 ky tu; khong luu mat khau tren may");
    confirmation = new QLineEdit(); confirmation->setObjectName("confirmPasswordInput");
    confirmation->setEchoMode(QLineEdit::Password); confirmation->setMaxLength(128);
    form->addRow("Mat khau", password); form->addRow("Nhap lai mat khau", confirmation);
    auto *confirmationLabel = form->labelForField(confirmation);
    confirmation->hide(); confirmationLabel->hide();
    connect(mode, &QComboBox::currentIndexChanged, this, [=](int index) {
        confirmation->setVisible(index == 1); confirmationLabel->setVisible(index == 1);
    });
    error = new QLabel(); error->setObjectName("connectionError");
    error->setTextFormat(Qt::PlainText); error->setWordWrap(true); error->setStyleSheet("color:#b91c1c"); layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Tiep tuc");
    buttons->button(QDialogButtonBox::Cancel)->setText("Thoat"); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        QString text;
        if (!ConnectionProfile::validate(username->text(), host->text(), port->value(), selected, text)) {
            error->setText(text); return;
        }
        if (!AuthProtocol::validPassword(password->text())) { error->setText("Mat khau phai co 12-128 ky tu."); return; }
        if (mode->currentIndex() == 1 && password->text() != confirmation->text()) {
            error->setText("Hai lan nhap mat khau chua khop."); return;
        }
        selected.password = password->text(); selected.registerAccount = mode->currentIndex() == 1;
        QSettings settings; selected.save(settings); accept();
    });
    username->setFocus();
}
