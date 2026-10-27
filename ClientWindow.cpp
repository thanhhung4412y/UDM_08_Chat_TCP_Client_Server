#include "ClientWindow.h"
#include "ui_ClientWindow.h"

#include <QMessageBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QRegularExpression>


// =====================================================
// 1. HAM KHOI TAO CUA SO
// =====================================================

ClientWindow::ClientWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::ClientWindow)
{
    ui->setupUi(this);

    setupInitialState();

    setupConnections();
}


// =====================================================
// 2. HAM HUY
// =====================================================

ClientWindow::~ClientWindow()
{
    delete ui;
}


// =====================================================
// 3. KHOI TAO TRANG THAI BAN DAU
// =====================================================

void ClientWindow::setupInitialState()
{
    // Dat tieu de cua so
    setWindowTitle("UDM_08 - Chat TCP Client-Server");

    // Gia tri IP va Port mac dinh
    ui->lineEditIP->setText("127.0.0.1");
    ui->lineEditPort->setText("5000");

    // Trang thai ban dau
    ui->labelConnectionStatus->setText("Chua ket noi");

    ui->labelCurrentUser->setText("Chua chon nguoi dung");

    ui->labelCurrentStatus->setText(
        "Chua xac dinh trang thai"
    );

    // Ban dau chua cho phep ngat ket noi
    ui->btnDisconnect->setEnabled(false);

    // Chua ket noi TCP that nen chua cho phep gui tin
    ui->btnSend->setEnabled(false);

    // Dat noi dung goi y
    ui->lineEditSearch->setPlaceholderText(
        "Tim kiem nguoi dung..."
    );

    ui->textEditMessage->setPlaceholderText(
        "Nhap tin nhan..."
    );

    // Du lieu mau de kiem tra giao dien
    // Sau nay se thay bang danh sach tu server
    ui->listContacts->addItem("NguyenVanA");
    ui->listContacts->addItem("TranVanB");
    ui->listContacts->addItem("LeVanC");

    // Tao layout cho khu vuc tin nhan neu chua co
    if (ui->messageContainer->layout() == nullptr)
    {
        QVBoxLayout *messageLayout =
            new QVBoxLayout(ui->messageContainer);

        messageLayout->setAlignment(Qt::AlignTop);

        messageLayout->setSpacing(8);

        messageLayout->setContentsMargins(
            12, 12, 12, 12
        );
    }
}


// =====================================================
// 4. KET NOI CAC SU KIEN
// =====================================================

void ClientWindow::setupConnections()
{
    // Nut Ket noi
    connect(
        ui->btnConnect,
        &QPushButton::clicked,
        this,
        &ClientWindow::onConnectClicked
    );

    // Nut Ngat ket noi
    connect(
        ui->btnDisconnect,
        &QPushButton::clicked,
        this,
        &ClientWindow::onDisconnectClicked
    );

    // Nut Gui
    connect(
        ui->btnSend,
        &QPushButton::clicked,
        this,
        &ClientWindow::onSendClicked
    );

    // Chon nguoi dung
    connect(
        ui->listContacts,
        &QListWidget::itemClicked,
        this,
        &ClientWindow::onContactSelected
    );

    // Tim kiem nguoi dung
    connect(
        ui->lineEditSearch,
        &QLineEdit::textChanged,
        this,
        &ClientWindow::onSearchTextChanged
    );

    // Nut Emoji
    connect(
        ui->btnEmoji,
        &QPushButton::clicked,
        this,
        &ClientWindow::onEmojiClicked
    );
}


// =====================================================
// 5. XU LY NUT KET NOI
// =====================================================

void ClientWindow::onConnectClicked()
{
    QString ip = ui->lineEditIP->text().trimmed();

    QString portText =
        ui->lineEditPort->text().trimmed();

    // Kiem tra IP co bi bo trong khong
    if (ip.isEmpty())
    {
        QMessageBox::warning(
            this,
            "Loi",
            "Vui long nhap dia chi IP server."
        );

        return;
    }

    // Kiem tra Port
    bool ok = false;

    int port = portText.toInt(&ok);

    if (!ok || port < 1 || port > 65535)
    {
        QMessageBox::warning(
            this,
            "Loi",
            "Port phai nam trong khoang 1 den 65535."
        );

        return;
    }

    /*
        PHAN TICH HOP TCP:

        Day moi chi kiem tra IP va Port.
        Chua ket noi TCP that.

        Khi thanh vien so 2 hoan thanh NetworkClient,
        ban se goi ham ket noi cua module do tai day.

        Khong duoc hien thi "Da ket noi" khi server
        chua xac nhan ket noi thanh cong.
    */

    QMessageBox::information(
        this,
        "Thong tin",
        "IP va Port hop le.\n"
        "Chua tich hop NetworkClient nen "
        "chua thuc hien ket noi TCP."
    );
}


// =====================================================
// 6. XU LY NUT NGAT KET NOI
// =====================================================

void ClientWindow::onDisconnectClicked()
{
    /*
        Khi tich hop TCP that:

        1. Goi ham ngat ket noi cua NetworkClient.
        2. Cho biet ket qua ngat ket noi.
        3. Cap nhat trang thai giao dien.

        Hien tai chua co NetworkClient.
    */

    ui->labelConnectionStatus->setText(
        "Chua ket noi"
    );

    ui->btnConnect->setEnabled(true);

    ui->btnDisconnect->setEnabled(false);

    ui->btnSend->setEnabled(false);
}


// =====================================================
// 7. XU LY NUT GUI TIN NHAN
// =====================================================

void ClientWindow::onSendClicked()
{
    // Lay noi dung tu o nhap
    QString message =
        ui->textEditMessage->toPlainText().trimmed();

    // Khong cho gui tin nhan rong
    if (message.isEmpty())
    {
        QMessageBox::warning(
            this,
            "Thong bao",
            "Vui long nhap noi dung tin nhan."
        );

        return;
    }

    // Phai chon nguoi nhan
    if (ui->listContacts->currentItem() == nullptr)
    {
        QMessageBox::warning(
            this,
            "Thong bao",
            "Vui long chon nguoi nhan."
        );

        return;
    }

    /*
        PHAN TICH HOP TCP:

        Khi NetworkClient san sang, gui tin nhan
        qua module mang truoc.

        Chi xu ly trang thai gui thanh cong theo
        co che ma nhom da thong nhat.

        Khong tu dong xem tin nhan la da gui thanh cong
        chi vi da hien thi no tren giao dien.
    */

    QMessageBox::information(
        this,
        "Chua tich hop TCP",
        "Noi dung hop le, nhung tin nhan chua duoc "
        "gui qua TCP vi NetworkClient chua duoc "
        "tich hop."
    );
}


// =====================================================
// 8. XU LY KHI CHON NGUOI DUNG
// =====================================================

void ClientWindow::onContactSelected(QListWidgetItem *item)
{
    if (item == nullptr)
    {
        return;
    }

    // Cap nhat ten nguoi dang chat
    ui->labelCurrentUser->setText(item->text());

    // Chua co du lieu trang thai tu server
    ui->labelCurrentStatus->setText(
        "Chua xac dinh trang thai"
    );

    /*
        Sau nay:
        - Tai lich su chat voi nguoi duoc chon.
        - Hien thi avatar.
        - Hien thi tin nhan tu server.
    */
}


// =====================================================
// 9. TIM KIEM NGUOI DUNG
// =====================================================

void ClientWindow::onSearchTextChanged(const QString &text)
{
    QString keyword = text.trimmed();

    for (int i = 0; i < ui->listContacts->count(); ++i)
    {
        QListWidgetItem *item =
            ui->listContacts->item(i);

        bool match = item->text().contains(
            keyword,
            Qt::CaseInsensitive
        );

        item->setHidden(!match);
    }
}


// =====================================================
// 10. XU LY NUT EMOJI
// =====================================================

void ClientWindow::onEmojiClicked()
{
    QStringList emojis = {
        "😀",
        "😂",
        "😊",
        "😍",
        "😎",
        "😭",
        "👍",
        "❤️",
        "🎉",
        "🔥",
        "🙏",
        "🥰"
    };

    bool ok = false;

    QString emoji = QInputDialog::getItem(
        this,
        "Chon Emoji",
        "Chon bieu tuong:",
        emojis,
        0,
        false,
        &ok
    );

    if (ok && !emoji.isEmpty())
    {
        ui->textEditMessage->insertPlainText(emoji);

        ui->textEditMessage->setFocus();
    }
}


// =====================================================
// 11. HIEN THI TIN NHAN
// =====================================================

void ClientWindow::addMessage(
    const QString &sender,
    const QString &content,
    bool isMine)
{
    QVBoxLayout *messageLayout =
        qobject_cast<QVBoxLayout *>(
            ui->messageContainer->layout()
        );

    if (messageLayout == nullptr)
    {
        return;
    }

    QString displayText =
        sender + ": " + content;

    QLabel *messageLabel = new QLabel(
        displayText,
        ui->messageContainer
    );

    messageLabel->setWordWrap(true);

    messageLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse
    );

    if (isMine)
    {
        messageLabel->setStyleSheet(
            "QLabel {"
            "background-color: #D9FDD3;"
            "color: #1F2937;"
            "border-radius: 10px;"
            "padding: 10px;"
            "}"
        );

        messageLayout->addWidget(
            messageLabel,
            0,
            Qt::AlignRight
        );
    }
    else
    {
        messageLabel->setStyleSheet(
            "QLabel {"
            "background-color: white;"
            "color: #1F2937;"
            "border-radius: 10px;"
            "padding: 10px;"
            "}"
        );

        messageLayout->addWidget(
            messageLabel,
            0,
            Qt::AlignLeft
        );
    }

    scrollToBottom();
}


// =====================================================
// 12. CUON XUONG CUOI KHU VUC TIN NHAN
// =====================================================

void ClientWindow::scrollToBottom()
{
    QScrollBar *scrollBar =
        ui->scrollAreaMessages->verticalScrollBar();

    scrollBar->setValue(
        scrollBar->maximum()
    );
}