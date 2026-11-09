#include "chathistory.h"
#include "historyimage.h"
#include "historyprotocol.h"
#include "videoclient.h"
#include <QtTest>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QPointer>
#include <QProcess>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextDocument>

class HistoryTests : public QObject {
    Q_OBJECT
    static QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
    static QJsonObject message(const QString &type = "chat") {
        QJsonObject p{{"type", type}, {"sender", "Alice"}, {"to", ""},
            {"text", QString::fromUtf8("Xin chào 🐱 ' ; DROP TABLE events; <b>hello</b>")},
            {"_event_id", uuid()}, {"_utc", "2026-10-29T10:00:00.000Z"}};
        if (type == "private") p["to"] = "Bob";
        if (type == "reply") p["quote"] = "Original <script>quote</script>";
        if (type == "forward") p["to"] = "Bob";
        if (type == "sticker") { p.remove("text"); p["id"] = "nobita"; }
        if (type == "image") {
            QImage image(12, 8, QImage::Format_RGB32); image.fill(Qt::green);
            QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "JPEG");
            p.remove("text"); p["data"] = QString::fromLatin1(bytes.toBase64());
        }
        return p;
    }
    static QJsonObject video(const QString &path) {
        auto p = message("video");
        p.remove("text"); p["id"] = p["_event_id"]; p["name"] = "demo.mp4";
        p["local_path"] = path; p["size"] = 12; p["sha256"] = QString(64, '0');
        return p;
    }
    static QString temporaryRoot(QTemporaryDir &dir) { return dir.path(); }
private slots:
    void persistence_data() {
        QTest::addColumn<QString>("type");
        for (const auto &t : {"chat", "private", "reply", "forward", "sticker", "image", "video"})
            QTest::newRow(t) << QString(t);
    }
    void persistence() {
        QFETCH(QString, type);
        QTemporaryDir dir(QDir::currentPath() + "/sqlite-test-XXXXXX"); QVERIFY(dir.isValid());
        const QString path = dir.filePath("history.sqlite");
        auto packet = type == "video" ? video(dir.filePath("already-moved.mp4")) : message(type);
        HistoryRecord saved; QString error;
        {
            HistoryStore store; QVERIFY2(store.open(path, error), qPrintable(error));
            QCOMPARE(store.append(packet, saved, error), HistoryStore::SaveResult::Saved);
            QVERIFY(saved.sequence > 0);
        }
        HistoryStore reopened; QVERIFY2(reopened.open(path, error), qPrintable(error));
        const auto page = reopened.page(100, 0, error);
        QVERIFY2(error.isEmpty(), qPrintable(error)); QCOMPARE(page.records.size(), 1);
        QCOMPARE(page.records.first().packet, saved.packet);
        QCOMPARE(page.records.first().image, saved.image);
        if (type == "image") {
            QVERIFY(!saved.image.isEmpty()); QVERIFY(!saved.packet.contains("data"));
            QVERIFY(!QImage::fromData(page.records.first().image, "JPEG").isNull());
        }
        if (type == "reply") QCOMPARE(page.records.first().packet.value("quote"), packet.value("quote"));
    }
    void separateProcessRestart() {
        QTemporaryDir dir(QDir::currentPath() + "/restart-test-XXXXXX"); QVERIFY(dir.isValid());
        const QString helper = QDir(QCoreApplication::applicationDirPath()).filePath("history_probe.exe");
        QVERIFY(QFile::exists(helper));
        QProcess writer; writer.start(helper, {"write", dir.filePath("history.sqlite")});
        QVERIFY(writer.waitForFinished(10000)); QCOMPARE(writer.exitStatus(), QProcess::NormalExit); QCOMPARE(writer.exitCode(), 0);
        QProcess reader; reader.start(helper, {"read", dir.filePath("history.sqlite")});
        QVERIFY(reader.waitForFinished(10000)); QCOMPARE(reader.exitStatus(), QProcess::NormalExit); QCOMPARE(reader.exitCode(), 0);
        const auto record = QJsonDocument::fromJson(reader.readAllStandardOutput()).object();
        QCOMPARE(record.value("type"), "reply");
        QVERIFY(record.value("text").toString().contains(QString::fromUtf8("Đã lưu")));
    }
    void paginationAndConcurrentConnections() {
        QTemporaryDir dir(QDir::currentPath() + "/paging-test-XXXXXX"); QVERIFY(dir.isValid());
        HistoryStore a, b; QString error; HistoryRecord record;
        QVERIFY(a.open(dir.filePath("history.sqlite"), error)); QVERIFY(b.open(a.path(), error));
        for (int i = 0; i < 125; ++i) {
            auto p = message(); p["text"] = QString::number(i);
            QCOMPARE((i % 2 ? a : b).append(p, record, error), HistoryStore::SaveResult::Saved);
        }
        const auto latest = a.page(50, 0, error); QVERIFY(error.isEmpty()); QVERIFY(latest.hasMore);
        QCOMPARE(latest.records.size(), 50); QCOMPARE(latest.records.first().packet.value("text").toString(), "75");
        const auto duplicate = latest.records.last().packet;
        QCOMPARE(b.append(duplicate, record, error), HistoryStore::SaveResult::Duplicate);
        QCOMPARE(b.append(message(), record, error), HistoryStore::SaveResult::Saved); // new event between page reads
        const auto previous = a.page(50, latest.records.first().sequence, error);
        QCOMPARE(previous.records.first().packet.value("text").toString(), "25");
        const auto first = a.page(50, previous.records.first().sequence, error);
        QCOMPARE(first.records.size(), 25); QVERIFY(!first.hasMore);
        QCOMPARE(first.records.first().packet.value("text").toString(), "0");
        QCOMPARE(a.page(10000, 0, error).records.size(), 100);
    }
    void accountIsolationAndAcceptedOnly() {
        QTemporaryDir dir(QDir::currentPath() + "/accounts-test-XXXXXX"); QVERIFY(dir.isValid());
        const auto alice = HistoryStore::accountPath(dir.path(), "127.0.0.1", 12345, "Alice");
        QCOMPARE(alice, HistoryStore::accountPath(dir.path(), "127.0.0.1", 12345, " alice "));
        QVERIFY(alice != HistoryStore::accountPath(dir.path(), "127.0.0.1", 12345, "Bob"));
        QVERIFY(alice != HistoryStore::accountPath(dir.path(), "127.0.0.1", 12346, "Alice"));
        QVERIFY(alice != HistoryStore::accountPath(dir.path(), "other-host", 12345, "Alice"));
        QTextBrowser browser;
        ChatHistory history(&browser, "Alice", "127.0.0.1", 12345); QVERIFY(history.initialize(dir.path()));
        auto accepted = message(); accepted["text"] = "only-once";
        QVERIFY(history.receive(accepted)); QVERIFY(history.receive(accepted));
        QCOMPARE(browser.toPlainText().count("only-once"), 1);
        auto foreign = message("private"); foreign["sender"] = "Bob"; foreign["to"] = "Eve"; foreign["text"] = "must-not-leak";
        QVERIFY(history.receive(foreign)); QVERIFY(!browser.toPlainText().contains("must-not-leak"));
        QVERIFY(!history.receive({{"type", "video_chunk"}}));
        QVERIFY(!history.receive({{"type", "system"}}));
        QVERIFY(!history.receive(video(dir.filePath("x.mp4")))); // only completedVideo accepts local paths
        HistoryStore store; QString error; QVERIFY(store.open(alice, error)); QCOMPARE(store.page(100, 0, error).records.size(), 1);
        QTextBrowser afterRestart; ChatHistory restored(&afterRestart, "Alice", "127.0.0.1", 12345);
        QVERIFY(restored.initialize(dir.path())); QVERIFY(afterRestart.toPlainText().contains("only-once"));
        QTextBrowser bob; ChatHistory isolated(&bob, "Bob", "127.0.0.1", 12345);
        QVERIFY(isolated.initialize(dir.path())); QVERIFY(!bob.toPlainText().contains("only-once"));
    }
    void renderingAndMissingVideo() {
        QTemporaryDir dir(QDir::currentPath() + "/render-test-XXXXXX"); QVERIFY(dir.isValid());
        QString error; HistoryRecord text, image, sticker, clip;
        QVERIFY(HistoryStore::normalize(message("reply"), text, error));
        QVERIFY(HistoryStore::normalize(message("image"), image, error));
        QVERIFY(HistoryStore::normalize(message("sticker"), sticker, error));
        const QString filePath = dir.filePath("movie.mp4"); QFile f(filePath); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("test"); f.close();
        QVERIFY(HistoryStore::normalize(video(filePath), clip, error));
        QTextBrowser browser; HistoryView view(&browser, "Alice");
        QPointer<QTextDocument> old = browser.document();
        view.replace({text, image, sticker, clip}); QVERIFY(old.isNull());
        QVERIFY(browser.toPlainText().contains("<b>hello</b>")); QVERIFY(browser.toPlainText().contains("<script>quote</script>"));
        const QUrl imageUrl("history-image:/" + image.packet.value("_event_id").toString());
        QVERIFY(!browser.document()->resource(QTextDocument::ImageResource, imageUrl).value<QImage>().isNull());
        const QUrl link("chatvideo:/" + clip.packet.value("_event_id").toString());
        QCOMPARE(view.videoPath(link, error), filePath); QVERIFY(error.isEmpty());
        QVERIFY(QFile::remove(filePath)); QVERIFY(view.videoPath(link, error).isEmpty()); QVERIFY(!error.isEmpty());
        QVERIFY(view.videoPath(QUrl("https://example.org"), error).isEmpty());
        view.replace({text}); QVERIFY(view.videoPath(link, error).isEmpty());
    }
    void corruptAndFutureDatabasePreserved() {
        QTemporaryDir dir(QDir::currentPath() + "/corrupt-test-XXXXXX"); QVERIFY(dir.isValid());
        const QString path = dir.filePath("bad.sqlite"); QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
        const QByteArray content("not a SQLite database"); f.write(content); f.close();
        HistoryStore store; QString error; QVERIFY(!store.open(path, error)); QVERIFY(!error.isEmpty());
        QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(), content); f.close();
        const QString future = dir.filePath("future.sqlite");
        const QString connection = uuid();
        { auto db = QSqlDatabase::addDatabase("QSQLITE", connection); db.setDatabaseName(future); QVERIFY(db.open());
          { QSqlQuery q(db); QVERIFY(q.exec("PRAGMA user_version=77")); } db.close(); }
        QSqlDatabase::removeDatabase(connection);
        QVERIFY(!store.open(future, error)); QVERIFY(error.contains("moi hon"));
        const QString check = uuid();
        { auto db = QSqlDatabase::addDatabase("QSQLITE", check); db.setDatabaseName(future); QVERIFY(db.open());
          { QSqlQuery q(db); QVERIFY(q.exec("PRAGMA user_version")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 77); } db.close(); }
        QSqlDatabase::removeDatabase(check);
    }
    void boundedImagePreviews() {
        QImage large(1280, 1280, QImage::Format_RGB32); large.fill(Qt::red);
        QByteArray bytes; QBuffer buffer(&bytes); QVERIFY(buffer.open(QIODevice::WriteOnly)); QVERIFY(large.save(&buffer, "JPEG"));
        const auto preview = historyImagePreview(bytes);
        QVERIFY(!preview.isNull()); QVERIFY(preview.width() <= 240 && preview.height() <= 240);
        QVERIFY(historyImagePreview("not a JPEG").isNull());
    }
    void failedStorageStillDisplaysLive() {
        QTemporaryDir dir(QDir::currentPath() + "/storage-test-XXXXXX"); QVERIFY(dir.isValid());
        QFile blocker(dir.filePath("file")); QVERIFY(blocker.open(QIODevice::WriteOnly)); blocker.close();
        QTextBrowser browser; ChatHistory history(&browser, "Alice", "127.0.0.1", 12345);
        QSignalSpy status(&history, &ChatHistory::storageStatus);
        QVERIFY(!history.initialize(blocker.fileName() + "/child"));
        auto p = message(); p["text"] = "live-even-when-database-fails";
        QVERIFY(history.receive(p)); QVERIFY(browser.toPlainText().contains("live-even-when-database-fails")); QVERIFY(status.size() >= 2);
    }
    void databaseBusyReportsFailure() {
        QTemporaryDir dir(QDir::currentPath() + "/busy-test-XXXXXX"); QVERIFY(dir.isValid());
        HistoryStore store; QString error; QVERIFY(store.open(dir.filePath("history.sqlite"), error));
        const QString connection = uuid();
        { auto lock = QSqlDatabase::addDatabase("QSQLITE", connection); lock.setDatabaseName(store.path()); QVERIFY(lock.open());
          QSqlQuery query(lock); QVERIFY(query.exec("BEGIN IMMEDIATE"));
          HistoryRecord record; QCOMPARE(store.append(message(), record, error), HistoryStore::SaveResult::Error);
          QVERIFY(!error.isEmpty()); QVERIFY(!record.packet.isEmpty()); QVERIFY(query.exec("ROLLBACK")); lock.close(); }
        QSqlDatabase::removeDatabase(connection);
        HistoryRecord record; QCOMPARE(store.append(message(), record, error), HistoryStore::SaveResult::Saved);
    }
    void excludesIncompleteVideoAndProtocolFrames() {
        QTemporaryDir dir(QDir::currentPath() + "/filter-test-XXXXXX"); QVERIFY(dir.isValid());
        HistoryStore store; QString error; QVERIFY(store.open(dir.filePath("history.sqlite"), error));
        for (const auto &type : {"video_start", "video_chunk", "video_end", "video_error", "users", "system", "error"}) {
            HistoryRecord record; QCOMPARE(store.append({{"type", type}}, record, error), HistoryStore::SaveResult::Error);
        }
        QCOMPARE(store.page(100, 0, error).records.size(), 0);
        const auto stamped = HistoryProtocol::stamp(message());
        QCOMPARE(HistoryProtocol::stamp(stamped), stamped);
    }
};
QTEST_MAIN(HistoryTests)
#include "history_tests.moc"
