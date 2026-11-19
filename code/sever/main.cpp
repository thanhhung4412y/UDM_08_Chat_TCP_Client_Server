#include "mainwindow.h"
#include "chatserver.h"
#include "gmailconfig.h"
#include "gmailsender.h"
#include "gmailsettingsdialog.h"
#include <QApplication>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QNetworkInterface>
#include <QSettings>
#include <QVBoxLayout>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/branding/chat-logo.png"));
    app.setOrganizationName("ChatTCP1");
    app.setApplicationName("ChatServer");
    QSettings settings;
    int previous = settings.value("server/port", 12345).toInt();
    if (previous < 1 || previous > 65535) previous = 12345;
    ChatServer server;
    while (true) {
        bool accepted = false;
        const int port = QInputDialog::getInt(nullptr, "Khoi dong Server", "Cong lang nghe (1-65535):",
                                             previous, 1, 65535, 1, &accepted);
        if (!accepted) return 0;
        previous = port;
        if (server.start(quint16(port))) break;
        QMessageBox::warning(nullptr, "Khong mo duoc Server",
            "Khong mo duoc Server: " + server.startupError() + "\nKiem tra cong, database va TLS backend.");
    }
    settings.setValue("server/port", int(server.port()));
    MainWindow window;
    window.setWindowTitle("Chat TCP Server - Cong " + QString::number(server.port()));
    window.resize(650, 350);
    auto *layout = new QVBoxLayout(window.centralWidget());
    QStringList addresses;
    for (const auto &network : QNetworkInterface::allInterfaces()) {
        if (!(network.flags() & QNetworkInterface::IsUp) || (network.flags() & QNetworkInterface::IsLoopBack)) continue;
        for (const auto &entry : network.addressEntries()) {
            const auto ip = entry.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol && !ip.isLoopback() && !ip.isLinkLocal()
                && !addresses.contains(ip.toString())) addresses.append(ip.toString());
        }
    }
    auto *status = new QLabel("Server dang lang nghe cong " + QString::number(server.port())
        + "\n\nClient cung may: 127.0.0.1"
        + "\nClient may khac: nhap IP cua may Server ben duoi va cung cong."
        + "\n\nIP mang hien tai: " + (addresses.isEmpty() ? "Chua tim thay IPv4 mang LAN" : addresses.join(", "))
        + "\n\nNeu co nhieu IP, chon IP cua Wi-Fi/Ethernet dang ket noi cung mang voi Client."
        + "\nNeu ket noi bi chan, kiem tra quyen tuong lua cho ung dung tren mang Private.");
    status->setTextFormat(Qt::PlainText); status->setWordWrap(true);
    status->setText(status->text() + "\n\nTLS SHA-256 (doi chieu tren Client):\n" + server.certificateFingerprint()
        + "\n\nDu lieu Server: " + server.dataPath());
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status);
    auto *mailStatus = new QLabel(&window);
    mailStatus->setTextFormat(Qt::PlainText);
    mailStatus->setWordWrap(true);
    mailStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto updateMailStatus = [mailStatus]() {
        QString user; QByteArray password;
        if (GmailConfig::load(user, password)) {
            mailStatus->setText("Gmail: đã lưu cấu hình cho " + user + ". Gửi thư thử trong Cài đặt Gmail để xác nhận.");
        } else if (GmailSender::configured()) {
            mailStatus->setText("Gmail: đã có cấu hình từ môi trường khởi động. Bạn có thể cấu hình lại bằng nút bên dưới.");
        } else {
            mailStatus->setText("Gmail: chưa cấu hình. Nhấn 'Cài đặt Gmail' để thiết lập mà không dùng PowerShell.");
        }
        password.fill('\0');
    };
    updateMailStatus();
    QObject::connect(&server, &ChatServer::mailStatus, mailStatus,
                     [mailStatus](const QString &text) { mailStatus->setText("Gmail: " + text); });
    layout->addWidget(mailStatus);
    auto *buttons = new QHBoxLayout();
    auto *settingsButton = new QPushButton("Cài đặt Gmail", &window);
    buttons->addWidget(settingsButton);
    buttons->addStretch();
    layout->addLayout(buttons);
    QObject::connect(settingsButton, &QPushButton::clicked, &window, [&window, updateMailStatus] {
        GmailSettingsDialog dialog(&window);
        dialog.exec();
        updateMailStatus();
    });
    layout->addStretch();
    window.show();
    return app.exec();
}
