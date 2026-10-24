#include "NetworkClient.h"
#include <QDebug> 

NetworkClient::NetworkClient(QObject *parent) : QObject(parent) {
    
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &NetworkClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkClient::onReadyRead);
}

void NetworkClient::connectToServer(const QString &host, quint16 port) {
    qDebug() << "Đang kết nối tới server:" << host << ":" << port;
    m_socket->connectToHost(host, port);
}

void NetworkClient::sendMessage(const QString &message) {
    
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(message.toUtf8());
    } else {
        qDebug() << "Chưa kết nối tới server, không gửi được!";
    }
}

void NetworkClient::sendData(const QByteArray &data) {
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(data);
    } else {
        qDebug() << "Chưa kết nối tới server, không gửi được dữ liệu!";
    }
}

void NetworkClient::onConnected() {
    qDebug() << "Đã kết nối thành công tới Server!";
    emit connectedToServer(); 
}

void NetworkClient::onDisconnected() {
    qDebug() << "Đã bị mất kết nối với Server!";
    emit disconnectedFromServer(); 
}

void NetworkClient::onReadyRead() {
    QByteArray data = m_socket->readAll(); 
    QString message = QString::fromUtf8(data);
    qDebug() << "Nhận được tin từ server:" << message;
    emit messageReceived(message);
}