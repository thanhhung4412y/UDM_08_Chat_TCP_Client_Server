#include "chatshell.h"
#include "connectiondialog.h"
#include "passworddialog.h"
#include "recoverydialog.h"
#include "sessionconnection.h"
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QPainter>
#include <QBuffer>
#include "chattheme.h"
#include "historyview.h"
#include "historyprotocol.h"
#include "editmessagedialog.h"
#include "recallclient.h"
#include "chathistory.h"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QTimer>
#include <QUuid>
#include <QTextBrowser>
#include <QScrollBar>

// Synthetic UI fixture only: does not connect, authenticate, save history or open media.
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    for (const auto &font : {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/seguisb.ttf", "C:/Windows/Fonts/seguiemj.ttf", "C:/Windows/Fonts/seguisym.ttf"})
        if (QFile::exists(font)) QFontDatabase::addApplicationFont(font);
    QTemporaryDir settings(QDir::currentPath()+"/preview-settings-XXXXXX");
    app.setOrganizationName("ChatTCP1-Preview"); app.setApplicationName("ProfilePreview");
    QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    ChatTheme::apply(app);
    ChatShell window("Bạn", "192.168.1.10:12345"); window.resize(1120, 760);
    window.setConnectionState(true, false, "Đã xác thực tài khoản và mã hóa TLS"); window.setUsers({"Bạn", "Minh", "Thảo", "Hoàng Anh"});
    window.historyButton->setEnabled(true); window.searchButton->setEnabled(true); window.setHistoryStatus("SQLite: ready");
    QImage face(128,128,QImage::Format_RGB32); face.fill(QColor("#d8ede8"));
    { QPainter p(&face); p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen); p.setBrush(QColor("#14786c")); p.drawEllipse(43,23,42,42); p.drawEllipse(25,72,78,72); }
    QByteArray faceBytes; QBuffer faceBuffer(&faceBytes); faceBuffer.open(QIODevice::WriteOnly); face.save(&faceBuffer,"JPEG",80);
    window.setAvatar("Bạn",faceBytes); window.setAvatar("Minh",faceBytes); window.noteMessage("Minh",true);
    HistoryView view(window.chatBox, "Bạn"); QVector<HistoryRecord> records;
    auto add = [&](const QString &type, const QString &sender, const QString &text, const QString &quote = QString()) {
        HistoryRecord record;
        record.packet = {{"type", type}, {"sender", sender}, {"text", text}, {"quote", quote},
            {"_event_id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        records.append(record);
    };
    add("chat", "Minh", "Chào mọi người! Mình vừa cập nhật bản giao diện mới.");
    add("chat", "Bạn", "Mình đã vào được rồi. Danh sách bên trái dễ nhìn hơn hẳn 😊");
    add("reply", "Thảo", "Mình sẽ kiểm tra phần lịch sử và gửi video nhé.", "Danh sách bên trái dễ nhìn hơn hẳn");
    add("chat", "Bạn", "Được nhé. Mọi người thử gửi thêm ảnh và sticker nữa nha!");
    records[1].packet=HistoryProtocol::tombstone(records[1].packet);
    records.last().packet["text"]="Mình đã cập nhật lịch hẹn: 8 giờ sáng thứ Sáu nhé!"; records.last().packet["_edit_revision"]=1;
    records.clear();
    auto privateDemo=[&](const QString &sender,const QString &text,int state) {
        add("private",sender,text);records.last().packet["to"]=sender=="Bạn"?QString("Minh"):QString("Bạn");
        if(state>0) {records.last().packet["_receipt_state"]=state;records.last().packet["_receipt_revision"]=state;}
    };
    privateDemo("Bạn","Mình gửi thông tin ở đây nhé.",1);
    privateDemo("Minh","Mình đã vào cuộc trò chuyện rồi.",0);
    privateDemo("Bạn","Mình gửi thêm nội dung để bạn xem nhé.",2);
    privateDemo("Bạn","Hẹn gặp bạn lúc 8 giờ sáng thứ Sáu!",3);records.last().packet["_edit_revision"]=1;
    window.receiverBox->setCurrentIndex(window.receiverBox->findData("Minh"));
    view.replace(records); window.show();
    const QString directory = app.arguments().size() > 1 ? app.arguments()[1] : QDir::currentPath(); QDir().mkpath(directory);
    QTimer::singleShot(150, &window, [&] {
        ConnectionDialog login; login.findChild<QLineEdit *>("usernameInput")->setText("Ban"); login.show(); app.processEvents();
        const bool loginSaved=login.grab().save(QDir(directory).filePath("profile-login.png"));
        for(const auto &item:QList<QPair<int,QString>>{{1,"email-register.png"},{3,"email-reset.png"},{4,"email-link.png"}}) {
            login.findChild<QComboBox *>("authMode")->setCurrentIndex(item.first);app.processEvents();
            if(!login.grab().save(QDir(directory).filePath(item.second))) {app.exit(2);return;}
        }
        login.findChild<QComboBox *>("authMode")->setCurrentIndex(2); app.processEvents();
        const bool resetSaved=login.grab().save(QDir(directory).filePath("profile-reset.png")); login.hide();
        QSslSocket socket; JsonChannel channel(&socket); SessionConnection session(&socket,&channel,{"Bạn","127.0.0.1",12345,"synthetic-password-only",false});
        QTextBrowser dummy; ChatHistory history(&dummy,"Bạn","127.0.0.1",12345); RecallClient updates(&session,&channel,&history);
        EditMessageDialog editor(&updates,&history,records.last().packet); editor.show();app.processEvents();
        const bool editSaved=editor.grab().save(QDir(directory).filePath("edit-message.png"));editor.hide();
        const bool wide = editSaved && loginSaved && resetSaved && window.grab().save(QDir(directory).filePath("chat-ui-wide.png"));
        window.resize(800, 620);
        QTimer::singleShot(100, &window, [&, wide] { window.chatBox->verticalScrollBar()->setValue(window.chatBox->verticalScrollBar()->maximum()); const bool compact = window.grab().save(QDir(directory).filePath("chat-ui-compact.png")); app.exit(wide && compact ? 0 : 1); });
    });
    return app.exec();
}
