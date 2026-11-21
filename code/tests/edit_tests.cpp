#include "testserver.h"
#include "recallclient.h"
#include "recallstore.h"
#include "chathistory.h"
#include "historyprotocol.h"
#include "historysearchdialog.h"
#include "editmessagedialog.h"
#include <QtTest>
#include <QJsonDocument>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextCursor>
static QJsonObject original(const QString &to={}) {
    return HistoryProtocol::stamp({{"type",to.isEmpty()?"chat":"private"},{"sender","Alice"},{"to",to},{"text","old token"}});
}
static QJsonObject edited(const QJsonObject &p,const QString &text,int revision) {
    return {{"type","message_edited"},{"event_id",p.value("_event_id")},{"sender",p.value("sender")},{"to",p.value("to").toString()},{"kind",p.value("type")},{"_utc",p.value("_utc")},
        {"text",text},{"revision",revision},{"edited_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
}
static QJsonObject recalled(const QJsonObject &p) {
    return {{"type","message_recalled"},{"event_id",p.value("_event_id")},{"sender",p.value("sender")},{"to",p.value("to").toString()},{"kind",p.value("type")},{"_utc",p.value("_utc")}};
}
struct EditPeer {
    QList<QJsonObject> packets;QSslSocket socket;JsonChannel channel;SessionConnection session;QTextBrowser browser;ChatHistory history;RecallClient updates;
    EditPeer(const QString &name,const ChatServer &server,const QString &root)
        :channel(&socket),session(&socket,&channel,{name,"127.0.0.1",server.port(),TestPassword,true}),history(&browser,name,"127.0.0.1",server.port()),updates(&session,&channel,&history) {
        history.initialize(root);trustFixture(session,server);
        QObject::connect(&session,&SessionConnection::failed,&history,[](const QString &text){qWarning()<<"Fixture connection:"<<text;});
        QObject::connect(&channel,&JsonChannel::packetReceived,&history,[this](const QJsonObject &p){packets.append(p);if(HistoryProtocol::messageType(p.value("type").toString())) history.receive(p);});
    }
    QJsonObject latest(const QString &type) const {for(auto i=packets.crbegin();i!=packets.crend();++i) if(i->value("type")==type) return *i;return {};}
};
class EditTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings{QDir::currentPath()+"/edit-settings-XXXXXX"};
private slots:
    void initTestCase() {
        QVERIFY(settings.isValid());QCoreApplication::setOrganizationName("ChatTCP1-Edit-Tests");QCoreApplication::setApplicationName("EditTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    }
    void serverAuthorityRevisionRecallAndPersistence() {
        QTemporaryDir dir(QDir::currentPath()+"/edit-server-XXXXXX");const auto file=dir.filePath("recalls.sqlite");QString error,id;double revision=0;
        {
            RecallStore store;QVERIFY(store.open(file,error));const auto p=original("Bob");id=p.value("_event_id").toString();QVERIFY(store.remember(p));
            QVERIFY(store.edit(id,"Bob","unauthorized",0,error).isEmpty());QVERIFY(!error.isEmpty());
            QVERIFY(store.edit(id,"Alice","  ",0,error).isEmpty());QVERIFY(store.edit(id,"Alice",QString(4001,'x'),0,error).isEmpty());
            auto first=store.edit(id,"ALICE","new token <b>literal</b>",0,error);QVERIFY2(!first.isEmpty(),qPrintable(error));revision=first.value("revision").toDouble();QVERIFY(revision>0);
            QVERIFY(store.edit(id,"Alice","stale overwrite",0,error).isEmpty());QVERIFY(!error.isEmpty());
            auto second=store.edit(id,"Alice","newest token",qint64(revision),error);QVERIFY(!second.isEmpty());revision=second.value("revision").toDouble();
            auto image=original();image["type"]="image";QVERIFY(store.remember(image));QVERIFY(store.edit(image.value("_event_id").toString(),"Alice","bad image edit",0,error).isEmpty());
            qint64 cursor=0;bool more=false;QVERIFY(store.editPage("Carol",0,cursor,more,error).isEmpty());
        }
        RecallStore store;QVERIFY(store.open(file,error));qint64 cursor=0;bool more=false;const auto page=store.editPage("Bob",0,cursor,more,error);QCOMPARE(page.size(),1);QCOMPARE(page.first().toObject().value("text").toString(),"newest token");QCOMPARE(page.first().toObject().value("revision").toDouble(),revision);
        QVERIFY(!store.recall(id,"Alice",error).isEmpty());QVERIFY(store.edit(id,"Alice","resurrect",qint64(revision),error).isEmpty());QVERIFY(store.editPage("Bob",0,cursor,more,error).isEmpty());
    }
    void escapedTextSyncFramesRemainBounded() {
        QTemporaryDir dir(QDir::currentPath()+"/edit-frames-XXXXXX");RecallStore store;QString error;QVERIFY(store.open(dir.filePath("recalls.sqlite"),error));
        for(int i=0;i<30;++i) {auto p=original();QVERIFY(store.remember(p));QVERIFY(!store.edit(p.value("_event_id").toString(),"Alice",QString(4000,QChar(1)),0,error).isEmpty());}
        qint64 cursor=0;bool more=true;int count=0,pages=0;
        while(more) {const auto before=cursor;const auto items=store.editPage("Bob",cursor,cursor,more,error);QVERIFY2(error.isEmpty(),qPrintable(error));QVERIFY(cursor>before || !more);count+=items.size();++pages;
            const QJsonObject frame{{"type","edit_sync_page"},{"items",items},{"cursor",double(cursor)},{"more",more}};
            QVERIFY(QJsonDocument(frame).toJson(QJsonDocument::Compact).size()<VideoProtocol::MaxFrame);
        }
        QCOMPARE(count,30);QVERIFY(pages>1);
    }
    void clientRevisionReplayRecallAndSearch() {
        QTemporaryDir dir(QDir::currentPath()+"/edit-client-XXXXXX");const auto path=dir.filePath("history.sqlite");QString error;QJsonObject p;
        {
            HistoryStore store;QVERIFY(store.open(path,error));p=original();p["type"]="reply";p["quote"]="quoted source unchanged";HistoryRecord record;QCOMPARE(store.append(p,record,error),HistoryStore::SaveResult::Saved);const auto sequence=record.sequence;
            auto mismatch=edited(p,"wrong route",1);mismatch["sender"]="Bob";QVERIFY(!store.applyEdit(mismatch,error));QCOMPARE(HistoryProtocol::withEdit(p,mismatch),p);
            QVERIFY(store.applyEdit(edited(p,"new token <b>literal</b>",1),error));QVERIFY(store.applyEdit(edited(p,"newest token",3),error));QVERIFY(store.applyEdit(edited(p,"stale token",2),error));
            const auto rows=store.page(100,0,error).records;QCOMPARE(rows.size(),1);QCOMPARE(rows.first().sequence,sequence);QCOMPARE(rows.first().packet.value("text").toString(),"newest token");QCOMPARE(rows.first().packet.value("quote"),p.value("quote"));
            QVERIFY(store.search("old token","",0,error).records.isEmpty());QCOMPARE(store.search("newest token","",0,error).records.size(),1);
            auto late=original("Bob");QVERIFY(store.applyEdit(edited(late,"late latest",10),error));QCOMPARE(store.append(late,record,error),HistoryStore::SaveResult::Saved);QCOMPARE(record.packet.value("text").toString(),"late latest");
            QCOMPARE(store.append(late,record,error),HistoryStore::SaveResult::Duplicate);
        }
        HistoryStore store;QVERIFY(store.open(path,error));QCOMPARE(store.search("newest token","",0,error).records.size(),1);QVERIFY(store.applyRecall(recalled(p),error));QVERIFY(store.applyEdit(edited(p,"must not resurrect",99),error));
        QVERIFY(store.search("must not resurrect","",0,error).records.isEmpty());QVERIFY(store.search("newest token","",0,error).records.isEmpty());
    }
    void openViewsRefreshAndFormValidation() {
        QTemporaryDir dir(QDir::currentPath()+"/edit-ui-XXXXXX");QTextBrowser browser;ChatHistory history(&browser,"Alice","host",12345);QVERIFY(history.initialize(dir.path()));const auto p=original();QVERIFY(history.receive(p));
        HistorySearchDialog search(history.path(),"Alice","");QObject::connect(&history,&ChatHistory::messageEdited,&search,&HistorySearchDialog::invalidateEdit);
        auto *query=search.findChild<QLineEdit *>("historySearchInput");auto *list=search.findChild<QListWidget *>("searchResults");query->setText("old token");search.findChild<QPushButton *>("runHistorySearch")->click();QTRY_COMPARE_WITH_TIMEOUT(list->count(),1,5000);
        QVERIFY(history.receive(edited(p,"new token <b>literal</b>",1)));QTRY_COMPARE_WITH_TIMEOUT(list->count(),0,5000);QTest::qWait(100);QCOMPARE(list->count(),0);
        QVERIFY(browser.toPlainText().contains("new token <b>literal</b>"));QVERIFY(browser.toPlainText().contains("Đã chỉnh sửa"));QVERIFY(!browser.toPlainText().contains("old token"));
        query->setText("new token");search.findChild<QPushButton *>("runHistorySearch")->click();QTRY_COMPARE_WITH_TIMEOUT(list->count(),1,5000);
        auto bad=edited(p,QString(4001,'a'),2);QVERIFY(history.receive(bad));QVERIFY(browser.toPlainText().contains("new token"));
        QSslSocket socket;JsonChannel channel(&socket);SessionConnection session(&socket,&channel,{"Alice","host",12345,TestPassword,false});RecallClient client(&session,&channel,&history);EditMessageDialog dialog(&client,&history,p);
        auto *input=dialog.findChild<QPlainTextEdit *>("editMessageInput");auto *submit=dialog.findChild<QPushButton *>("submitMessageEdit");QSignalSpy failed(&client,&RecallClient::editFailed);
        input->setPlainText(" ");submit->click();QCOMPARE(failed.size(),0);input->setPlainText(QString(4001,'x'));submit->click();QCOMPARE(failed.size(),0);input->setPlainText("old token");submit->click();QCOMPARE(failed.size(),0);
        input->setPlainText("valid new content");submit->click();QCOMPARE(failed.size(),1);QVERIFY(submit->isEnabled());
    }
    void tlsEditPrivateOfflineRestartRecallAndPublic() {
        TestServer server;QVERIFY(server.start());QTemporaryDir data(QDir::currentPath()+"/edit-peers-XXXXXX");EditPeer alice("Alice",server,data.path()),bob("Bob",server,data.path()),carol("Carol",server,data.path());
        QSignalSpy done(&alice.updates,&RecallClient::editCompleted),failure(&alice.updates,&RecallClient::editFailed),unauthorized(&bob.updates,&RecallClient::editFailed),synced(&bob.updates,&RecallClient::editsSynchronized),recalledDone(&alice.updates,&RecallClient::completed);
        alice.session.connectNow();bob.session.connectNow();carol.session.connectNow();QTRY_COMPARE_WITH_TIMEOUT(alice.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(bob.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(carol.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(synced.size(),1,5000);
        alice.history.selectConversation("Bob");bob.history.selectConversation("Alice");
        QVERIFY(alice.channel.send({{"type","private"},{"to","Bob"},{"text","private old"}}));QTRY_VERIFY_WITH_TIMEOUT(!bob.latest("private").isEmpty(),5000);const auto id=bob.latest("private").value("_event_id").toString();
        QVERIFY(bob.updates.edit(id,"forged owner",0));QTRY_COMPARE_WITH_TIMEOUT(unauthorized.size(),1,5000);QVERIFY(bob.browser.toPlainText().contains("private old"));
        QVERIFY(alice.updates.edit(id,"private new <b>literal</b>",0));QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,5000);QTRY_VERIFY_WITH_TIMEOUT(bob.browser.toPlainText().contains("private new <b>literal</b>"),5000);QVERIFY(bob.browser.toPlainText().contains("Đã chỉnh sửa"));QVERIFY(carol.latest("message_edited").isEmpty());
        const auto revision=qint64(bob.latest("message_edited").value("revision").toDouble());QVERIFY(alice.updates.edit(id,"stale overwrite",0));QTRY_COMPARE_WITH_TIMEOUT(failure.size(),1,5000);
        bob.session.stop();QTRY_COMPARE(bob.session.state(),SessionConnection::Idle);QVERIFY(alice.updates.edit(id,"private latest offline",revision));QTRY_COMPARE_WITH_TIMEOUT(done.size(),2,5000);const auto last=qint64(alice.latest("message_edited").value("revision").toDouble());
        bob.session.connectNow();QTRY_COMPARE_WITH_TIMEOUT(bob.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(synced.size(),2,5000);QVERIFY(bob.browser.toPlainText().contains("private latest offline"));
        const auto port=server.port();server.shutdown();QTRY_COMPARE(alice.session.state(),SessionConnection::Idle);QTRY_COMPARE(bob.session.state(),SessionConnection::Idle);QTRY_COMPARE(carol.session.state(),SessionConnection::Idle);QVERIFY(server.start(port));bob.packets.clear();carol.packets.clear();
        alice.session.connectNow();bob.session.connectNow();carol.session.connectNow();QTRY_COMPARE_WITH_TIMEOUT(alice.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(bob.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(carol.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(synced.size(),3,5000);
        QVERIFY(!bob.latest("edit_sync_page").value("items").toArray().isEmpty());QTRY_VERIFY_WITH_TIMEOUT(!carol.latest("edit_sync_page").isEmpty(),5000);QVERIFY(carol.latest("edit_sync_page").value("items").toArray().isEmpty());
        QVERIFY(alice.updates.request(id));QTRY_COMPARE_WITH_TIMEOUT(recalledDone.size(),1,5000);QTRY_VERIFY_WITH_TIMEOUT(bob.browser.toPlainText().contains("Tin nhắn đã được thu hồi"),5000);QVERIFY(!bob.browser.toPlainText().contains("private latest offline"));
        QVERIFY(alice.updates.edit(id,"revive edited content",last));QTRY_COMPARE_WITH_TIMEOUT(failure.size(),2,5000);
        QVERIFY(alice.channel.send({{"type","chat"},{"text","room old"}}));QTRY_VERIFY_WITH_TIMEOUT(!carol.latest("chat").isEmpty(),5000);const auto roomId=carol.latest("chat").value("_event_id").toString();QVERIFY(alice.updates.edit(roomId,"room new",0));QTRY_COMPARE_WITH_TIMEOUT(done.size(),3,5000);QTRY_VERIFY_WITH_TIMEOUT(carol.browser.toPlainText().contains("room new"),5000);QVERIFY(carol.browser.toPlainText().contains("Đã chỉnh sửa"));
        alice.session.stop();bob.session.stop();carol.session.stop();
    }
};
QTEST_MAIN(EditTests)
#include "edit_tests.moc"
