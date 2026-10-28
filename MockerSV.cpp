#include <iostream>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    sockaddr_in server, client;
    int c;

    std::cout << "[Mock Server] Dang khoi tao Winsock...\n";
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "[Mock Server] Khoi tao that bai. Ma loi: " << WSAGetLastError() << "\n";
        return 1;
    }

    // Tao socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        std::cout << "[Mock Server] Khong the tao socket.\n";
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(5000); // Port mac dinh la 5000 nhu trong ClientWindow

    // Bind
    if (bind(server_socket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cout << "[Mock Server] Bind loi. Ma loi: " << WSAGetLastError() << "\n";
        return 1;
    }

    listen(server_socket, 3);
    std::cout << "[Mock Server] Dang lang nghe tai cong 5000...\n";

    c = sizeof(sockaddr_in);
    client_socket = accept(server_socket, (sockaddr*)&client, &c);
    if (client_socket == INVALID_SOCKET) {
        std::cout << "[Mock Server] Chap nhan ket noi that bai.\n";
        return 1;
    }

    std::cout << "[Mock Server] Da co Client ket noi thanh cong!\n";
    // vong lap nhan va phan hoi du lieu thu nghiem
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytesRead > 0) {
            std::cout << "[Mock Server] Nhan duoc " << bytesRead << " bytes tu Client.\n";
            // Echo lai chinh duoc do cho client de test
            send(client_socket, buffer, bytesRead, 0);
        } else {
            std::cout << "[Mock Server] Client da ngat ket noi.\n";
            break;
        }
    }

    closesocket(client_socket);
    closesocket(server_socket);
    WSACleanup();
    return 0;
}