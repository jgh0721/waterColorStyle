#include "fmwidgets/PathEntryDelegate.h"
#include <fmwidgets/SettingsWidgets.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeManager.h>
#include <QApplication>
#include <QPainter>

namespace fm::ui {
PathEntryDelegate::PathEntryDelegate(QObject* Parent) : QStyledItemDelegate(Parent) {}
QSize PathEntryDelegate::sizeHint(const QStyleOptionViewItem& Option, const QModelIndex&) const
{
    return QSize(240, qMax(48, Option.fontMetrics.height() * 2 + 12));
}
void PathEntryDelegate::paint(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const
{
    auto Item = Option; initStyleOption(&Item, Index); const auto Raw = Item.text; Item.text.clear();
    const auto* Widget = Item.widget; auto* Style = Widget ? Widget->style() : QApplication::style();
    Painter->save(); Style->drawControl(QStyle::CE_ItemViewItem, &Item, Painter, Widget);
    const bool Rtl = Item.direction == Qt::RightToLeft;
    auto Content = Item.rect.adjusted(Rtl ? 8 : 34, 4, Rtl ? -34 : -8, -4);
    const auto GripX = Rtl ? Item.rect.right() - 17 : Item.rect.left() + 14;
    Painter->setPen(Qt::NoPen); Painter->setBrush(Item.palette.color(QPalette::Disabled, QPalette::Text));
    for (int X = 0; X < 2; ++X) for (int Y = 0; Y < 3; ++Y)
        Painter->drawEllipse(QPointF(GripX + X * 4, Item.rect.center().y() - 4 + Y * 4), 1, 1);
    const auto State = Status(Index.data(PathStatusRole).toInt());
    const auto Label = State == Exists ? tr("폴더") : State == Missing ? tr("없음") : State == Unresolved ? tr("확장 불가") : tr("확인 중…");
    const auto Chip = chipSize(Label, 11.5, 6, 20);
    const QRect Badge(Rtl ? Content.left() : Content.right() - Chip.width() + 1, Content.top(), Chip.width(), 20);
    paintChip(Painter, Badge, Label, State == Exists ? ChipKind::Ok : State == Pending ? ChipKind::Mute : ChipKind::Warn,
        fm::style::themeColorsFor(Widget), 4, 11.5, true);
    auto TextRect = Content; if (Rtl) TextRect.setLeft(Badge.right() + 8); else TextRect.setRight(Badge.left() - 8);
    TextRect.setHeight(Content.height() / 2);
    const bool Selected = Item.state.testFlag(QStyle::State_Selected);
    Painter->setPen(Selected ? Item.palette.highlightedText().color() : Item.palette.text().color());
    Painter->setFont(fm::style::monoFont(12));
    Painter->drawText(TextRect, Qt::AlignVCenter | Qt::AlignLeading, QFontMetrics(Painter->font()).elidedText(Raw, Qt::ElideMiddle, TextRect.width()));
    auto DetailRect = Content; DetailRect.setTop(TextRect.bottom() + 1);
    const auto Warning = Index.data(PathWarningRole).toString();
    const auto Detail = Warning.isEmpty() ? Index.data(ExpandedPathRole).toString() : Warning;
    if (!Selected) Painter->setPen(Item.palette.color(QPalette::Disabled, QPalette::Text));
    Painter->drawText(DetailRect, Qt::AlignVCenter | Qt::AlignLeading, QFontMetrics(Painter->font()).elidedText(Detail, Qt::ElideMiddle, DetailRect.width()));
    Painter->restore();
}
}