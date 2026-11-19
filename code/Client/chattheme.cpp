#include "chattheme.h"
#include <QApplication>
#include <QFont>
#include <QPalette>

void ChatTheme::apply(QApplication &app) {
    app.setStyle("Fusion");
    QFont font("Segoe UI", 10); font.setFamilies({"Segoe UI", "Segoe UI Emoji", "Segoe UI Symbol"}); app.setFont(font);
    QPalette palette; palette.setColor(QPalette::Window, QColor("#f4f7fa"));
    palette.setColor(QPalette::Base, Qt::white); palette.setColor(QPalette::Text, QColor("#193544"));
    palette.setColor(QPalette::Highlight, QColor("#0f766e")); palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);
    app.setStyleSheet(R"(
        QWidget { color:#193544; font-family:'Segoe UI','Segoe UI Emoji','Segoe UI Symbol'; font-size:10pt; }
        QDialog { background:#ffffff; }
        QLabel { background:transparent; }
        QFrame#Sidebar { background:#ffffff; border-right:1px solid #e2e9ef; }
        QFrame#ChatHeader { background:#ffffff; border-bottom:1px solid #e2e9ef; }
        QFrame#Composer { background:#ffffff; border:1px solid #dce6ed; border-radius:12px; }
        QFrame#TransferCard { background:#eaf5f2; border:1px solid #cce8df; border-radius:10px; }
        QLabel#Brand { color:#123b44; font-size:20px; font-weight:700; }
        QLabel#SectionLabel { color:#7b8f9c; font-size:10px; font-weight:600; }
        QLabel#Muted { color:#718592; font-size:11px; }
        QLabel#Title { color:#163745; font-size:20px; font-weight:600; }
        QLabel#Avatar { color:#ffffff; background:#0f766e; border-radius:20px; font-size:17px; font-weight:700; }
        QLabel#StatusBadge { color:#0f766e; background:#e3f3ec; border-radius:10px; padding:5px 10px; font-size:11px; }
        QLabel#StatusBadge[connected="false"] { color:#9a5e16; background:#fff0d9; }
        QLabel#Notice { color:#8b5e22; background:#fff4e3; border-radius:8px; padding:9px 12px; }
        QPushButton, QToolButton { color:#35515f; background:#ffffff; border:1px solid #dce5eb; border-radius:8px; padding:8px 12px; }
        QPushButton:hover, QToolButton:hover { background:#edf6f3; border-color:#b4d9ce; }
        QPushButton:pressed, QToolButton:pressed { background:#dbefe7; }
        QPushButton:disabled, QToolButton:disabled { color:#a0aeb7; background:#f1f4f6; border-color:#e7edf1; }
        QPushButton#SendButton { color:#ffffff; background:#0f766e; border-color:#0f766e; font-weight:600; }
        QPushButton#SendButton:hover { background:#0c655e; }
        QPushButton#SendButton:disabled { color:#a6beb8; background:#dceae5; border-color:#dceae5; }
        QPushButton#CreateGroup {background:#e4f4ef;border:0;color:#08776d;font-size:11px;font-weight:700;padding:8px 10px;}
        QPushButton#CreateGroup:hover {background:#d0ece4;}
        QToolButton#IconButton { padding:0; font-size:18px; }
        QToolButton::menu-indicator { image:none; }
        QLineEdit, QComboBox, QSpinBox { color:#193544; background:#ffffff; border:1px solid #d9e4eb; border-radius:8px; padding:9px 10px; selection-background-color:#0f766e; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color:#0f766e; }
        QLineEdit:disabled, QComboBox:disabled { color:#9aabb6; background:#f6f8fa; }
        QLineEdit#MessageInput { border:0; background:transparent; padding:9px 5px; }
        QComboBox QAbstractItemView { background:white; selection-background-color:#e1f2ea; selection-color:#0f766e; padding:5px; }
        QComboBox::drop-down { border:0; width:24px; subcontrol-origin:padding; subcontrol-position:top right; }
        QListWidget#PeopleList, QListWidget#GroupList { background:#ffffff; border:0; outline:0; }
        QListWidget#GroupList { border-bottom:1px solid #e3ebed; }
        QListWidget#PeopleList::item { padding:10px 8px; margin:2px 0; border-radius:8px; color:#35515f; }
        QListWidget#PeopleList::item:hover { background:#f0f6f5; }
        QListWidget#PeopleList::item:selected { background:#e6f4ee; color:#0f766e; }
        QTextBrowser#Messages { background:#f4f7fa; border:0; padding:12px; selection-background-color:#0f766e; }
        QTextBrowser { background:white; border:1px solid #dce5eb; border-radius:8px; padding:10px; }
        QMenu { color:#193544; background:#ffffff; border:1px solid #dbe6eb; padding:6px; }
        QMenu::item { padding:9px 25px 9px 14px; border-radius:5px; }
        QMenu::item:selected { background:#e7f4ee; color:#0f766e; }
        QMenu::item:disabled { color:#a0adb7; }
        QMenu::separator { height:1px; background:#e5edf1; margin:5px; }
        QScrollBar:vertical { background:transparent; width:9px; margin:2px; }
        QScrollBar::handle:vertical { background:#c3d1da; border-radius:4px; min-height:26px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }
        QToolTip { color:#ffffff; background:#234856; border:0; padding:6px; }
    )");
}
