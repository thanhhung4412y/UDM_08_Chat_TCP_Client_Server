#pragma once
#include <QAction>
#include <QLineEdit>
#include <QPainter>
#include <QIcon>
inline void addPasswordVisibility(QLineEdit *field) {
    QPixmap image(32,32); image.fill(Qt::transparent);
    { QPainter p(&image); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(QColor("#526e7b"),2));
      p.drawEllipse(QRectF(3,8,26,16)); p.drawEllipse(QRectF(12,12,8,8)); }
    auto *action = field->addAction(QIcon(image), QLineEdit::TrailingPosition);
    action->setText(QStringLiteral("Hiện mật khẩu"));
    action->setObjectName("toggle_" + field->objectName()); action->setCheckable(true);
    action->setToolTip(QStringLiteral("Hiện / ẩn nội dung đang nhập"));
    QObject::connect(action, &QAction::toggled, field, [field,action](bool visible) {
        action->setText(visible ? QStringLiteral("Ẩn mật khẩu") : QStringLiteral("Hiện mật khẩu"));
        field->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
        action->setToolTip(visible ? QStringLiteral("Ẩn") : QStringLiteral("Hiện"));
    });
}
