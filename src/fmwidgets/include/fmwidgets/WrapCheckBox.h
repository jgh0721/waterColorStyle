#pragma once
#include <QCheckBox>
#include <QStylePainter>
#include <QStyleOptionButton>
#include <QStyleOptionFocusRect>
#include <QEvent>
#include <algorithm>

namespace fm::ui {
// A native checkbox contract with a plain-text caption that can grow vertically.
// Retain QAbstractButton text so mnemonics and accessibility expose the full label.
class WrapCheckBox final : public QCheckBox
{
public:
    explicit WrapCheckBox(const QString& Text, QWidget* Parent = nullptr) : QCheckBox(Text, Parent)
    {
        auto Policy = sizePolicy(); Policy.setHeightForWidth(true); setSizePolicy(Policy);
    }
    QSize sizeHint() const override
    {
        const int Width = std::min(280, QCheckBox::sizeHint().width());
        return QSize(Width, heightForWidth(Width));
    }
    QSize minimumSizeHint() const override
    {
        return QSize(style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, this) + fontMetrics().averageCharWidth() * 4 + 12,
            QCheckBox::sizeHint().height());
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int Width) const override
    {
        QStyleOptionButton Option; initStyleOption(&Option); Option.rect = QRect(0, 0, Width, 1000);
        const auto Contents = style()->subElementRect(QStyle::SE_CheckBoxContents, &Option, this);
        const auto Bounds = fontMetrics().boundingRect(QRect(0, 0, std::max(1, Contents.width()), 10000), captionFlags(), text());
        return std::max(QCheckBox::sizeHint().height(), Bounds.height() + 8);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QStylePainter Painter(this); QStyleOptionButton Option; initStyleOption(&Option);
        const auto Contents = style()->subElementRect(QStyle::SE_CheckBoxContents, &Option, this);
        const bool Focus = Option.state.testFlag(QStyle::State_HasFocus);
        Option.text.clear(); Option.state &= ~QStyle::State_HasFocus;
        Painter.drawControl(QStyle::CE_CheckBox, Option);
        style()->drawItemText(&Painter, Contents, captionFlags(), Option.palette, isEnabled(), text(), QPalette::WindowText);
        if (Focus) {
            QStyleOptionFocusRect Frame; Frame.initFrom(this); Frame.rect = rect().adjusted(1, 1, -1, -1);
            Frame.backgroundColor = palette().color(QPalette::Window); Painter.drawPrimitive(QStyle::PE_FrameFocusRect, Frame);
        }
    }
    bool hitButton(const QPoint& Point) const override { return rect().contains(Point); }
    void changeEvent(QEvent* Event) override
    {
        QCheckBox::changeEvent(Event);
        if (Event->type() == QEvent::FontChange || Event->type() == QEvent::StyleChange) updateGeometry();
    }
private:
    int captionFlags() const
    {
        int Flags = QStyle::visualAlignment(layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter) | Qt::TextWordWrap | Qt::TextWrapAnywhere;
        Flags |= style()->styleHint(QStyle::SH_UnderlineShortcut, nullptr, this) ? Qt::TextShowMnemonic : Qt::TextHideMnemonic;
        return Flags;
    }
};
} // namespace fm::ui
