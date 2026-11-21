#include "chatshell.h"
#include "chattheme.h"
#include "historyview.h"
#include <QtTest>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextCursor>
#include <QMenu>

class UiTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { ChatTheme::apply(*qobject_cast<QApplication *>(qApp)); }
    void recipientsAndPresence() {
        ChatShell window("Alice", "127.0.0.1:12345"); window.setConnectionState(true, false, "ready");
        window.setUsers({"Alice", "Bob", "Thảo"}); QCOMPARE(window.receiverBox->count(), 3);
        const int bob = window.receiverBox->findData("Bob"); window.receiverBox->setCurrentIndex(bob);
        QSignalSpy changes(window.receiverBox, &QComboBox::currentIndexChanged);
        window.setUsers({"Alice", "Bob", "Thảo", "Minh"}); QCOMPARE(window.receiverBox->currentData().toString(), "Bob"); QCOMPARE(changes.size(), 0);
        window.setUsers({"Alice", "Thảo"}); QCOMPARE(window.receiverBox->currentData().toString(), "Bob"); QCOMPARE(changes.size(), 0);
        QCOMPARE(window.onlineList->count(), 3);
    }
    void enabledControlsAndCompactLayout() {
        ChatShell window("Alice", "127.0.0.1:12345"); window.resize(800, 620); window.show(); QTest::qWait(30);
        window.setConnectionState(false, true, "connecting"); QVERIFY(!window.sendButton->isEnabled()); QVERIFY(!window.passwordButton->isEnabled());
        window.setConnectionState(true, false, "TLS ready");
        QVERIFY(!window.sendButton->isEnabled()); QVERIFY(window.passwordButton->isEnabled());
        window.setUsers({"Alice", "Bob"});
        window.receiverBox->setCurrentIndex(window.receiverBox->findData("Bob"));
        QVERIFY(window.sendButton->isEnabled());
        QVERIFY(!window.reconnectButton->isEnabled());
        QVERIFY(window.messageInput->width() >= 70); QVERIFY(window.chatBox->width() > 300);
        QVERIFY(window.rect().contains(QRect(window.messageInput->mapTo(&window, QPoint()), window.messageInput->size())));
        QVERIFY(window.rect().contains(QRect(window.sendButton->mapTo(&window, QPoint()), window.sendButton->size())));
        window.resize(760, 560); QTest::qWait(10);
        QVERIFY(window.rect().contains(QRect(window.messageInput->mapTo(&window, QPoint()), window.messageInput->size())));
        QVERIFY(window.rect().contains(QRect(window.sendButton->mapTo(&window, QPoint()), window.sendButton->size())));
        window.setConnectionState(false, false, "offline"); QVERIFY(window.reconnectButton->isEnabled()); QVERIFY(!window.sendButton->isEnabled());
    }
    void unreadConversations() {
        ChatShell window("Alice", "localhost"); window.setUsers({"Alice", "Bob"});
        window.noteMessage("Bob", true);
        QVERIFY(window.onlineList->item(0)->text().contains("(1)"));
        window.receiverBox->setCurrentIndex(window.receiverBox->findData("Bob"));
        QVERIFY(!window.onlineList->item(0)->text().contains("(1)"));
        window.noteMessage("Bob", true);
        QVERIFY(!window.onlineList->item(0)->text().contains("(1)"));
        window.noteMessage("OfflinePeer", true);
        QVERIFY(window.receiverBox->findData("OfflinePeer") > 0);
    }
    void transferControls() {
        ChatShell window("Alice", "localhost:12345"); window.showTransfer("Đang gửi 50%", true);
        QVERIFY(window.cancelVideo->isEnabled()); QVERIFY(!window.cancelVideo->isHidden());
        window.showTransfer("Đã nhận xong", false); QVERIFY(!window.cancelVideo->isEnabled()); QVERIFY(window.cancelVideo->isHidden());
    }
    void groupMediaMenuEnablesImagesAndVideo() {
        ChatShell window("Alice", "localhost");
        window.setConnectionState(true, false, "ready");
        window.addGroup("group-id", "Group", {"Alice", "Bob"});
        QAction *imageAction = nullptr;
        QAction *videoAction = nullptr;
        for (auto *menu : window.findChildren<QMenu *>()) {
            for (auto *action : menu->actions()) {
                if (action->text() == "Gửi ảnh") imageAction = action;
                if (action->text() == "Gửi video MP4") videoAction = action;
            }
        }
        QVERIFY(imageAction);
        QVERIFY(videoAction);
        QVERIFY(imageAction->isEnabled());
        QVERIFY(videoAction->isEnabled());
        QVERIFY(window.stickerButton->isEnabled());
        QSignalSpy image(window.imageButton, &QPushButton::clicked);
        QSignalSpy video(window.videoButton, &QPushButton::clicked);
        imageAction->trigger();
        videoAction->trigger();
        QCOMPARE(image.size(), 1);
        QCOMPARE(video.size(), 1);
        window.setUsers({"Alice", "Bob"});
        window.receiverBox->setCurrentIndex(window.receiverBox->findData("Bob"));
        QVERIFY(videoAction->isEnabled());
    }
    void richMessagesRemainSelectableAndEscaped() {
        QTextBrowser browser; HistoryView view(&browser, "Alice"); view.replace({});
        QVERIFY(browser.toPlainText().contains("Chưa có tin nhắn"));
        HistoryRecord record; QString error;
        QVERIFY(HistoryStore::normalize({{"type", "reply"}, {"sender", "Alice"}, {"text", "Hello <b>literal</b>"}, {"quote", "<script>quote</script>"}}, record, error));
        view.append(record); QVERIFY(!browser.toPlainText().contains("Chưa có tin nhắn"));
        QVERIFY(browser.toPlainText().contains("<b>literal</b>")); QVERIFY(browser.toPlainText().contains("<script>quote</script>"));
        auto cursor = browser.textCursor(); cursor.select(QTextCursor::Document); browser.setTextCursor(cursor);
        QVERIFY(browser.textCursor().selectedText().contains("literal"));
    }
    void menusAndEnterReachExistingControls() {
        ChatShell window("Alice", "127.0.0.1:12345"); window.setConnectionState(true, false, "ready"); window.setUsers({"Alice", "Bob"}); window.show(); QTest::qWait(20);
        const auto item = window.onlineList->item(0);
        QTest::mouseClick(window.onlineList->viewport(), Qt::LeftButton, Qt::NoModifier, window.onlineList->visualItemRect(item).center());
        QCOMPARE(window.receiverBox->currentData().toString(), "Bob");
        QSignalSpy image(window.imageButton, &QPushButton::clicked), video(window.videoButton, &QPushButton::clicked), password(window.passwordButton, &QPushButton::clicked);
        for (auto *menu : window.findChildren<QMenu *>()) for (auto *action : menu->actions()) {
            if (action->text() == "Gửi ảnh" || action->text() == "Gửi video MP4" || action->text() == "Đổi mật khẩu") action->trigger();
        }
        QCOMPARE(image.size(), 1); QCOMPARE(video.size(), 1); QCOMPARE(password.size(), 1);
        QSignalSpy enter(window.messageInput, &QLineEdit::returnPressed); window.messageInput->setFocus(); QTest::keyClick(window.messageInput, Qt::Key_Return); QCOMPARE(enter.size(), 1);
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
