#include "historystore.h"
#include "historysearchdialog.h"
#include "historyview.h"
#include <QtTest>
#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <QUuid>
#include <QSet>

class SearchTests : public QObject {
    Q_OBJECT
    static QJsonObject message(const QString &text,const QString &to={}) {
        return {{"type",to.isEmpty()?"chat":"private"},{"sender","Alice"},{"to",to},{"text",text},
            {"_event_id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"_utc","2026-10-30T03:00:00.000Z"}};
    }
    static qint64 save(HistoryStore &store,const QJsonObject &packet) {
        QString error; HistoryRecord record;
        if(store.append(packet,record,error)!=HistoryStore::SaveResult::Saved) { QTest::qFail(qPrintable(error),__FILE__,__LINE__); return 0; }
        return record.sequence;
    }
    static void populate(HistoryStore &store,int count=135) {
        for(int i=0;i<count;++i) { save(store,message(QString("needle %1").arg(i),"Bob")); save(store,message("needle public")); }
    }
private slots:
    void unicodeLiteralAndConversationIsolation() {
        QTemporaryDir dir(QDir::currentPath()+"/search-unicode-XXXXXX"); QVERIFY(dir.isValid()); HistoryStore store; QString error; QVERIFY(store.open(dir.filePath("history.sqlite"),error));
        save(store,message("Chào ĐƯỜNG phố: 100% _ <b>literal</b>"));
        save(store,message("' OR 1=1 --"));
        auto reply=message("Đã nhận được"); reply["type"]="reply"; reply["quote"]="Hẹn ở Đà Nẵng"; save(store,reply);
        save(store,message("Đà Nẵng riêng Bob","Bob")); save(store,message("Đà Nẵng riêng Carol","Carol"));
        auto video=message(""); video["type"]="video"; video.remove("text"); video["name"]="Chuyến đi Đà Nẵng.mp4";
        video["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces); video["local_path"]=dir.filePath("demo.mp4"); video["size"]=12; video["sha256"]=QString(64,'0'); save(store,video);
        QCOMPARE(store.search("chao duong","",0,error).records.size(),1); QVERIFY2(error.isEmpty(),qPrintable(error));
        QCOMPARE(store.search(QString("CHÀO ĐƯỜNG").normalized(QString::NormalizationForm_D),"",0,error).records.size(),1);
        QCOMPARE(store.search("%","",0,error).records.size(),1); QCOMPARE(store.search("_","",0,error).records.size(),1);
        QCOMPARE(store.search("' OR 1=1 --","",0,error).records.size(),1);
        QCOMPARE(store.search("da nang","",0,error).records.size(),2);
        QCOMPARE(store.search("da nang","Bob",0,error).records.size(),1); QCOMPARE(store.search("da nang","Carol",0,error).records.size(),1);
        QVERIFY(store.search("   ","",0,error).records.isEmpty()); QVERIFY(error.isEmpty());
        QVERIFY(store.search(QString(129,'a'),"",0,error).records.isEmpty()); QVERIFY(!error.isEmpty());
    }
    void paginationContextAndReadonlyRestart() {
        QTemporaryDir dir(QDir::currentPath()+"/search-pages-XXXXXX"); QVERIFY(dir.isValid()); const auto file=dir.filePath("history.sqlite");
        qint64 target=0,foreign=0,imageSequence=0; QByteArray bytes; QString error;
        {
            HistoryStore store; QVERIFY(store.open(file,error)); populate(store);
            const auto first=store.search("needle","Bob",0,error); QCOMPARE(first.records.size(),50); QVERIFY(first.hasMore);
            const auto second=store.search("needle","Bob",first.records.last().sequence,error); QCOMPARE(second.records.size(),50); QVERIFY(second.hasMore);
            const auto third=store.search("needle","Bob",second.records.last().sequence,error); QCOMPARE(third.records.size(),35); QVERIFY(!third.hasMore);
            QSet<qint64> ids; for(const auto &page : {first,second,third}) for(const auto &record : page.records) { QVERIFY(!ids.contains(record.sequence)); ids.insert(record.sequence); }
            QCOMPARE(ids.size(),135); target=second.records[20].sequence; foreign=save(store,message("only public"));
            QImage image(20,20,QImage::Format_RGB32); image.fill(Qt::blue); QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"JPEG");
            auto packet=message("","Bob"); packet["type"]="image"; packet.remove("text"); packet["name"]="photo needle"; packet["data"]=QString::fromLatin1(bytes.toBase64()); imageSequence=save(store,packet);
            const auto hits=store.search("photo needle","Bob",0,error); QCOMPARE(hits.records.size(),1); QVERIFY(hits.records.first().image.isEmpty()); QVERIFY(!hits.records.first().packet.contains("data"));
        }
        HistoryStore readonly; QVERIFY(readonly.openReadOnly(file,error));
        const auto records=readonly.context(target,"Bob",error); QVERIFY2(error.isEmpty(),qPrintable(error)); QCOMPARE(records.size(),21);
        bool found=false; for(const auto &record:records) { QCOMPARE(record.packet.value("to").toString(),"Bob"); found|=record.sequence==target; } QVERIFY(found);
        QVERIFY(readonly.context(foreign,"Bob",error).isEmpty()); QVERIFY(!error.isEmpty());
        const auto media=readonly.context(imageSequence,"Bob",error); QVERIFY2(error.isEmpty(),qPrintable(error)); QCOMPARE(media.last().image,bytes);
        HistoryRecord record; QCOMPARE(readonly.append(message("must not write"),record,error),HistoryStore::SaveResult::Error);
        auto cancelled=std::make_shared<std::atomic_bool>(true); QVERIFY(readonly.search("needle","Bob",0,error,50,cancelled).records.isEmpty());
        HistoryStore missing; const auto absent=dir.filePath("absent.sqlite"); QVERIFY(!missing.openReadOnly(absent,error)); QVERIFY(!QFileInfo::exists(absent));
    }
    void focusedRecordAndEscapedText() {
        QTextBrowser browser; HistoryView view(&browser,"Alice"); HistoryRecord a,b; QString error;
        QVERIFY(HistoryStore::normalize(message("<b>same text</b>"),a,error)); b=a; a.sequence=10; b.sequence=20;
        view.replace({a,b}); QVERIFY(view.focusRecord(20)); QCOMPARE(browser.extraSelections().size(),1);
        QVERIFY(browser.extraSelections().first().cursor.selectedText().contains("<b>same text</b>"));
        QVERIFY(!view.focusRecord(999)); view.replace({}); QVERIFY(browser.extraSelections().isEmpty());
    }
    void asyncSearchPagingSelectionAndCancellation() {
        QTemporaryDir dir(QDir::currentPath()+"/search-dialog-XXXXXX"); QVERIFY(dir.isValid()); const auto file=dir.filePath("history.sqlite"); QString error;
        { HistoryStore store; QVERIFY(store.open(file,error)); populate(store); save(store,message("Last token","Bob")); save(store,message("Secret Carol","Carol")); }
        HistorySearchDialog dialog(file,"Alice","Bob"); dialog.show();
        auto *input=dialog.findChild<QLineEdit *>("historySearchInput"); auto *list=dialog.findChild<QListWidget *>("searchResults");
        auto *browser=dialog.findChild<QTextBrowser *>("searchContext"); auto *older=dialog.findChild<QPushButton *>("searchOlder"); auto *newer=dialog.findChild<QPushButton *>("searchNewer");
        input->setText("needle"); QTest::keyClick(input,Qt::Key_Return); QTRY_COMPARE_WITH_TIMEOUT(list->count(),50,5000); QVERIFY(older->isEnabled());
        QTRY_COMPARE_WITH_TIMEOUT(browser->extraSelections().size(),1,5000); QVERIFY(!browser->toPlainText().contains("public")); QVERIFY(!browser->toPlainText().contains("Secret Carol"));
        older->click(); QTRY_COMPARE_WITH_TIMEOUT(list->count(),50,5000); QVERIFY(newer->isEnabled()); older->click(); QTRY_COMPARE_WITH_TIMEOUT(list->count(),35,5000); QVERIFY(!older->isEnabled());
        newer->click(); QTRY_COMPARE_WITH_TIMEOUT(list->count(),50,5000);
        input->setText("needle"); QTest::keyClick(input,Qt::Key_Return); input->setText("LAST TOKEN"); QTest::keyClick(input,Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(list->count(),1,5000); QVERIFY(list->item(0)->text().contains("Last token"));
        QTRY_VERIFY_WITH_TIMEOUT(browser->toPlainText().contains("Last token"),5000);
        input->clear(); QTRY_COMPARE_WITH_TIMEOUT(list->count(),0,5000); QVERIFY(!older->isEnabled());
        // Closing during an in-flight request cancels work and safely joins its thread.
        auto *closing=new HistorySearchDialog(file,"Alice","Bob"); auto *query=closing->findChild<QLineEdit *>("historySearchInput");
        query->setText("no match"); closing->findChild<QPushButton *>("runHistorySearch")->click(); delete closing;
    }
    void missingDatabaseShowsError() {
        HistorySearchDialog dialog("missing-search-db.sqlite","Alice","");
        dialog.findChild<QLineEdit *>("historySearchInput")->setText("hello"); dialog.findChild<QPushButton *>("runHistorySearch")->click();
        QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QLabel *>("searchStatus")->text().contains("Không tìm được tin"),5000);
    }
};
QTEST_MAIN(SearchTests)
#include "search_tests.moc"
