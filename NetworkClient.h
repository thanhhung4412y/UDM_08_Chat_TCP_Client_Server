#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QObject>     
#include <QTcpSocket>  
#include <QString>     

class NetworkClient : public QObject {  
    Q_OBJECT 

public:
    explicit NetworkClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void sendMessage(const QString &message); 
    void sendData(const QByteArray &data);               

signals:
    void connectedToServer();                       
    void disconnectedFromServer();                  
    void messageReceived(const QString &message);   

private slots:
    void onConnected();     
    void onDisconnected();  
    void onReadyRead();     

private:
    QTcpSocket *m_socket; 
};

#endif