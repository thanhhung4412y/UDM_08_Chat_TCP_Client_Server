#pragma once
#include <QWidget>
#include <QStringList>
#include <QHash>
#include <QPixmap>
class QLabel;
class QTextBrowser;
class QListWidget;
class QLineEdit;
class QPushButton;
class QComboBox;
class QToolButton;
class QAction;
class QFrame;

// Presentation only: existing network, account and persistence code use these controls.
class ChatShell : public QWidget {
    Q_OBJECT
public:
    ChatShell(QString owner, QString endpoint, QWidget *parent = nullptr);
    QLabel *status, *onlineLabel, *historyStatus, *videoStatus;
    QTextBrowser *chatBox;
    QListWidget *onlineList;
    QListWidget *groupList;
    QPushButton *createGroupButton;
    void addGroup(const QString &id, const QString &name, const QStringList &members, bool openGroup = true);
    QLineEdit *messageInput;
    QPushButton *emojiButton, *stickerButton, *imageButton, *videoButton, *sendButton, *replyButton, *forwardButton;
    QPushButton *searchButton, *historyButton, *reconnectButton, *changeButton, *passwordButton, *cancelVideo;
    QPushButton *recoveryButton, *avatarButton, *removeAvatarButton;
    void setAvatar(const QString &name, const QByteArray &bytes);
    QComboBox *receiverBox;
    void noteMessage(const QString &peer, bool incoming);
    void setUsers(const QStringList &users);
    void setConnectionState(bool ready, bool connecting, const QString &detail);
    void showTransfer(const QString &text, bool sending, bool receiving = false);
    void setHistoryStatus(const QString &text);
private:
    QString owner;
    QStringList knownPeers, onlineUsers;
    QHash<QString, int> unread;
    void refreshBadges();
    QLabel *title, *subtitle, *avatar, *notice, *selfAvatar;
    QHash<QString,QPixmap> avatarImages;
    QIcon iconFor(const QString &name) const;
    QHash<QString, QStringList> groupMembers;
    QToolButton *mediaButton, *emotionButton;
    QAction *reconnectAction, *passwordAction, *recoveryAction, *avatarAction, *removeAvatarAction;
    QFrame *transferCard;
    void updateRecipient();
};
