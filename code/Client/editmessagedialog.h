#pragma once
#include <QDialog>
#include <QJsonObject>
class RecallClient;
class ChatHistory;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class EditMessageDialog : public QDialog {
public:
    EditMessageDialog(RecallClient *client,ChatHistory *history,QJsonObject packet,QWidget *parent=nullptr);
protected:
    void reject() override;
private:
    bool busy=false,stale=false;
    QPlainTextEdit *input;QLabel *status;QPushButton *submit,*cancel;
    void setBusy(bool value);
};
