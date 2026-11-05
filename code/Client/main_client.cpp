#include <QComboBox>
#include <QApplication>
#include <QWidget>
#include <QTcpSocket>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QMessageBox>
#include <QByteArray>
#include <utility>
#include <QDialog>
#include <QScrollArea>
#include <QGridLayout>
#include <QStringList>
#include <QPushButton>
#include <QPixmap>
#include <QIcon>
#include <QSize>
#include <QTextDocument>
#include <QUrl>
#include <QFile>
#include <QDebug>
#include <QFileDialog>
#include <QImage>
#include <QBuffer>
#include <QUuid>
#include <QFileInfo>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QPoint>
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qDebug() << "Nobita exists:"
             << QFile::exists(":/stickers/nobita.jpg");

    qDebug() << "Doremon exists:"
             << QFile::exists(":/stickers/doremon.jpg");

    qDebug() << "Nobita loaded:"
             << !QPixmap(":/stickers/nobita.jpg").isNull();

    qDebug() << "Doremon loaded:"
             << !QPixmap(":/stickers/doremon.jpg").isNull();

    // Yeu cau nguoi dung nhap ten
    bool ok = false;

    QString username = QInputDialog::getText(
                           nullptr,
                           "Dang nhap Chat TCP",
                           "Nhap ten cua ban:",
                           QLineEdit::Normal,
                           "",
                           &ok
                           ).trimmed();

    if (!ok || username.isEmpty()) {
        return 0;
    }

    if (username.size() > 20) {
        QMessageBox::warning(
            nullptr, "Loi",
            "Ten khong duoc vuot qua 20 ky tu");
        return 0;
    }

    QWidget window;
    window.setWindowTitle(
        "Chat TCP - " + username
        );
    window.resize(800, 500);

    QLabel *status =
        new QLabel("Dang ket noi...");

    QTextEdit *chatBox = new QTextEdit();
    chatBox->setReadOnly(true);

    QListWidget *onlineList = new QListWidget();
    QLabel *onlineLabel =
        new QLabel("Online (0)");

    QVBoxLayout *usersLayout =
        new QVBoxLayout();

    usersLayout->addWidget(onlineLabel);
    usersLayout->addWidget(onlineList);

    QHBoxLayout *contentLayout =
        new QHBoxLayout();

    contentLayout->addWidget(chatBox, 3);
    contentLayout->addLayout(usersLayout, 1);

    QLineEdit *messageInput =
        new QLineEdit();

    messageInput->setPlaceholderText(
        "Nhap tin nhan..."
        );
    QPushButton *emojiButton =
        new QPushButton("😀");
    QPushButton *stickerButton =
        new QPushButton("Sticker");
    QPushButton *imageButton =
        new QPushButton("Anh");
    QPushButton *videoButton =
        new QPushButton("Video");
    QPushButton *sendButton =
        new QPushButton("Gui");
    QPushButton *replyButton =
        new QPushButton("Reply");
    QPushButton *forwardButton =
        new QPushButton("Forward");
    QComboBox *receiverBox = new QComboBox();

    receiverBox->addItem("Phong chung", "");

    receiverBox->setMinimumWidth(150);


    sendButton->setEnabled(false);
    messageInput->setEnabled(false);

    QHBoxLayout *inputLayout =
        new QHBoxLayout();



    inputLayout->addWidget(receiverBox);
    inputLayout->addWidget(messageInput);
    inputLayout->addWidget(emojiButton);
    inputLayout->addWidget(stickerButton);
    inputLayout->addWidget(imageButton);
    inputLayout->addWidget(videoButton);
    inputLayout->addWidget(replyButton);
    inputLayout->addWidget(forwardButton);
    inputLayout->addWidget(sendButton);

    QVBoxLayout *mainLayout =
        new QVBoxLayout();

    mainLayout->addWidget(status);
    mainLayout->addLayout(contentLayout);
    mainLayout->addLayout(inputLayout);

    window.setLayout(mainLayout);

    // An cac nut cu, van giu nguyen su kien
    emojiButton->hide();
    stickerButton->hide();
    imageButton->hide();
    videoButton->hide();
    replyButton->hide();
    forwardButton->hide();

    // Nut gom Anh va Video
    QToolButton *mediaButton =
        new QToolButton(&window);

    mediaButton->setText("+");
    mediaButton->setToolTip("Gui anh hoac video");
    mediaButton->setFixedWidth(42);

    QMenu *mediaMenu = new QMenu(mediaButton);

    QAction *photoAction =
        mediaMenu->addAction("Gui anh");

    QAction *videoAction =
        mediaMenu->addAction("Gui video");

    mediaButton->setMenu(mediaMenu);
    mediaButton->setPopupMode(
        QToolButton::InstantPopup
        );

    QObject::connect(
        photoAction, &QAction::triggered,
        &window, [=]() {
            imageButton->click();
        }
        );

    QObject::connect(
        videoAction, &QAction::triggered,
        &window, [=]() {
            videoButton->click();
        }
        );

    // Chen nut + vao truoc o nhap tin nhan
    inputLayout->insertWidget(1, mediaButton);

    // Nut gom Emoji va Sticker
    QToolButton *emotionButton =
        new QToolButton(&window);

    emotionButton->setText("😊");
    emotionButton->setToolTip("Emoji va Sticker");
    emotionButton->setFixedWidth(42);

    QMenu *emotionMenu =
        new QMenu(emotionButton);

    QAction *emojiAction =
        emotionMenu->addAction("Chon Emoji");

    QAction *stickerAction =
        emotionMenu->addAction("Chon Sticker");

    emotionButton->setMenu(emotionMenu);
    emotionButton->setPopupMode(
        QToolButton::InstantPopup
        );

    QObject::connect(
        emojiAction, &QAction::triggered,
        &window, [=]() {
            emojiButton->click();
        }
        );

    QObject::connect(
        stickerAction, &QAction::triggered,
        &window, [=]() {
            stickerButton->click();
        }
        );

    inputLayout->insertWidget(2, emotionButton);

    QObject::connect(
        emojiButton, &QPushButton::clicked,
        &window, [&]() {

            QDialog dialog(&window);
            dialog.setWindowTitle("Chon Emoji");
            dialog.resize(420, 380);

            QVBoxLayout *layout = new QVBoxLayout(&dialog);


            QScrollArea *scroll = new QScrollArea();
            scroll->setWidgetResizable(true);

            QWidget *container = new QWidget();

            // Tao bo cuc 6 cot Emoji
            QGridLayout *grid = new QGridLayout(container);

            // Danh sach emoji
            QStringList emojis = {
                "😀", "😃", "😄", "😁", "😆", "😅",
                "🤣", "😂", "🙂", "🙃", "😉", "😊",
                "😍", "🥰", "😘", "😋", "😎", "🤩",
                "🥳", "😏", "😢", "😭", "😡", "🤬",
                "😱", "😨", "🥺", "🤔", "🤗", "🤫",
                "😴", "🤒", "🤯", "🥶", "🥵", "😇",
                "👍", "👎", "👏", "🙌", "🙏", "💪",
                "👋", "👌", "✌️", "🤝", "🫶", "🖐️",
                "❤️", "🧡", "💛", "💚", "💙", "💜",
                "🖤", "💖", "💯", "🔥", "⭐", "✨",
                "🎉", "🎊", "🎂", "🎁", "🏆", "🎮",
                "🐱", "🐶", "🐼", "🦊", "🐸", "🐻",
                "🍕", "🍔", "🍟", "🍓", "☕", "🍉"
            };

            const int columns = 6;

            for (int i = 0; i < emojis.size(); ++i) {

                QString emoji = emojis.at(i);

                QPushButton *button =
                    new QPushButton(emoji);

                button->setFixedSize(55, 48);

                QFont font = button->font();
                font.setPointSize(18);
                button->setFont(font);

                int row = i / columns;
                int column = i % columns;

                grid->addWidget(button, row, column);

                QObject::connect(
                    button, &QPushButton::clicked,
                    &dialog, [&, emoji]() {

                        messageInput->insert(emoji);
                        dialog.accept();
                        messageInput->setFocus();
                    }
                    );
            }

            scroll->setWidget(container);
            layout->addWidget(scroll);

            dialog.exec();
        }
        );

    QTcpSocket socket;
    QByteArray buffer;
    QString replyQuote;

    QObject::connect(
        receiverBox,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        &window,
        [&](int) {
            replyQuote.clear();
            messageInput->setPlaceholderText(
                "Nhap tin nhan..."
                );
        }
        );



    // Ham gui JSON
    auto sendPacket = [&](const QJsonObject &packet) {
        QByteArray data =
            QJsonDocument(packet)
                .toJson(QJsonDocument::Compact);

        socket.write(data + "\n");
    };

    // =====================================

    // =====================================
    // GUI ANH TU MAY TINH
    // =====================================
    QObject::connect(
        imageButton, &QPushButton::clicked,
        &window, [&]() {

            if (!sendButton->isEnabled() ||
                socket.state() != QAbstractSocket::ConnectedState) {
                QMessageBox::warning(
                    &window, "Loi",
                    "Chua ket noi hoac dang nhap Server"
                    );
                return;
            }

            QString filePath = QFileDialog::getOpenFileName(
                &window,
                "Chon anh de gui",
                QString(),
                "Images (*.jpg *.jpeg *.png *.bmp)"
                );

            if (filePath.isEmpty())
                return;

            QImage image(filePath);

            if (image.isNull()) {
                QMessageBox::warning(
                    &window, "Loi",
                    "Khong doc duoc file anh"
                    );
                return;
            }

            if (image.width() > 1280 ||
                image.height() > 1280) {
                image = image.scaled(
                    1280, 1280,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                    );
            }

            QByteArray imageBytes;
            QBuffer imageBuffer(&imageBytes);

            if (!imageBuffer.open(QIODevice::WriteOnly) ||
                !image.save(&imageBuffer, "JPEG", 75)) {
                QMessageBox::warning(
                    &window, "Loi",
                    "Khong nen duoc anh"
                    );
                return;
            }

            if (imageBytes.size() > 256 * 1024) {
                QMessageBox::warning(
                    &window, "Loi",
                    "Anh sau khi nen van qua lon"
                    );
                return;
            }

            QString receiver =
                receiverBox->currentData().toString();

            QString encoded =
                QString::fromLatin1(
                    imageBytes.toBase64()
                    );

            sendPacket({
                {"type", "image"},
                {"to", receiver},
                {"data", encoded}
            });

            qDebug() << "Da gui anh:"
                     << imageBytes.size()
                     << "bytes";
        }
        );

    // =====================================
    // CHON VIDEO MP4 TU MAY TINH
    // =====================================
    QObject::connect(
        videoButton, &QPushButton::clicked,
        &window, [&]() {

            qDebug() << "Da bam nut Video";

            if (!sendButton->isEnabled() ||
                socket.state() != QAbstractSocket::ConnectedState) {
                QMessageBox::warning(
                    &window, "Loi",
                    "Chua ket noi hoac dang nhap Server"
                    );
                return;
            }

            QString filePath = QFileDialog::getOpenFileName(
                &window,
                "Chon video MP4",
                QString(),
                "Video MP4 (*.mp4)"
                );

            if (filePath.isEmpty())
                return;

            QFileInfo fileInfo(filePath);

            if (!fileInfo.exists() ||
                !fileInfo.isFile()) {
                QMessageBox::warning(
                    &window, "Loi",
                    "Khong tim thay video"
                    );
                return;
            }

            const qint64 maxSize =
                20LL * 1024 * 1024;

            if (fileInfo.size() <= 0 ||
                fileInfo.size() > maxSize) {
                QMessageBox::warning(
                    &window, "Loi",
                    "Video phai co dung luong tu 1 byte "
                    "den 20 MiB"
                    );
                return;
            }

            QMessageBox::information(
                &window,
                "Da chon Video",
                QString("Ten: %1\nDung luong: %2 KB")
                    .arg(fileInfo.fileName())
                    .arg(fileInfo.size() / 1024)
                );
        }
        );

    // Nhan nut Sticker
    QObject::connect(
        stickerButton, &QPushButton::clicked,
        &window, [&]() {

            if (!sendButton->isEnabled())
                return;

            QDialog dialog(&window);
            dialog.setWindowTitle("Chon Sticker");

            QGridLayout *grid =
                new QGridLayout(&dialog);

            QStringList stickers = {
                "nobita", "doremon"
            };

            for (int i = 0; i < stickers.size(); ++i) {

                QString id = stickers.at(i);

                QString path =
                    ":/stickers/" + id + ".jpg";

                QPixmap pixmap(path);

                QPushButton *button =
                    new QPushButton(&dialog);

                button->setIcon(QIcon(pixmap));
                button->setIconSize(QSize(90, 90));
                button->setFixedSize(110, 110);

                grid->addWidget(button, 0, i);

                QObject::connect(
                    button, &QPushButton::clicked,
                    &dialog, [&, id]() {

                        QString receiver =
                            receiverBox->currentData()
                                .toString();

                        sendPacket({
                            {"type", "sticker"},
                            {"to", receiver},
                            {"id", id}
                        });

                        dialog.accept();
                    }
                    );
            }

            dialog.exec();
        }
        );
    // Khi ket noi TCP thanh cong
    QObject::connect(
        &socket, &QTcpSocket::connected,
        &window, [&]() {

            status->setText(
                "Da ket noi TCP - Dang dang ky ten..."
                );

            sendPacket({
                {"type", "join"},
                {"name", username}
            });
        }
        );

    // Nhan va xu ly du lieu JSON
    QObject::connect(
        &socket, &QTcpSocket::readyRead,
        &window, [&]() {

            buffer.append(socket.readAll());

            while (true) {

                int pos = buffer.indexOf('\n');
                if (pos < 0)
                    break;

                if (pos > 512 * 1024) {
                    socket.disconnectFromHost();
                    break;
                }

                QByteArray line = buffer.left(pos);
                buffer.remove(0, pos + 1);

                QJsonParseError error;
                QJsonDocument doc =
                    QJsonDocument::fromJson(
                        line, &error);

                if (error.error !=
                        QJsonParseError::NoError ||
                    !doc.isObject())
                    continue;

                QJsonObject packet = doc.object();
                QString type =
                    packet.value("type").toString();

                if (type == "joined") {

                    status->setText(
                        "Da dang nhap: " + username
                        );

                    sendButton->setEnabled(true);
                    messageInput->setEnabled(true);

                    chatBox->append(
                        "[He thong] Dang nhap thanh cong!"
                        );
                }

                else if (type == "system") {

                    QString text =
                        packet.value("text").toString();

                    chatBox->append(
                        "[He thong] " +
                        text.toHtmlEscaped()
                        );
                }

                else if (type == "chat") {

                    QString sender =
                        packet.value("sender").toString();

                    QString text =
                        packet.value("text").toString();

                    chatBox->append(
                        "<b>" + sender.toHtmlEscaped() +
                        ":</b> " + text.toHtmlEscaped()
                        );
                }

                else if (type == "reply") {

                    QString sender =
                        packet.value("sender").toString();

                    QString text =
                        packet.value("text").toString();

                    QString quote =
                        packet.value("quote").toString();

                    QString html =
                        "<div style='margin:5px 0;'>"
                        "<span style='color:gray;'>"
                        "&#8618; Reply: " + quote.toHtmlEscaped() +
                        "</span>"
                        "<div><b>" + sender.toHtmlEscaped() +
                        ":</b> " + text.toHtmlEscaped() +
                        "</div></div>";

                    chatBox->append(html);
                }

                else if (type == "forward") {

                    QString sender =
                        packet.value("sender").toString();

                    QString receiver =
                        packet.value("to").toString();

                    QString text =
                        packet.value("text").toString();

                    QString destination =
                        receiver.isEmpty()
                            ? "Phong chung"
                            : receiver;

                    QString title =
                        "[Forward boi " + sender +
                        " -> " + destination + "]";

                    chatBox->append(
                        "<b><span style='color:#7c3aed'>"
                        + title.toHtmlEscaped() +
                        "</span></b> "
                        + text.toHtmlEscaped()
                        );
                }


                else if (type == "sticker") {

                    QString sender =
                        packet.value("sender").toString();

                    QString id =
                        packet.value("id").toString();

                    QStringList allowed = {
                        "nobita", "doremon"
                    };

                    if (!allowed.contains(id))
                        continue;

                    QString path =
                        ":/stickers/" + id + ".jpg";

                    QPixmap pixmap(path);

                    if (!pixmap.isNull()) {

                        chatBox->document()->addResource(
                            QTextDocument::ImageResource,
                            QUrl(path),
                            pixmap.toImage()
                            );

                        chatBox->append(
                            "<b>" + sender.toHtmlEscaped() +
                            " da gui Sticker:</b>"
                            "<div><img src='" + path +
                            "' width='120'></div>"
                            );

                    } else {

                        chatBox->append(
                            "[Loi] Khong tai duoc Sticker"
                            );
                    }
                }

                else if (type == "image") {

                    QString sender =
                        packet.value("sender").toString();

                    QString encoded =
                        packet.value("data").toString();

                    if (encoded.isEmpty() ||
                        encoded.size() > 350000)
                        continue;

                    QByteArray imageBytes =
                        QByteArray::fromBase64(
                            encoded.toLatin1()
                            );

                    if (imageBytes.isEmpty() ||
                        imageBytes.size() > 256 * 1024)
                        continue;

                    QImage image;

                    if (!image.loadFromData(imageBytes, "JPEG")) {

                        chatBox->append(
                            "[Loi] Anh nhan duoc khong hop le"
                            );
                        continue;
                    }

                    QString imageUrl =
                        "chatimg:/" +
                        QUuid::createUuid().toString(
                            QUuid::WithoutBraces
                            );

                    chatBox->document()->addResource(
                        QTextDocument::ImageResource,
                        QUrl(imageUrl),
                        image
                        );

                    QString title =
                        sender.toHtmlEscaped() +
                        " da gui anh:";

                    QString receiver =
                        packet.value("to").toString();

                    if (!receiver.isEmpty()) {
                        title = "[Anh rieng] " + title;
                    }

                    chatBox->append(
                        "<b>" + title + "</b>"
                                        "<div><img src='" + imageUrl +
                        "' width='240'></div>"
                        );
                }
else if (type == "private") {
                    QString sender =
                        packet.value("sender").toString();

                    QString receiver =
                        packet.value("to").toString();

                    QString text =
                        packet.value("text").toString();

                    QString title;

                    if (sender == username) {
                        title = "[Ban -> " + receiver + "]";
                    } else {
                        title = "[Tin rieng tu " + sender + "]";
                    }

                    chatBox->append(
                        "<span style='color:#2563eb'>"
                        "<b>" + title.toHtmlEscaped() +
                        "</b></span>: " +
                        text.toHtmlEscaped()
                        );
                }


                else if (type == "users") {

                    QString selectedReceiver =
                        receiverBox->currentData().toString();

                    onlineList->clear();
                    receiverBox->clear();

                    receiverBox->addItem("Phong chung", "");

                    QJsonArray users =
                        packet.value("names").toArray();

                    for (const QJsonValue &item : std::as_const(users)) {

                        QString name = item.toString();

                        onlineList->addItem(name);

                        // Khong them chinh minh vao danh sach nhan
                        if (name != username) {
                            receiverBox->addItem(name, name);
                        }
                    }

                    onlineLabel->setText(
                        QString("Online (%1)").arg(users.size())
                        );

                    int index =
                        receiverBox->findData(selectedReceiver);

                    receiverBox->setCurrentIndex(
                        index >= 0 ? index : 0
                        );
                }

                else if (type == "error") {

                    QString message =
                        packet.value("text").toString();

                    chatBox->append(
                        "<span style='color:red;'>[Loi] " +
                        message.toHtmlEscaped() +
                        "</span>"
                        );

                    // Chi bao dang ky that bai khi chua dang nhap
                    if (!sendButton->isEnabled()) {
                        status->setText("Dang ky that bai");
                    }

                    // Khong khoa Chat neu da dang nhap
                }

            }

if (buffer.size() > 512 * 1024) {
    socket.disconnectFromHost();
}
        }
        );

    QObject::connect(
        replyButton, &QPushButton::clicked,
        &window, [&]() {

            if (!receiverBox->currentData()
                     .toString().isEmpty()) {

                QMessageBox::information(
                    &window,
                    "Thong bao",
                    "Hay chon Phong chung de Reply"
                    );
                return;
            }

            QString selected =
                chatBox->textCursor()
                    .selectedText().trimmed();

            if (selected.isEmpty()) {
                QMessageBox::information(
                    &window,
                    "Thong bao",
                    "Hay boi den noi dung can Reply"
                    );
                return;
            }

            replyQuote = selected.left(500);

            messageInput->setPlaceholderText(
                "Dang Reply: " + replyQuote.left(40)
                );

            messageInput->setFocus();
        }
        );

    // Ham gui tin nhan

    // ===============================
    // SU KIEN NUT FORWARD
    // ===============================
    QObject::connect(
        forwardButton, &QPushButton::clicked,
        &window, [&]() {

            // Chua dang nhap thi khong Forward
            if (!sendButton->isEnabled() ||
                socket.state() != QAbstractSocket::ConnectedState) {

                QMessageBox::warning(
                    &window,
                    "Loi",
                    "Chua ket noi hoac dang nhap Server"
                    );

                return;
            }

            // Lay phan van ban duoc boi den
            QString selected =
                chatBox->textCursor()
                    .selectedText().trimmed();

            if (selected.isEmpty()) {
                QMessageBox::information(
                    &window,
                    "Thong bao",
                    "Hay boi den tin nhan can Forward"
                    );
                return;
            }

            if (selected.toUtf8().size() > 4000) {
                QMessageBox::warning(
                    &window,
                    "Loi",
                    "Noi dung Forward qua dai"
                    );
                return;
            }

            // Lay nguoi nhan tu ComboBox
            QString receiver =
                receiverBox->currentData().toString();

            // Gui Forward den Server
            sendPacket({
                {"type", "forward"},
                {"to", receiver},
                {"text", selected}
            });
        }
        );

    // Menu chuot phai tren khung chat
    chatBox->setContextMenuPolicy(
        Qt::CustomContextMenu
        );

    QObject::connect(
        chatBox,
        &QTextEdit::customContextMenuRequested,
        &window,
        [&](const QPoint &pos) {

            QMenu *menu =
                chatBox->createStandardContextMenu();

            menu->addSeparator();

            QAction *replyAction =
                menu->addAction("Tra loi (Reply)");

            QAction *forwardAction =
                menu->addAction("Chuyen tiep (Forward)");

            bool hasSelection =
                !chatBox->textCursor()
                     .selectedText()
                     .trimmed()
                     .isEmpty();

            replyAction->setEnabled(hasSelection);
            forwardAction->setEnabled(hasSelection);

            QObject::connect(
                replyAction, &QAction::triggered,
                menu, [=]() {
                    replyButton->click();
                }
                );

            QObject::connect(
                forwardAction, &QAction::triggered,
                menu, [=]() {
                    forwardButton->click();
                }
                );

            menu->exec(
                chatBox->mapToGlobal(pos)
                );

            delete menu;
        }
        );

    auto sendMessage = [&]() {

        QString text =
            messageInput->text().trimmed();

        if (text.isEmpty())
            return;


        if (!sendButton->isEnabled() ||
            socket.state() != QAbstractSocket::ConnectedState) {

            QMessageBox::warning(
                &window,
                "Loi",
                "Chua ket noi hoac dang nhap Server"
                );

            return;
        }



        QString receiver =
            receiverBox->currentData().toString();

        if (!receiver.isEmpty()) {

            sendPacket({
                {"type", "private"},
                {"to", receiver},
                {"text", text}
            });

        } else if (!replyQuote.isEmpty()) {

            sendPacket({
                {"type", "reply"},
                {"text", text},
                {"quote", replyQuote}
            });

        } else {

            sendPacket({
                {"type", "chat"},
                {"text", text}
            });
        }

        replyQuote.clear();
        messageInput->clear();
        messageInput->setPlaceholderText("Nhap tin nhan...");

    };


    QObject::connect(
        sendButton, &QPushButton::clicked,
        &window, sendMessage
        );

    QObject::connect(
        messageInput, &QLineEdit::returnPressed,
        &window, sendMessage
        );
    // Ngat ket noi
    QObject::connect(
        &socket, &QTcpSocket::disconnected,
        &window, [&]() {

            status->setText("Mat ket noi Server");
            sendButton->setEnabled(false);
            messageInput->setEnabled(false);

            onlineList->clear();
            onlineLabel->setText("Online (0)");

            chatBox->append(
                "[He thong] Da ngat ket noi"
                );
        }
        );

    QObject::connect(
        &socket, &QTcpSocket::errorOccurred,
        &window,
        [&](QAbstractSocket::SocketError) {

            chatBox->append(
                "[Loi] " +
                socket.errorString().toHtmlEscaped()
                );
        }
        );

    window.show();

    socket.connectToHost("127.0.0.1", 12345);

    return app.exec();
}
