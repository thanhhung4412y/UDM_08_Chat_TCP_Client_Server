#include "historystore.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
#include <cstdio>

// Separate process used to verify persistence across application restarts.
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 3) return 2;
    HistoryStore store; QString error;
    if (!store.open(args[2], error)) { QTextStream(stderr) << error; return 3; }
    if (args[1] == "write") {
        HistoryRecord record;
        const QJsonObject packet{{"type", "reply"}, {"sender", "Alice"},
            {"text", QString::fromUtf8("Đã lưu sau khi đóng ứng dụng — ' SQLite")}, {"quote", "original"},
            {"_event_id", "11111111-2222-4333-8444-555555555555"}};
        return store.append(packet, record, error) == HistoryStore::SaveResult::Saved ? 0 : 4;
    }
    const auto page = store.page(10, 0, error);
    if (!error.isEmpty() || page.records.size() != 1) return 5;
    const QByteArray json = QJsonDocument(page.records.first().packet).toJson(QJsonDocument::Compact);
    std::fwrite(json.constData(), 1, size_t(json.size()), stdout);
    return 0;
}
