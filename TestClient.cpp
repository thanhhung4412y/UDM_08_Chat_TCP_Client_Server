#include <QCoreApplication>
#include <QTimer>
#include <iostream>
#include "NetworkClient.h"
#include "Protocol.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "[Test Client] Dang khoi tao chuong trinh kiem thu...\n";

    NetworkClient client;
    // nghe tin hieu ket noi thanh cong 
    QObject::connect(&client, &NetworkClient::connectedToServer, [&client]() {
        std::cout << "[Test Client] [TC01 - PASS] Ket noi thanh cong toi Mock Server!\n";
        
        // Khi da ket noi, thu dong goi tin nhan qua Protocol va gui di
        QByteArray packet = Protocol::packMessage(Protocol::ChatText, "TestUser", "Xin chao server!", "");
        client.sendData(packet);
        
        std::cout << "[Test Client] Da gui goi tin test qua mang.\n";
    });

    //  nghe tin hieu mat ket noi
    QObject::connect(&client, &NetworkClient::disconnectedFromServer, []() {
        std::cout << "[Test Client] Da bi ngat ket noi khoi Server.\n";
    });

    //  nghe tin hieu nhan du lieu tu server tra ve
    QObject::connect(&client, &NetworkClient::messageReceived, [](const QString &msg) {
        std::cout << "[Test Client] Nhan duoc phan hoi: " << msg.toStdString() << "\n";
    });

    // Goi ham ket noi (ham nay tra ve void, ket qua se duoc thong qua signal connectedToServer o tren)
    client.connectToServer("127.0.0.1", 5000);

    // Tu dong thoat chuong trinh sau 5 giay chay test
    QTimer::singleShot(5000, &app, &QCoreApplication::quit);

    return app.exec();
}