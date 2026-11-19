#include "gmailsettingsdialog.h"
#include "gmailconfig.h"
#include "gmailsender.h"
#include <QApplication>
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QThreadPool>
#include <QRunnable>
#include <QVBoxLayout>
#include <QSslConfiguration>

GmailSettingsDialog::GmailSettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Cài đặt Gmail SMTP — ChatTCP1 Server");
    resize(520, 300);
    setModal(true);
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel("Gmail dùng để gửi mã xác minh và khôi phục mật khẩu.\n"
                             "Cần bật xác minh 2 bước và tạo Mật khẩu ứng dụng Google (16 ký tự).", this);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *form = new QFormLayout();
    emailInput = new QLineEdit(this);
    emailInput->setPlaceholderText("tenban@gmail.com");
    passwordInput = new QLineEdit(this);
    passwordInput->setPlaceholderText("Mật khẩu ứng dụng gồm 16 ký tự");
    passwordInput->setEchoMode(QLineEdit::Password);
    auto *showPassword = new QCheckBox("Hiện mật khẩu", this);
    connect(showPassword, &QCheckBox::toggled, this, [this](bool show) {
        passwordInput->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
    });
    form->addRow("Gmail quản trị:", emailInput);
    form->addRow("App Password:", passwordInput);
    form->addRow("SMTP:", new QLabel("smtp.gmail.com:465 (TLS)", this));
    form->addRow("", showPassword);
    layout->addLayout(form);
    resultLabel = new QLabel("Mật khẩu lưu trên Windows bằng DPAPI của tài khoản hiện tại.", this);
    resultLabel->setWordWrap(true);
    layout->addWidget(resultLabel);
    auto *buttons = new QHBoxLayout();
    testButton = new QPushButton("Gửi thư kiểm tra", this);
    saveButton = new QPushButton("Lưu cấu hình", this);
    auto *removeButton = new QPushButton("Xóa cấu hình", this);
    auto *closeButton = new QPushButton("Đóng", this);
    buttons->addWidget(testButton);
    buttons->addWidget(saveButton);
    buttons->addWidget(removeButton);
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);
    connect(testButton, &QPushButton::clicked, this, [this] { testGmail(); });
    connect(saveButton, &QPushButton::clicked, this, [this] { saveGmail(); });
    connect(removeButton, &QPushButton::clicked, this, [this] { removeGmail(); });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    QString storedUser, error;
    QByteArray password;
    if (GmailConfig::load(storedUser, password, &error)) {
        emailInput->setText(storedUser);
        passwordInput->setText(QString::fromLatin1(password));
        resultLabel->setText("Đã tải cấu hình Gmail được bảo vệ từ Windows.");
    }
    password.fill('\0');
}

void GmailSettingsDialog::saveGmail() {
    QByteArray password = passwordInput->text().toLatin1();
    QString error;
    const bool saved = GmailConfig::save(emailInput->text().trimmed(), password, &error);
    password.fill('\0');
    if (!saved) { QMessageBox::warning(this, "Không lưu được Gmail", error); return; }
    resultLabel->setText("Đã lưu Gmail và mật khẩu ứng dụng được mã hóa bằng Windows DPAPI. Có thể gửi thử ngay.");
    QMessageBox::information(this, "Lưu thành công", "Gmail đã được cấu hình. Không cần dùng PowerShell khi mở Server lần sau.");
}

void GmailSettingsDialog::removeGmail() {
    if (QMessageBox::question(this, "Xóa cấu hình", "Xóa Gmail và mật khẩu ứng dụng đã lưu trên máy?") != QMessageBox::Yes) return;
    QString error;
    if (!GmailConfig::clear(&error)) { QMessageBox::warning(this, "Lỗi", error); return; }
    emailInput->clear(); passwordInput->clear();
    resultLabel->setText("Đã xóa cấu hình Gmail đã lưu.");
}

void GmailSettingsDialog::testGmail() {
    const QString email = emailInput->text().trimmed();
    QByteArray password = passwordInput->text().toLatin1();
    password.replace(" ", "");
    if (email.isEmpty() || password.size() != 16) {
        QMessageBox::warning(this, "Thiếu thông tin", "Nhập địa chỉ Gmail và Mật khẩu ứng dụng 16 ký tự trước khi kiểm tra.");
        password.fill('\0'); return;
    }
    testButton->setEnabled(false);
    saveButton->setEnabled(false);
    resultLabel->setText("Đang gửi thư thử đến chính Gmail đã nhập... (có thể cần tới 30 giây)");
    QPointer<GmailSettingsDialog> guard(this);
    QThreadPool::globalInstance()->start(QRunnable::create([guard, email, password = std::move(password)]() mutable {
        const bool sent = GmailSender::sendWithSettings(email, password, email, "TEST", "smtp-test",
            "smtp.gmail.com", 465, QSslConfiguration::defaultConfiguration());
        const QString detail = GmailSender::lastError();
        password.fill('\0');
        QMetaObject::invokeMethod(qApp, [guard, sent, detail] {
            if (!guard) return;
            guard->testButton->setEnabled(true);
            guard->saveButton->setEnabled(true);
            guard->resultLabel->setText(sent
                ? "Gmail SMTP đã chấp nhận thư thử. Hãy kiểm tra Hộp thư đến/Spam. Nhấn Lưu cấu hình để sử dụng."
                : "Gửi thử thất bại: " + detail);
        }, Qt::QueuedConnection);
    }));
}
