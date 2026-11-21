#include "historysearchdialog.h"
#include "historystore.h"
#include "chattheme.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
// Synthetic fixture, temporary SQLite only; no network or real account data.
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    for(const auto &font : {"C:/Windows/Fonts/segoeui.ttf","C:/Windows/Fonts/seguisb.ttf","C:/Windows/Fonts/seguiemj.ttf"})
        if(QFile::exists(font)) QFontDatabase::addApplicationFont(font);
    ChatTheme::apply(app);
    QTemporaryDir data(QDir::currentPath()+"/search-preview-XXXXXX"); if(!data.isValid()) return 2;
    const auto file=data.filePath("synthetic.sqlite"); QString error;
    {
        HistoryStore store; if(!store.open(file,error)) return 3;
        auto add=[&](const QString &type,const QString &sender,const QString &text,const QString &quote=QString()) {
            HistoryRecord record; return store.append({{"type",type},{"sender",sender},{"text",text},{"quote",quote},
                {"_utc","2026-10-29T08:30:00.000Z"}},record,error)==HistoryStore::SaveResult::Saved;
        };
        if(!add("chat","Minh","Mình gửi kế hoạch chuyến đi Đà Nẵng ở đây nhé.")) return 4;
        if(!add("chat","Bạn","Mọi người xem giúp mình lịch khởi hành vào thứ Sáu.")) return 4;
        if(!add("reply","Thảo","Mình đã xem rồi. Hẹn mọi người lúc 8 giờ sáng!","Mình gửi kế hoạch chuyến đi Đà Nẵng ở đây nhé.")) return 4;
        if(!add("chat","Bạn","Cảm ơn mọi người, mình sẽ cập nhật thông tin tiếp.")) return 4;
    }
    HistorySearchDialog dialog(file,"Bạn",""); dialog.show();
    dialog.findChild<QLineEdit *>("historySearchInput")->setText("da nang");
    dialog.findChild<QPushButton *>("runHistorySearch")->click();
    const QString directory=app.arguments().size()>1 ? app.arguments()[1] : QDir::currentPath(); QDir().mkpath(directory);
    QTimer timer; int attempts=0; timer.setInterval(50);
    QObject::connect(&timer,&QTimer::timeout,&dialog,[&] {
        if(dialog.findChild<QTextBrowser *>("searchContext")->extraSelections().size()==1) {
            app.exit(dialog.grab().save(QDir(directory).filePath("chat-search.png")) ? 0 : 5);
        } else if(++attempts>100) app.exit(6);
    });
    timer.start(); return app.exec();
}
