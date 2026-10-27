#ifndef CLIENTWINDOW_H
#define CLIENTWINDOW_H

#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class ClientWindow;
}
QT_END_NAMESPACE

class QListWidgetItem;
class QWidget;

class ClientWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ClientWindow(QWidget *parent = nullptr);

    ~ClientWindow();

private slots:

    // Xử lý nút Kết nối
    void onConnectClicked();

    // Xử lý nút Ngắt kết nối
    void onDisconnectClicked();

    // Xử lý nút Gửi
    void onSendClicked();

    // Xử lý khi chọn người dùng
    void onContactSelected(QListWidgetItem *item);

    // Xử lý tìm kiếm người dùng
    void onSearchTextChanged(const QString &text);

    // Xử lý nút Emoji
    void onEmojiClicked();

private:

    // Đối tượng giao diện được tạo từ ClientWindow.ui
    Ui::ClientWindow *ui;

    // Khởi tạo trạng thái ban đầu
    void setupInitialState();

    // Kết nối các nút với hàm xử lý
    void setupConnections();

    // Hiển thị một tin nhắn trên giao diện
    void addMessage(
        const QString &sender,
        const QString &content,
        bool isMine
    );

    // Cuộn xuống cuối khu vực tin nhắn
    void scrollToBottom();
};

#endif // CLIENTWINDOW_H