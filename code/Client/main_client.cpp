#include "connectiondialog.h"
#include "sessionconnection.h"
#include "chathistory.h"
#include "videoclient.h"
#include <QStandardPaths>
#include <QDesktopServices>
#include <QTextBrowser>
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
static bool runChat(const ConnectionProfile &profile)
{
    const QString username = profile.username;
    const QString serverHost = profile.host;
    const quint16 serverPort = profile.port;
    bool changeRequested = false;
    qDebug() << "Nobita exists:"
             << QFile::exists(":/stickers/nobita.jpg");

    qDebug() << "Doremon exists:"
             << QFile::exists(":/stickers/doremon.jpg");

    qDebug() << "Nobita loaded:"
             << !QPixmap(":/stickers/nobita.jpg").isNull();

    qDebug() << "Doremon loaded:"
             << !QPixmap(":/stickers/doremon.jpg").isNull();

    QWidget window;
    window.setWindowTitle(
        "Chat TCP - " + username + " @ " + profile.endpoint()
        );
    window.resize(800, 500);

    QLabel *status =
        new QLabel("Dang ket noi...");

    QTextBrowser *chatBox = new QTextBrowser();
    chatBox->setOpenLinks(false);
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

    QSslSocket socket;
    JsonChannel channel(&socket);
    SessionConnection connection(&socket, &channel, profile);
    connection.setTrustPrompt([&](const QString &fingerprint, const QString &previous) {
        const QString notice = previous.isEmpty() ? "Lan dau ket noi Server nay." : "CANH BAO: Danh tinh Server da thay doi.";
        return QMessageBox::question(&window, "Xac minh Server",
            notice + "\nSo sanh SHA-256 duoi day voi cua so Server:\n\n" + fingerprint
            + (previous.isEmpty() ? QString() : "\n\nDau van tay cu:\n" + previous)
            + "\n\nChi bam Yes neu dau van tay khop voi Server cua ban.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
    });
    ChatHistory history(chatBox, username, serverHost, serverPort);
    auto *historyStatus = new QLabel();
    historyStatus->setTextFormat(Qt::PlainText);
    historyStatus->setWordWrap(true);
    auto *historyButton = new QPushButton("Lich su");
    inputLayout->insertWidget(0, historyButton);
    mainLayout->insertWidget(1, historyStatus);
    QObject::connect(&history, &ChatHistory::storageStatus, &window,
        [historyStatus](const QString &text) { historyStatus->setText(text); });
    QObject::connect(&history, &ChatHistory::linkError, &window,
        [&](const QString &text) { QMessageBox::warning(&window, "Video", text); });
    QObject::connect(historyButton, &QPushButton::clicked, &window, [&] { history.showDialog(&window); });
    bool historyInitialized = false;
    historyButton->setEnabled(false);
    VideoClient videos(&channel, &socket,
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/ChatTCP_Videos");
    auto *videoStatus = new QLabel("Video MP4: toi da 1 GiB");
    videoStatus->setTextFormat(Qt::PlainText);
    auto *cancelVideo = new QPushButton("Huy video");
    cancelVideo->setEnabled(false);
    mainLayout->insertWidget(1, videoStatus);
    mainLayout->insertWidget(2, cancelVideo);
    QObject::connect(cancelVideo, &QPushButton::clicked, &videos, &VideoClient::cancel);
    QObject::connect(&videos, &VideoClient::progress, &window, [&](const QString &text) {
        videoStatus->setText(text);
        cancelVideo->setEnabled(videos.isSending());
    });
    QObject::connect(&videos, &VideoClient::failed, &window, [&](const QString &text) {
        videoStatus->setText("Video: " + text);
        cancelVideo->setEnabled(videos.isSending());
        chatBox->append("[Loi video] " + text.toHtmlEscaped());
    });
    QObject::connect(&videos, &VideoClient::historyReady, &history, &ChatHistory::completedVideo);
    QObject::connect(&videos, &VideoClient::completed, &window,
        [&](const QString &, const QString &, const QString &name, bool) {
            videoStatus->setText("Video hoan tat: " + name);
            cancelVideo->setEnabled(videos.isSending());
        });
    QString replyQuote;
    status->setTextFormat(Qt::PlainText);
    auto *reconnectButton = new QPushButton("Ket noi lai");
    auto *changeButton = new QPushButton("Dang xuat / Doi ket noi");
    auto *connectionBar = new QHBoxLayout();
    connectionBar->addWidget(reconnectButton);
    connectionBar->addWidget(changeButton);
    connectionBar->addStretch();
    mainLayout->insertLayout(1, connectionBar);
    QObject::connect(reconnectButton, &QPushButton::clicked, &connection, &SessionConnection::connectNow);
    QObject::connect(changeButton, &QPushButton::clicked, &window, [&] {
        if ((videos.isSending() || videos.incomingCount() > 0)
            && QMessageBox::question(&window, "Doi ket noi",
                "Dang truyen video. Doi ket noi se dung video dang truyen. Tiep tuc?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        changeRequested = true;
        window.close();
    });
    QObject::connect(&connection, &SessionConnection::changed, &window,
        [&](SessionConnection::State state, const QString &text) {
            status->setText(text);
            sendButton->setEnabled(state == SessionConnection::Ready);
            messageInput->setEnabled(state == SessionConnection::Ready);
            reconnectButton->setEnabled(state == SessionConnection::Idle);
            if (state == SessionConnection::Ready && !historyInitialized) {
                history.initialize(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/history");
                historyInitialized = true;
                historyButton->setEnabled(true);
            }
            if (state == SessionConnection::Idle) {
                onlineList->clear(); onlineLabel->setText("Online (0)");
                receiverBox->clear(); receiverBox->addItem("Phong chung", "");
                replyQuote.clear(); messageInput->setPlaceholderText("Nhap tin nhan...");
            }
        });
    QObject::connect(&connection, &SessionConnection::failed, &window, [&](const QString &text) {
        chatBox->append("[Loi ket noi] " + text.toHtmlEscaped());
    });

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
        channel.send(packet);
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

            videos.start(filePath, receiverBox->currentData().toString());
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
    // Nhan va xu ly du lieu JSON
    QObject::connect(
        &channel, &JsonChannel::packetReceived,
        &window, [&](const QJsonObject &packet) {
                if (connection.state() != SessionConnection::Ready && packet.value("type") != "joined" && packet.value("type") != "error") return;
                if (videos.handle(packet)) return;
                if (history.receive(packet)) return;
                QString type =
                    packet.value("type").toString();

                if (type == "joined") {
                    if (connection.state() == SessionConnection::Ready)
                        chatBox->append("[He thong] Dang nhap thanh cong!");
                }
                else if (type == "system") {

                    QString text =
                        packet.value("text").toString();

                    chatBox->append(
                        "[He thong] " +
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

                    // Connection status and retries are handled by SessionConnection.
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
    window.show();

    connection.connectNow();

    QApplication::exec();
    connection.stop();
    return changeRequested;
}



int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("ChatTCP1");
    app.setApplicationName("ChatClient");
    while (true) {
        ConnectionDialog dialog;
        if (dialog.exec() != QDialog::Accepted) break;
        if (!runChat(dialog.profile())) break;
    }
    return 0;
}
