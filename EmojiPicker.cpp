#include "EmojiPicker.h"

#include <QScrollBar>

//danh sach emoji
QStringList EmojiPicker::smileysEmojis()
{
    return {
        "😀", "😃", "😄", "😁", "😆", "😅", "🤣", "😂",
        "🙂", "🙃", "😉", "😊", "😇", "🥰", "😍", "🤩",
        "😘", "😗", "😚", "😙", "🥲", "😋", "😛", "😜",
        "🤪", "😝", "🤑", "🤗", "🤭", "🤫", "🤔", "🫡",
        "😐", "😑", "😶", "😏", "😒", "🙄", "😬", "🤥",
        "😌", "😔", "😪", "🤤", "😴", "😷", "🤒", "🤕",
        "🤢", "🤮", "🥵", "🥶", "🥴", "😵", "🤯", "🤠",
        "🥳", "🥸", "😎", "🤓", "🧐", "😕", "🫤", "😟",
        "🙁", "☹️", "😮", "😯", "😲", "😳", "🥺", "🥹",
        "😦", "😧", "😨", "😰", "😥", "😢", "😭", "😱",
        "😖", "😣", "😞", "😓", "😩", "😫", "🥱", "😤",
        "😡", "😠", "🤬", "😈", "👿", "💀", "☠️", "💩",
        "🤡", "👹", "👺", "👻", "👽", "👾", "🤖", "😺",
        "😸", "😹", "😻", "😼", "😽", "🙀", "😿", "😾"
    };
}

QStringList EmojiPicker::gesturesEmojis()
{
    return {
        "👋", "🤚", "🖐️", "✋", "🖖", "🫱", "🫲", "🫳",
        "🫴", "👌", "🤌", "🤏", "✌️", "🤞", "🫰", "🤟",
        "🤘", "🤙", "👈", "👉", "👆", "🖕", "👇", "☝️",
        "🫵", "👍", "👎", "✊", "👊", "🤛", "🤜", "👏",
        "🙌", "🫶", "👐", "🤲", "🤝", "🙏", "✍️", "💅",
        "🤳", "💪", "🦾", "🦿", "🦵", "🦶", "👂", "🦻",
        "👃", "🧠", "🫀", "🫁", "🦷", "🦴", "👀", "👁️",
        "👅", "👄", "🫦", "👶", "🧒", "👦", "👧", "🧑"
    };
}

QStringList EmojiPicker::heartsEmojis()
{
    return {
        "❤️", "🧡", "💛", "💚", "💙", "💜", "🖤", "🤍",
        "🤎", "💔", "❤️‍🔥", "❤️‍🩹", "❣️", "💕", "💞", "💓",
        "💗", "💖", "💘", "💝", "💟", "♥️", "💋", "💌",
        "💐", "🌹", "🥀", "🌺", "🌸", "💮", "🏵️", "🌻",
        "🌼", "🌷", "🪻", "🪷", "🌱", "🪴", "🌲", "🌳"
    };
}

QStringList EmojiPicker::animalsEmojis()
{
    return {
        "🐶", "🐱", "🐭", "🐹", "🐰", "🦊", "🐻", "🐼",
        "🐻‍❄️", "🐨", "🐯", "🦁", "🐮", "🐷", "🐸", "🐵",
        "🙈", "🙉", "🙊", "🐒", "🐔", "🐧", "🐦", "🐤",
        "🐣", "🐥", "🦆", "🦅", "🦉", "🦇", "🐺", "🐗",
        "🐴", "🦄", "🐝", "🪱", "🐛", "🦋", "🐌", "🐞",
        "🐜", "🪰", "🪲", "🪳", "🦟", "🦗", "🕷️", "🕸️",
        "🦂", "🐢", "🐍", "🦎", "🦖", "🦕", "🐙", "🦑",
        "🦐", "🦞", "🦀", "🐡", "🐠", "🐟", "🐬", "🐳"
    };
}

QStringList EmojiPicker::foodEmojis()
{
    return {
        "🍏", "🍎", "🍐", "🍊", "🍋", "🍌", "🍉", "🍇",
        "🍓", "🫐", "🍈", "🍒", "🍑", "🥭", "🍍", "🥥",
        "🥝", "🍅", "🍆", "🥑", "🥦", "🥬", "🥒", "🌶️",
        "🫑", "🌽", "🥕", "🫒", "🧄", "🧅", "🥔", "🍠",
        "🍕", "🍔", "🍟", "🌭", "🍿", "🧂", "🥓", "🥚",
        "🍳", "🧇", "🥞", "🧈", "🍞", "🥐", "🥖", "🫓",
        "🥨", "🧀", "🥗", "🥙", "🥪", "🌮", "🌯", "🫔",
        "🍱", "🍣", "🍜", "🍝", "🍛", "🍲", "🍰", "🎂"
    };
}

QStringList EmojiPicker::objectsEmojis()
{
    return {
        "⚽", "🏀", "🏈", "⚾", "🥎", "🎾", "🏐", "🏉",
        "🥏", "🎱", "🪀", "🏓", "🏸", "🏒", "🏑", "🥍",
        "🏏", "🪃", "🥅", "⛳", "🪁", "🎣", "🤿", "🎽",
        "🎿", "🛷", "🥌", "🎯", "🪀", "🎮", "🕹️", "🎲",
        "🎭", "🎨", "🎬", "🎤", "🎧", "🎼", "🎹", "🥁",
        "🪘", "🎷", "🎺", "🪗", "🎸", "🎻", "🎵", "🎶",
        "🔔", "📱", "💻", "⌨️", "🖥️", "🖨️", "📷", "📹",
        "🔦", "💡", "📚", "📖", "✏️", "📝", "🔑", "🔒"
    };
}

//tao va thiet lap UI

EmojiPicker::EmojiPicker(QWidget *parent)
    : QWidget(parent)
{
    setupUI();

    //cau hinh cua so popup
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(360, 320);
}

void EmojiPicker::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    //Widget nen bo goc
    QWidget *bgWidget = new QWidget(this);
    bgWidget->setStyleSheet(
        "background-color: #FFFFFF; "
        "border: 1px solid #E0E0E0; "
        "border-radius: 12px;"
    );

    QVBoxLayout *bgLayout = new QVBoxLayout(bgWidget);
    bgLayout->setContentsMargins(6, 6, 6, 6);

    //tab widget cho cac danh muc
    m_tabWidget = new QTabWidget(bgWidget);
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { border: none; }"
        "QTabBar::tab {"
        "  padding: 6px 8px;"
        "  font-size: 16px;"
        "  border: none;"
        "  background: transparent;"
        "  margin: 0 2px;"
        "}"
        "QTabBar::tab:selected {"
        "  border-bottom: 2px solid #25D366;"
        "  background: rgba(37, 211, 102, 0.1);"
        "  border-radius: 6px 6px 0 0;"
        "}"
        "QTabBar::tab:hover {"
        "  background: rgba(0, 0, 0, 0.05);"
        "  border-radius: 6px 6px 0 0;"
        "}"
    );

    //them cac tab
    m_tabWidget->addTab(
        createEmojiTab(smileysEmojis()), "😀"
    );
    m_tabWidget->addTab(
        createEmojiTab(gesturesEmojis()), "👋"
    );
    m_tabWidget->addTab(
        createEmojiTab(heartsEmojis()), "❤️"
    );
    m_tabWidget->addTab(
        createEmojiTab(animalsEmojis()), "🐶"
    );
    m_tabWidget->addTab(
        createEmojiTab(foodEmojis()), "🍕"
    );
    m_tabWidget->addTab(
        createEmojiTab(objectsEmojis()), "⚽"
    );

    bgLayout->addWidget(m_tabWidget);
    mainLayout->addWidget(bgWidget);
}

//tab chua luoi emoji
QWidget* EmojiPicker::createEmojiTab(
    const QStringList &emojis)
{
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff
    );
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical {"
        "  width: 4px; background: transparent;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #C0C0C0; border-radius: 2px;"
        "}"
        "QScrollBar::add-line:vertical,"
        "QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
    );

    QWidget *container = new QWidget();
    QGridLayout *gridLayout = new QGridLayout(container);
    gridLayout->setContentsMargins(4, 4, 4, 4);
    gridLayout->setSpacing(2);

    int columns = 8;  //8 emoji moi hang

    for (int i = 0; i < emojis.size(); ++i)
    {
        QPushButton *btn = new QPushButton(
            emojis[i], container
        );
        btn->setFixedSize(38, 38);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton {"
            "  font-size: 20px;"
            "  border: none;"
            "  background: transparent;"
            "  border-radius: 6px;"
            "  padding: 0;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(0, 0, 0, 0.08);"
            "}"
            "QPushButton:pressed {"
            "  background-color: rgba(0, 0, 0, 0.15);"
            "}"
        );

        //click emoji -> phat signal emojiSelected
        QString emoji = emojis[i];
        connect(btn, &QPushButton::clicked,
                [this, emoji]() {
                    emit emojiSelected(emoji);
                });

        gridLayout->addWidget(
            btn, i / columns, i % columns
        );
    }

    scrollArea->setWidget(container);
    return scrollArea;
}
