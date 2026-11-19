#pragma once
#include <QDialog>
class SessionConnection;
class RecoveryDialog : public QDialog {
public:
    explicit RecoveryDialog(SessionConnection *connection, QWidget *parent=nullptr);
protected:
    void reject() override;
private:
    bool busy=false;
};
