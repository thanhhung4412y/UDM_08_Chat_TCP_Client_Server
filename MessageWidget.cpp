#include "MessageWidget.h"

#include <QContextMenuEvent>
#include <QDebug>
#include <QFileInfo>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

// messagebubble
MessageBubble::MessageBubble(const ChatMessageData &msgData, bool isOwnMessage,
                             QWidget *parent)
    : QWidget(parent), m_msgData(msgData), m_avatarLabel(nullptr),
      m_senderLabel(nullptr), m_replyPreviewLabel(nullptr),
      m_contentLabel(nullptr), m_timestampLabel(nullptr), m_fileLabel(nullptr) {
  setupUI(isOwnMessage);
}

ChatMessageData MessageBubble::messageData() const { return m_msgData; }

// thiet lap giao dien messagebubble
void MessageBubble::setupUI(bool isOwnMessage) {
  // layout ngang chinh: [avatar] [bong bong] hoac [bong bong] [avatar]
  QHBoxLayout *mainLayout = new QHBoxLayout(this);
  mainLayout->setContentsMargins(4, 2, 4, 2);
  mainLayout->setSpacing(8);

  // Avatar
  m_avatarLabel = new QLabel(this);
  m_avatarLabel->setFixedSize(36, 36);

  QPixmap avatar;
  if (!m_msgData.avatarPath.isEmpty() &&
      QFileInfo::exists(m_msgData.avatarPath)) {
    avatar.load(m_msgData.avatarPath);
  } else {
    // tao avatar mac dinh: hinh tron voi mau hash theo ten
    avatar = QPixmap(36, 36);
    avatar.fill(Qt::transparent);

    QPainter painter(&avatar);
    painter.setRenderHint(QPainter::Antialiasing);

    // moi nguoi dung co mau khac nhau
    int hash = 0;
    for (const QChar &ch : m_msgData.sender) {
      hash = (hash * 31 + ch.unicode()) % 360;
    }
    QColor avatarColor = QColor::fromHsv(hash, 120, 200);

    painter.setBrush(avatarColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, 36, 36);

    // draw chu cai dau cua ten
    painter.setPen(Qt::white);
    QFont font;
    font.setPointSize(14);
    font.setBold(true);
    painter.setFont(font);

    QString initial =
        m_msgData.sender.isEmpty() ? "?" : m_msgData.sender.left(1).toUpper();
    painter.drawText(QRect(0, 0, 36, 36), Qt::AlignCenter, initial);
    painter.end();
  }

  m_avatarLabel->setPixmap(
      avatar.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  m_avatarLabel->setStyleSheet("border: none; background: transparent;");

  // bong bong noi dung
  QWidget *bubbleWidget = new QWidget(this);
  QVBoxLayout *bubbleLayout = new QVBoxLayout(bubbleWidget);
  bubbleLayout->setContentsMargins(10, 6, 10, 6);
  bubbleLayout->setSpacing(3);

  // ten nguoi gui
  m_senderLabel = new QLabel(m_msgData.sender, this);
  m_senderLabel->setStyleSheet("font-weight: bold; "
                               "font-size: 12px; "
                               "color: #075E54; "
                               "background: transparent; "
                               "border: none;");
  bubbleLayout->addWidget(m_senderLabel);

  // phan trich dan khi reply
  if (m_msgData.type == Protocol::ReplyMessage && !m_msgData.extra.isEmpty()) {
    QString previewText = m_msgData.extra;
    if (previewText.length() > 80) {
      previewText = previewText.left(80) + "...";
    }

    m_replyPreviewLabel = new QLabel(this);
    m_replyPreviewLabel->setText(QString::fromUtf8("↩ Trả lời: ") +
                                 previewText);
    m_replyPreviewLabel->setWordWrap(true);
    m_replyPreviewLabel->setStyleSheet("background-color: rgba(0, 0, 0, 0.05); "
                                       "border-left: 3px solid #25D366; "
                                       "padding: 4px 8px; "
                                       "border-radius: 4px; "
                                       "font-size: 11px; "
                                       "color: #667781; "
                                       "margin-bottom: 2px;");
    bubbleLayout->addWidget(m_replyPreviewLabel);
  }

  // noi dung tin nhan
  m_contentLabel = new QLabel(this);
  QString displayContent = m_msgData.content;

  if (m_msgData.type == Protocol::ForwardMessage &&
      !m_msgData.extra.isEmpty()) {
    displayContent = QString::fromUtf8("⤳ Chuyển tiếp từ [") + m_msgData.extra +
                     "]:\n" + displayContent;
  }

  m_contentLabel->setText(displayContent);
  m_contentLabel->setWordWrap(true);
  m_contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_contentLabel->setStyleSheet("font-size: 13px; "
                                "color: #303030; "
                                "background: transparent; "
                                "border: none;");
  bubbleLayout->addWidget(m_contentLabel);

  // file dinh kem
  if (m_msgData.type == Protocol::ChatText && !m_msgData.extra.isEmpty()) {
    m_fileLabel = new QLabel(this);
    m_fileLabel->setText(QString::fromUtf8("📎 ") + m_msgData.extra);
    m_fileLabel->setStyleSheet("font-size: 12px; "
                               "color: #0078D7; "
                               "text-decoration: underline; "
                               "background: transparent; "
                               "border: none;");
    m_fileLabel->setCursor(Qt::PointingHandCursor);
    bubbleLayout->addWidget(m_fileLabel);
  }

  // thoi gian gui
  QString timeStr = m_msgData.timestamp.toString("HH:mm");
  m_timestampLabel = new QLabel(timeStr, this);
  m_timestampLabel->setAlignment(Qt::AlignRight);
  m_timestampLabel->setStyleSheet("font-size: 10px; "
                                  "color: #999999; "
                                  "background: transparent; "
                                  "border: none;");
  bubbleLayout->addWidget(m_timestampLabel);

  // Mau sac bong bong va sxep
  bubbleWidget->setMaximumWidth(400);

  if (isOwnMessage) {
    // tin nhan ng dang nhan: xanh la nhat, can phai
    bubbleWidget->setStyleSheet("background-color: #D9FDD3; "
                                "border-radius: 10px; "
                                "padding: 6px;");
    mainLayout->addStretch();
    mainLayout->addWidget(bubbleWidget);
    mainLayout->addWidget(m_avatarLabel);
  } else {
    // tin nhan ng khac: trang, can trai
    bubbleWidget->setStyleSheet("background-color: #FFFFFF; "
                                "border-radius: 10px; "
                                "border: 1px solid #E0E0E0; "
                                "padding: 6px;");
    mainLayout->addWidget(m_avatarLabel);
    mainLayout->addWidget(bubbleWidget);
    mainLayout->addStretch();
  }
}

// click phai hien tra loi/chuyen tiep
void MessageBubble::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);

  QAction *replyAction = menu.addAction(QString::fromUtf8("↩ Trả lời"));
  QAction *forwardAction = menu.addAction(QString::fromUtf8("⤳ Chuyển tiếp"));

  menu.setStyleSheet("QMenu {"
                     "  background: #FFFFFF;"
                     "  border: 1px solid #E0E0E0;"
                     "  border-radius: 8px;"
                     "  padding: 4px;"
                     "}"
                     "QMenu::item {"
                     "  padding: 6px 20px;"
                     "  font-size: 13px;"
                     "}"
                     "QMenu::item:selected {"
                     "  background-color: #F0F0F0;"
                     "  border-radius: 4px;"
                     "}");

  QAction *selectedAction = menu.exec(event->globalPos());

  if (selectedAction == replyAction) {
    emit replyRequested(m_msgData);
  } else if (selectedAction == forwardAction) {
    emit forwardRequested(m_msgData);
  }
}

// quan ly toan bo khu vuc tin nhan
MessageWidget::MessageWidget(QWidget *parent)
    : QWidget(parent), m_defaultAvatarPath("") {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  // cuon tin nhan
  QScrollArea *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setStyleSheet("QScrollArea { border: none; background: #ECE5DD; }"
                            "QScrollBar:vertical {"
                            "  width: 6px; background: transparent;"
                            "}"
                            "QScrollBar::handle:vertical {"
                            "  background: #B0B0B0; border-radius: 3px;"
                            "}"
                            "QScrollBar::add-line:vertical,"
                            "QScrollBar::sub-line:vertical {"
                            "  height: 0px;"
                            "}");

  m_messagesContainer = new QWidget();
  m_messagesLayout = new QVBoxLayout(m_messagesContainer);
  m_messagesLayout->setContentsMargins(8, 8, 8, 8);
  m_messagesLayout->setSpacing(4);
  m_messagesLayout->addStretch(); // day tin nhan xuong duoi

  scrollArea->setWidget(m_messagesContainer);
  mainLayout->addWidget(scrollArea);
}

// them mot bong bong vao khu vuc hien thi
void MessageWidget::addBubble(const ChatMessageData &data, bool isOwnMessage) {
  MessageBubble *bubble =
      new MessageBubble(data, isOwnMessage, m_messagesContainer);

  connect(bubble, &MessageBubble::replyRequested, this,
          &MessageWidget::onReplyRequested);

  connect(bubble, &MessageBubble::forwardRequested, this,
          &MessageWidget::onForwardRequested);

  // chen truoc spacer cuoi
  m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);

  scrollToBottom();
}

// them tin nhan thuong
void MessageWidget::addMessage(const QString &sender, const QString &content,
                               bool isOwnMessage, const QString &avatarPath) {
  ChatMessageData data;
  data.type = Protocol::ChatText;
  data.sender = sender;
  data.content = content;
  data.extra = "";
  data.avatarPath = avatarPath.isEmpty() ? m_defaultAvatarPath : avatarPath;
  data.timestamp = QDateTime::currentDateTime();

  addBubble(data, isOwnMessage);
}

// them tin nhan tra loi
// luu noi dung tin nhan goc vao truong extra
void MessageWidget::addReplyMessage(const QString &sender,
                                    const QString &content,
                                    const QString &originalContent,
                                    bool isOwnMessage,
                                    const QString &avatarPath) {
  ChatMessageData data;
  data.type = Protocol::ReplyMessage;
  data.sender = sender;
  data.content = content;
  data.extra = originalContent;
  data.avatarPath = avatarPath.isEmpty() ? m_defaultAvatarPath : avatarPath;
  data.timestamp = QDateTime::currentDateTime();

  addBubble(data, isOwnMessage);
}

// them tin nhan chuyen tiep
// luu ten nguoi gui goc vao truong extra
void MessageWidget::addForwardMessage(const QString &sender,
                                      const QString &content,
                                      const QString &originalSender,
                                      bool isOwnMessage,
                                      const QString &avatarPath) {
  ChatMessageData data;
  data.type = Protocol::ForwardMessage;
  data.sender = sender;
  data.content = content;
  data.extra = originalSender;
  data.avatarPath = avatarPath.isEmpty() ? m_defaultAvatarPath : avatarPath;
  data.timestamp = QDateTime::currentDateTime();

  addBubble(data, isOwnMessage);
}

// them tin nhan dinh kem file
// ten file duoc luu vao truong extra
void MessageWidget::addFileMessage(const QString &sender,
                                   const QString &fileName, bool isOwnMessage,
                                   const QString &avatarPath) {
  ChatMessageData data;
  data.type = Protocol::ChatText;
  data.sender = sender;
  data.content = QString::fromUtf8("Đã gửi một tập tin");
  data.extra = fileName;
  data.avatarPath = avatarPath.isEmpty() ? m_defaultAvatarPath : avatarPath;
  data.timestamp = QDateTime::currentDateTime();

  addBubble(data, isOwnMessage);
}

void MessageWidget::handleIncomingMessage(const QByteArray &rawData,
                                          const QString &myUsername) {
  int type;
  QString sender, content, extra;

  if (!Protocol::unpackMessage(rawData, type, sender, content, extra)) {
    qDebug() << "MessageWidget: Khong the giai ma tin nhan!";
    return;
  }

  bool isOwn = (sender == myUsername);

  switch (type) {
  case Protocol::ChatText:
    if (!extra.isEmpty()) {
      addFileMessage(sender, extra, isOwn);
    } else {
      addMessage(sender, content, isOwn);
    }
    break;

  case Protocol::ReplyMessage:
    addReplyMessage(sender, content, extra, isOwn);
    break;

  case Protocol::ForwardMessage:
    addForwardMessage(sender, content, extra, isOwn);
    break;

  default:
    addMessage(sender, content, isOwn);
    break;
  }
}

// dong goi tin nhan reply de gui qua mang
QByteArray MessageWidget::packReply(const QString &sender,
                                    const QString &replyContent,
                                    const QString &originalContent) {
  return Protocol::packMessage(Protocol::ReplyMessage, sender, replyContent,
                               originalContent);
}

// dong goi tin nhan chuyen tiep de gui qua mang
QByteArray MessageWidget::packForward(const QString &sender,
                                      const QString &forwardedContent,
                                      const QString &originalSender) {
  return Protocol::packMessage(Protocol::ForwardMessage, sender,
                               forwardedContent, originalSender);
}

// quan ly avatar mac dinh
void MessageWidget::setDefaultAvatar(const QString &path) {
  m_defaultAvatarPath = path;
}

QString MessageWidget::defaultAvatar() const { return m_defaultAvatarPath; }

// xoa toan bo tin nhan
void MessageWidget::clearMessages() {
  QLayoutItem *item;
  while ((item = m_messagesLayout->takeAt(0)) != nullptr) {
    if (item->widget()) {
      delete item->widget();
    }
    delete item;
  }

  // them lai spacer
  m_messagesLayout->addStretch();
}

// slot xu ly yeu cau tra loi
void MessageWidget::onReplyRequested(const ChatMessageData &originalMsg) {
  qDebug() << "Reply requested:" << originalMsg.sender << originalMsg.content;

  emit replyToMessage(originalMsg.content);
}

// slot xu ly yeu cau chuyen tiep
void MessageWidget::onForwardRequested(const ChatMessageData &originalMsg) {
  qDebug() << "Forward requested:" << originalMsg.sender << originalMsg.content;

  emit forwardMessage(originalMsg.content, originalMsg.sender);
}

// cuon xuong cuoi danh sach tin nhan
void MessageWidget::scrollToBottom() {
  QScrollArea *scrollArea = nullptr;
  QWidget *p = m_messagesContainer->parentWidget();

  while (p) {
    scrollArea = qobject_cast<QScrollArea *>(p);
    if (scrollArea)
      break;
    p = p->parentWidget();
  }

  if (scrollArea) {
    QTimer::singleShot(50, [scrollArea]() {
      scrollArea->verticalScrollBar()->setValue(
          scrollArea->verticalScrollBar()->maximum());
    });
  }
}
