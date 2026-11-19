#pragma once
#include <QDialog>
class QLineEdit;
class QLabel;
class QPushButton;

class GmailSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit GmailSettingsDialog(QWidget *parent = nullptr);
private:
    QLineEdit *emailInput = nullptr;
    QLineEdit *passwordInput = nullptr;
    QLabel *resultLabel = nullptr;
    QPushButton *testButton = nullptr;
    QPushButton *saveButton = nullptr;
    void testGmail();
    void saveGmail();
    void removeGmail();
};
