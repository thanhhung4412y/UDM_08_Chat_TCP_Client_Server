#pragma once
#include <QDialog>
#include <functional>
class SessionConnection;
class QLineEdit;
class QLabel;
class QPushButton;

class PasswordDialog : public QDialog {
    Q_OBJECT
public:
    PasswordDialog(SessionConnection *connection, std::function<bool()> canSubmit, QWidget *parent = nullptr);
    void reject() override;
private:
    bool busy = false;
    QLineEdit *currentPassword, *newPassword, *confirmation;
    QLabel *status;
    QPushButton *submit, *cancel;
    void setBusy(bool value);
};
