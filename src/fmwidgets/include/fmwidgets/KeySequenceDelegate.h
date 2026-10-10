#pragma once
#include "SettingsWidgets.h"
#include <fmstyle/ThemeManager.h>
#include <QApplication>
#include <QPainter>
#include <QStyledItemDelegate>

namespace fm::ui {
class KeySequenceDelegate final : public QStyledItemDelegate {
public:
    explicit KeySequenceDelegate(bool Dim, QObject* Parent = nullptr) : QStyledItemDelegate(Parent), dim_(Dim) {}
    void paint(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const override {
        QStyleOptionViewItem Background(Option); initStyleOption(&Background, Index); Background.text.clear();
        const auto* Style = Option.widget ? Option.widget->style() : QApplication::style(); Style->drawControl(QStyle::CE_ItemViewItem, &Background, Painter, Option.widget);
        const auto& Colors = fm::style::ThemeManager::instance().colors(); int X = Option.rect.left() + 5;
        Painter->save(); Painter->setClipRect(Option.rect);
        for (const auto& Key : Index.data().toString().split(", ", Qt::SkipEmptyParts)) {
            const auto Size = keyCapSize(Key); const QRect Box(X, Option.rect.center().y() - Size.height() / 2, Size.width(), Size.height()); paintKeyCap(Painter, Box, Key, Colors, dim_); X += Size.width() + 4;
        } Painter->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem& Option, const QModelIndex& Index) const override { auto Size = QStyledItemDelegate::sizeHint(Option, Index); int Width = 10;
        for (const auto& Key : Index.data().toString().split(", ", Qt::SkipEmptyParts)) Width += keyCapSize(Key).width() + 4;
        Size.setWidth(qMax(Size.width(), Width)); Size.setHeight(qMax(Size.height(), 26)); return Size; }
private:
    bool dim_;
};
}
