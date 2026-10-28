#ifndef MESSAGEWIDGET_H
#define MESSAGEWIDGET_H

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "Protocol.h"

// cau truc tin nhan
struct ChatMessageData {
  int type;
  QString sender;
  QString content;
  QString extra;
  QString avatarPath;
  QDateTime timestamp;
};

// messagebubble
class MessageBubble : public QWidget {
  Q_OBJECT

public:
  explicit MessageBubble(const ChatMessageData &msgData, bool isOwnMessage,
                         QWidget *parent = nullptr);

  // lay du lieu tin nhan goc
  ChatMessageData messageData() const;

signals:
  // khi nguoi dung click phai chon "tra loi"
  void replyRequested(const ChatMessageData &originalMsg);

  // khi nguoi dung click phai chon "chuyen tiep"
  void forwardRequested(const ChatMessageData &originalMsg);

protected:
  // hien context menu khi click chuot phai
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  void setupUI(bool isOwnMessage);

  ChatMessageData m_msgData;

  QLabel *m_avatarLabel;
  QLabel *m_senderLabel;
  QLabel *m_replyPreviewLabel;
  QLabel *m_contentLabel;
  QLabel *m_timestampLabel;
  QLabel *m_fileLabel;
};

class MessageWidget : public QWidget {
  Q_OBJECT

public:
  explicit MessageWidget(QWidget *parent = nullptr);

  // them tin nhan hien thi
  void addMessage(const QString &sender, const QString &content,
                  bool isOwnMessage, const QString &avatarPath = "");

  void addReplyMessage(const QString &sender, const QString &content,
                       const QString &originalContent, bool isOwnMessage,
                       const QString &avatarPath = "");

  void addForwardMessage(const QString &sender, const QString &content,
                         const QString &originalSender, bool isOwnMessage,
                         const QString &avatarPath = "");

  void addFileMessage(const QString &sender, const QString &fileName,
                      bool isOwnMessage, const QString &avatarPath = "");

  void handleIncomingMessage(const QByteArray &rawData,
                             const QString &myUsername);

  // dong goi tin nhan de gui qua mang
  static QByteArray packReply(const QString &sender,
                              const QString &replyContent,
                              const QString &originalContent);

  static QByteArray packForward(const QString &sender,
                                const QString &forwardedContent,
                                const QString &originalSender);

  // quan ly avatar
  void setDefaultAvatar(const QString &path);
  QString defaultAvatar() const;

  // xoa toan bo tin nhan
  void clearMessages();

signals:
  // khi nguoi dung yeu cau tra loi tin nhan
  void replyToMessage(const QString &originalContent);

  // khi nguoi dung yeu cau chuyen tiep tin nhan
  void forwardMessage(const QString &originalContent,
                      const QString &originalSender);

public slots:
  void onReplyRequested(const ChatMessageData &originalMsg);
  void onForwardRequested(const ChatMessageData &originalMsg);

private:
  void addBubble(const ChatMessageData &data, bool isOwnMessage);
  void scrollToBottom();

  QVBoxLayout *m_messagesLayout;
  QWidget *m_messagesContainer;
  QString m_defaultAvatarPath;
};

#endif
