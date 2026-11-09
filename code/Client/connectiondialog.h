#pragma once
#include "connectionprofile.h"
#include <QDialog>
class QLineEdit;
class QSpinBox;
class QLabel;
class QComboBox;

class ConnectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConnectionDialog(QWidget *parent = nullptr);
    ConnectionProfile profile() const { return selected; }
private:
    ConnectionProfile selected;
    QLineEdit *username, *host;
    QSpinBox *port;
    QLabel *error;
    QLineEdit *password, *confirmation;
    QComboBox *mode;
};
