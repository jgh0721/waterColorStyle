#include "fmwidgets/SegmentedControl.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyleOptionButton>

#include <algorithm>

namespace fm::ui {

namespace {

// ▾ 조각: 체크 상태가 없는 메뉴 단추다(접근성에도 그렇게 보인다). 켜고 끄는 조각이 켜져 있으면 같은 단위로
// 읽히도록 켜진 모양(State_On — 바탕 · 아이콘 켜짐 색)으로 그린다.
class ToggleMenuButton final : public QPushButton
{
public:
    ToggleMenuButton(QAbstractButton *toggle, QWidget *parent)
        : QPushButton(parent)
        , m_toggle(toggle)
    {
    }

protected:
    void initStyleOption(QStyleOptionButton *option) const override
    {
        QPushButton::initStyleOption(option);
        if (m_toggle && m_toggle->isChecked())
            option->state |= QStyle::State_On;
    }

private:
    QPointer<QAbstractButton> m_toggle;
};

} // namespace

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
    // 레이아웃에는 켜고 끄는 조각도 있으므로 고르기 조각 수는 버튼 그룹으로 센다.
    if (m_items == items && m_group->buttons().size() == items.size())
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
    for (int i = 0; i < m_items.size(); ++i) {
        auto *b = new QPushButton(m_items.at(i), this);
        // 이 위젯에만 따로 스타일이 걸려 있으면(예: Qt Designer 미리보기) 조각에도 건다.
        if (testAttribute(Qt::WA_SetStyle))
            b->setStyle(style());
        m_group->addButton(b, i);
        m_layout->addWidget(b);
    }
    placeToggle();
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
    const auto sizeAndFont = [this](QAbstractButton *b) {
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
    };
    // 켜고 끄는 조각은 균등 폭으로 늘리지 않는다. ▾ 조각은 아이콘 폭만.
    if (m_toggle) {
        sizeAndFont(m_toggle);
        m_toggle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        m_toggle->setToolTip(m_toggleToolTip);
    }
    if (m_toggleMenuButton) {
        sizeAndFont(m_toggleMenuButton);
        m_toggleMenuButton->setFixedWidth(m_size == Mini ? 18 : 22);
    }
    const auto buttons = m_group->buttons();
    for (QAbstractButton *b : buttons) {
        const int i = m_group->id(b);
        sizeAndFont(b);
        b->setSizePolicy(m_expanding ? QSizePolicy::Expanding : QSizePolicy::Preferred, QSizePolicy::Fixed);
        m_layout->setStretch(i, m_expanding ? 1 : 0);
        // 균등 폭(목업 .seg-b: flex 1 1 0 · 좌우 6)은 글자 폭 + 12까지 줄어든다 — 좁은 카드에서도 들어가게.
        if (m_expanding && i >= 0 && i < m_items.size())
            b->setMinimumWidth(QFontMetrics(b->font()).horizontalAdvance(m_items.at(i)) + 12);
        else
            b->setMinimumWidth(0);
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
    if (m_toggleMenuButton) {
        m_toggleMenuButton->setIcon(fm::style::glyphIcon(glyph::toStyle(glyph::ChevronDown), colors, 10));
        m_toggleMenuButton->setIconSize(QSize(10, 10));
    }
    updateGeometry();
}

void SegmentedControl::setToggleItem(const QString &text, const QString &toolTip, QMenu *menu)
{
    if (text.isEmpty()) {
        delete m_toggleMenuButton;
        m_toggleMenuButton = nullptr;
        delete m_toggle;
        m_toggle = nullptr;
        m_toggleMenu = nullptr;
        m_toggleToolTip.clear();
        placeToggle();
        updateGeometry();
        return;
    }
    if (!m_toggle) {
        m_toggle = new QPushButton(this);
        if (testAttribute(Qt::WA_SetStyle))
            m_toggle->setStyle(style());
        m_toggle->setContextMenuPolicy(Qt::CustomContextMenu);
        m_toggle->installEventFilter(this);
        connect(m_toggle, &QAbstractButton::clicked, this, &SegmentedControl::toggleClicked);
        connect(m_toggle, &QAbstractButton::toggled, this, [this] {
            if (m_toggleMenuButton)
                m_toggleMenuButton->update();
        });
        connect(m_toggle, &QWidget::customContextMenuRequested, this, &SegmentedControl::showToggleMenu);
    }
    m_toggle->setText(text);
    m_toggleToolTip = toolTip;
    m_toggleMenu = menu;
    if (menu && !m_toggleMenuButton) {
        m_toggleMenuButton = new ToggleMenuButton(m_toggle, this);
        if (testAttribute(Qt::WA_SetStyle))
            m_toggleMenuButton->setStyle(style());
        m_toggleMenuButton->setEnabled(m_toggle->isEnabled());
        m_toggleMenuButton->installEventFilter(this);
        connect(m_toggleMenuButton, &QAbstractButton::clicked, this, &SegmentedControl::showToggleMenu);
    } else if (!menu && m_toggleMenuButton) {
        delete m_toggleMenuButton;
        m_toggleMenuButton = nullptr;
    }
    placeToggle();
    applyLook();
    refreshIcons();
}

QAbstractButton *SegmentedControl::toggleButton() const
{
    return m_toggle;
}

QAbstractButton *SegmentedControl::toggleMenuButton() const
{
    return m_toggleMenuButton;
}

bool SegmentedControl::isToggleChecked() const
{
    return m_toggle && m_toggle->isChecked();
}

void SegmentedControl::setToggleChecked(bool checked)
{
    if (!m_toggle || m_toggle->isChecked() == checked)
        return;
    {
        const QSignalBlocker blocker(m_toggle);
        m_toggle->setChecked(checked);
    }
    if (m_toggleMenuButton)
        m_toggleMenuButton->update();
}

void SegmentedControl::setToggleEnabled(bool enabled)
{
    if (m_toggle)
        m_toggle->setEnabled(enabled);
    if (m_toggleMenuButton)
        m_toggleMenuButton->setEnabled(enabled);
}

// 켜고 끄는 조각 · ▾ 조각을 고르기 조각 뒤로 옮기고 조각 위치(first · middle · last)를 다시 매긴다.
void SegmentedControl::placeToggle()
{
    QList<QAbstractButton *> buttons;
    for (int i = 0; i < m_items.size(); ++i)
        if (QAbstractButton *b = m_group->button(i))
            buttons.append(b);
    for (QPushButton *b : {m_toggle, m_toggleMenuButton}) {
        if (!b)
            continue;
        m_layout->removeWidget(b);
        m_layout->addWidget(b);
        buttons.append(b);
    }
    fm::style::setSegments(buttons);
    if (m_toggle)
        fm::style::setSegmentBreak(m_toggle, !m_items.isEmpty());
    if (m_toggleMenuButton)
        m_toggleMenuButton->setCheckable(false);  // setSegments가 모든 조각을 체크 가능으로 만든다
}

void SegmentedControl::showToggleMenu()
{
    if (!m_toggleMenu || !m_toggle || !m_toggle->isEnabled())
        return;
    // QPushButton의 메뉴처럼 열린 동안 ▾를 눌린 모양으로 둔다. exec 중에 위젯이 사라질 수 있다.
    const QPointer<QPushButton> arrow = m_toggleMenuButton;
    if (arrow)
        arrow->setDown(true);
    m_toggleMenu->exec(m_toggle->mapToGlobal(QPoint(0, m_toggle->height())));
    if (arrow)
        arrow->setDown(false);
}

bool SegmentedControl::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_toggle || watched == m_toggleMenuButton) && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Down && (key->modifiers() & Qt::AltModifier) && m_toggleMenu) {
            showToggleMenu();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
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
        for (QPushButton *b : {m_toggle, m_toggleMenuButton})
            if (b)
                b->setStyle(style());
    }
    if (event->type() == QEvent::PaletteChange && !m_glyphs.isEmpty())
        refreshIcons();  // ThemeScope 등으로 색이 바뀌었다
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange && m_expanding)
        applyLook();  // Normal 크기는 위젯 글꼴을 따른다 — 균등 폭의 최소 폭을 다시 잰다
}

} // namespace fm::ui
