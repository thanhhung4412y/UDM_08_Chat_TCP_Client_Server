#include "testserver.h"
#include "recallclient.h"
#include "recallstore.h"
#include "chathistory.h"
#include "historyprotocol.h"
#include "historyview.h"
#include "historysearchdialog.h"
#include "videoclient.h"
#include <QtTest>
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextCursor>

static QJsonObject message(const QString &to={}) {
    return HistoryProtocol::stamp({{"type",to.isEmpty()?"chat":"private"},{"sender","Alice"},{"to",to},{"text","recall-original-secret"}});
}
static QJsonObject notice(const QJsonObject &p) {
    return {{"type","message_recalled"},{"event_id",p.value("_event_id")},{"sender",p.value("sender")},{"to",p.value("to").toString()},
        {"kind",p.value("type")},{"_utc",p.value("_utc")},{"revoked_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
}
struct RecallPeer {
    QList<QJsonObject> packets;
    QSslSocket socket; JsonChannel channel; SessionConnection session;
    QTextBrowser browser; ChatHistory history; RecallClient recall;
    RecallPeer(const QString &name,const ChatServer &server,const QString &root,bool registering=true)
        : channel(&socket),session(&socket,&channel,{name,"127.0.0.1",server.port(),TestPassword,registering}),
          history(&browser,name,"127.0.0.1",server.port()),recall(&session,&channel,&history) {
        history.initialize(root); trustFixture(session,server);
        QObject::connect(&channel,&JsonChannel::packetReceived,&history,[this](const QJsonObject &p) {
            packets.append(p); if(HistoryProtocol::messageType(p.value("type").toString())) history.receive(p);
        });
    }
    QJsonObject latest(const QString &type) const { for(auto i=packets.crbegin();i!=packets.crend();++i) if(i->value("type")==type) return *i;return {}; }
};
class RecallTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings{QDir::currentPath()+"/recall-settings-XXXXXX"};
private slots:
    void initTestCase() {
        QVERIFY(settings.isValid());QCoreApplication::setOrganizationName("ChatTCP1-Recall-Tests");QCoreApplication::setApplicationName("RecallTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    }
    void authorityPersistentMetadataAndPagedPrivacy() {
        QTemporaryDir dir(QDir::currentPath()+"/recall-server-db-XXXXXX");QString error;const auto path=dir.filePath("recalls.sqlite");QString id;
        {
            RecallStore store;QVERIFY(store.open(path,error));auto p=message("Bob");id=p.value("_event_id").toString();QVERIFY(store.remember(p));
            QVERIFY(store.recall(id,"Bob",error).isEmpty());QVERIFY(!error.isEmpty());
            const auto first=store.recall(id,"ALICE",error);QVERIFY2(!first.isEmpty(),qPrintable(error));QCOMPARE(store.recall(id,"Alice",error),first);
            for(int i=0;i<60;++i) {auto room=message();QVERIFY(store.remember(room));QVERIFY(!store.recall(room.value("_event_id").toString(),"Alice",error).isEmpty());}
            qint64 cursor=0;bool more=true;int received=0;
            while(more) {const auto items=store.page("Carol",cursor,cursor,more,error);QVERIFY(error.isEmpty());QVERIFY(items.size()<=50);received+=items.size();for(const auto &item:items) {QVERIFY(item.toObject().value("to").toString().isEmpty());QVERIFY(!item.toObject().contains("text"));}}
            QCOMPARE(received,60);
        }
        RecallStore reopened;QVERIFY(reopened.open(path,error));QVERIFY(reopened.contains(id));QVERIFY(!reopened.recall(id,"Alice",error).isEmpty());
    }
    void clientTombstonesMediaReplayAndSearch() {
        QTemporaryDir dir(QDir::currentPath()+"/recall-client-db-XXXXXX");QString error;const auto path=dir.filePath("history.sqlite");
        QVector<QJsonObject> packets;
        {
            HistoryStore store;QVERIFY(store.open(path,error));
            for(const auto &kind:{"chat","private","reply","forward","image","sticker","video"}) {
                auto p=message(QString(kind)=="private"?"Bob":"");p["type"]=kind;
                if(QString(kind)=="reply") p["quote"]="quoted-secret";
                if(QString(kind)=="image") {QImage image(20,20,QImage::Format_RGB32);image.fill(Qt::red);QByteArray data;QBuffer b(&data);b.open(QIODevice::WriteOnly);image.save(&b,"JPEG");p["data"]=QString::fromLatin1(data.toBase64());}
                if(QString(kind)=="sticker") p["id"]="nobita";
                if(QString(kind)=="video") {p["id"]=p["_event_id"];p["name"]="demo.mp4";p["local_path"]=dir.filePath("demo.mp4");p["size"]=12;p["sha256"]=QString(64,'0');}
                HistoryRecord record;QCOMPARE(store.append(p,record,error),HistoryStore::SaveResult::Saved);
                QVERIFY2(store.applyRecall(notice(p),error),qPrintable(error));QVERIFY(store.applyRecall(notice(p),error));packets.append(p);
            }
            auto late=message();QVERIFY(store.applyRecall(notice(late),error));HistoryRecord record;QCOMPARE(store.append(late,record,error),HistoryStore::SaveResult::Saved);QVERIFY(record.packet.value("_recalled").toBool());QVERIFY(!record.packet.contains("text"));
            QVERIFY(store.search("recall-original-secret","",0,error).records.isEmpty());
        }
        HistoryStore reopened;QVERIFY(reopened.open(path,error));const auto page=reopened.page(100,0,error);QCOMPARE(page.records.size(),8);
        for(const auto &record:page.records) {QVERIFY(record.packet.value("_recalled").toBool());QVERIFY(!record.packet.contains("text"));QVERIFY(!record.packet.contains("quote"));QVERIFY(!record.packet.contains("local_path"));QVERIFY(record.image.isEmpty());}
        QTextBrowser browser;HistoryView view(&browser,"Alice");view.replace(page.records);QVERIFY(!browser.toPlainText().contains("recall-original-secret"));QCOMPARE(browser.toPlainText().count("Tin nhắn đã được thu hồi"),8);
        QVERIFY(view.videoPath(QUrl("chatvideo:/"+packets.last().value("_event_id").toString()),error).isEmpty());
    }
    void ownershipHitAndOpenSearchInvalidation() {
        QTemporaryDir dir(QDir::currentPath()+"/recall-view-XXXXXX");QTextBrowser browser;ChatHistory history(&browser,"Alice","host",12345);QVERIFY(history.initialize(dir.path()));
        const auto p=message();QVERIFY(history.receive(p));const int pos=browser.document()->find("recall-original-secret").selectionStart();QVERIFY(pos>=0);
        QCOMPARE(history.recallCandidate(pos).value("_event_id"),p.value("_event_id"));
        HistorySearchDialog dialog(history.path(),"Alice","");QObject::connect(&history,&ChatHistory::messageRecalled,&dialog,&HistorySearchDialog::invalidateRecall);
        auto *input=dialog.findChild<QLineEdit *>("historySearchInput");auto *list=dialog.findChild<QListWidget *>("searchResults");input->setText("recall-original-secret");dialog.findChild<QPushButton *>("runHistorySearch")->click();
        QTRY_COMPARE_WITH_TIMEOUT(list->count(),1,5000);QVERIFY(history.receive(notice(p)));QTRY_COMPARE_WITH_TIMEOUT(list->count(),0,5000);QTest::qWait(100);QCOMPARE(list->count(),0);QVERIFY(!browser.toPlainText().contains("recall-original-secret"));
        const int removed=browser.document()->find("Tin nhắn đã được thu hồi").selectionStart();QVERIFY(history.recallCandidate(removed).isEmpty());
    }
    void hitTestingSurvivesDocumentTrimming() {
        QTextBrowser browser; HistoryView view(&browser,"Alice"); view.replace({}); QVector<QJsonObject> packets;
        for(int i=0;i<180;++i) {
            auto p=message(); p["type"]="reply";p["quote"]="quoted";
            p["text"]=QString("row-%1-END").arg(i,3,10,QChar('0'))+QString("\nline").repeated(15);
            HistoryRecord record;QString error;QVERIFY(HistoryStore::normalize(p,record,error));record.sequence=i+1;view.append(record);packets.append(record.packet);
        }
        for(const auto &p:packets) {
            const auto token=p.value("text").toString().section('\n',0,0);const auto cursor=browser.document()->find(token);
            if(!cursor.isNull()) QCOMPARE(view.packetAt(cursor.selectionStart()).value("_event_id"),p.value("_event_id"));
        }
        view.enableRecallMenu();QCOMPARE(browser.contextMenuPolicy(),Qt::CustomContextMenu);
    }
    void tlsAuthorityPrivateIsolationAndOfflineResync() {
        TestServer server;QVERIFY(server.start());QTemporaryDir clients(QDir::currentPath()+"/recall-peers-XXXXXX");
        RecallPeer alice("Alice",server,clients.path()),bob("Bob",server,clients.path()),carol("Carol",server,clients.path());
        QSignalSpy synced(&bob.recall,&RecallClient::synchronized),failed(&bob.recall,&RecallClient::failed),done(&alice.recall,&RecallClient::completed);
        alice.session.connectNow();bob.session.connectNow();carol.session.connectNow();
        QTRY_COMPARE_WITH_TIMEOUT(alice.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(bob.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(carol.session.state(),SessionConnection::Ready,15000);QTRY_VERIFY_WITH_TIMEOUT(!synced.isEmpty(),5000);
        alice.history.selectConversation("Bob");bob.history.selectConversation("Alice");
        QVERIFY(alice.channel.send({{"type","private"},{"to","Bob"},{"text","private-original"}}));QTRY_VERIFY_WITH_TIMEOUT(!bob.latest("private").isEmpty(),5000);
        const QString id=bob.latest("private").value("_event_id").toString();QVERIFY(bob.recall.request(id));QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,5000);QVERIFY(bob.browser.toPlainText().contains("private-original"));
        QVERIFY(bob.channel.send({{"type","message_recalled"},{"event_id",id},{"sender","Alice"},{"to","Bob"}}));QTest::qWait(50);QVERIFY(alice.browser.toPlainText().contains("private-original"));
        bob.session.stop();QTRY_COMPARE(bob.session.state(),SessionConnection::Idle);QVERIFY(alice.recall.request(id));QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,5000);QVERIFY(!alice.browser.toPlainText().contains("private-original"));
        QTest::qWait(50);QVERIFY(carol.latest("message_recalled").isEmpty());
        bob.session.connectNow();QTRY_COMPARE_WITH_TIMEOUT(bob.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(synced.size(),2,5000);
        bob.history.selectConversation("Alice");QVERIFY(bob.browser.toPlainText().contains("Tin nhắn đã được thu hồi"));QVERIFY(!bob.browser.toPlainText().contains("private-original"));
        // A public message reaches all online clients, and a repeated recall stays idempotent.
        QVERIFY(alice.channel.send({{"type","chat"},{"text","public-original"}}));QTRY_VERIFY_WITH_TIMEOUT(!carol.latest("chat").isEmpty(),5000);const QString roomId=carol.latest("chat").value("_event_id").toString();
        QVERIFY(alice.recall.request(roomId));QTRY_COMPARE_WITH_TIMEOUT(done.size(),2,5000);QTRY_VERIFY_WITH_TIMEOUT(carol.browser.toPlainText().contains("Tin nhắn đã được thu hồi"),5000);
        QVERIFY(alice.recall.request(roomId));QTRY_COMPARE_WITH_TIMEOUT(done.size(),3,5000);
        alice.session.stop();bob.session.stop();carol.session.stop();
    }
    void completedVideoCanRecallWithoutDeletingFile() {
        TestServer server;QVERIFY(server.start());QTemporaryDir data(QDir::currentPath()+"/recall-video-XXXXXX");RecallPeer alice("Alice",server,data.path()),bob("Bob",server,data.path());
        alice.session.connectNow();bob.session.connectNow();QTRY_COMPARE_WITH_TIMEOUT(alice.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(bob.session.state(),SessionConnection::Ready,15000);
        QFile file(data.filePath("demo.mp4"));QVERIFY(file.open(QIODevice::WriteOnly));file.write(QByteArray::fromHex("000000186674797069736F6D0000000069736F6D6D703432"));file.close();
        VideoClient outgoing(&alice.channel,&alice.socket,data.path()+"/alice-video");VideoClient incoming(&bob.channel,&bob.socket,data.path()+"/bob-video");
        QObject::connect(&alice.channel,&JsonChannel::packetReceived,&outgoing,[&](const QJsonObject &p){outgoing.handle(p);});QObject::connect(&bob.channel,&JsonChannel::packetReceived,&incoming,[&](const QJsonObject &p){incoming.handle(p);});
        QString id,saved;
        QObject::connect(&outgoing,&VideoClient::historyReady,&alice.history,[&](const QJsonObject &p){id=p.value("_event_id").toString();alice.history.completedVideo(p);});
        QObject::connect(&incoming,&VideoClient::historyReady,&bob.history,[&](const QJsonObject &p){saved=p.value("local_path").toString();bob.history.completedVideo(p);});
        QSignalSpy done(&alice.recall,&RecallClient::completed);QVERIFY(outgoing.start(file.fileName(),"Bob"));QTRY_VERIFY_WITH_TIMEOUT(!id.isEmpty()&&!saved.isEmpty(),10000);QVERIFY(alice.recall.request(id));QTRY_COMPARE_WITH_TIMEOUT(done.size(),1,5000);
        bob.history.selectConversation("Alice");QVERIFY(bob.browser.toPlainText().contains("Tin nhắn đã được thu hồi"));QVERIFY(QFileInfo::exists(saved));
        alice.session.stop();bob.session.stop();
    }
};
QTEST_MAIN(RecallTests)
#include "recall_tests.moc"
