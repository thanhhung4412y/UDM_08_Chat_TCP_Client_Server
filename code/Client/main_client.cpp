#include "chatshell.h"
#include "chattheme.h"
#include "connectiondialog.h"
#include "passworddialog.h"
#include "recoverydialog.h"
#include "authprotocol.h"
#include "avatarprotocol.h"
#include "groupimageprotocol.h"
#include <QImageReader>
#include <QTimer>
#include <QDateTime>
#include "sessionconnection.h"
#include "chathistory.h"
#include "recallclient.h"
#include "receiptclient.h"
#include "editmessagedialog.h"
#include "historyprotocol.h"
#include <QShortcut>
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
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHash>
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

    ChatShell window(username, profile.endpoint());
    auto *chatBox = window.chatBox;
    auto *messageInput = window.messageInput;
    auto *emojiButton = window.emojiButton;
    auto *stickerButton = window.stickerButton;
    auto *imageButton = window.imageButton;
    auto *videoButton = window.videoButton;
    auto *sendButton = window.sendButton;
    auto *replyButton = window.replyButton;
    auto *forwardButton = window.forwardButton;
    auto *receiverBox = window.receiverBox;
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
    auto *historyButton = window.historyButton;
    QObject::connect(&history, &ChatHistory::storageStatus, &window,
        [&](const QString &text) { window.setHistoryStatus(text); });
    QObject::connect(&history, &ChatHistory::linkError, &window,
        [&](const QString &text) { QMessageBox::warning(&window, "Video", text); });
    QObject::connect(historyButton, &QPushButton::clicked, &window, [&] { history.showDialog(&window); });
    QObject::connect(&history, &ChatHistory::conversationMessage, &window, &ChatShell::noteMessage);
    QString groupSearch;
    QObject::connect(window.searchButton, &QPushButton::clicked, &window, [&] {
        const QString key = receiverBox->currentData().toString();
        if (!key.startsWith("group:")) {
            history.showSearch(&window);
            return;
        }
        bool accepted = false;
        const QString query = QInputDialog::getText(&window, "Tìm trong nhóm", "Nội dung cần tìm",
                                                    QLineEdit::Normal, groupSearch, &accepted).trimmed();
        if (!accepted || query.isEmpty() || receiverBox->currentData().toString() != key) return;
        groupSearch = query;
        if (chatBox->find(query)) return;
        const auto previous = chatBox->textCursor();
        chatBox->moveCursor(QTextCursor::Start);
        if (chatBox->find(query)) return;
        chatBox->setTextCursor(previous);
        QMessageBox::information(&window, "Tìm trong nhóm", "Không tìm thấy trong lịch sử nhóm đang hiển thị.");
    });
    auto *searchShortcut=new QShortcut(QKeySequence::Find,&window);
    QObject::connect(searchShortcut,&QShortcut::activated,&window,[&]{ if(window.searchButton->isEnabled()) window.searchButton->click(); });
    bool historyInitialized = false;
    QString pendingGroupHistory;
    QList<QJsonObject> localGroupVideos;
    const auto appendGroupVideo = [&](const QJsonObject &video) {
        const QString author = video.value("outgoing").toBool() ? QStringLiteral("Bạn") : video.value("sender").toString();
        chatBox->append(QStringLiteral("<p><b>%1:</b> %2<br><a href='group-video:/%3'>▶ Mở video MP4</a></p>")
            .arg(author.toHtmlEscaped(), video.value("name").toString().toHtmlEscaped(),
                 video.value("_event_id").toString()));
    };
    const auto showLocalGroupVideos = [&](const QString &key) {
        for (const auto &video : localGroupVideos)
            if (video.value("to").toString() == key) appendGroupVideo(video);
    };
    QObject::connect(chatBox, &QTextBrowser::anchorClicked, &window, [&](const QUrl &url) {
        if (url.scheme() != "group-video") return;
        const QString key = receiverBox->currentData().toString();
        for (const auto &video : localGroupVideos) {
            if (video.value("to").toString() != key
                || url.path() != "/" + video.value("_event_id").toString()) continue;
            const QString path = video.value("local_path").toString();
            if (QFileInfo(path).isFile() && QDesktopServices::openUrl(QUrl::fromLocalFile(path))) return;
            break;
        }
        QMessageBox::warning(&window, "Video nhóm", "Không mở được tệp video trên máy này.");
    });
    const auto requestGroupHistory = [&] {
        const QString key = receiverBox->currentData().toString();
        if (!key.startsWith("group:") || !pendingGroupHistory.isEmpty()
            || connection.state() != SessionConnection::Ready) return;
        const QString id = key.mid(6);
        if (channel.send({{"type","group_history_request"},{"id",id}})) pendingGroupHistory = id;
    };
    QHash<QString, QJsonObject> groupMessageActions;
    QObject::connect(chatBox, &QTextBrowser::anchorClicked, &window, [&](const QUrl &url) {
        if (url.scheme() != "group-message" || connection.state() != SessionConnection::Ready) return;
        const QString key = receiverBox->currentData().toString();
        const QString messageId = url.path().mid(1);
        const auto message = groupMessageActions.value(messageId);
        if (!key.startsWith("group:") || message.isEmpty()) return;
        QMenu menu(&window);
        const QStringList emojis{"👍", "❤️", "😂"};
        for (const auto &emoji : emojis) menu.addAction(emoji)->setData(emoji);
        const bool own = AuthProtocol::canonical(message.value("sender").toString()) == AuthProtocol::canonical(username);
        QAction *edit = nullptr;
        QAction *recall = nullptr;
        if (own) {
            menu.addSeparator();
            if (message.contains("text")) edit = menu.addAction("Sửa tin nhắn");
            recall = menu.addAction("Thu hồi tin nhắn");
        }
        const auto *action = menu.exec(QCursor::pos());
        if (!action) return;
        QJsonObject update{{"type","group_message_update"},{"id",key.mid(6)},{"message_id",messageId}};
        if (action == edit) {
            bool accepted = false;
            const auto text = QInputDialog::getMultiLineText(&window, "Sửa tin nhắn", "Nội dung",
                message.value("text").toString(), &accepted).trimmed();
            if (!accepted || text.isEmpty()) return;
            if (text.toUtf8().size() > 4000) {
                QMessageBox::warning(&window, "Sửa tin nhắn", "Tin nhắn vượt giới hạn 4.000 byte.");
                return;
            }
            update["operation"] = "edit";
            update["text"] = text;
        } else if (action == recall) {
            if (QMessageBox::question(&window, "Thu hồi tin nhắn", "Thu hồi tin này với cả nhóm?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
            update["operation"] = "recall";
        } else {
            update["operation"] = "react";
            update["emoji"] = action->data().toString();
        }
        if (receiverBox->currentData().toString() == key && connection.state() == SessionConnection::Ready)
            channel.send(update);
    });
    const auto appendGroupMessage = [&](const QJsonObject &message) {
        const QString sender = message.value("sender").toString();
        if (sender.isEmpty()) return;
        QString body;
        if (message.value("recalled").toBool()) {
            body = QStringLiteral("<i>Tin nhắn đã được thu hồi</i>");
        } else if (message.contains("image")) {
            const QImage preview = GroupImageProtocol::decode(message.value("image").toString(), 240);
            if (preview.isNull()) {
                body = QStringLiteral("[Không đọc được ảnh nhóm]");
            } else {
                const QString url = "group-image:/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
                chatBox->document()->addResource(QTextDocument::ImageResource, QUrl(url), preview);
                body = QStringLiteral("<br><img src='%1' width='%2' height='%3'>")
                    .arg(url).arg(preview.width()).arg(preview.height());
            }
        } else if (message.contains("sticker")) {
            const QString sticker = message.value("sticker").toString();
            if (sticker != "nobita" && sticker != "doremon") return;
            body = QStringLiteral("<br><img src=':/stickers/%1.jpg' width='120'>").arg(sticker);
        } else {
            const QString text = message.value("text").toString();
            if (text.isEmpty() || text.toUtf8().size() > 4000) return;
            body = text.toHtmlEscaped().replace("\n", "<br>");
            const QString quote = message.value("quote").toString().left(500);
            if (!quote.isEmpty())
                body.prepend(QStringLiteral("<br><i>↪ %1</i><br>").arg(quote.toHtmlEscaped()));
        }
        const QString messageId = message.value("message_id").toString();
        if (VideoProtocol::validId(messageId) && !message.value("recalled").toBool()) {
            groupMessageActions.insert(messageId, {{"sender",sender}});
            if (message.contains("text")) groupMessageActions[messageId]["text"] = message.value("text");
            const auto reactions = message.value("reactions").toObject();
            for (const auto &emoji : QStringList{"👍", "❤️", "😂"}) {
                int count = 0;
                for (auto it = reactions.cbegin(); it != reactions.cend(); ++it)
                    if (it.value() == emoji) ++count;
                if (count) body += QStringLiteral(" · %1 %2").arg(emoji).arg(count);
            }
            if (message.value("edited").toBool()) body += QStringLiteral(" <i>Đã sửa</i>");
            body += QStringLiteral(" · <a href='group-message:/%1'>Thao tác</a>").arg(messageId);
        }
        const auto sentAt = QDateTime::fromString(message.value("utc").toString(), Qt::ISODateWithMs);
        chatBox->append(QStringLiteral("<p><span style='color:#769399'>%1</span> <b>%2:</b> %3</p>")
            .arg(sentAt.toLocalTime().toString("dd/MM HH:mm"), sender.toHtmlEscaped(), body));
    };
    QObject::connect(receiverBox, &QComboBox::currentIndexChanged, &window, [&](int) {
        const QString selected = receiverBox->currentData().toString();
        groupMessageActions.clear();
        history.selectConversation(selected.isEmpty() ? "__no_conversation__" : selected);
        if (selected.startsWith("group:")) {
            chatBox->clear();
            showLocalGroupVideos(selected);
            requestGroupHistory();
        }
    });

    RecallClient recall(&connection,&channel,&history);
    ReceiptClient receipts(&connection,&channel,&history);
    ReceiptVisibility receiptVisibility(&receipts,&window,receiverBox);
    QObject::connect(&receipts,&ReceiptClient::failed,&window,[&](const QString &text){window.setHistoryStatus(text);});
    QObject::connect(&history,&ChatHistory::recallRequested,&recall,&RecallClient::request);
    auto editMessage=[&](const QJsonObject &packet){
        if(packet.value("_recalled").toBool() || !HistoryProtocol::editableType(packet.value("type").toString())) return;
        EditMessageDialog dialog(&recall,&history,packet,&window);dialog.exec();
    };
    QObject::connect(&history,&ChatHistory::editRequested,&window,editMessage);
    QObject::connect(&recall,&RecallClient::failed,&window,[&](const QString &text){
        QMessageBox::warning(&window,"Cập nhật tin nhắn",text);
    });
    historyButton->setEnabled(false);
    VideoClient videos(&channel, &socket,
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/ChatTCP_Videos");
    auto *cancelVideo = window.cancelVideo;
    QObject::connect(cancelVideo, &QPushButton::clicked, &videos, &VideoClient::cancel);
    QObject::connect(&videos, &VideoClient::progress, &window, [&](const QString &text) {
        window.showTransfer(text, videos.isSending(), videos.incomingCount() > 0);
        cancelVideo->setEnabled(videos.isSending());
    });
    QObject::connect(&videos, &VideoClient::failed, &window, [&](const QString &text) {
        window.showTransfer("Video: " + text, videos.isSending(), videos.incomingCount() > 0);
        cancelVideo->setEnabled(videos.isSending());
        chatBox->append("<p style=\"color:#a34f45\">Video: " + text.toHtmlEscaped() + "</p>");
    });
    QObject::connect(&videos, &VideoClient::historyReady, &window, [&](const QJsonObject &video) {
        const QString key = video.value("to").toString();
        if (!VideoProtocol::groupTarget(key)) {
            history.completedVideo(video);
            return;
        }
        localGroupVideos.append(video);
        if (localGroupVideos.size() > 100) localGroupVideos.removeFirst();
        if (receiverBox->currentData().toString() == key) appendGroupVideo(video);
        else window.noteMessage(key, !video.value("outgoing").toBool());
    });
    QObject::connect(&videos, &VideoClient::completed, &window,
        [&](const QString &, const QString &, const QString &name, bool) {
            window.showTransfer("Đã hoàn tất: " + name, videos.isSending(), videos.incomingCount() > 0);
            cancelVideo->setEnabled(videos.isSending());
        });
    QString replyQuote;
    auto *reconnectButton = window.reconnectButton;
    auto *changeButton = window.changeButton;
    auto *passwordButton = window.passwordButton;
    QObject::connect(reconnectButton, &QPushButton::clicked, &connection, &SessionConnection::connectNow);
    QObject::connect(&connection,&SessionConnection::accountSetupFinished,&window,[&](const QString &text) {
        QMessageBox::information(&window,"Tài khoản",text);changeRequested=true;window.close();
    });
    QObject::connect(&connection,&SessionConnection::passwordReset,&window,[&] {
        QMessageBox::information(&window,"Khôi phục mật khẩu","Đã đặt mật khẩu mới. Mã dự phòng đã dùng và không dùng lại được. Đăng nhập lại rồi tạo mã mới; lịch sử vẫn được giữ.");
        changeRequested=true; window.close();
    });
    QObject::connect(window.recoveryButton,&QPushButton::clicked,&window,[&] {
        RecoveryDialog dialog(&connection,&window); dialog.exec();
    });
    QString avatarRequest;
    QTimer avatarTimer; avatarTimer.setSingleShot(true); avatarTimer.setInterval(30000);
    QObject::connect(&avatarTimer,&QTimer::timeout,&window,[&] { avatarRequest.clear(); QMessageBox::warning(&window,"Ảnh đại diện","Chưa xác nhận được việc lưu ảnh. Kết nối lại để kiểm tra trước khi thử lại."); });
    auto sendAvatar=[&](const QByteArray &bytes) {
        if(connection.state()!=SessionConnection::Ready || !avatarRequest.isEmpty()) return;
        avatarRequest=QUuid::createUuid().toString(QUuid::WithoutBraces); avatarTimer.start();
        if (!channel.send({{"type","avatar_set"},{"request_id",avatarRequest},{"data",QString::fromLatin1(bytes.toBase64())}})) { avatarTimer.stop(); avatarRequest.clear(); }
    };
    QObject::connect(window.avatarButton,&QPushButton::clicked,&window,[&] {
        if(!avatarRequest.isEmpty()) { QMessageBox::information(&window,"Ảnh đại diện","Đang chờ Server lưu ảnh trước."); return; }
        const QString path=QFileDialog::getOpenFileName(&window,"Chọn ảnh đại diện",{},"Ảnh (*.jpg *.jpeg *.png *.bmp *.webp)");
        if(path.isEmpty()) return;
        QImageReader reader(path); reader.setAutoTransform(true); const QSize size=reader.size();
        if(!QList<QByteArray>{"jpeg","png","bmp","webp"}.contains(reader.format().toLower()) || QFileInfo(path).size()>10*1024*1024 || !size.isValid() || size.width()>8192 || size.height()>8192 || qint64(size.width())*size.height()>32000000) {
            QMessageBox::warning(&window,"Ảnh đại diện","Chọn ảnh tối đa 10 MiB, 8192 pixel mỗi chiều và 32 triệu pixel."); return;
        }
        reader.setScaledSize(size.scaled(QSize(128,128),Qt::KeepAspectRatio));
        QImage image=reader.read();
        if(image.isNull()) { QMessageBox::warning(&window,"Ảnh đại diện","Không đọc được ảnh."); return; }
        const int edge=qMin(image.width(),image.height()); image=image.copy((image.width()-edge)/2,(image.height()-edge)/2,edge,edge).scaled(128,128,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"JPEG",80);
        if(AvatarProtocol::decode(bytes).isNull()) { QMessageBox::warning(&window,"Ảnh đại diện","Không tạo được ảnh thu nhỏ hợp lệ."); return; }
        if(QMessageBox::question(&window,"Ảnh đại diện","Dùng ảnh này làm ảnh đại diện? Ảnh sẽ cắt vuông ở giữa và hiển thị cho người đang online.")==QMessageBox::Yes) sendAvatar(bytes);
    });
    QObject::connect(window.removeAvatarButton,&QPushButton::clicked,&window,[&] { sendAvatar({}); });
    QObject::connect(&channel,&JsonChannel::packetReceived,&window,[&](const QJsonObject &packet) {
        if(packet.value("type")=="avatar") {
            const QString name=packet.value("name").toString(), encoded=packet.value("data").toString();
            const auto bytes=AvatarProtocol::fromBase64(encoded);
            if(AuthProtocol::validName(name) && (encoded.isEmpty() || !bytes.isEmpty())) window.setAvatar(name,bytes);
        } else if(packet.value("type")=="avatar_result" && !avatarRequest.isEmpty() && packet.value("request_id").toString()==avatarRequest) {
            avatarTimer.stop(); avatarRequest.clear();
            if(!packet.value("ok").toBool()) QMessageBox::warning(&window,"Ảnh đại diện",packet.value("text").toString().left(300));
        }
    });
    QObject::connect(passwordButton, &QPushButton::clicked, &window, [&] {
        PasswordDialog dialog(&connection, [&] {
            return connection.state() == SessionConnection::Ready && !videos.isSending() && videos.incomingCount() == 0;
        }, &window);
        if (dialog.exec() == QDialog::Accepted) {
            QMessageBox::information(&window, "Doi mat khau", "Da doi mat khau. Hay dang nhap lai bang mat khau moi. Lich su van duoc giu nguyen.");
            changeRequested = true; window.close();
        }
    });
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
            window.setConnectionState(state == SessionConnection::Ready, state == SessionConnection::Connecting || state == SessionConnection::Registering, text);
            reconnectButton->setEnabled(state == SessionConnection::Idle);
            passwordButton->setEnabled(state == SessionConnection::Ready);
            if (state == SessionConnection::Ready && !historyInitialized) {
                history.initialize(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/history");
                historyInitialized = true;
                if(receiverBox->currentData().toString().isEmpty())
                    history.selectConversation("__no_conversation__");
                historyButton->setEnabled(true); window.searchButton->setEnabled(true);
            }
            if (state == SessionConnection::Idle) {
                pendingGroupHistory.clear();
                window.setUsers({});
                replyQuote.clear(); messageInput->setPlaceholderText("Nhập tin nhắn…");
            }
        });
    QObject::connect(&connection, &SessionConnection::failed, &window, [&](const QString &text) {
        chatBox->append("<p style=\"color:#a34f45\">Kết nối: " + text.toHtmlEscaped() + "</p>");
    });

    QObject::connect(
        receiverBox,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        &window,
        [&](int) {
            replyQuote.clear();
            messageInput->setPlaceholderText(
                "Nhập tin nhắn…"
                );
        }
        );



    // Ham gui JSON
    auto sendPacket = [&](const QJsonObject &packet) {
        channel.send(packet);
    };
    window.groupList->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(window.groupList, &QListWidget::customContextMenuRequested, &window, [&](const QPoint &pos) {
        const auto *item = window.groupList->itemAt(pos);
        if (!item) return;
        const QString id = item->data(Qt::UserRole).toString().mid(6);
        const QStringList members = item->data(Qt::UserRole + 2).toStringList();
        if (members.isEmpty()) return;
        QMenu menu(&window);
        auto *showMembers = menu.addAction("Xem thành viên");
        auto *renameGroup = menu.addAction("Đổi tên nhóm");
        auto *invite = menu.addAction("Thêm thành viên");
        const bool owner = AuthProtocol::canonical(members.first()) == AuthProtocol::canonical(username);
        renameGroup->setEnabled(owner);
        invite->setEnabled(owner && members.size() < 16);
        const auto *action = menu.exec(window.groupList->viewport()->mapToGlobal(pos));
        if (action == showMembers) {
            QStringList labels = members;
            labels.first() += " (người tạo)";
            QMessageBox::information(&window, "Thành viên nhóm", labels.join('\n'));
        } else if (action == invite) {
            QStringList candidates;
            for (int i = 0; i < window.onlineList->count(); ++i) {
                const auto *entry = window.onlineList->item(i);
                const QString person = entry->data(Qt::UserRole).toString();
                if (!entry->text().contains(" · Ngoại tuyến") && !members.contains(person, Qt::CaseInsensitive))
                    candidates.append(person);
            }
            if (candidates.isEmpty()) {
                QMessageBox::information(&window, "Thêm thành viên", "Chưa có tài khoản phù hợp đang online.");
                return;
            }
            bool accepted = false;
            const QString person = QInputDialog::getItem(&window, "Thêm thành viên",
                "Thành viên mới có thể xem lịch sử nhóm đã lưu.", candidates, 0, false, &accepted);
            if (accepted && connection.state() == SessionConnection::Ready)
                sendPacket({{"type","group_invite"},{"id",id},{"name",person}});
        } else if (action == renameGroup) {
            bool accepted = false;
            const QString name = QInputDialog::getText(&window, "Đổi tên nhóm", "Tên mới",
                                                       QLineEdit::Normal, QString(), &accepted).trimmed();
            if (accepted && connection.state() == SessionConnection::Ready)
                sendPacket({{"type","group_rename"},{"id",id},{"name",name}});
        }
    });

    // Groups: creator selects the exact online participants; server validates membership.
    QObject::connect(window.createGroupButton,&QPushButton::clicked,&window,[&] {
        if(connection.state()!=SessionConnection::Ready) return;
        QDialog dialog(&window);dialog.setWindowTitle("Tạo nhóm chat riêng");dialog.resize(410,470);
        auto *layout=new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel("Tên nhóm (tối đa 48 ký tự):"));
        auto *name=new QLineEdit();name->setMaxLength(48);name->setPlaceholderText("Ví dụ: Nhóm đồ án ChatTCP");layout->addWidget(name);
        auto *hint=new QLabel("Chọn người đang online để mời vào nhóm. Bạn được thêm tự động.");
        hint->setWordWrap(true);layout->addWidget(hint);
        auto *people=new QListWidget();layout->addWidget(people);
        for(int i=0;i<window.onlineList->count();++i) {
            auto *online=window.onlineList->item(i);
            const QString person=online->data(Qt::UserRole).toString();
            if(person.isEmpty() || person.compare(username,Qt::CaseInsensitive)==0 ||
               online->text().contains(" · Ngoại tuyến")) continue;
            auto *entry=new QListWidgetItem(person,people);
            entry->setFlags(entry->flags()|Qt::ItemIsUserCheckable);
            entry->setCheckState(Qt::Unchecked);
        }
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText("Tạo nhóm");
        QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
        QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        layout->addWidget(buttons);
        if(dialog.exec()!=QDialog::Accepted) return;
        QJsonArray members;
        for(int i=0;i<people->count();++i) if(people->item(i)->checkState()==Qt::Checked)
            members.append(people->item(i)->text());
        if(name->text().trimmed().isEmpty() || members.isEmpty()) {
            QMessageBox::warning(&window,"Tạo nhóm","Hãy nhập tên nhóm và chọn ít nhất một người khác.");return;
        }
        sendPacket({{"type","group_create"},{"name",name->text().trimmed()},{"members",members}});
    });

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

            const QString receiver = receiverBox->currentData().toString();
            const bool group = receiver.startsWith("group:");
            const QString filePath = QFileDialog::getOpenFileName(
                &window,
                "Chon anh de gui",
                QString(),
                "Images (*.jpg *.jpeg *.png *.bmp)"
                );

            if (filePath.isEmpty() || receiver != receiverBox->currentData().toString()
                || connection.state() != SessionConnection::Ready)
                return;

            QImage image(filePath);

            if (image.isNull()) {
                QMessageBox::warning(
                    &window, "Loi",
                    "Khong doc duoc file anh"
                    );
                return;
            }

            const int maxSide = group ? GroupImageProtocol::MaxSide : 1280;
            if (image.width() > maxSide || image.height() > maxSide)
                image = image.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);

            const auto encodeJpeg = [](const QImage &source, int quality) {
                QByteArray bytes;
                QBuffer buffer(&bytes);
                if (!buffer.open(QIODevice::WriteOnly) || !source.save(&buffer, "JPEG", quality))
                    return QByteArray();
                return bytes;
            };
            QByteArray imageBytes = encodeJpeg(image, 75);
            if (group && imageBytes.size() > GroupImageProtocol::MaxBytes) {
                if (image.width() > 640 || image.height() > 640)
                    image = image.scaled(640, 640, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imageBytes = encodeJpeg(image, 55);
            }
            if (imageBytes.isEmpty()) {
                QMessageBox::warning(
                    &window, "Loi",
                    "Khong nen duoc anh"
                    );
                return;
            }

            if (imageBytes.size() > (group ? GroupImageProtocol::MaxBytes : 256 * 1024)) {
                QMessageBox::warning(
                    &window, "Loi",
                    group ? "Ảnh nhóm sau khi nén vẫn vượt 96 KiB." : "Anh sau khi nen van qua lon"
                    );
                return;
            }

            const QString encoded = QString::fromLatin1(imageBytes.toBase64());
            if (group)
                sendPacket({{"type","group_image"},{"id",receiver.mid(6)},{"data",encoded}});
            else
                sendPacket({{"type","image"},{"to",receiver},{"data",encoded}});

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

            const QString receiver = receiverBox->currentData().toString();
            const QString filePath = QFileDialog::getOpenFileName(
                &window,
                "Chon video MP4",
                QString(),
                "Video MP4 (*.mp4)"
                );

            if (filePath.isEmpty())
                return;

            if (connection.state() == SessionConnection::Ready && receiver == receiverBox->currentData().toString())
                videos.start(filePath, receiver);
        }
        );

    // Nhan nut Sticker
    QObject::connect(
        stickerButton, &QPushButton::clicked,
        &window, [&]() {

            if (!sendButton->isEnabled())
                return;

            const QString recipient = receiverBox->currentData().toString();
            QDialog dialog(&window);
            dialog.setWindowTitle("Chọn sticker");

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

                        if (connection.state() == SessionConnection::Ready
                            && recipient == receiverBox->currentData().toString()) {
                            if (recipient.startsWith("group:"))
                                sendPacket({{"type","group_sticker"},{"id",recipient.mid(6)},{"sticker",id}});
                            else
                                sendPacket({{"type","sticker"},{"to",recipient},{"id",id}});
                        }

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
                if(packet.value("type")=="group_created") {
                    const QString id=packet.value("id").toString();
                    const QString name=packet.value("name").toString();
                    if(QUuid(id).isNull() || name.isEmpty()) return;
                    QStringList members;
                    for(const auto &v:packet.value("members").toArray()) members.append(v.toString());
                    const bool existing = packet.value("existing").toBool();
                    window.addGroup(id, name, members, !existing);
                    if (existing && receiverBox->currentData().toString() == "group:" + id)
                        requestGroupHistory();
                    return;
                }
                if (packet.value("type") == "group_history_changed") {
                    if (receiverBox->currentData().toString() == "group:" + packet.value("id").toString())
                        requestGroupHistory();
                    return;
                }
                if (packet.value("type") == "group_history") {
                    const QString id = packet.value("id").toString();
                    if (pendingGroupHistory.isEmpty() || id != pendingGroupHistory) return;
                    pendingGroupHistory.clear();
                    if (receiverBox->currentData().toString() != "group:" + id) {
                        requestGroupHistory();
                        return;
                    }
                    const QString error = packet.value("error").toString();
                    if (!error.isEmpty()) {
                        chatBox->append(error.toHtmlEscaped());
                        return;
                    }
                    // The server sends the snapshot before any subsequent live messages.
                    chatBox->clear();
                    groupMessageActions.clear();
                    const auto messages = packet.value("messages").toArray();
                    for (const auto &message : messages) appendGroupMessage(message.toObject());
                    showLocalGroupVideos("group:" + id);
                    return;
                }
                if (packet.value("type") == "group_chat") {
                    const QString id = packet.value("id").toString();
                    if (QUuid(id).isNull()) return;
                    const QString key = "group:" + id;
                    if (receiverBox->currentData().toString() == key) appendGroupMessage(packet);
                    else window.noteMessage(key, packet.value("sender").toString().compare(username, Qt::CaseInsensitive) != 0);
                    return;
                }
                if(packet.value("type")=="message_recalled" || packet.value("type")=="message_edited" || packet.value("type")=="message_receipt") return; // RecallClient updates storage and every open view.
                if (history.receive(packet)) return;
                QString type =
                    packet.value("type").toString();

                if (type == "joined") {
                    if (connection.state() == SessionConnection::Ready)
                        chatBox->append("<p align=\"center\" style=\"font-size:11px;color:#7b929d\">Đã đăng nhập thành công</p>");
                }
                else if (type == "system") {

                    QString text =
                        packet.value("text").toString();

                    chatBox->append(
                        "<span style=\"color:#7b929d;font-size:11px\">Thông báo:</span> " +
                        text.toHtmlEscaped()
                        );
                }

                else if (type == "users") {

                    QStringList users;
                    for (const auto &user : packet.value("names").toArray()) users.append(user.toString());
                    window.setUsers(users);
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

            if (receiverBox->currentData().toString().isEmpty()) {

                QMessageBox::information(
                    &window,
                    "Thong bao",
                    "Hãy chọn một cuộc trò chuyện để trả lời."
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
                "Đang trả lời: " + replyQuote.left(40)
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
            if(receiver.startsWith("group:")) {
                sendPacket({{"type","group_chat"},{"id",receiver.mid(6)},
                    {"text","↪ Chuyển tiếp: "+selected}});
            } else if(!receiver.isEmpty()) {
                sendPacket({{"type","forward"},{"to",receiver},{"text",selected}});
            } else QMessageBox::information(&window,"Chuyển tiếp","Hãy chọn người hoặc nhóm nhận.");
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
                menu->addAction("Trả lời (Reply)");

            QAction *forwardAction =
                menu->addAction("Chuyển tiếp (Forward)");

            const auto candidate=history.recallCandidate(chatBox->cursorForPosition(pos).position());
            auto *recallAction=menu->addAction("Thu hồi tin nhắn của tôi");
            recallAction->setEnabled(connection.state()==SessionConnection::Ready && !candidate.isEmpty());
            QObject::connect(recallAction,&QAction::triggered,menu,[&,candidate]{
                if(QMessageBox::question(&window,"Thu hồi tin nhắn","Thu hồi tin này cho cuộc trò chuyện? Các bản sao hoặc tệp người nhận đã tải riêng vẫn có thể còn.",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes)
                    recall.request(candidate.value("_event_id").toString());
            });
            auto *editAction=menu->addAction("Chỉnh sửa tin nhắn của tôi");
            editAction->setEnabled(recallAction->isEnabled() && HistoryProtocol::editableType(candidate.value("type").toString()));
            QObject::connect(editAction,&QAction::triggered,menu,[&,candidate]{editMessage(candidate);});
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

        if (receiver.startsWith("group:")) {
            QJsonObject message{{"type","group_chat"},{"id",receiver.mid(6)},{"text",text}};
            if (!replyQuote.isEmpty()) message["quote"] = replyQuote;
            sendPacket(message);
        } else if (!receiver.isEmpty() && !replyQuote.isEmpty()) {
            sendPacket({{"type","reply"},{"to",receiver},{"text",text},{"quote",replyQuote}});
        } else if (!receiver.isEmpty()) {

            sendPacket({
                {"type", "private"},
                {"to", receiver},
                {"text", text}
            });

        } else {
            QMessageBox::information(&window,"Chọn cuộc trò chuyện",
                                     "Hãy chọn người nhận hoặc tạo nhóm trước khi gửi.");
            return;
        }

        replyQuote.clear();
        messageInput->clear();
        messageInput->setPlaceholderText("Nhập tin nhắn…");

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
    app.setWindowIcon(QIcon(":/branding/chat-logo.png"));
    app.setOrganizationName("ChatTCP1");
    app.setApplicationName("ChatClient");
    ChatTheme::apply(app);
    while (true) {
        ConnectionDialog dialog;
        if (dialog.exec() != QDialog::Accepted) break;
        if (!runChat(dialog.profile())) break;
    }
    return 0;
}


