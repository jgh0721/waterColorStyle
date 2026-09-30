// 패널 부품 — 탭 줄 · 드라이브 단추 · 경로 이동 줄 · 상태 줄 (01 §1.3 · 06 §4.12 · §4.14 · §4.15).

#include "fmwidgets/BreadcrumbBar.h"
#include "fmwidgets/DriveButton.h"
#include "fmwidgets/PanelStatusBar.h"
#include "fmwidgets/PanelTabStrip.h"

#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace fm::ui {

using fm::style::Token;

namespace {

bool isWatercolor(const QWidget *w)
{
    return fm::style::themeColorsFor(w).isWatercolor();
}

} // namespace

// ---------------------------------------------------------------- PanelTabStrip

PanelTabStrip::PanelTabStrip(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
    , m_tabs(new QTabBar(this))
    , m_add(new QToolButton(this))
{
    m_tabs->setDocumentMode(true);
    m_tabs->setDrawBase(false);
    m_tabs->setExpanding(false);
    m_tabs->setElideMode(Qt::ElideNone);
    m_tabs->setMovable(true);
    m_tabs->setUsesScrollButtons(true);
    m_tabs->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);  // 탭을 닫으면 직전 탭으로
    m_tabs->setFocusPolicy(Qt::NoFocus);
    m_tabs->installEventFilter(this);
    m_add->setToolTip(u"새 탭 (Ctrl+T)"_s);
    m_add->setAccessibleName(u"새 탭"_s);
    m_add->setFocusPolicy(Qt::NoFocus);

    m_layout->setSpacing(2);
    m_layout->addWidget(m_tabs, 0, Qt::AlignBottom);
    m_addBox = new QVBoxLayout;
    m_addBox->setSpacing(0);
    m_addBox->addStretch(1);
    m_addBox->addWidget(m_add);
    m_layout->addLayout(m_addBox);
    m_layout->addStretch(1);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    connect(m_add, &QToolButton::clicked, this, &PanelTabStrip::newTabRequested);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, &PanelTabStrip::applyMetrics);
    applyMetrics();
}

bool PanelTabStrip::isPaneActive() const
{
    return property(fm::style::props::kPaneActive).toBool();
}

void PanelTabStrip::setPaneActive(bool active)
{
    fm::style::setPaneActive(this, active);
    m_tabs->update();
}

void PanelTabStrip::applyMetrics()
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    const bool wc = tc.isWatercolor();
    // 시안1: 좌우 8 · 탭과 새 탭 사이 2 · 새 탭 28 × 28 투명 / 시안2: 좌우 4 · 새 탭 21 × 21 입체, 바깥 여백 0 0 2 6
    m_layout->setContentsMargins(wc ? 4 : 8, 0, wc ? 4 : 8, 0);
    m_layout->setSpacing(wc ? 6 : 2);
    m_add->setAutoRaise(!wc);
    m_add->setFixedSize(wc ? QSize(21, 21) : QSize(28, 28));
    m_add->setIcon(fm::style::glyphIcon(fm::style::Glyph::PlusSmall, wc ? tc[Token::Fg] : tc[Token::Fg3], 12));
    m_add->setIconSize(QSize(12, 12));
    m_addBox->setContentsMargins(0, 0, 0, wc ? 2 : 0);  // 시안2의 새 탭 단추는 밑선에서 2 px 떨어진다
    setFixedHeight(sizeHint().height());
    updateGeometry();
    update();
}

QSize PanelTabStrip::sizeHint() const
{
    return QSize(320, isWatercolor(this) ? 28 : 32);
}

QSize PanelTabStrip::minimumSizeHint() const
{
    return QSize(80, sizeHint().height());
}

void PanelTabStrip::paintEvent(QPaintEvent *)
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    QPainter p(this);
    p.fillRect(rect(), tc[Token::Win]);
    const QColor line = tc.isWatercolor() ? fm::style::watercolorChrome(tc.variant()).tabLine : tc[Token::Line];
    p.fillRect(QRect(0, height() - 1, width(), 1), line);
}

void PanelTabStrip::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange)
        applyMetrics();
}

bool PanelTabStrip::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tabs && event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::MiddleButton) {
            const int index = m_tabs->tabAt(mouse->position().toPoint());
            if (index >= 0) {
                Q_EMIT closeTabRequested(index);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ---------------------------------------------------------------- DriveButton

DriveButton::DriveButton(QWidget *parent)
    : QPushButton(parent)
{
    setText(u"D:"_s);
    setAccessibleName(u"드라이브 선택"_s);
    setFocusPolicy(Qt::TabFocus);
    setAutoDefault(false);
    fm::style::setSmall(this);  // 시안2: 작은 입체 단추
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, [this] {
        updateGeometry();
        update();
    });
}

QFont DriveButton::labelFont() const
{
    return fm::style::pixelFont(font(), 12.5, isWatercolor(this) ? QFont::Normal : QFont::DemiBold);
}

QSize DriveButton::sizeHint() const
{
    // 좌우 8 · 아이콘 16 · 간격 6 · 글자 · 간격 6 · 꺾쇠 10
    const int textWidth = QFontMetrics(labelFont()).horizontalAdvance(text());
    return QSize(8 + 16 + 6 + textWidth + 6 + 10 + 8, isWatercolor(this) ? 24 : 26);
}

QSize DriveButton::minimumSizeHint() const
{
    return sizeHint();
}

void DriveButton::paintEvent(QPaintEvent *)
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    const bool wc = tc.isWatercolor();
    QStylePainter p(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    option.features &= ~QStyleOptionButton::HasMenu;  // 꺾쇠는 직접 그린다
    option.text.clear();
    p.drawControl(QStyle::CE_PushButtonBevel, option);

    const bool hot = wc && (isDown() || underMouse()) && isEnabled();
    const QColor text = !isEnabled() ? tc[Token::Fg3]
                      : hot          ? fm::style::watercolorChrome(tc.variant()).hoverFg
                                     : tc[Token::Fg];
    const int shift = isDown() && wc ? 1 : 0;
    const QRect r = rect().translated(shift, shift);
    const qreal cy = r.center().y() + 0.5;
    fm::style::paintGlyph(&p, fm::style::Glyph::Drive, QRectF(r.left() + 8, cy - 8, 16, 16), hot ? text : tc[Token::Fg2]);
    const QFont font = labelFont();
    p.setFont(font);
    p.setPen(text);
    const int textWidth = QFontMetrics(font).horizontalAdvance(this->text());
    p.drawText(QRect(r.left() + 30, r.top(), textWidth + 1, r.height()), Qt::AlignLeft | Qt::AlignVCenter, this->text());
    fm::style::paintGlyph(&p, fm::style::Glyph::ChevronDown, QRectF(r.left() + 30 + textWidth + 6, cy - 5, 10, 10),
                          hot ? text : tc[Token::Fg3]);
}

// ---------------------------------------------------------------- BreadcrumbBar

BreadcrumbBar::BreadcrumbBar(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAccessibleName(u"경로"_s);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, [this] {
        updateGeometry();
        layoutCrumbs();
        update();
    });
}

bool BreadcrumbBar::watercolor() const
{
    return isWatercolor(this);
}

void BreadcrumbBar::setSegments(const QStringList &segments)
{
    m_segments = segments;
    m_hover = m_pressed = -2;
    layoutCrumbs();
    update();
}

QSize BreadcrumbBar::sizeHint() const
{
    return QSize(240, watercolor() ? 24 : 26);
}

QSize BreadcrumbBar::minimumSizeHint() const
{
    return QSize(60, sizeHint().height());
}

void BreadcrumbBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutCrumbs();
    if (m_editor)
        m_editor->setGeometry(rect());
}

void BreadcrumbBar::layoutCrumbs()
{
    m_crumbs.clear();
    const bool wc = watercolor();
    // 시안1: 꺾쇠 12 + 조각(좌우 6, 높이 26) / 시안2: 들어간 칸 안쪽 2, 조각 20 · 좌우 4
    const int pad = wc ? 4 : 6;
    const int inset = wc ? 2 : 0;
    const int crumbHeight = wc ? 20 : 26;
    const QFont regular = fm::style::pixelFont(font(), 13);
    QFont bold = regular;
    if (!wc)
        bold.setWeight(QFont::DemiBold);
    const int top = (height() - crumbHeight) / 2;
    const int available = width() - 2 * inset;

    auto crumbWidth = [&](int i) {
        const QFont &f = i == m_segments.size() - 1 ? bold : regular;
        return 12 + QFontMetrics(f).horizontalAdvance(m_segments.at(i)) + 2 * pad;
    };
    int first = 0;
    int total = 0;
    for (int i = 0; i < m_segments.size(); ++i)
        total += crumbWidth(i);
    const int ellipsis = 12 + QFontMetrics(regular).horizontalAdvance(u"…"_s) + 2 * pad;
    while (first < m_segments.size() - 1 && total > available) {
        total -= crumbWidth(first);
        if (first == 0)
            total += ellipsis;
        ++first;
    }
    int x = inset;
    auto place = [&](int index, int textWidth) {
        Crumb c;
        c.index = index;
        c.chevron = QRect(x, (height() - 12) / 2, 12, 12);
        x += 12;
        c.rect = QRect(x, top, textWidth + 2 * pad, crumbHeight);
        x += c.rect.width();
        m_crumbs.append(c);
    };
    if (first > 0)
        place(-1, QFontMetrics(regular).horizontalAdvance(u"…"_s));
    for (int i = first; i < m_segments.size(); ++i) {
        const QFont &f = i == m_segments.size() - 1 ? bold : regular;
        place(i, QFontMetrics(f).horizontalAdvance(m_segments.at(i)));
    }
}

int BreadcrumbBar::crumbAt(const QPoint &pos) const
{
    for (const Crumb &c : m_crumbs) {
        if (c.rect.contains(pos))
            return c.index;
    }
    return -2;
}

void BreadcrumbBar::paintEvent(QPaintEvent *)
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    const bool wc = tc.isWatercolor();
    QPainter p(this);
    if (wc) {
        // 들어간 칸 — 위 · 왼 --x-fo, 아래 · 오른 --x-fb, 안쪽 위 · 왼 --x-fi, 바탕 --field
        const fm::style::WatercolorChrome &x = fm::style::watercolorChrome(tc.variant());
        const QRect r = rect();
        p.fillRect(r, tc[Token::Field]);
        p.fillRect(QRect(r.left(), r.top(), r.width(), 1), x.fieldOuter);
        p.fillRect(QRect(r.left(), r.top(), 1, r.height()), x.fieldOuter);
        p.fillRect(QRect(r.left(), r.bottom(), r.width(), 1), x.fieldBright);
        p.fillRect(QRect(r.right(), r.top(), 1, r.height()), x.fieldBright);
        p.fillRect(QRect(r.left() + 1, r.top() + 1, r.width() - 2, 1), x.fieldInner);
        p.fillRect(QRect(r.left() + 1, r.top() + 1, 1, r.height() - 2), x.fieldInner);
        p.setClipRect(r.adjusted(2, 2, -2, -2));
    }
    const QFont regular = fm::style::pixelFont(font(), 13);
    QFont bold = regular;
    if (!wc)
        bold.setWeight(QFont::DemiBold);
    for (const Crumb &c : std::as_const(m_crumbs)) {
        const bool last = c.index == m_segments.size() - 1;
        const bool hover = c.index == m_hover;
        fm::style::paintGlyph(&p, fm::style::Glyph::ChevronRight, QRectF(c.chevron), tc[Token::Fg3]);
        QColor text = last ? tc[Token::Fg] : tc[Token::Fg2];
        if (wc)
            text = tc[Token::Fg];
        if (hover) {
            p.setRenderHint(QPainter::Antialiasing, !wc);
            p.setPen(Qt::NoPen);
            if (wc) {
                p.setBrush(tc[Token::Accent]);
                text = QColor(0xFF, 0xFF, 0xFF);
            } else {
                QColor fill = tc[Token::Fg];
                fill.setAlphaF(c.index == m_pressed ? 0.10 : 0.06);
                p.setBrush(fill);
            }
            p.drawRoundedRect(QRectF(c.rect), wc ? 0 : 4, wc ? 0 : 4);
        }
        p.setFont(last ? bold : regular);
        p.setPen(text);
        p.drawText(c.rect, Qt::AlignCenter, c.index < 0 ? u"…"_s : m_segments.at(c.index));
    }
}

void BreadcrumbBar::mouseMoveEvent(QMouseEvent *event)
{
    const int hover = crumbAt(event->position().toPoint());
    if (hover != m_hover) {
        m_hover = hover;
        setCursor(hover > -2 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void BreadcrumbBar::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hover = m_pressed = -2;
    update();
}

void BreadcrumbBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = crumbAt(event->position().toPoint());
    update();
    if (m_pressed == -1) {
        // "…": 숨겨진 마지막 조각으로
        const int firstShown = m_crumbs.size() > 1 ? m_crumbs.at(1).index : 0;
        Q_EMIT segmentClicked(std::max(0, firstShown - 1));
    } else if (m_pressed >= 0) {
        Q_EMIT segmentClicked(m_pressed);
    }
}

void BreadcrumbBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (crumbAt(event->position().toPoint()) == -2)
        startEditing();
}

void BreadcrumbBar::startEditing()
{
    if (!m_editor) {
        m_editor = new QLineEdit(this);
        m_editor->setFont(fm::style::monoFont(12));
        auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), m_editor, [this] { m_editor->hide(); });
        escape->setContext(Qt::WidgetShortcut);
        connect(m_editor, &QLineEdit::returnPressed, this, [this] {
            const QString path = m_editor->text();
            m_editor->hide();
            Q_EMIT pathEntered(path);
        });
        connect(m_editor, &QLineEdit::editingFinished, m_editor, &QWidget::hide);
    }
    m_editor->setGeometry(rect());
    m_editor->setText(m_fullPath);
    m_editor->selectAll();
    m_editor->show();
    m_editor->setFocus();
}

// ---------------------------------------------------------------- PanelStatusBar

PanelStatusBar::PanelStatusBar(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, qOverload<>(&QWidget::update));
}

void PanelStatusBar::setLeftText(const QString &text)
{
    if (m_left == text)
        return;
    m_left = text;
    update();
}

void PanelStatusBar::setRightText(const QString &text)
{
    if (m_right == text)
        return;
    m_right = text;
    update();
}

QSize PanelStatusBar::sizeHint() const
{
    return QSize(320, 26);  // 시안1 26, 시안2 23 + 위 여백 3
}

QSize PanelStatusBar::minimumSizeHint() const
{
    return QSize(80, 26);
}

void PanelStatusBar::paintEvent(QPaintEvent *)
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    QPainter p(this);
    p.fillRect(rect(), tc[Token::Win]);
    QRect text = rect().adjusted(12, 0, -12, 0);
    if (tc.isWatercolor()) {
        // 얕게 들어간 칸(위 · 왼 --x-lo, 아래 · 오른 --x-hi), 바깥 여백 위 3 · 좌우 3
        const fm::style::WatercolorChrome &x = fm::style::watercolorChrome(tc.variant());
        const QRect box = rect().adjusted(3, 3, -3, 0);
        p.fillRect(QRect(box.left(), box.top(), box.width(), 1), x.lo);
        p.fillRect(QRect(box.left(), box.top(), 1, box.height()), x.lo);
        p.fillRect(QRect(box.left(), box.bottom(), box.width(), 1), x.hi);
        p.fillRect(QRect(box.right(), box.top(), 1, box.height()), x.hi);
        text = box.adjusted(7, 1, -7, -1);
    } else {
        p.fillRect(QRect(0, 0, width(), 1), tc[Token::Line]);
        text.setTop(1);
    }
    const QFont font = fm::style::withTabularNumbers(fm::style::pixelFont(this->font(), 12));
    p.setFont(font);
    p.setPen(tc.isWatercolor() ? tc[Token::Fg] : tc[Token::Fg2]);
    const QFontMetrics fm(font);
    const int rightWidth = fm.horizontalAdvance(m_right);
    p.drawText(text, Qt::AlignRight | Qt::AlignVCenter, m_right);
    p.drawText(text.adjusted(0, 0, -(rightWidth + 16), 0), Qt::AlignLeft | Qt::AlignVCenter,
               fm.elidedText(m_left, Qt::ElideRight, std::max(0, text.width() - rightWidth - 16)));
}

} // namespace fm::ui
