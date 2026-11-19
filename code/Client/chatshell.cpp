#include "chatshell.h"
#include "avatarprotocol.h"
#include <QPainterPath>
#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

class RecipientCombo : public QComboBox {
public:
    using QComboBox::QComboBox;
protected:
    void paintEvent(QPaintEvent *event) override {
        QComboBox::paintEvent(event);
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(isEnabled() ? QColor("#6a8792") : QColor("#b3c1c8"), 1.5));
        const qreal x = width() - 15, y = height() / 2.0;
        p.drawPolyline(QPolygonF({QPointF(x - 4, y - 2), QPointF(x, y + 2), QPointF(x + 4, y - 2)}));
    }
};

static QLabel *label(const QString &text, const char *style, QWidget *parent = nullptr) {
    auto *widget = new QLabel(text, parent); widget->setObjectName(style); widget->setTextFormat(Qt::PlainText); return widget;
}
static QIcon personIcon(const QString &name) {
    QPixmap image(36, 36); image.fill(Qt::transparent); QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen); painter.setBrush(QColor("#e3f1ed")); painter.drawEllipse(0, 0, 36, 36);
    painter.setPen(QColor("#14786c")); painter.setFont(QFont("Segoe UI", 11, QFont::DemiBold)); painter.drawText(image.rect(), Qt::AlignCenter, name.left(1).toUpper());
    return QIcon(image);
}
static QIcon smileIcon() {
    QPixmap image(48, 48); image.setDevicePixelRatio(2); image.fill(Qt::transparent);
    QPainter p(&image); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(QColor("#547380"), 1.6));
    p.drawEllipse(QRectF(3, 3, 18, 18)); p.drawPoint(QPointF(8.5, 9)); p.drawPoint(QPointF(15.5, 9));
    p.drawArc(QRectF(7, 8, 10, 9), 200 * 16, 140 * 16); return QIcon(image);
}
ChatShell::ChatShell(QString name, QString endpoint, QWidget *parent) : QWidget(parent), owner(std::move(name)) {
    setObjectName("ChatShell"); setWindowTitle("ChatTCP — " + owner + " @ " + endpoint); setMinimumSize(760, 560);
    const auto area = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->availableGeometry().size() : QSize(1200, 850);
    resize(qMax(760, qMin(1120, area.width() - 48)), qMax(560, qMin(760, area.height() - 70)));
    auto *root = new QHBoxLayout(this); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    auto *split = new QSplitter(Qt::Horizontal); split->setChildrenCollapsible(false); root->addWidget(split);
    auto *sidebar = new QFrame(); sidebar->setObjectName("Sidebar"); sidebar->setMinimumWidth(200); sidebar->setMaximumWidth(290);
    auto *side = new QVBoxLayout(sidebar); side->setContentsMargins(18, 25, 18, 18); side->setSpacing(12);
    auto *brandRow = new QHBoxLayout(); auto *brandIcon = label("C", "Avatar"); brandIcon->setFixedSize(40, 40); brandIcon->setAlignment(Qt::AlignCenter);
    brandRow->addWidget(brandIcon); auto *brandWords = new QVBoxLayout(); brandWords->setSpacing(2);
    brandWords->addWidget(label("ChatTCP", "Brand")); brandWords->addWidget(label("Kết nối trong mạng LAN", "Muted")); brandRow->addLayout(brandWords); side->addLayout(brandRow);
    side->addSpacing(18);
    auto *groupHeader = new QHBoxLayout();groupHeader->addWidget(label("NHÓM CỦA TÔI", "SectionLabel"));
    createGroupButton=new QPushButton("+ Tạo nhóm");createGroupButton->setObjectName("CreateGroup");
    groupHeader->addWidget(createGroupButton);side->addLayout(groupHeader);
    groupList=new QListWidget();groupList->setObjectName("GroupList");
    groupList->setMaximumHeight(165);groupList->setMinimumHeight(65);side->addWidget(groupList);

    onlineLabel = label("ĐANG TRỰC TUYẾN · 0", "SectionLabel"); side->addSpacing(12); side->addWidget(onlineLabel);
    onlineList = new QListWidget(); onlineList->setObjectName("PeopleList"); onlineList->setIconSize(QSize(36, 36)); onlineList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); side->addWidget(onlineList, 1);
    auto *self = new QHBoxLayout(); selfAvatar = label(owner.left(1).toUpper(), "Avatar"); selfAvatar->setFixedSize(40, 40); selfAvatar->setAlignment(Qt::AlignCenter); self->addWidget(selfAvatar);
    auto *selfText = new QVBoxLayout(); auto *selfName = label(owner, ""); selfName->setWordWrap(true); selfName->setStyleSheet("font-weight:600"); selfText->addWidget(selfName);
    auto *endpointLabel = label(endpoint, "Muted"); endpointLabel->setWordWrap(true); selfText->addWidget(endpointLabel); self->addLayout(selfText); side->addLayout(self);
    historyStatus = label("Lịch sử sẽ tải sau đăng nhập", "Muted"); historyStatus->setWordWrap(true); side->addWidget(historyStatus);
    auto *main = new QWidget(); auto *column = new QVBoxLayout(main); column->setContentsMargins(0, 0, 0, 0); column->setSpacing(0);
    auto *header = new QFrame(); header->setObjectName("ChatHeader"); auto *headerLayout = new QHBoxLayout(header); headerLayout->setContentsMargins(22, 18, 22, 18); headerLayout->setSpacing(12);
    avatar = label("✉", "Avatar"); avatar->setAlignment(Qt::AlignCenter); avatar->setFixedSize(40, 40); headerLayout->addWidget(avatar);
    auto *heading = new QVBoxLayout(); heading->setSpacing(3); title = label("Chọn cuộc trò chuyện", "Title"); title->setWordWrap(true); title->setMinimumWidth(0);
    subtitle = label("Chọn người bên trái hoặc tạo nhóm riêng", "Muted"); subtitle->setWordWrap(true); heading->addWidget(title); heading->addWidget(subtitle); headerLayout->addLayout(heading, 1);
    searchButton=new QPushButton("Tìm tin"); searchButton->setObjectName("searchMessages"); searchButton->setToolTip("Tìm tin nhắn trong cuộc trò chuyện · Ctrl+F"); searchButton->setEnabled(false); headerLayout->addWidget(searchButton);
    historyButton = new QPushButton("Lịch sử"); historyButton->setToolTip("Xem lại tin nhắn đã lưu trên máy"); historyButton->setEnabled(false); headerLayout->addWidget(historyButton);
    auto *accountButton = new QToolButton(); accountButton->setText("Tài khoản"); accountButton->setToolTip("Đổi mật khẩu, kết nối lại hoặc đăng xuất"); accountButton->setPopupMode(QToolButton::InstantPopup); auto *accountMenu = new QMenu(accountButton);
    reconnectButton = new QPushButton(this); changeButton = new QPushButton(this); passwordButton = new QPushButton(this);
    recoveryButton=new QPushButton(this); avatarButton=new QPushButton(this); removeAvatarButton=new QPushButton(this);
    for (auto *button : {reconnectButton, changeButton, passwordButton, recoveryButton, avatarButton, removeAvatarButton}) button->hide();
    reconnectAction = accountMenu->addAction("Kết nối lại"); passwordAction = accountMenu->addAction("Đổi mật khẩu");
    recoveryAction=accountMenu->addAction("Tạo mã dự phòng");
    avatarAction=accountMenu->addAction("Đổi ảnh đại diện"); removeAvatarAction=accountMenu->addAction("Xóa ảnh đại diện");
    connect(recoveryAction,&QAction::triggered,recoveryButton,&QPushButton::click);
    connect(avatarAction,&QAction::triggered,avatarButton,&QPushButton::click);
    connect(removeAvatarAction,&QAction::triggered,removeAvatarButton,&QPushButton::click);
    accountMenu->addSeparator(); auto *logout = accountMenu->addAction("Đăng xuất / Đổi máy chủ");
    connect(reconnectAction, &QAction::triggered, reconnectButton, &QPushButton::click);
    connect(passwordAction, &QAction::triggered, passwordButton, &QPushButton::click); connect(logout, &QAction::triggered, changeButton, &QPushButton::click);
    accountButton->setMenu(accountMenu); headerLayout->addWidget(accountButton); column->addWidget(header);
    auto *body = new QVBoxLayout(); body->setContentsMargins(20, 12, 20, 18); body->setSpacing(10); column->addLayout(body, 1);
    auto *statusRow = new QHBoxLayout(); status = label("Đang kết nối", "StatusBadge"); status->setProperty("connected", false); statusRow->addWidget(status); statusRow->addStretch();
    statusRow->addWidget(label("Ảnh, sticker & video đến 1 GiB", "Muted")); body->addLayout(statusRow);
    notice = label("Đang kết nối tới máy chủ…", "Notice"); notice->setWordWrap(true); body->addWidget(notice);
    chatBox = new QTextBrowser(); chatBox->setObjectName("Messages"); chatBox->setReadOnly(true); chatBox->setOpenLinks(false); chatBox->setOpenExternalLinks(false);
    chatBox->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); chatBox->setMinimumWidth(0); body->addWidget(chatBox, 1);
    transferCard = new QFrame(); transferCard->setObjectName("TransferCard"); auto *transfer = new QHBoxLayout(transferCard);
    videoStatus = label("", ""); videoStatus->setWordWrap(true); transfer->addWidget(videoStatus, 1);
    cancelVideo = new QPushButton("Hủy gửi"); cancelVideo->setEnabled(false); transfer->addWidget(cancelVideo); transferCard->hide(); body->addWidget(transferCard);
    auto *composer = new QFrame(); composer->setObjectName("Composer"); auto *compose = new QVBoxLayout(composer); compose->setContentsMargins(14, 12, 14, 12); compose->setSpacing(9);
    auto *to = new QHBoxLayout(); to->addWidget(label("GỬI ĐẾN", "SectionLabel")); receiverBox = new RecipientCombo(); receiverBox->setObjectName("RecipientBox"); receiverBox->addItem("Chọn cuộc trò chuyện…", ""); receiverBox->setMinimumWidth(170); receiverBox->setMaximumWidth(250); to->addWidget(receiverBox); to->addStretch(); compose->addLayout(to);
    auto *input = new QHBoxLayout(); input->setSpacing(8);
    mediaButton = new QToolButton(); mediaButton->setObjectName("IconButton"); mediaButton->setFixedSize(40, 40); mediaButton->setText("+"); mediaButton->setToolTip("Gửi ảnh hoặc video"); mediaButton->setPopupMode(QToolButton::InstantPopup);
    emotionButton = new QToolButton(); emotionButton->setObjectName("IconButton"); emotionButton->setFixedSize(40, 40); emotionButton->setIcon(smileIcon()); emotionButton->setIconSize(QSize(24,24)); emotionButton->setToolTip("Emoji và sticker"); emotionButton->setPopupMode(QToolButton::InstantPopup);
    emojiButton = new QPushButton(this); stickerButton = new QPushButton(this); imageButton = new QPushButton(this); videoButton = new QPushButton(this); replyButton = new QPushButton(this); forwardButton = new QPushButton(this);
    for (auto *button : {emojiButton, stickerButton, imageButton, videoButton, replyButton, forwardButton}) button->hide();
    auto *media = new QMenu(mediaButton);
    media->addAction("Gửi ảnh", imageButton, &QPushButton::click);
    media->addAction("Gửi video MP4", videoButton, &QPushButton::click);
    mediaButton->setMenu(media);
    auto *emotions = new QMenu(emotionButton); emotions->addAction("Chọn emoji", emojiButton, &QPushButton::click); emotions->addAction("Chọn sticker", stickerButton, &QPushButton::click); emotionButton->setMenu(emotions);
    messageInput = new QLineEdit(); messageInput->setObjectName("MessageInput"); messageInput->setPlaceholderText("Nhập tin nhắn…"); messageInput->setMinimumWidth(70);
    sendButton = new QPushButton("Gửi  →"); sendButton->setObjectName("SendButton"); sendButton->setMinimumWidth(75);
    input->addWidget(mediaButton); input->addWidget(emotionButton); input->addWidget(messageInput, 1); input->addWidget(sendButton); compose->addLayout(input);
    auto *hintRow = new QHBoxLayout(); auto *usageHint = label("Bôi đen tin nhắn, nhấp phải để trả lời / chuyển tiếp", "Muted"); usageHint->setWordWrap(true); usageHint->setMinimumWidth(0); hintRow->addWidget(usageHint, 1); hintRow->addWidget(label("Enter để gửi", "Muted")); compose->addLayout(hintRow); body->addWidget(composer);
    split->addWidget(sidebar); split->addWidget(main); split->setStretchFactor(0, 0); split->setStretchFactor(1, 1); split->setSizes({245, 875});
    connect(groupList,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){
        const int index=receiverBox->findData(item->data(Qt::UserRole));
        if(index>=0) {receiverBox->setCurrentIndex(index);messageInput->setFocus();}
    });
    connect(onlineList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const int index = receiverBox->findData(item->data(Qt::UserRole));
        if (index >= 0) { receiverBox->setCurrentIndex(index); messageInput->setFocus(); }
    });
    connect(receiverBox, &QComboBox::currentIndexChanged, this, [this] { updateRecipient(); });
    setConnectionState(false, true, "Đang kết nối tới " + endpoint);
}
void ChatShell::updateRecipient() {
    const QString recipient=receiverBox->currentData().toString();
    const bool group=recipient.startsWith("group:");
    const bool empty=recipient.isEmpty();
    const QString display=receiverBox->currentText();
    title->setText(empty ? "Chọn cuộc trò chuyện" : group ? display : recipient);
    if(!empty) unread[recipient]=0;
    refreshBadges();
    subtitle->setText(empty ? "Chọn một người hoặc tạo nhóm để bắt đầu"
        : group ? "Nhóm riêng · " + QString::number(groupMembers.value(recipient).size())+" thành viên"
        : QStringLiteral("Cuộc trò chuyện riêng · ") + (onlineUsers.contains(recipient)
            ? QStringLiteral("Trực tuyến") : QStringLiteral("Ngoại tuyến")));
    avatar->setPixmap(QPixmap());avatar->setText(group?"✦":empty?"✉":recipient.left(1).toUpper());
    if(!group && !empty && avatarImages.contains(recipient.toCaseFolded()))
        avatar->setPixmap(avatarImages.value(recipient.toCaseFolded()).scaled(40,40,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    onlineList->clearSelection();groupList->clearSelection();
    for(int i=0;i<onlineList->count();++i)
        if(onlineList->item(i)->data(Qt::UserRole).toString()==recipient) onlineList->item(i)->setSelected(true);
    for(int i=0;i<groupList->count();++i)
        if(groupList->item(i)->data(Qt::UserRole).toString()==recipient) groupList->item(i)->setSelected(true);
    imageButton->setEnabled(!empty);
    videoButton->setEnabled(!empty);
    stickerButton->setEnabled(!empty);
    mediaButton->setEnabled(!empty && status->property("connected").toBool());
    emotionButton->setEnabled(!empty && status->property("connected").toBool());
    sendButton->setEnabled(!empty && status->property("connected").toBool());
    messageInput->setEnabled(!empty && status->property("connected").toBool());
}
void ChatShell::addGroup(const QString &id,const QString &name,const QStringList &members,bool openGroup) {
    const QString key="group:"+id;
    groupMembers[key]=members;
    const QString display="👥 "+name;
    int index=receiverBox->findData(key);
    if(index<0) receiverBox->addItem(display,key);
    else receiverBox->setItemText(index,display);
    bool exists=false;
    for(int i=0;i<groupList->count();++i) {
        auto *item=groupList->item(i);
        if(item->data(Qt::UserRole).toString()==key) {item->setText(display);item->setData(Qt::UserRole+1,display);item->setData(Qt::UserRole+2,members);exists=true;break;}
    }
    if(!exists) {auto *item=new QListWidgetItem(display,groupList);item->setData(Qt::UserRole,key);item->setData(Qt::UserRole+1,display);item->setData(Qt::UserRole+2,members);}
    if (openGroup) receiverBox->setCurrentIndex(receiverBox->findData(key));
    if (receiverBox->currentData().toString() == key) updateRecipient();
}
void ChatShell::setUsers(const QStringList &users) {
    onlineUsers=users;
    for(const auto &name:users) if(name.compare(owner,Qt::CaseInsensitive)!=0 && !knownPeers.contains(name)) knownPeers.append(name);
    const QString previous=receiverBox->currentData().toString();
    { QSignalBlocker blocker(receiverBox);onlineList->clear();receiverBox->clear();
      receiverBox->addItem("Chọn cuộc trò chuyện…", "");
      for(const QString &name:knownPeers) {
          auto *item=new QListWidgetItem(iconFor(name),name+(users.contains(name)?"":" · Ngoại tuyến"),onlineList);
          item->setData(Qt::UserRole,name);
          if(name==owner) item->setFlags(Qt::ItemIsEnabled);
          else receiverBox->addItem(name,name);
      }
      for(int i=0;i<groupList->count();++i) {
          const auto *item=groupList->item(i);
          receiverBox->addItem(item->data(Qt::UserRole+1).toString(),item->data(Qt::UserRole));
      }
      const int index=receiverBox->findData(previous);receiverBox->setCurrentIndex(index>=0?index:0);
    }
    onlineLabel->setText(QString("ĐANG TRỰC TUYẾN · %1").arg(users.size()));updateRecipient();
    if(previous!=receiverBox->currentData().toString()) emit receiverBox->currentIndexChanged(receiverBox->currentIndex());
}
void ChatShell::setConnectionState(bool ready, bool connecting, const QString &detail) {
    for (auto *button : {recoveryButton,avatarButton,removeAvatarButton}) button->setEnabled(ready);
    for (auto *action : {recoveryAction,avatarAction,removeAvatarAction}) action->setEnabled(ready);
    status->setText(ready ? "●  Đã kết nối · TLS" : connecting ? "●  Đang kết nối" : "●  Ngoại tuyến"); status->setToolTip(detail);
    status->setProperty("connected", ready); status->style()->unpolish(status); status->style()->polish(status);
    notice->setText(detail); notice->setVisible(!ready);
    sendButton->setEnabled(ready && !receiverBox->currentData().toString().isEmpty()); messageInput->setEnabled(ready && !receiverBox->currentData().toString().isEmpty()); receiverBox->setEnabled(ready); onlineList->setEnabled(ready); createGroupButton->setEnabled(ready); groupList->setEnabled(ready);
    mediaButton->setEnabled(ready && !receiverBox->currentData().toString().isEmpty()); emotionButton->setEnabled(ready && !receiverBox->currentData().toString().isEmpty()); reconnectAction->setEnabled(!ready && !connecting); passwordAction->setEnabled(ready);
    reconnectButton->setEnabled(!ready && !connecting); passwordButton->setEnabled(ready);
}
void ChatShell::showTransfer(const QString &text, bool sending, bool receiving) {
    videoStatus->setText(text); cancelVideo->setEnabled(sending); cancelVideo->setVisible(sending); transferCard->show();
    transferCard->setToolTip(sending || receiving ? "Đang truyền từng khối; bạn vẫn có thể chat" : "Trạng thái video gần nhất");
}
void ChatShell::setHistoryStatus(const QString &text) {
    historyStatus->setText(text.startsWith("SQLite:") ? "Lịch sử đã sẵn sàng" : text); historyStatus->setToolTip(text);
}


void ChatShell::noteMessage(const QString &peer, bool incoming) {
    if (!peer.isEmpty() && !peer.startsWith("group:") && !knownPeers.contains(peer)) {
        knownPeers.append(peer); setUsers(onlineUsers);
    }
    if (incoming && receiverBox->currentData().toString() != peer) ++unread[peer];
    refreshBadges();
}
void ChatShell::refreshBadges() {
    for(int i=0;i<groupList->count();++i) {
        auto *item=groupList->item(i);const QString key=item->data(Qt::UserRole).toString();
        const QString label=item->data(Qt::UserRole+1).toString();
        item->setText(label+(unread.value(key)?QString("  (%1)").arg(unread.value(key)):QString()));
    }
    for (int i = 0; i < onlineList->count(); ++i) {
        auto *item = onlineList->item(i); const QString peer = item->data(Qt::UserRole).toString();
        item->setText(peer + (onlineUsers.contains(peer) ? QString() : QString(" · Ngoại tuyến"))
            + (unread.value(peer) ? QString("  (%1)").arg(unread.value(peer)) : QString()));
    }
}

QIcon ChatShell::iconFor(const QString &name) const {
    return avatarImages.contains(name.toCaseFolded()) ? QIcon(avatarImages.value(name.toCaseFolded())) : personIcon(name);
}
void ChatShell::setAvatar(const QString &name, const QByteArray &bytes) {
    const QString key=name.toCaseFolded();
    if (bytes.isEmpty()) avatarImages.remove(key);
    else {
        const auto decoded=AvatarProtocol::decode(bytes); if(decoded.isNull()) return;
        QPixmap image(128,128); image.fill(Qt::transparent); QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath path; path.addEllipse(QRectF(0,0,128,128)); painter.setClipPath(path);
        painter.drawImage(image.rect(),decoded); painter.end();
        if (avatarImages.size()>=256 && !avatarImages.contains(key)) {
            auto victim=avatarImages.begin();
            while(victim!=avatarImages.end() && victim.key()==owner.toCaseFolded()) ++victim;
            if(victim!=avatarImages.end()) avatarImages.erase(victim);
        }
        avatarImages.insert(key,image);
    }
    if (key==owner.toCaseFolded()) {
        selfAvatar->setPixmap(QPixmap()); selfAvatar->setText(owner.left(1).toUpper());
        if(avatarImages.contains(key)) selfAvatar->setPixmap(avatarImages.value(key).scaled(40,40,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    }
    for(int i=0;i<onlineList->count();++i) {
        auto *item=onlineList->item(i); item->setIcon(iconFor(item->data(Qt::UserRole).toString()));
    }
    updateRecipient();
}
