#ifndef EMOJIPICKER_H
#define EMOJIPICKER_H

#include <QGridLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

class EmojiPicker : public QWidget {
  Q_OBJECT

public:
  explicit EmojiPicker(QWidget *parent = nullptr);

  // danh sach emoji theo danh muc
  static QStringList smileysEmojis();  // mat cuoi
  static QStringList gesturesEmojis(); // cu chi tay
  static QStringList heartsEmojis();   // trai tim
  static QStringList animalsEmojis();  // dong vat
  static QStringList foodEmojis();     // do an
  static QStringList objectsEmojis();  // do vat

signals:
  // signal khi nguoi dung click chon mot emoji
  void emojiSelected(const QString &emoji);

private:
  void setupUI();

  // tab chua luoi emoji
  QWidget *createEmojiTab(const QStringList &emojis);

  QTabWidget *m_tabWidget;
};

#endif
