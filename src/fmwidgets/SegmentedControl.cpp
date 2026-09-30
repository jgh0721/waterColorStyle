#include "fmwidgets/SegmentedControl.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QPushButton>

#include <algorithm>

namespace fm::ui {

SegmentedControl::SegmentedControl(QWidget *parent)
    : SegmentedControl(QStringList(), parent)
{
}

SegmentedControl::SegmentedControl(const QStringList &items, QWidget *parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
    , m_group(new QButtonGroup(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);  // 조각의 테두리를 겹쳐 1 px 구분선으로
    m_group->setExclusive(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_group, &QButtonGroup::idClicked, this, &SegmentedControl::setCurrentIndex);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, &SegmentedControl::refreshIcons);
    setItems(items);
}

void SegmentedControl::setItems(const QStringList &items)
{
    if (m_items == items && m_layout->count() == items.size())
        return;
    m_items = items;
    rebuild();
}

void SegmentedControl::rebuild()
{
    const auto old = m_group->buttons();
    for (QAbstractButton *b : old) {
        m_group->removeButton(b);
        delete b;
    }
    QList<QAbstractButton *> buttons;
    for (int i = 0; i < m_items.size(); ++i) {
        auto *b = new QPushButton(m_items.at(i), this);
        // 이 위젯에만 따로 스타일이 걸려 있으면(예: Qt Designer 미리보기) 조각에도 건다.
        if (testAttribute(Qt::WA_SetStyle))
            b->setStyle(style());
        m_group->addButton(b, i);
        m_layout->addWidget(b);
        buttons.append(b);
    }
    fm::style::setSegments(buttons);
    applyLook();
    refreshIcons();
    if (m_items.isEmpty())
        m_current = -1;
    else
        m_current = std::clamp(m_current < 0 ? 0 : m_current, 0, int(m_items.size()) - 1);
    if (QAbstractButton *b = button(m_current))
        b->setChecked(true);
    updateGeometry();
}

void SegmentedControl::setSegmentSize(Size size)
{
    if (m_size == size)
        return;
    m_size = size;
    applyLook();
}

void SegmentedControl::setExpanding(bool expanding)
{
    if (m_expanding == expanding)
        return;
    m_expanding = expanding;
    applyLook();
}

void SegmentedControl::setItemToolTips(const QStringList &toolTips)
{
    m_toolTips = toolTips;
    applyLook();
}

// 크기 변형 · 글꼴 · 균등 폭 · 도구 설명을 조각에 건다.
void SegmentedControl::applyLook()
{
    const auto buttons = m_group->buttons();
    for (QAbstractButton *b : buttons) {
        const int i = m_group->id(b);
        fm::style::setSizeVariant(b, m_size == Mini      ? fm::style::SizeVariant::Mini
                                     : m_size == Small   ? fm::style::SizeVariant::Small
                                     : m_size == Compact ? fm::style::SizeVariant::Compact
                                                         : fm::style::SizeVariant::Normal);
        if (m_size == Mini)
            b->setFont(fm::style::pixelFont(QFont(), 11.5));
        else if (m_size == Small || m_size == Compact)
            b->setFont(fm::style::pixelFont(QFont(), 12));
        else
            b->setFont(QFont());
        b->setSizePolicy(m_expanding ? QSizePolicy::Expanding : QSizePolicy::Preferred, QSizePolicy::Fixed);
        m_layout->setStretch(i, m_expanding ? 1 : 0);
        b->setToolTip(i < m_toolTips.size() ? m_toolTips.at(i) : QString());
    }
    setSizePolicy(m_expanding ? QSizePolicy::Expanding : QSizePolicy::Fixed, QSizePolicy::Fixed);
    updateGeometry();
}

void SegmentedControl::setItemGlyphs(const QList<glyph::Glyph> &glyphs)
{
    m_glyphs = glyphs;
    refreshIcons();
}

// 아이콘 색은 조각 글자색을 따른다: 시안1 --fg2 · 켜짐 --accent-fg, 시안2 --fg · 마우스 올림/켜짐 흰 글자(입체 hover).
void SegmentedControl::refreshIcons()
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    fm::style::GlyphStateColors colors;
    if (tc.isWatercolor()) {
        const fm::style::WatercolorChrome &x = fm::style::watercolorChrome(tc.variant());
        colors.normal = tc[fm::style::Token::Fg];
        colors.active = x.hoverFg;
        colors.on = x.hoverFg;
        colors.disabled = x.disFg;
    } else {
        colors.normal = tc[fm::style::Token::Fg2];
        colors.on = tc[fm::style::Token::AccentFg];
        colors.disabled = tc[fm::style::Token::Fg3];
    }
    const auto buttons = m_group->buttons();
    for (QAbstractButton *b : buttons) {
        const int i = m_group->id(b);
        const glyph::Glyph g = i < m_glyphs.size() ? m_glyphs.at(i) : glyph::None;
        if (g == glyph::None) {
            b->setIcon(QIcon());
            continue;
        }
        b->setIcon(fm::style::glyphIcon(glyph::toStyle(g), colors, 14));
        b->setIconSize(QSize(14, 14));
    }
    updateGeometry();
}

void SegmentedControl::setCurrentIndex(int index)
{
    if (m_items.isEmpty()) {  // .ui에서 항목보다 먼저 올 수 있다 — 항목이 생기면 적용
        m_current = index;
        return;
    }
    if (index < -1 || index >= m_items.size())
        return;
    if (QAbstractButton *b = button(index))
        b->setChecked(true);
    if (m_current == index)
        return;
    m_current = index;
    Q_EMIT currentIndexChanged(index);
}

QAbstractButton *SegmentedControl::button(int index) const
{
    return m_group->button(index);
}

void SegmentedControl::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::StyleChange && testAttribute(Qt::WA_SetStyle)) {
        const auto buttons = m_group->buttons();
        for (QAbstractButton *b : buttons)
            b->setStyle(style());
    }
    if (event->type() == QEvent::PaletteChange && !m_glyphs.isEmpty())
        refreshIcons();  // ThemeScope 등으로 색이 바뀌었다
    QWidget::changeEvent(event);
}

} // namespace fm::ui
