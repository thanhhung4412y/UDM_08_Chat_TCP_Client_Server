
#include "mainwindow.h"
#include "chatserver.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    ChatServer server;

    if (!server.start(12345)) {
        return 1;
    }

    MainWindow w;
    w.show();

    return a.exec();
}
