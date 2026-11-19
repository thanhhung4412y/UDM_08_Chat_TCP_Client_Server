#include "editmessagedialog.h"
#include "recallclient.h"
#include "chathistory.h"
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>
EditMessageDialog::EditMessageDialog(RecallClient *client,ChatHistory *history,QJsonObject packet,QWidget *parent) : QDialog(parent) {
    setWindowTitle("Chỉnh sửa tin nhắn");resize(540,370);auto *layout=new QVBoxLayout(this);layout->setContentsMargins(22,22,22,22);
    auto *hint=new QLabel("Sửa phần lời nhắn rồi bấm Lưu. Câu trích dẫn trong Reply và những bản sao đã chuyển tiếp vẫn giữ nguyên.");hint->setWordWrap(true);layout->addWidget(hint);
    input=new QPlainTextEdit();input->setObjectName("editMessageInput");input->setPlainText(packet.value("text").toString());layout->addWidget(input,1);
    status=new QLabel();status->setObjectName("editMessageStatus");status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    submit=buttons->button(QDialogButtonBox::Save);submit->setObjectName("submitMessageEdit");submit->setText("Lưu chỉnh sửa");cancel=buttons->button(QDialogButtonBox::Cancel);cancel->setText("Hủy");
    connect(buttons,&QDialogButtonBox::rejected,this,&EditMessageDialog::reject);
    connect(submit,&QPushButton::clicked,this,[this,client,packet]{
        const auto text=input->toPlainText().trimmed();
        if(text.isEmpty() || text.toUtf8().size()>4000 || text.contains(QChar(0))) {status->setText("Nội dung trống hoặc quá dài. Hãy rút ngắn tin nhắn.");return;}
        if(text==packet.value("text").toString().trimmed()) {status->setText("Nội dung chưa thay đổi.");return;}
        setBusy(true);status->setText("Đang chờ Server lưu chỉnh sửa…");
        if(!client->edit(packet.value("_event_id").toString(),text,qint64(packet.value("_edit_revision").toDouble()))) setBusy(false);
    });
    connect(client,&RecallClient::editCompleted,this,[this]{busy=false;accept();});
    connect(client,&RecallClient::editFailed,this,[this](const QString &text){setBusy(false);status->setText(text);});
    connect(history,&ChatHistory::messageRecalled,this,[this,packet](const QString &id){if(id==packet.value("_event_id").toString()){stale=true;setBusy(busy);status->setText("Tin đã thu hồi, không thể chỉnh sửa.");}});
    connect(history,&ChatHistory::messageEdited,this,[this,packet](const QJsonObject &notice){
        if(!busy && notice.value("event_id")==packet.value("_event_id") && notice.value("revision").toDouble()>packet.value("_edit_revision").toDouble()) {stale=true;setBusy(false);status->setText("Tin đã được cập nhật. Đóng và mở lại để sửa bản mới nhất.");}
    });
    input->setFocus();
}
void EditMessageDialog::setBusy(bool value) {busy=value;input->setReadOnly(busy||stale);submit->setEnabled(!busy&&!stale);cancel->setEnabled(!busy);}
void EditMessageDialog::reject() {if(!busy) QDialog::reject();}
