#include "testserver.h"
#include "receiptclient.h"
#include "recallclient.h"
#include "recallstore.h"
#include "chathistory.h"
#include "historyprotocol.h"
#include "videoclient.h"
#include <QtTest>
#include <QBuffer>
#include <QDialog>
#include <QImage>
#include <QFile>
#include <QSettings>
#include <QSqlQuery>
#include <QTextBrowser>

static QJsonObject msg(const QString &sender="Alice",const QString &to="Bob") {
    return HistoryProtocol::stamp({{"type",to.isEmpty()?"chat":"private"},{"sender",sender},{"to",to},{"text","receipt hello"}});
}
static QJsonObject status(const QJsonObject &p,int state,int revision) {
    return {{"type","message_receipt"},{"event_id",p.value("_event_id")},{"sender",p.value("sender")},{"to",p.value("to")},{"state",state},{"revision",revision}};
}
struct ReceiptPeer {
    QList<QJsonObject> packets;QSslSocket socket;JsonChannel channel;SessionConnection session;QTextBrowser browser;ChatHistory history;RecallClient updates;ReceiptClient receipts;
    ReceiptPeer(const QString &name,const ChatServer &server,const QString &root)
        :channel(&socket),session(&socket,&channel,{name,"127.0.0.1",server.port(),TestPassword,true}),history(&browser,name,"127.0.0.1",server.port()),updates(&session,&channel,&history),receipts(&session,&channel,&history) {
        history.initialize(root);trustFixture(session,server);
        QObject::connect(&session,&SessionConnection::failed,&history,[](const QString &s){qWarning()<<"Fixture session:"<<s;});
        QObject::connect(&channel,&JsonChannel::packetReceived,&history,[this](const QJsonObject &p){packets.append(p);if(HistoryProtocol::messageType(p.value("type").toString())) history.receive(p);});
    }
    QJsonObject latest(const QString &type) const {for(auto i=packets.crbegin();i!=packets.crend();++i) if(i->value("type")==type) return *i;return {};}
    int state(const QString &id) const {
        int result=0;for(const auto &p:packets) {
            if(p.value("type")=="message_receipt" && p.value("event_id").toString()==id) result=qMax(result,p.value("state").toInt());
            if(p.value("_event_id").toString()==id) result=qMax(result,p.value("_receipt_state").toInt());
            if(p.value("type")=="receipt_sync_page") for(const auto &item:p.value("items").toArray()) if(item.toObject().value("event_id").toString()==id) result=qMax(result,item.toObject().value("state").toInt());
        }return result;
    }
};
class ReceiptTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings{QDir::currentPath()+"/receipt-settings-XXXXXX"};
    static qint64 save(HistoryStore &store,const QJsonObject &p) {HistoryRecord record;QString error;if(store.append(p,record,error)!=HistoryStore::SaveResult::Saved){QTest::qFail(qPrintable(error),__FILE__,__LINE__);return 0;}return record.sequence;}
private slots:
    void initTestCase() {
        QVERIFY(settings.isValid());QCoreApplication::setOrganizationName("ChatTCP1-Receipt-Tests");QCoreApplication::setApplicationName("ReceiptTests");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    }
    void serverAuthorityMonotonicAtomicAndPersistent() {
        QTemporaryDir dir(QDir::currentPath()+"/receipt-server-XXXXXX");QString error,id;const auto path=dir.filePath("recalls.sqlite");
        {
            RecallStore store;QVERIFY(store.open(path,error));const auto p=msg();id=p.value("_event_id").toString();QVERIFY(store.remember(p));QCOMPARE(store.receipt(id).value("state").toInt(),1);
            QJsonArray ignored;auto ack=[&](const QString &actor,int state){return store.acknowledge(actor,QJsonArray{QJsonObject{{"event_id",id},{"state",state}}},ignored,error);};
            QVERIFY(ack("Alice",3).isEmpty());QCOMPARE(ignored.size(),1);QVERIFY(ack("Carol",3).isEmpty());QCOMPARE(store.receipt(id).value("state").toInt(),1);
            QCOMPARE(ack("Bob",2).size(),1);QCOMPARE(store.receipt(id).value("state").toInt(),2);QCOMPARE(ack("Bob",3).size(),1);const auto seen=store.receipt(id);QCOMPARE(seen.value("state").toInt(),3);
            QCOMPARE(ack("Bob",2).first().toObject(),seen);QCOMPARE(store.receipt(id),seen);
            auto second=msg();QVERIFY(store.remember(second));const auto secondId=second.value("_event_id").toString();
            const QJsonArray bad{QJsonObject{{"event_id",secondId},{"state",2}},QJsonObject{{"event_id",id},{"state",4}}};QVERIFY(store.acknowledge("Bob",bad,ignored,error).isEmpty());QVERIFY(!error.isEmpty());QCOMPARE(store.receipt(secondId).value("state").toInt(),1);
            qint64 cursor=0;bool more=false;QVERIFY(store.receiptPage("Carol",0,cursor,more,error).isEmpty());QCOMPARE(store.receiptPage("Alice",0,cursor,more,error).size(),2);
        }
        RecallStore store;QVERIFY(store.open(path,error));QCOMPARE(store.receipt(id).value("state").toInt(),3);
    }
    void clientPersistencePendingReorderAndInventory() {
        QTemporaryDir dir(QDir::currentPath()+"/receipt-client-XXXXXX");QString error;const auto path=dir.filePath("history.sqlite");QJsonObject p;
        {
            HistoryStore store;QVERIFY(store.open(path,error));p=msg();const auto seq=save(store,p);QVERIFY(store.applyReceipt(status(p,3,3),error));QVERIFY(store.applyReceipt(status(p,2,4),error));QVERIFY(store.applyReceipt(status(p,1,1),error));
            auto row=store.page(100,0,error).records.first();QCOMPARE(row.sequence,seq);QCOMPARE(row.packet.value("_receipt_state").toInt(),3);QCOMPARE(row.packet.value("text"),p.value("text"));
            auto late=msg("Carol","Bob");QVERIFY(store.applyReceipt(status(late,2,5),error));HistoryRecord record;QCOMPARE(store.append(late,record,error),HistoryStore::SaveResult::Saved);QCOMPARE(record.packet.value("_receipt_state").toInt(),2);
            auto work=store.scanReceipts("Bob","Carol",false,0,error);QVERIFY(work.value("items").toArray().isEmpty());work=store.scanReceipts("Bob","Alice",true,0,error);QVERIFY(work.value("items").toArray().isEmpty());
            work=store.scanReceipts("bOb","Carol",true,0,error);QCOMPARE(work.value("items").toArray().size(),1);QCOMPARE(work.value("items").toArray().first().toObject().value("state").toInt(),3);
            auto pending=msg("Alice","Đức");save(store,pending);work=store.scanReceipts("đức","Alice",false,0,error);QCOMPARE(work.value("items").toArray().size(),1);QCOMPARE(work.value("items").toArray().first().toObject().value("state").toInt(),2);
            QVERIFY(store.ignoreReceipts(QJsonArray{pending.value("_event_id")},error));QVERIFY(store.scanReceipts("đức","Alice",true,0,error).value("items").toArray().isEmpty());
        }
        HistoryStore store;QVERIFY(store.open(path,error));const auto rows=store.page(100,0,error).records;bool found=false;for(const auto &row:rows) if(row.packet.value("_event_id")==p.value("_event_id")){found=true;QCOMPARE(row.packet.value("_receipt_state").toInt(),3);}QVERIFY(found);
    }
    void boundedInventoryAndSenderOnlyLabels() {
        QTemporaryDir dir(QDir::currentPath()+"/receipt-scan-XXXXXX");HistoryStore store;QString error;QVERIFY(store.open(dir.filePath("history.sqlite"),error));
        for(int i=0;i<205;++i) save(store,msg("Bob","Alice"));for(int i=0;i<123;++i) save(store,msg("Alice","Bob"));
        qint64 cursor=0;bool more=true;int count=0;QSet<QString> ids;while(more) {const auto work=store.scanReceipts("Bob","Alice",false,cursor,error);QVERIFY(error.isEmpty());const auto items=work.value("items").toArray();QVERIFY(items.size()<=50);for(const auto &item:items){QVERIFY(!ids.contains(item.toObject().value("event_id").toString()));ids.insert(item.toObject().value("event_id").toString());QCOMPARE(item.toObject().value("state").toInt(),2);}count+=items.size();cursor=qint64(work.value("cursor").toDouble());more=work.value("more").toBool();}QCOMPARE(count,123);
        auto p=msg();HistoryRecord record;QVERIFY(HistoryStore::normalize(p,record,error));record.sequence=1;QTextBrowser sender,receiver;HistoryView a(&sender,"Alice"),b(&receiver,"Bob");a.replace({record});b.replace({record});
        a.receipt(status(p,1,1));QVERIFY(sender.toPlainText().contains("Đã gửi"));a.receipt(status(p,2,2));QVERIFY(sender.toPlainText().contains("Đã nhận"));a.receipt(status(p,3,3));QVERIFY(sender.toPlainText().contains("Đã xem"));b.receipt(status(p,3,3));QVERIFY(!receiver.toPlainText().contains("Đã xem"));
        a.recall(p.value("_event_id").toString());a.receipt(status(p,3,4));QVERIFY(!sender.toPlainText().contains("Đã xem"));
        QWidget window;QVERIFY(!ReceiptVisibility::canMarkSeen(&window));window.showMinimized();QTest::qWait(20);QVERIFY(!ReceiptVisibility::canMarkSeen(&window));window.showNormal();QDialog modal(&window);modal.setWindowModality(Qt::ApplicationModal);modal.show();QTest::qWait(20);QVERIFY(!ReceiptVisibility::canMarkSeen(&window));
    }
    void failedStorageDoesNotDowngradeSeen() {
        QTemporaryDir dir(QDir::currentPath()+"/receipt-busy-XXXXXX");QTextBrowser browser;ChatHistory history(&browser,"Alice","host",12345);QVERIFY(history.initialize(dir.path()));history.selectConversation("Bob");const auto p=msg();QVERIFY(history.receive(p));
        const QString name="receipt-lock-"+QUuid::createUuid().toString();
        {
            auto lock=QSqlDatabase::addDatabase("QSQLITE",name);lock.setDatabaseName(history.path());QVERIFY(lock.open());
            {QSqlQuery q(lock);QVERIFY(q.exec("BEGIN IMMEDIATE"));QVERIFY(history.receive(status(p,3,3)));history.deferReceipt(status(p,2,2));QVERIFY(q.exec("ROLLBACK"));}
            lock.close();
        }
        QSqlDatabase::removeDatabase(name);history.selectConversation("Bob");QVERIFY(browser.toPlainText().contains("Đã xem"));
        QVERIFY(history.receive(status(p,1,1)));history.selectConversation("Bob");QVERIFY(browser.toPlainText().contains("Đã xem"));
        QVERIFY(history.receive(status(p,3,3)));QTextBrowser restored;ChatHistory again(&restored,"Alice","host",12345);QVERIFY(again.initialize(dir.path()));again.selectConversation("Bob");QVERIFY(restored.toPlainText().contains("Đã xem"));
    }
    void tlsReceivedThenSeenCorrectPeerOfflineRestartAndEdit() {
        TestServer server;QVERIFY(server.start());QTemporaryDir data(QDir::currentPath()+"/receipt-peers-XXXXXX");ReceiptPeer alice("Alice",server,data.path()),bob("Bob",server,data.path()),carol("Carol",server,data.path());
        QSignalSpy synced(&alice.receipts,&ReceiptClient::synchronized),bobSynced(&bob.receipts,&ReceiptClient::synchronized),editDone(&alice.updates,&RecallClient::editCompleted),recallDone(&alice.updates,&RecallClient::completed);
        alice.session.connectNow();bob.session.connectNow();carol.session.connectNow();QTRY_COMPARE_WITH_TIMEOUT(alice.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(bob.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(carol.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(synced.size(),1,5000);QTRY_COMPARE_WITH_TIMEOUT(bobSynced.size(),1,5000);
        alice.history.selectConversation("Bob");bob.history.selectConversation("Alice");
        QVERIFY(alice.channel.send({{"type","private"},{"to","Bob"},{"text","first receipt"}}));QTRY_VERIFY_WITH_TIMEOUT(!alice.latest("private").isEmpty(),5000);const auto id=alice.latest("private").value("_event_id").toString();QCOMPARE(alice.latest("private").value("_receipt_state").toInt(),1);QTRY_COMPARE_WITH_TIMEOUT(alice.state(id),2,5000);QVERIFY(alice.browser.toPlainText().contains("Đã nhận"));QVERIFY(!alice.browser.toPlainText().contains("Đã xem"));
        const auto request=QUuid::createUuid().toString(QUuid::WithoutBraces);QVERIFY(carol.channel.send({{"type","receipt_ack"},{"request_id",request},{"items",QJsonArray{QJsonObject{{"event_id",id},{"state",3}}}}}));QTRY_VERIFY_WITH_TIMEOUT(!carol.latest("receipt_ack_result").isEmpty(),5000);QCOMPARE(carol.latest("receipt_ack_result").value("ignored").toArray().size(),1);QCOMPARE(alice.state(id),2);QVERIFY(carol.latest("message_receipt").isEmpty());
        bob.receipts.setViewing("Carol",true);QTest::qWait(100);QCOMPARE(alice.state(id),2);
        alice.session.stop();QTRY_COMPARE(alice.session.state(),SessionConnection::Idle);bob.receipts.setViewing("Alice",true);QTRY_COMPARE_WITH_TIMEOUT(bob.state(id),3,5000);
        alice.session.connectNow();QTRY_COMPARE_WITH_TIMEOUT(alice.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(synced.size(),2,5000);QTRY_COMPARE_WITH_TIMEOUT(alice.state(id),3,5000);QVERIFY(alice.browser.toPlainText().contains("Đã xem"));
        bob.receipts.setReadGuard([]{return false;});QVERIFY(alice.channel.send({{"type","private"},{"to","Bob"},{"text","background receipt"}}));QTRY_VERIFY_WITH_TIMEOUT(alice.latest("private").value("_event_id").toString()!=id,5000);const auto second=alice.latest("private").value("_event_id").toString();QTRY_COMPARE_WITH_TIMEOUT(alice.state(second),2,5000);QTest::qWait(100);QCOMPARE(alice.state(second),2);bob.receipts.setReadGuard([]{return true;});QTRY_COMPARE_WITH_TIMEOUT(alice.state(second),3,5000);
        const auto port=server.port();server.shutdown();QTRY_COMPARE(alice.session.state(),SessionConnection::Idle);QTRY_COMPARE(bob.session.state(),SessionConnection::Idle);QTRY_COMPARE(carol.session.state(),SessionConnection::Idle);QVERIFY(server.start(port));alice.packets.clear();alice.session.connectNow();bob.session.connectNow();carol.session.connectNow();QTRY_COMPARE_WITH_TIMEOUT(alice.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(bob.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(carol.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(synced.size(),3,5000);QCOMPARE(alice.state(id),3);
        QVERIFY(alice.updates.edit(id,"edited receipt",0));QTRY_COMPARE_WITH_TIMEOUT(editDone.size(),1,5000);QTRY_VERIFY_WITH_TIMEOUT(alice.browser.toPlainText().contains("edited receipt"),5000);QVERIFY(alice.browser.toPlainText().contains("Đã xem"));QVERIFY(alice.browser.toPlainText().contains("Đã chỉnh sửa"));QVERIFY(alice.updates.request(id));QTRY_COMPARE_WITH_TIMEOUT(recallDone.size(),1,5000);QVERIFY(!alice.browser.toPlainText().contains("edited receipt"));
        alice.session.stop();bob.session.stop();carol.session.stop();
    }
    void privateImageStickerAndCompletedVideo() {
        TestServer server;QVERIFY(server.start());QTemporaryDir data(QDir::currentPath()+"/receipt-media-XXXXXX");ReceiptPeer alice("Alice",server,data.path()),bob("Bob",server,data.path());alice.session.connectNow();bob.session.connectNow();QTRY_COMPARE_WITH_TIMEOUT(alice.session.state(),SessionConnection::Ready,15000);QTRY_COMPARE_WITH_TIMEOUT(bob.session.state(),SessionConnection::Ready,15000);
        QImage image(12,12,QImage::Format_RGB32);image.fill(Qt::red);QByteArray bytes;QBuffer b(&bytes);b.open(QIODevice::WriteOnly);image.save(&b,"JPEG");
        QVERIFY(alice.channel.send({{"type","image"},{"to","Bob"},{"data",QString::fromLatin1(bytes.toBase64())}}));QTRY_VERIFY_WITH_TIMEOUT(!alice.latest("image").isEmpty(),5000);const auto imageId=alice.latest("image").value("_event_id").toString();QTRY_COMPARE_WITH_TIMEOUT(alice.state(imageId),2,5000);
        QVERIFY(alice.channel.send({{"type","sticker"},{"to","Bob"},{"id","nobita"}}));QTRY_VERIFY_WITH_TIMEOUT(!alice.latest("sticker").isEmpty(),5000);const auto stickerId=alice.latest("sticker").value("_event_id").toString();QTRY_COMPARE_WITH_TIMEOUT(alice.state(stickerId),2,5000);
        bob.receipts.setViewing("Alice",true);QTRY_COMPARE_WITH_TIMEOUT(alice.state(imageId),3,5000);QTRY_COMPARE_WITH_TIMEOUT(alice.state(stickerId),3,5000);
        QFile file(data.filePath("demo.mp4"));QVERIFY(file.open(QIODevice::WriteOnly));file.write(QByteArray::fromHex("000000186674797069736F6D0000000069736F6D6D703432"));file.close();VideoClient out(&alice.channel,&alice.socket,data.path()+"/out"),in(&bob.channel,&bob.socket,data.path()+"/in");
        QObject::connect(&alice.channel,&JsonChannel::packetReceived,&out,[&](const QJsonObject &p){out.handle(p);});QObject::connect(&bob.channel,&JsonChannel::packetReceived,&in,[&](const QJsonObject &p){in.handle(p);});QString id;
        QObject::connect(&out,&VideoClient::historyReady,&alice.history,[&](const QJsonObject &p){id=p.value("_event_id").toString();alice.history.completedVideo(p);});QObject::connect(&in,&VideoClient::historyReady,&bob.history,[&](const QJsonObject &p){bob.history.completedVideo(p);});
        QVERIFY(out.start(file.fileName(),"Bob"));QTRY_VERIFY_WITH_TIMEOUT(!id.isEmpty(),10000);QTRY_COMPARE_WITH_TIMEOUT(alice.state(id),3,5000);HistoryStore reader;QString error;QVERIFY(reader.openReadOnly(alice.history.path(),error));bool found=false;for(const auto &record:reader.page(100,0,error).records) if(record.packet.value("_event_id").toString()==id){found=true;QCOMPARE(record.packet.value("_receipt_state").toInt(),3);}QVERIFY(found);
        alice.session.stop();bob.session.stop();
    }
};
QTEST_MAIN(ReceiptTests)
#include "receipt_tests.moc"
