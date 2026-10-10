#include "fmstyle/FmStyle.h"
#include "fmstyle/Glyphs.h"
#include "fmstyle/StylePaint.h"

#include "fmstyle/StyleProps.h"
#include "fmstyle/ThemeManager.h"

#include "ColorMath_p.h"
#include "StyleCommon_p.h"
#include "StyleShared_p.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSlider>
#include <QStyleOption>
#include <QTabBar>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Qt::StringLiterals;

namespace fm::style {

using T = Token;
using namespace detail;

namespace {

// ---------------------------------------------------------------------------------------------
// 크기 — 목업 CSS 값 (장치 독립 픽셀)

constexpr qreal kRadius = 4.0;           // 버튼 · 입력
constexpr qreal kCardRadius = 6.0;       // 카드 · 메뉴 · 탭
constexpr int kButtonHeight = 32;
constexpr int kButtonHeightSmall = 30;
constexpr int kButtonMinWidth = 88;
constexpr int kButtonPadX = 16;
constexpr int kButtonPadXSmall = 12;
constexpr int kSegmentPadX = 11;
constexpr int kInputHeight = 30;
constexpr int kInputPadX = 10;
constexpr int kComboArrowWidth = 28;
constexpr int kSpinButtonWidth = 20;
constexpr int kIndicator = 16;
constexpr int kSwitchWidth = 40;
constexpr int kSwitchHeight = 20;
constexpr int kSwitchMargin = 2;         // 포커스 링 자리
constexpr int kSwitchGap = 10;
constexpr int kTabHeight = 28;
constexpr int kTabPadX = 14;
constexpr int kHeaderHeight = 26;
constexpr int kItemMinHeight = 24;
constexpr int kMenuItemHeight = 28;
constexpr int kMenuBarItemHeight = 24;
constexpr int kScrollBarExtent = 12;
constexpr int kProgressThickness = 6;
constexpr int kProgressThin = 4;          // fmSize=thin (진행 창 '현재 파일')
constexpr int kProgressThick = 8;         // fmSize=thick (진행 창 '전체')
// 밀도(fmDensity) — 대화상자는 목업 파일 작업 대화상자, 설정은 설정 창
constexpr int kInputHeightDialog = 32;
constexpr int kButtonHeightSmallDialog = 28;
constexpr int kSegmentHeightDialog = 32;
constexpr int kSegmentPadXDialog = 6;
constexpr int kSegmentHeightSmall = 26;   // fmSize=small
constexpr int kSegmentPadXSmall = 12;
constexpr int kSegmentHeightMini = 20;    // fmSize=mini
constexpr int kSegmentPadXMini = 8;
constexpr int kSegmentHeightCompact = 28; // fmSize=compact — 메인 창 주소 줄(전체 28, 안쪽 26)
constexpr int kSegmentPadXCompact = 10;
constexpr int kHeaderHeightFlat = 28;     // 대화상자 · 설정 표 머리글
constexpr int kLinkHeight = 24;           // fmRole=link
constexpr int kKeyChipGap = 8;            // 글자와 키 칩 사이
// Qt 표준 위젯 — 목업이 없어 Windows 11 컨트롤을 토큰 색으로 옮겼다.
constexpr int kTabClose = 16;             // 탭 닫기 단추
constexpr int kSliderHandle = 20;         // 슬라이더 손잡이 지름
constexpr qreal kSliderTrack = 4.0;       // 슬라이더 홈 두께
constexpr int kTitleBarHeight = 30;       // MDI 창 제목 표시줄
constexpr int kCaptionButtonW = 36;       // 제목 표시줄 단추 폭 (높이는 제목 표시줄)
constexpr int kMdiButton = 22;            // 메뉴 막대의 MDI 단추 묶음 한 칸

int inputHeight(const QWidget *w) { return density(w) == Density::Dialog ? kInputHeightDialog : kInputHeight; }

int progressThickness(const QWidget *w)
{
    switch (sizeVariant(w)) {
    case SizeVariant::Thin: return kProgressThin;
    case SizeVariant::Thick: return kProgressThick;
    case SizeVariant::Normal:
    case SizeVariant::Small:
    case SizeVariant::Mini:
    case SizeVariant::Compact: break;
    }
    return kProgressThickness;
}

int segmentHeight(const QWidget *w)
{
    switch (sizeVariant(w)) {
    case SizeVariant::Mini: return kSegmentHeightMini;
    case SizeVariant::Small: return kSegmentHeightSmall;
    case SizeVariant::Compact: return kSegmentHeightCompact;
    case SizeVariant::Normal:
    case SizeVariant::Thin:
    case SizeVariant::Thick: break;
    }
    return density(w) == Density::Dialog ? kSegmentHeightDialog : kButtonHeightSmall;
}

int segmentPadX(const QWidget *w)
{
    switch (sizeVariant(w)) {
    case SizeVariant::Mini: return kSegmentPadXMini;
    case SizeVariant::Small: return kSegmentPadXSmall;
    case SizeVariant::Compact: return kSegmentPadXCompact;
    case SizeVariant::Normal:
    case SizeVariant::Thin:
    case SizeVariant::Thick: break;
    }
    return density(w) == Density::Dialog ? kSegmentPadXDialog : kSegmentPadX;
}

// ---------------------------------------------------------------------------------------------
// 그리기 도우미

void fillRounded(QPainter *p, const QRectF &r, qreal radius, const QBrush &brush)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    p->setBrush(brush);
    p->drawRoundedRect(r, radius, radius);
    p->restore();
}

void strokeRounded(QPainter *p, const QRectF &r, qreal radius, const QColor &color,
                   qreal width = 1.0)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(QPen(color, width));
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(r, radius, radius);
    p->restore();
}

QColor overlay(const QColor &ink, qreal alpha) { return withAlpha(ink, alpha); }

void drawChevron(QPainter *p, const QPointF &c, qreal s, Qt::ArrowType dir, const QColor &color,
                 qreal width = 1.3)
{
    QPolygonF poly;
    switch (dir) {
    case Qt::DownArrow:
        poly << QPointF(c.x() - s, c.y() - s / 2) << QPointF(c.x(), c.y() + s / 2)
             << QPointF(c.x() + s, c.y() - s / 2);
        break;
    case Qt::UpArrow:
        poly << QPointF(c.x() - s, c.y() + s / 2) << QPointF(c.x(), c.y() - s / 2)
             << QPointF(c.x() + s, c.y() + s / 2);
        break;
    case Qt::RightArrow:
        poly << QPointF(c.x() - s / 2, c.y() - s) << QPointF(c.x() + s / 2, c.y())
             << QPointF(c.x() - s / 2, c.y() + s);
        break;
    case Qt::LeftArrow:
        poly << QPointF(c.x() + s / 2, c.y() - s) << QPointF(c.x() - s / 2, c.y())
             << QPointF(c.x() + s / 2, c.y() + s);
        break;
    default:
        return;
    }
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawPolyline(poly);
    p->restore();
}

void drawCheckMark(QPainter *p, const QRectF &box, const QColor &color, qreal width = 1.6)
{
    QPainterPath path;
    path.moveTo(box.left() + box.width() * 0.24, box.top() + box.height() * 0.52);
    path.lineTo(box.left() + box.width() * 0.43, box.top() + box.height() * 0.70);
    path.lineTo(box.left() + box.width() * 0.77, box.top() + box.height() * 0.32);
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);
    p->restore();
}

// 왼쪽 · 오른쪽 모서리 반지름을 따로 주는 사각형 (세그먼트용).
QPainterPath sidesPath(const QRectF &r, qreal rl, qreal rr)
{
    QPainterPath path;
    path.moveTo(r.left() + rl, r.top());
    path.lineTo(r.right() - rr, r.top());
    if (rr > 0)
        path.arcTo(QRectF(r.right() - 2 * rr, r.top(), 2 * rr, 2 * rr), 90, -90);
    path.lineTo(r.right(), r.bottom() - rr);
    if (rr > 0)
        path.arcTo(QRectF(r.right() - 2 * rr, r.bottom() - 2 * rr, 2 * rr, 2 * rr), 0, -90);
    path.lineTo(r.left() + rl, r.bottom());
    if (rl > 0)
        path.arcTo(QRectF(r.left(), r.bottom() - 2 * rl, 2 * rl, 2 * rl), 270, -90);
    path.lineTo(r.left(), r.top() + rl);
    if (rl > 0)
        path.arcTo(QRectF(r.left(), r.top(), 2 * rl, 2 * rl), 180, -90);
    path.closeSubpath();
    return path;
}

// 위쪽만 둥근 사각형 (탭용).
QPainterPath topRoundedPath(const QRectF &r, qreal radius)
{
    QPainterPath path;
    path.moveTo(r.left(), r.bottom());
    path.lineTo(r.left(), r.top() + radius);
    path.arcTo(QRectF(r.left(), r.top(), 2 * radius, 2 * radius), 180, -90);
    path.lineTo(r.right() - radius, r.top());
    path.arcTo(QRectF(r.right() - 2 * radius, r.top(), 2 * radius, 2 * radius), 90, -90);
    path.lineTo(r.right(), r.bottom());
    path.closeSubpath();
    return path;
}

QColor buttonTextColor(QStyle::State s, ButtonRole role, bool segment, const ThemeColors &tc)
{
    if (!(s & QStyle::State_Enabled))
        return tc[T::Fg3];
    if (segment)
        return (s & QStyle::State_On) ? tc[T::AccentFg] : tc[T::Fg2];
    switch (role) {
    case ButtonRole::Primary: return tc[T::OnAccent];
    case ButtonRole::Danger: return tc[T::OnDanger];
    default: return (s & QStyle::State_On) ? tc[T::AccentFg] : tc[T::Fg];
    }
}

// 입력 상자 바탕: 필드 색 + 버튼 테두리, 아래 선은 메타 글자색(포커스면 강조색 2 px) — Windows 11 방식.
// 오류(fmInvalid)면 테두리와 아래 선이 위험색.
void drawInputFrame(QPainter *p, const QRect &rect, QStyle::State s, const ThemeColors &tc, bool invalid = false)
{
    const bool enabled = s & QStyle::State_Enabled;
    const bool focus = s & QStyle::State_HasFocus;
    const bool hover = s & QStyle::State_MouseOver;
    const QRectF r = crisp(rect);

    QColor bg = enabled ? tc[T::Field] : tc[T::Win];
    if (enabled && hover && !focus)
        bg = mix(bg, tc[T::Fg], 0.025);
    fillRounded(p, r, kRadius, bg);
    const QColor border = !enabled ? mix(tc[T::BtnLine], tc[T::Win], 0.5) : invalid ? tc[T::Danger] : tc[T::BtnLine];
    strokeRounded(p, r, kRadius, border);

    if (!enabled)
        return;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect), kRadius, kRadius);
    p->setClipPath(clip);
    if (focus || invalid)
        p->fillRect(QRectF(rect.left(), rect.bottom() - 1.0, rect.width(), 2.0), invalid ? tc[T::Danger] : tc[T::Accent]);
    else
        p->fillRect(QRectF(rect.left(), rect.bottom(), rect.width(), 1.0), tc[T::Fg3]);
    p->restore();
}

void drawCheckIndicator(QPainter *p, const QRect &rect, QStyle::State s, const ThemeColors &tc)
{
    const bool enabled = s & QStyle::State_Enabled;
    const bool hover = s & QStyle::State_MouseOver;
    const bool pressed = s & QStyle::State_Sunken;
    const bool on = s & QStyle::State_On;
    const bool partial = s & QStyle::State_NoChange;

    QRectF box(0, 0, kIndicator - 1, kIndicator - 1);
    box.moveCenter(QRectF(rect).center());

    if (on || partial) {
        QColor fill = enabled ? tc[T::Accent] : tc[T::Fg3];
        if (enabled && pressed)
            fill = mix(fill, tc[T::Fg], 0.15);
        else if (enabled && hover)
            fill = mix(fill, tc[T::Fg], 0.08);
        fillRounded(p, box, 3.0, fill);
        const QColor mark = enabled ? tc[T::OnAccent] : tc[T::Win];
        if (partial) {
            p->fillRect(QRectF(box.left() + 3.5, box.center().y() - 0.75, box.width() - 7, 1.5), mark);
        } else {
            drawCheckMark(p, box, mark);
        }
        return;
    }
    QColor fill = enabled ? tc[T::Field] : tc[T::Win];
    if (enabled && pressed)
        fill = mix(fill, tc[T::Fg], 0.10);
    else if (enabled && hover)
        fill = mix(fill, tc[T::Fg], 0.05);
    fillRounded(p, box, 3.0, fill);
    strokeRounded(p, box, 3.0, !enabled ? tc[T::BtnLine] : hover ? tc[T::Fg2] : tc[T::Fg3]);
}

void drawRadioIndicator(QPainter *p, const QRect &rect, QStyle::State s, const ThemeColors &tc)
{
    const bool enabled = s & QStyle::State_Enabled;
    const bool hover = s & QStyle::State_MouseOver;
    const bool pressed = s & QStyle::State_Sunken;
    const bool on = s & QStyle::State_On;

    QRectF box(0, 0, kIndicator - 1, kIndicator - 1);
    box.moveCenter(QRectF(rect).center());
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    if (on) {
        QColor fill = enabled ? tc[T::Accent] : tc[T::Fg3];
        if (enabled && pressed)
            fill = mix(fill, tc[T::Fg], 0.15);
        else if (enabled && hover)
            fill = mix(fill, tc[T::Fg], 0.08);
        p->setPen(Qt::NoPen);
        p->setBrush(fill);
        p->drawEllipse(box);
        const qreal dot = pressed ? 2.5 : hover ? 3.5 : 3.0;
        p->setBrush(enabled ? tc[T::OnAccent] : tc[T::Win]);
        p->drawEllipse(box.center(), dot, dot);
    } else {
        QColor fill = enabled ? tc[T::Field] : tc[T::Win];
        if (enabled && pressed)
            fill = mix(fill, tc[T::Fg], 0.10);
        else if (enabled && hover)
            fill = mix(fill, tc[T::Fg], 0.05);
        p->setPen(QPen(!enabled ? tc[T::BtnLine] : hover ? tc[T::Fg2] : tc[T::Fg3], 1.0));
        p->setBrush(fill);
        p->drawEllipse(box);
    }
    p->restore();
}

// 스위치 — 목업 .swt: 40 × 20, 손잡이 12 px, 가장자리에서 3 px.
void drawSwitch(QPainter *p, const QRect &rect, QStyle::State s, const ThemeColors &tc)
{
    const bool enabled = s & QStyle::State_Enabled;
    const bool hover = s & QStyle::State_MouseOver;
    const bool pressed = s & QStyle::State_Sunken;
    const bool on = s & QStyle::State_On;

    QRectF track(0, 0, kSwitchWidth - 1, kSwitchHeight - 1);
    track.moveCenter(QRectF(rect).center());
    const qreal radius = track.height() / 2.0;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    if (!enabled)
        p->setOpacity(0.45);
    if (on) {
        QColor fill = tc[T::Accent];
        if (pressed)
            fill = mix(fill, tc[T::Fg], 0.15);
        else if (hover)
            fill = mix(fill, tc[T::Fg], 0.08);
        p->setPen(Qt::NoPen);
        p->setBrush(fill);
        p->drawRoundedRect(track, radius, radius);
        p->setBrush(tc[T::OnAccent]);
        const qreal knob = hover && !pressed ? 7.0 : 6.0;
        p->drawEllipse(QPointF(track.right() - 9.5, track.center().y()), knob, knob);
    } else {
        QColor fill = tc[T::Field];
        if (pressed)
            fill = mix(fill, tc[T::Fg], 0.10);
        else if (hover)
            fill = mix(fill, tc[T::Fg], 0.05);
        p->setPen(QPen(hover ? tc[T::Fg2] : tc[T::Fg3], 1.0));
        p->setBrush(fill);
        p->drawRoundedRect(track, radius, radius);
        p->setPen(Qt::NoPen);
        p->setBrush(tc[T::Fg2]);
        const qreal knob = hover && !pressed ? 7.0 : 6.0;
        p->drawEllipse(QPointF(track.left() + 9.5, track.center().y()), knob, knob);
    }
    p->restore();
}

QColor progressColor(const QWidget *w, const ThemeColors &tc)
{
    const QString state = stringProp(w, props::kProgressState);
    if (state == u"paused")
        return tc[T::Paused];
    if (state == u"error")
        return tc[T::Danger];
    return tc[T::Accent];
}

} // namespace

// =============================================================================================

FmStyle::FmStyle()
    : QProxyStyle(u"Fusion"_s)
{
    setObjectName(u"FmStyle"_s);
}

FmStyle::~FmStyle() = default;

const ThemeColors &FmStyle::colorsFor(const QWidget *widget)
{
    // 범위에 걸린 색이 다른 디자인 것이면 같은 변형의 시안1 색을 쓴다.
    const ThemeColors *scoped = ThemeScope::find(widget);
    if (scoped && scoped->design() == Design::Standard)
        return *scoped;
    const ThemeManager &manager = ThemeManager::instance();
    return manager.colors(Design::Standard, scoped ? scoped->variant() : manager.effectiveVariant());
}

void FmStyle::setAlwaysShowMnemonics(bool on)
{
    if (m_alwaysMnemonics == on)
        return;
    m_alwaysMnemonics = on;
    const auto windows = QApplication::topLevelWidgets();
    for (QWidget *w : windows)
        w->update();
}

QPalette FmStyle::standardPalette() const
{
    return ThemeManager::instance().colors().toPalette();
}

void FmStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    if (qobject_cast<QAbstractButton *>(widget) || qobject_cast<QComboBox *>(widget)
        || qobject_cast<QAbstractSpinBox *>(widget) || qobject_cast<QLineEdit *>(widget)
        || qobject_cast<QTabBar *>(widget) || qobject_cast<QScrollBar *>(widget)
        || qobject_cast<QHeaderView *>(widget) || qobject_cast<QMenuBar *>(widget)) {
        widget->setAttribute(Qt::WA_Hover, true);
    }
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    // 메뉴의 둥근 모서리: 반투명 배경이 필요하다 (창을 만들기 전, 즉 polish 시점에 설정).
    // 합성 관리자가 없을 수 있는 X11/Wayland에서는 네모난 메뉴로 둔다.
    if (qobject_cast<QMenu *>(widget) && widget->isWindow())
        widget->setAttribute(Qt::WA_TranslucentBackground, true);
#endif

    // 목업: 열 머리글과 탭은 12 px. 앱에서 따로 글꼴을 준 위젯은 건드리지 않는다
    // (스타일이 준 글꼴은 fmStyledFont로 표시해 두어 디자인을 바꿀 때 다시 건다).
    if ((qobject_cast<QHeaderView *>(widget) || qobject_cast<QTabBar *>(widget))
        && (!widget->testAttribute(Qt::WA_SetFont) || boolProp(widget, props::kStyledFont))) {
        QFont f = widget->font();
        f.setPixelSize(12);
        widget->setFont(f);
        widget->setProperty(props::kStyledFont, true);
    }
    polishCalendarPart(widget);
}

void FmStyle::unpolish(QWidget *widget)
{
    if (boolProp(widget, props::kStyledFont)) {
        widget->setProperty(props::kStyledFont, QVariant());
        widget->setFont(QFont());
        widget->setAttribute(Qt::WA_SetFont, false);
    }
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    if (qobject_cast<QMenu *>(widget) && widget->isWindow())
        widget->setAttribute(Qt::WA_TranslucentBackground, false);
#endif
    unpolishCalendarPart(widget);
    QProxyStyle::unpolish(widget);
}

void FmStyle::polish(QApplication *app)
{
    QProxyStyle::polish(app);
    app->installEventFilter(this);
}

void FmStyle::unpolish(QApplication *app)
{
    app->removeEventFilter(this);
    QProxyStyle::unpolish(app);
}

void FmStyle::setAltDown(bool down, QObject *source)
{
    if (m_altDown == down)
        return;
    m_altDown = down;
    if (m_alwaysMnemonics)
        return;
    if (auto *w = qobject_cast<QWidget *>(source)) {
        if (QWidget *win = w->window())
            win->update();
    }
}

bool FmStyle::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress: {
        const auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Alt && !(ke->modifiers() & ~Qt::AltModifier))
            setAltDown(true, watched);
        break;
    }
    case QEvent::KeyRelease:
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Alt)
            setAltDown(false, watched);
        break;
    case QEvent::WindowDeactivate:
    case QEvent::FocusOut:
        setAltDown(false, watched);
        break;
    default:
        break;
    }
    return QProxyStyle::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------------------------
// 버튼

void FmStyle::drawButtonPanel(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    if (!segmentOf(w).isEmpty()) {
        drawSegment(option, p, w);
        return;
    }
    const ThemeColors &tc = colorsFor(w);
    const QStyle::State s = option->state;
    const bool enabled = s & State_Enabled;
    const bool hover = s & State_MouseOver;
    const bool pressed = s & State_Sunken;
    const bool on = s & State_On;
    const ButtonRole role = buttonRole(w);
    const QRectF r = crisp(option->rect);

    if (role == ButtonRole::Link) {
        // 링크 단추: 패널 없음. 키보드 포커스만 2 px 둥근 링.
        if (enabled && keyboardFocus(s))
            strokeRounded(p, QRectF(option->rect).adjusted(1, 1, -1, -1), kRadius - 1, tc[T::Focus], 2.0);
        return;
    }

    if (role == ButtonRole::Subtle) {
        if (enabled && (pressed || hover || on)) {
            const QColor bg = on ? tc[T::AccentSoft] : overlay(tc[T::Fg], pressed ? 0.12 : 0.07);
            fillRounded(p, r, kRadius, bg);
        }
    } else {
        QColor bg = tc[T::Btn];
        QColor border = tc[T::BtnLine];
        qreal hoverAmount = 0.04;
        qreal pressAmount = 0.09;
        if (enabled && role == ButtonRole::Primary) {
            bg = border = tc[T::Accent];
            hoverAmount = 0.10;
            pressAmount = 0.18;
        } else if (enabled && role == ButtonRole::Danger) {
            bg = border = tc[T::DangerFill];
            hoverAmount = 0.10;
            pressAmount = 0.18;
        } else if (enabled && on) {
            bg = tc[T::AccentSoft];
        }
        if (!enabled)
            border = mix(tc[T::BtnLine], tc[T::Win], 0.4);
        else if (pressed)
            bg = mix(bg, tc[T::Fg], pressAmount);
        else if (hover)
            bg = mix(bg, tc[T::Fg], hoverAmount);
        fillRounded(p, r, kRadius, bg);
        strokeRounded(p, r, kRadius, border);
    }
    if (enabled && keyboardFocus(s))
        strokeRounded(p, QRectF(option->rect).adjusted(1, 1, -1, -1), kRadius - 1, tc[T::Focus], 2.0);
}

void FmStyle::drawSegment(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const ThemeColors &tc = colorsFor(w);
    const QString pos = segmentOf(w);
    const bool first = pos == u"first" || pos == u"only";
    const bool last = pos == u"last" || pos == u"only";
    const QStyle::State s = option->state;
    const bool enabled = s & State_Enabled;
    const bool on = s & State_On;

    QColor bg = on ? tc[T::AccentSoft] : tc[T::Btn];
    if (enabled && (s & State_Sunken))
        bg = mix(bg, tc[T::Fg], 0.09);
    else if (enabled && (s & State_MouseOver))
        bg = mix(bg, tc[T::Fg], 0.04);

    // 첫 조각이 아니면 왼쪽 테두리를 위젯 바깥(-0.5)으로 밀어 앞 조각의 오른쪽 테두리와 겹친다.
    QRectF r(option->rect);
    r.adjust(first ? 0.5 : -0.5, 0.5, -0.5, -0.5);
    const QPainterPath path = sidesPath(r, first ? kRadius : 0.0, last ? kRadius : 0.0);

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->fillPath(path, bg);
    p->setPen(QPen(enabled ? tc[T::BtnLine] : mix(tc[T::BtnLine], tc[T::Win], 0.4), 1.0));
    p->drawPath(path);
    if (enabled && keyboardFocus(s)) {
        const QRectF fr = QRectF(option->rect).adjusted(first ? 2 : 1, 2, -2, -2);
        p->setPen(QPen(tc[T::Focus], 2.0));
        p->drawPath(sidesPath(fr, first ? kRadius - 1 : 0.0, last ? kRadius - 1 : 0.0));
    }
    p->restore();
}

void FmStyle::drawToolPanel(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    if (!segmentOf(w).isEmpty()) {
        drawSegment(option, p, w);
        return;
    }
    if (!(option->state & State_AutoRaise)) {
        drawButtonPanel(option, p, w);
        return;
    }
    const ThemeColors &tc = colorsFor(w);
    const QStyle::State s = option->state;
    if (!(s & State_Enabled))
        return;
    QColor bg;
    if (s & State_On)
        bg = tc[T::AccentSoft];
    else if (s & State_Sunken)
        bg = overlay(tc[T::Fg], 0.12);
    else if (s & State_MouseOver)
        bg = overlay(tc[T::Fg], 0.07);
    if (bg.isValid())
        fillRounded(p, crisp(option->rect), kRadius, bg);
    if (keyboardFocus(s))
        strokeRounded(p, QRectF(option->rect).adjusted(1, 1, -1, -1), kRadius - 1, tc[T::Focus], 2.0);
}

void FmStyle::drawFocus(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const ThemeColors &tc = colorsFor(w);
    // 목록의 커서 (TC의 커서 막대 테두리)
    if (qobject_cast<const QAbstractItemView *>(w)) {
        p->save();
        const QRectF r = crisp(option->rect);
        if (paneActive(option, w)) {
            p->setPen(QPen(tc[T::Focus], 1.0));
        } else {
            QPen pen(tc[T::Fg3], 1.0);
            pen.setStyle(Qt::DashLine);
            p->setPen(pen);
        }
        p->setBrush(Qt::NoBrush);
        p->drawRect(r);
        p->restore();
        return;
    }
    if (!keyboardFocus(option->state))
        return;
    // 버튼 종류는 패널에서 이미 그렸다
    if (qobject_cast<const QAbstractButton *>(w) && !qobject_cast<const QCheckBox *>(w)
        && !qobject_cast<const QRadioButton *>(w))
        return;
    const qreal radius = isSwitch(w) ? (kSwitchHeight + 2) / 2.0 : kRadius;
    strokeRounded(p, QRectF(option->rect).adjusted(0.75, 0.75, -0.75, -0.75), radius, tc[T::Focus],
                  1.5);
}

// ---------------------------------------------------------------------------------------------
// 탭 · 머리글

void FmStyle::drawTabShape(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
    if (!tab)
        return;
    const ThemeColors &tc = colorsFor(w);
    const bool selected = tab->state & State_Selected;
    const bool hover = tab->state & State_MouseOver;
    // 위쪽 탭 모양으로 그리고 아래 · 왼쪽 · 오른쪽 탭은 좌표를 뒤집거나 돌린다(바깥 쪽이 둥글다).
    const TabSide side = tabSide(tab->shape);
    const QRectF r = QRectF(QPointF(0, 0), QSizeF(tabLocalSize(side, tab->rect))).adjusted(0.5, 0.5, -0.5, 0);

    p->save();
    p->setTransform(tabTransform(side, tab->rect), true);
    p->setRenderHint(QPainter::Antialiasing, true);
    if (selected) {
        // 선택 탭은 목록 바탕색으로 아래 내용과 이어진다 (아래 구분선을 덮음).
        p->fillPath(topRoundedPath(r, kCardRadius), tc[T::Surface]);
        QPainterPath edge;
        edge.moveTo(r.left(), r.bottom() + 0.5);
        edge.lineTo(r.left(), r.top() + kCardRadius);
        edge.arcTo(QRectF(r.left(), r.top(), 2 * kCardRadius, 2 * kCardRadius), 180, -90);
        edge.lineTo(r.right() - kCardRadius, r.top());
        edge.arcTo(QRectF(r.right() - 2 * kCardRadius, r.top(), 2 * kCardRadius, 2 * kCardRadius), 90,
                   -90);
        edge.lineTo(r.right(), r.bottom() + 0.5);
        p->setPen(QPen(tc[T::Line], 1.0));
        p->setBrush(Qt::NoBrush);
        p->drawPath(edge);
        // 활성 패널: 선택 탭 안쪽 위에 강조색 2 px (목업 .pane.on .tab.is-on)
        if (paneActiveProperty(w)) {
            p->setClipPath(topRoundedPath(r.adjusted(0.5, 0.5, -0.5, 0), kCardRadius - 0.5));
            p->fillRect(QRectF(r.left(), r.top(), r.width(), 2.5), tc[T::Accent]);
        }
    } else if (hover && (tab->state & State_Enabled)) {
        p->fillPath(topRoundedPath(r.adjusted(0, 2, 0, -1), kCardRadius), overlay(tc[T::Fg], 0.05));
    }
    p->restore();
}

void FmStyle::drawHeaderSection(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(option);
    if (!h)
        return;
    const ThemeColors &tc = colorsFor(w);
    QColor bg = tc[T::Head];
    if (h->state & State_Sunken)
        bg = mix(bg, tc[T::Fg], 0.08);
    else if ((h->state & State_MouseOver) && (h->state & State_Enabled))
        bg = mix(bg, tc[T::Fg], 0.04);
    const QRect r = h->rect;
    p->fillRect(r, bg);
    if (h->orientation == Qt::Horizontal) {
        p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), tc[T::Line]);
        if (h->position != QStyleOptionHeader::End && h->position != QStyleOptionHeader::OnlyOneSection)
            p->fillRect(QRect(r.right(), r.top(), 1, r.height() - 1), tc[T::Grid]);
    } else {
        p->fillRect(QRect(r.right(), r.top(), 1, r.height()), tc[T::Line]);
        p->fillRect(QRect(r.left(), r.bottom(), r.width() - 1, 1), tc[T::Grid]);
    }
}

// ---------------------------------------------------------------------------------------------
// 메뉴

void FmStyle::drawMenuBarItem(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if (!mi)
        return;
    const ThemeColors &tc = colorsFor(w);
    const bool enabled = mi->state & State_Enabled;
    const bool active = (mi->state & State_Selected) && enabled;
    const bool down = (mi->state & State_Sunken) && enabled;
    const QRect r = mi->rect;
    if (active || down) {
        QRectF bg = QRectF(r).adjusted(0, (r.height() - kMenuBarItemHeight) / 2.0, 0,
                                       -(r.height() - kMenuBarItemHeight) / 2.0);
        fillRounded(p, bg, kRadius, overlay(tc[T::Fg], down ? 0.10 : 0.06));
    }
    int flags = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
    if (!proxy()->styleHint(SH_UnderlineShortcut, mi, w))
        flags |= Qt::TextHideMnemonic;
    p->save();
    p->setPen(enabled ? tc[T::Fg] : tc[T::Fg3]);
    p->drawText(r, flags, mi->text);
    p->restore();
}

void FmStyle::drawMenuItem(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if (!mi)
        return;
    const ThemeColors &tc = colorsFor(w);
    const QRect r = mi->rect;

    if (mi->menuItemType == QStyleOptionMenuItem::Separator) {
        p->fillRect(QRect(r.left() + 8, r.center().y(), r.width() - 16, 1), tc[T::Grid]);
        return;
    }
    if (mi->menuItemType == QStyleOptionMenuItem::EmptyArea)
        return;

    const bool enabled = mi->state & State_Enabled;
    const bool selected = (mi->state & State_Selected) && enabled;
    if (selected)
        fillRounded(p, QRectF(r).adjusted(0, 1, 0, -1), kRadius, overlay(tc[T::Fg], 0.07));

    // 체크 · 아이콘 열
    const QRect column(r.left() + 6, r.top(), 20, r.height());
    const bool checkable = mi->checkType != QStyleOptionMenuItem::NotCheckable;
    if (checkable && mi->checked) {
        if (mi->checkType == QStyleOptionMenuItem::Exclusive) {
            p->save();
            p->setRenderHint(QPainter::Antialiasing, true);
            p->setPen(Qt::NoPen);
            p->setBrush(enabled ? tc[T::Fg] : tc[T::Fg3]);
            p->drawEllipse(QRectF(column).center(), 3.0, 3.0);
            p->restore();
        } else {
            QRectF box(0, 0, 14, 14);
            box.moveCenter(QRectF(column).center());
            drawCheckMark(p, box, enabled ? tc[T::Fg] : tc[T::Fg3], 1.5);
        }
    } else if (!mi->icon.isNull()) {
        const QIcon::Mode mode = enabled ? QIcon::Normal : QIcon::Disabled;
        mi->icon.paint(p, QRect(column.center().x() - 8, column.center().y() - 8, 16, 16),
                       Qt::AlignCenter, mode);
    }

    // 글자 · 단축키
    QString text = mi->text;
    QString shortcut;
    if (const qsizetype tab = text.indexOf(u'\t'); tab >= 0) {
        shortcut = text.mid(tab + 1);
        text = text.left(tab);
    }
    const bool subMenu = mi->menuItemType == QStyleOptionMenuItem::SubMenu;
    const int left = column.right() + 8;
    const QRect textRect(left, r.top(), r.right() - 12 - left - (subMenu ? 12 : 0), r.height());

    int flags = Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine | Qt::TextShowMnemonic;
    if (!proxy()->styleHint(SH_UnderlineShortcut, mi, w))
        flags |= Qt::TextHideMnemonic;

    p->save();
    p->setFont(mi->font);
    p->setPen(enabled ? tc[T::Fg] : tc[T::Fg3]);
    p->drawText(textRect, flags, text);
    if (!shortcut.isEmpty()) {
        p->setPen(enabled ? tc[T::Fg3] : mix(tc[T::Fg3], tc[T::Surface], 0.4));
        p->drawText(textRect, Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine, shortcut);
    }
    p->restore();

    if (subMenu)
        drawChevron(p, QPointF(r.right() - 12, QRectF(r).center().y()), 3.5, Qt::RightArrow,
                    enabled ? tc[T::Fg2] : tc[T::Fg3]);
}

// ---------------------------------------------------------------------------------------------
// 진행 막대 · 틀

void FmStyle::drawProgress(ControlElement element, const QStyleOption *option, QPainter *p,
                           const QWidget *w) const
{
    const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(option);
    if (!pb)
        return;
    const ThemeColors &tc = colorsFor(w);
    const bool horizontal = pb->state & State_Horizontal;
    const QRectF g(pb->rect);
    const qreal radius = (horizontal ? g.height() : g.width()) / 2.0;

    if (element == CE_ProgressBarGroove) {
        QColor groove = tc[T::Grid];
        if (w) {
            // 테마 팔레트가 아닌 바탕(예: Qt Designer의 폼) 위에서도 홈이 보이도록
            const QColor bg = w->palette().color(w->backgroundRole());
            const double wanted = std::min(detail::contrast(groove, tc[T::Win]), 1.15);
            if (detail::contrast(groove, bg) < wanted)
                groove = detail::pushUntil(groove, tc[T::Fg3], bg, wanted);
        }
        fillRounded(p, g, radius, groove);
        return;
    }
    if (element == CE_ProgressBarLabel) {
        if (!pb->textVisible || pb->text.isEmpty())
            return;
        p->save();
        p->setPen((pb->state & State_Enabled) ? tc[T::Fg2] : tc[T::Fg3]);
        p->drawText(pb->rect, Qt::AlignVCenter | Qt::AlignRight, pb->text);
        p->restore();
        return;
    }

    // CE_ProgressBarContents
    const QColor color = (pb->state & State_Enabled) ? progressColor(w, tc) : tc[T::Fg3];
    if (pb->minimum == pb->maximum) {
        // 진행률을 알 수 없음 — 30 % 구간이 왼쪽에서 오른쪽으로 흐른다. 위상(0 ~ 1)은 fm::ui::ProgressBar가
        // 동적 속성 fmBusyPhase로 넘긴다. 속성이 없으면(일반 QProgressBar) 가운데에 멈춰 있다.
        const QVariant phase = w ? w->property(props::kBusyPhase) : QVariant();
        const qreal start = phase.isValid() ? -0.30 + 1.30 * phase.toReal() : 0.35;
        QRectF seg = horizontal ? QRectF(g.left() + g.width() * start, g.top(), g.width() * 0.30, g.height())
                                : QRectF(g.left(), g.top() + g.height() * start, g.width(), g.height() * 0.30);
        seg = seg.intersected(g);
        if (!seg.isEmpty()) {
            p->save();
            QPainterPath clip;
            clip.addRoundedRect(g, radius, radius);
            p->setClipPath(clip);
            fillRounded(p, seg, radius, color);
            p->restore();
        }
        return;
    }
    const double span = double(pb->maximum) - double(pb->minimum);
    const double fraction = std::clamp((double(pb->progress) - double(pb->minimum)) / span, 0.0, 1.0);
    if (fraction <= 0.0)
        return;
    QRectF chunk = g;
    if (horizontal) {
        const qreal length = std::max(g.height(), g.width() * fraction);
        const bool reversed = pb->invertedAppearance != (pb->direction == Qt::RightToLeft);
        if (reversed)
            chunk.setLeft(g.right() - length);
        else
            chunk.setWidth(length);
    } else {
        const qreal length = std::max(g.width(), g.height() * fraction);
        const bool fromTop = pb->invertedAppearance != !pb->bottomToTop;
        if (fromTop)
            chunk.setHeight(length);
        else
            chunk.setTop(g.bottom() - length);
    }
    fillRounded(p, chunk, radius, color);
}

void FmStyle::drawShapedFrame(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(option);
    if (!f)
        return;
    const ThemeColors &tc = colorsFor(w);
    if (isFooter(w)) {
        // 대화상자 버튼 영역 — --foot 바탕 + 위 1 px --line
        p->fillRect(f->rect, tc[T::Foot]);
        p->fillRect(QRect(f->rect.left(), f->rect.top(), f->rect.width(), 1), tc[T::Line]);
        return;
    }
    if (boolProp(w, props::kCard)) {
        const QRectF r = crisp(f->rect);
        fillRounded(p, r, kCardRadius, tc[T::Surface]);
        strokeRounded(p, r, kCardRadius, tc[T::Line]);
        return;
    }
    switch (f->frameShape) {
    case QFrame::NoFrame:
        return;
    case QFrame::HLine:
        p->fillRect(QRect(f->rect.left(), f->rect.center().y(), f->rect.width(), 1), tc[T::Line]);
        return;
    case QFrame::VLine:
        p->fillRect(QRect(f->rect.center().x(), f->rect.top(), 1, f->rect.height()), tc[T::Line]);
        return;
    default:
        if (f->lineWidth > 0) {
            p->save();
            p->setPen(tc[T::Line]);
            p->setBrush(Qt::NoBrush);
            p->drawRect(f->rect.adjusted(0, 0, -1, -1));
            p->restore();
        }
        return;
    }
}

// ---------------------------------------------------------------------------------------------
// Qt 표준 위젯 — 목업이 없어 Windows 11 컨트롤을 토큰 색으로 옮겼다.

// 도구 상자: 펼침 머리(Expander) — --head 바탕 · 아래 --line, 왼쪽 꺾쇠(열림 = 아래), 열린 칸 글자는 굵게.
void FmStyle::drawToolBoxTab(ControlElement element, const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *tb = qstyleoption_cast<const QStyleOptionToolBox *>(option);
    if (!tb)
        return;
    const ThemeColors &tc = colorsFor(w);
    const QStyle::State s = tb->state;
    const bool enabled = s & State_Enabled;
    const bool selected = s & State_Selected;
    const QRect r = tb->rect;

    if (element == CE_ToolBoxTabShape) {
        QColor bg = tc[T::Head];
        if (enabled && (s & State_Sunken))
            bg = mix(bg, tc[T::Fg], 0.08);
        else if (enabled && (s & State_MouseOver))
            bg = mix(bg, tc[T::Fg], 0.04);
        p->fillRect(r, bg);
        p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), tc[T::Line]);
        if (enabled && keyboardFocus(s))
            strokeRounded(p, QRectF(r).adjusted(1, 1, -1, -1), kRadius - 1, tc[T::Focus], 2.0);
        return;
    }

    const QRect chevron = visualRect(tb->direction, r, QRect(r.left() + 6, r.top(), 16, r.height()));
    const Qt::ArrowType closed = tb->direction == Qt::RightToLeft ? Qt::LeftArrow : Qt::RightArrow;
    drawChevron(p, QRectF(chevron).center(), 3.5, selected ? Qt::DownArrow : closed,
                enabled ? tc[T::Fg2] : tc[T::Fg3]);
    int left = r.left() + 28;
    if (!tb->icon.isNull()) {
        const int icon = proxy()->pixelMetric(PM_SmallIconSize, tb, w);
        const QRect ir = visualRect(tb->direction, r, QRect(left, r.top() + (r.height() - icon) / 2, icon, icon));
        tb->icon.paint(p, ir, Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled);
        left += icon + 6;
    }
    const QRect textRect = visualRect(tb->direction, r, QRect(left, r.top(), r.right() - 8 - left, r.height()));
    int flags = Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextShowMnemonic
              | (tb->direction == Qt::RightToLeft ? Qt::AlignRight : Qt::AlignLeft);
    if (!proxy()->styleHint(SH_UnderlineShortcut, tb, w))
        flags |= Qt::TextHideMnemonic;
    p->save();
    QFont f = p->font();
    if (selected)
        f.setWeight(QFont::DemiBold);
    p->setFont(f);
    p->setPen(enabled ? tc[T::Fg] : tc[T::Fg3]);
    p->drawText(textRect, flags, QFontMetrics(f).elidedText(tb->text, Qt::ElideRight, textRect.width()));
    p->restore();
}

// 슬라이더: 4 px 둥근 홈(채운 쪽 강조색), 지름 20 원 손잡이 안에 강조색 점(마우스 올림 크게, 누름 작게).
void FmStyle::drawSlider(const QStyleOptionComplex *option, QPainter *p, const QWidget *w) const
{
    const auto *sl = qstyleoption_cast<const QStyleOptionSlider *>(option);
    if (!sl)
        return;
    const ThemeColors &tc = colorsFor(w);
    const bool enabled = sl->state & State_Enabled;
    const bool horizontal = sl->orientation == Qt::Horizontal;
    const QRect handle = proxy()->subControlRect(CC_Slider, sl, SC_SliderHandle, w);
    const int handleLength = horizontal ? handle.width() : handle.height();
    const QPointF hc = QRectF(handle).center();
    const QRectF r(sl->rect);

    if ((sl->subControls & SC_SliderTickmarks) && sl->tickPosition != QSlider::NoTicks) {
        const QColor tick = enabled ? tc[T::Fg3] : mix(tc[T::Fg3], tc[T::Win], 0.5);
        const bool mirrored = horizontal && sl->direction == Qt::RightToLeft;
        for (const int pos : sliderTickPositions(sl, handleLength)) {
            if (horizontal) {
                const int x = mirrored ? sl->rect.right() - pos : sl->rect.left() + pos;
                if (sl->tickPosition & QSlider::TicksAbove)
                    p->fillRect(QRect(x, handle.top() - 4, 1, 3), tick);
                if (sl->tickPosition & QSlider::TicksBelow)
                    p->fillRect(QRect(x, handle.bottom() + 2, 1, 3), tick);
            } else {
                const int y = sl->rect.top() + pos;
                if (sl->tickPosition & QSlider::TicksLeft)
                    p->fillRect(QRect(handle.left() - 4, y, 3, 1), tick);
                if (sl->tickPosition & QSlider::TicksRight)
                    p->fillRect(QRect(handle.right() + 2, y, 3, 1), tick);
            }
        }
    }

    // 홈은 손잡이 가운데가 움직이는 구간. 채운 쪽은 최솟값 쪽(QCommonStyle과 같이 upsideDown · 방향으로 정한다).
    const qreal half = handleLength / 2.0;
    QRectF groove;
    QRectF filled;
    if (horizontal) {
        groove = QRectF(r.left() + half, hc.y() - kSliderTrack / 2, r.width() - 2 * half, kSliderTrack);
        const bool minAtLeft = sl->upsideDown == (sl->direction == Qt::RightToLeft);
        filled = minAtLeft ? QRectF(groove.left(), groove.top(), hc.x() - groove.left(), kSliderTrack)
                           : QRectF(hc.x(), groove.top(), groove.right() - hc.x(), kSliderTrack);
    } else {
        groove = QRectF(hc.x() - kSliderTrack / 2, r.top() + half, kSliderTrack, r.height() - 2 * half);
        filled = sl->upsideDown ? QRectF(groove.left(), hc.y(), kSliderTrack, groove.bottom() - hc.y())
                                : QRectF(groove.left(), groove.top(), kSliderTrack, hc.y() - groove.top());
    }
    const QColor track = mix(tc[T::Fg3], tc[T::Win], 0.45);
    fillRounded(p, groove, kSliderTrack / 2, enabled ? track : mix(track, tc[T::Win], 0.5));
    if (filled.width() > 0 && filled.height() > 0)
        fillRounded(p, filled, kSliderTrack / 2, enabled ? tc[T::Accent] : tc[T::Fg3]);

    const bool active = sl->activeSubControls & SC_SliderHandle;
    const bool pressed = enabled && active && (sl->state & State_Sunken);
    const bool hover = enabled && active && (sl->state & State_MouseOver);
    const bool focus = enabled && keyboardFocus(sl->state);
    QRectF knob(0, 0, kSliderHandle - 1, kSliderHandle - 1);
    knob.moveCenter(hc);
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(QPen(focus ? tc[T::Focus] : enabled ? tc[T::BtnLine] : mix(tc[T::BtnLine], tc[T::Win], 0.4),
                   focus ? 2.0 : 1.0));
    p->setBrush(enabled ? tc[T::Field] : tc[T::Win]);
    p->drawEllipse(focus ? knob.adjusted(0.5, 0.5, -0.5, -0.5) : knob);
    p->setPen(Qt::NoPen);
    p->setBrush(enabled ? tc[T::Accent] : tc[T::Fg3]);
    const qreal dot = pressed ? 4.0 : hover ? 6.0 : 5.0;
    p->drawEllipse(hc, dot, dot);
    p->restore();
}

// 다이얼: 원형 슬라이더 — 300° 홈(채운 쪽 강조색) 위에 슬라이더와 같은 손잡이, 바깥에 눈금.
void FmStyle::drawDial(const QStyleOptionComplex *option, QPainter *p, const QWidget *w) const
{
    const auto *d = qstyleoption_cast<const QStyleOptionSlider *>(option);
    if (!d)
        return;
    const ThemeColors &tc = colorsFor(w);
    const bool enabled = d->state & State_Enabled;
    const bool hover = enabled && (d->state & State_MouseOver);
    const bool focus = enabled && keyboardFocus(d->state);
    const QRectF r(d->rect);
    const QPointF c = r.center();
    const bool notches = d->subControls & SC_DialTickmarks;
    const qreal outer = std::min(r.width(), r.height()) / 2.0 - 1.0;
    const qreal knobRadius = 7.0;
    const qreal ring = outer - (notches ? 12.0 : 8.0);
    if (ring < 4.0)
        return;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    if (notches) {
        p->setPen(QPen(enabled ? tc[T::Fg3] : mix(tc[T::Fg3], tc[T::Win], 0.5), 1.0));
        for (const DialNotch &n : dialNotches(d)) {
            const QPointF dir(std::cos(n.angle), -std::sin(n.angle));
            p->drawLine(c + dir * (outer - (n.major ? 4.0 : 2.0)), c + dir * outer);
        }
    }
    const QRectF arc(c.x() - ring, c.y() - ring, 2 * ring, 2 * ring);
    const QColor track = mix(tc[T::Fg3], tc[T::Win], 0.45);
    QPen pen(enabled ? track : mix(track, tc[T::Win], 0.5), kSliderTrack, Qt::SolidLine, Qt::RoundCap);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    const qreal angle = dialAngle(d);
    if (d->dialWrapping) {
        p->drawEllipse(arc);
    } else {
        p->drawArc(arc, 240 * 16, -300 * 16);
        // 최솟값(240°)에서 지금 값까지
        const int span = qRound((angle * 180.0 / std::numbers::pi - 240.0) * 16.0);
        if (span != 0) {
            pen.setColor(enabled ? tc[T::Accent] : tc[T::Fg3]);
            p->setPen(pen);
            p->drawArc(arc, 240 * 16, span);
        }
    }
    const QPointF k = c + QPointF(ring * std::cos(angle), -ring * std::sin(angle));
    p->setPen(QPen(focus ? tc[T::Focus] : enabled ? tc[T::BtnLine] : mix(tc[T::BtnLine], tc[T::Win], 0.4),
                   focus ? 2.0 : 1.0));
    p->setBrush(enabled ? tc[T::Field] : tc[T::Win]);
    p->drawEllipse(k, knobRadius - 0.5, knobRadius - 0.5);
    p->setPen(Qt::NoPen);
    p->setBrush(enabled ? tc[T::Accent] : tc[T::Fg3]);
    p->drawEllipse(k, hover ? 3.5 : 3.0, hover ? 3.5 : 3.0);
    p->restore();
}

// MDI 창 제목 표시줄: 창 바탕 · 12 px 제목(비활성 창은 흐리게), 오른쪽에 붙인 36 px 단추(닫기는 위험색).
void FmStyle::drawTitleBar(const QStyleOptionComplex *option, QPainter *p, const QWidget *w) const
{
    const auto *tb = qstyleoption_cast<const QStyleOptionTitleBar *>(option);
    if (!tb)
        return;
    const ThemeColors &tc = colorsFor(w);
    const bool active = tb->state & State_Active;
    p->fillRect(tb->rect, tc[T::Win]);

    if ((tb->subControls & SC_TitleBarSysMenu) && !tb->icon.isNull()) {
        const QRect ir = proxy()->subControlRect(CC_TitleBar, tb, SC_TitleBarSysMenu, w);
        tb->icon.paint(p, ir, Qt::AlignCenter, active ? QIcon::Normal : QIcon::Disabled);
    }
    if ((tb->subControls & SC_TitleBarLabel) && !tb->text.isEmpty()) {
        const QRect label = proxy()->subControlRect(CC_TitleBar, tb, SC_TitleBarLabel, w);
        p->save();
        QFont f = p->font();
        f.setPixelSize(12);
        f.setWeight(QFont::Normal);
        p->setFont(f);
        p->setPen(active ? tc[T::Fg] : tc[T::Fg3]);
        p->drawText(label, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                    QFontMetrics(f).elidedText(tb->text, Qt::ElideRight, label.width()));
        p->restore();
    }

    struct Btn { SubControl sc; ChromeGlyph glyph; };
    const Btn buttons[] = {{SC_TitleBarMinButton, ChromeGlyph::Minimize},
                           {SC_TitleBarMaxButton, ChromeGlyph::Maximize},
                           {SC_TitleBarNormalButton, ChromeGlyph::Restore},
                           {SC_TitleBarCloseButton, ChromeGlyph::Close}};
    for (const Btn &b : buttons) {
        if (!(tb->subControls & b.sc))
            continue;
        const QRect r = proxy()->subControlRect(CC_TitleBar, tb, b.sc, w);
        if (r.isEmpty())
            continue;
        const bool on = tb->activeSubControls & b.sc;
        const bool pressed = on && (tb->state & State_Sunken);
        const bool hover = on && (tb->state & State_MouseOver);
        QColor glyph = active ? tc[T::Fg] : tc[T::Fg3];
        if (b.sc == SC_TitleBarCloseButton && (hover || pressed)) {
            p->fillRect(r, pressed ? mix(tc[T::DangerFill], tc[T::Fg], 0.15) : tc[T::DangerFill]);
            glyph = tc[T::OnDanger];
        } else if (hover || pressed) {
            p->fillRect(r, overlay(tc[T::Fg], pressed ? 0.12 : 0.07));
            glyph = tc[T::Fg];
        }
        QRectF box(0, 0, 16, 16);
        box.moveCenter(QRectF(r).center());
        paintChromeGlyph(p, b.glyph, box, glyph, false);
    }
}

// 최대화한 MDI 창이 메뉴 막대에 두는 단추 묶음: 제목 표시줄 단추와 같은 모양을 둥근 겹침으로.
void FmStyle::drawMdiControls(const QStyleOptionComplex *option, QPainter *p, const QWidget *w) const
{
    const ThemeColors &tc = colorsFor(w);
    const bool enabled = option->state & State_Enabled;
    struct Btn { SubControl sc; ChromeGlyph glyph; };
    const Btn buttons[] = {{SC_MdiMinButton, ChromeGlyph::Minimize},
                           {SC_MdiNormalButton, ChromeGlyph::Restore},
                           {SC_MdiCloseButton, ChromeGlyph::Close}};
    for (const Btn &b : buttons) {
        if (!(option->subControls & b.sc))
            continue;
        const QRect r = proxy()->subControlRect(CC_MdiControls, option, b.sc, w);
        if (r.isEmpty())
            continue;
        const bool on = enabled && (option->activeSubControls & b.sc);
        const bool pressed = on && (option->state & State_Sunken);
        const bool hover = on && (option->state & State_MouseOver);
        QColor glyph = enabled ? tc[T::Fg2] : tc[T::Fg3];
        if (b.sc == SC_MdiCloseButton && (hover || pressed)) {
            fillRounded(p, QRectF(r), kRadius, pressed ? mix(tc[T::DangerFill], tc[T::Fg], 0.15) : tc[T::DangerFill]);
            glyph = tc[T::OnDanger];
        } else if (hover || pressed) {
            fillRounded(p, QRectF(r), kRadius, overlay(tc[T::Fg], pressed ? 0.12 : 0.07));
            glyph = tc[T::Fg];
        }
        QRectF box(0, 0, 16, 16);
        box.moveCenter(QRectF(r).center());
        paintChromeGlyph(p, b.glyph, box, glyph, false);
    }
}

// =============================================================================================
// drawPrimitive

void FmStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *p,
                            const QWidget *w) const
{
    applyPreviewState(option, w);
    const ThemeColors &tc = colorsFor(w);

    switch (element) {
    case PE_PanelButtonCommand:
    case PE_PanelButtonBevel:
        drawButtonPanel(option, p, w);
        return;
    case PE_FrameDefaultButton:
    case PE_FrameButtonTool:
    case PE_FrameButtonBevel:
    case PE_IndicatorButtonDropDown:
        return;
    case PE_PanelButtonTool:
        drawToolPanel(option, p, w);
        return;
    case PE_FrameFocusRect:
        drawFocus(option, p, w);
        return;

    case PE_PanelLineEdit:
        if (const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
            if (f->lineWidth > 0) {
                drawInputFrame(p, f->rect, f->state, tc, isInvalid(w));
            } else {
                // 스핀 · 콤보 상자 안의 편집기는 바깥 상자가 바탕을 이미 칠했다.
                const QWidget *parent = w ? w->parentWidget() : nullptr;
                if (!qobject_cast<const QAbstractSpinBox *>(parent) && !qobject_cast<const QComboBox *>(parent))
                    p->fillRect(f->rect, f->palette.base());
            }
        }
        return;
    case PE_FrameLineEdit:
        return;

    case PE_IndicatorCheckBox:
        if (isSwitch(w))
            drawSwitch(p, option->rect, option->state, tc);
        else
            drawCheckIndicator(p, option->rect, option->state, tc);
        return;
    case PE_IndicatorItemViewItemCheck:
        drawCheckIndicator(p, option->rect, option->state, tc);
        return;
    case PE_IndicatorRadioButton:
        drawRadioIndicator(p, option->rect, option->state, tc);
        return;

    case PE_IndicatorArrowUp:
    case PE_IndicatorArrowDown:
    case PE_IndicatorArrowLeft:
    case PE_IndicatorArrowRight: {
        const Qt::ArrowType dir = element == PE_IndicatorArrowUp     ? Qt::UpArrow
                                : element == PE_IndicatorArrowDown   ? Qt::DownArrow
                                : element == PE_IndicatorArrowLeft   ? Qt::LeftArrow
                                                                     : Qt::RightArrow;
        const qreal size = std::clamp(std::min(option->rect.width(), option->rect.height()) / 4.0, 2.5, 4.0);
        drawChevron(p, QRectF(option->rect).center(), size, dir,
                    (option->state & State_Enabled) ? tc[T::Fg2] : tc[T::Fg3]);
        return;
    }
    case PE_IndicatorHeaderArrow:
        if (const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            if (h->sortIndicator == QStyleOptionHeader::None)
                return;
            // QHeaderView는 오름차순을 SortDown으로 넘긴다. 목업은 오름차순에 위쪽 꺾쇠.
            drawChevron(p, QRectF(option->rect).center(), 3.2,
                        h->sortIndicator == QStyleOptionHeader::SortDown ? Qt::UpArrow : Qt::DownArrow,
                        tc[T::Fg3]);
        }
        return;
    case PE_IndicatorBranch:
        if (option->state & State_Children)
            drawChevron(p, QRectF(option->rect).center(), 3.2,
                        (option->state & State_Open) ? Qt::DownArrow : Qt::RightArrow, tc[T::Fg3]);
        return;

    case PE_PanelItemViewItem:
        if (const auto *v = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            if (v->state & State_Selected) {
                p->fillRect(v->rect, paneActive(v, w) ? tc[T::Sel] : tc[T::SelIn]);
                return;
            }
            if (v->backgroundBrush.style() != Qt::NoBrush)
                p->fillRect(v->rect, v->backgroundBrush);
        }
        return;
    case PE_PanelItemViewRow:
        if (const auto *v = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            if ((v->state & State_Selected)
                && proxy()->styleHint(SH_ItemView_ShowDecorationSelected, option, w))
                p->fillRect(v->rect, paneActive(v, w) ? tc[T::Sel] : tc[T::SelIn]);
            else if (v->features & QStyleOptionViewItem::Alternate)
                p->fillRect(v->rect, tc[T::Alt]);
        }
        return;

    case PE_PanelMenu: {
        const bool rounded = w && w->testAttribute(Qt::WA_TranslucentBackground);
        if (rounded)
            fillRounded(p, crisp(option->rect), kCardRadius, tc[T::Surface]);
        else
            p->fillRect(option->rect, tc[T::Surface]);
        return;
    }
    case PE_FrameMenu: {
        const bool rounded = w && w->testAttribute(Qt::WA_TranslucentBackground);
        if (rounded) {
            strokeRounded(p, crisp(option->rect), kCardRadius, tc[T::Line]);
        } else {
            p->save();
            p->setPen(tc[T::Line]);
            p->drawRect(option->rect.adjusted(0, 0, -1, -1));
            p->restore();
        }
        return;
    }
    case PE_PanelMenuBar:
        return;
    case PE_PanelTipLabel:
        p->fillRect(option->rect, tc[T::Surface]);
        p->save();
        p->setPen(tc[T::Line]);
        p->drawRect(option->rect.adjusted(0, 0, -1, -1));
        p->restore();
        return;

    case PE_FrameTabBarBase:
        if (const auto *tb = qstyleoption_cast<const QStyleOptionTabBarBase *>(option)) {
            p->fillRect(tabBaseLine(tabSide(tb->shape), tb->rect), tc[T::Line]);
            return;
        }
        break;
    case PE_FrameTabWidget:
        p->fillRect(option->rect, tc[T::Surface]);
        p->save();
        p->setPen(tc[T::Line]);
        p->drawRect(option->rect.adjusted(0, 0, -1, -1));
        p->restore();
        return;
    case PE_IndicatorTabClose: {
        // 탭 닫기 단추: 평소 Fg3 X, 선택 탭 · 마우스 올림이면 Fg. 마우스 올림 · 누름은 둥근 겹침.
        const QStyle::State s = option->state;
        const bool enabled = s & State_Enabled;
        const bool pressed = enabled && (s & State_Sunken);
        const bool hover = enabled && (s & (State_Raised | State_MouseOver));
        QRectF box(0, 0, kTabClose, kTabClose);
        box.moveCenter(QRectF(option->rect).center());
        if (pressed || hover)
            fillRounded(p, box, kRadius, overlay(tc[T::Fg], pressed ? 0.14 : 0.08));
        const QColor color = !enabled                                        ? tc[T::Fg3]
                           : (pressed || hover || (s & State_Selected))      ? tc[T::Fg]
                                                                             : tc[T::Fg3];
        paintChromeGlyph(p, ChromeGlyph::Close, box.adjusted(1, 1, -1, -1), color, false);
        return;
    }
    case PE_Frame:
        p->save();
        p->setPen(tc[T::Line]);
        p->setBrush(Qt::NoBrush);
        p->drawRect(option->rect.adjusted(0, 0, -1, -1));
        p->restore();
        return;
    case PE_FrameWindow: {
        // MDI 창 틀: 제목 표시줄 아래 양옆 · 아래 띠를 창 바탕으로 메우고 바깥 1 px 선.
        const QRect r = option->rect;
        int width = proxy()->pixelMetric(PM_MdiSubWindowFrameWidth, option, w);
        if (const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(option); f && f->lineWidth > 0)
            width = f->lineWidth;
        const int top = std::min(r.bottom() + 1, r.top() + proxy()->pixelMetric(PM_TitleBarHeight, option, w));
        const int bandHeight = r.bottom() + 1 - top;
        p->fillRect(QRect(r.left(), top, width, bandHeight), tc[T::Win]);
        p->fillRect(QRect(r.right() - width + 1, top, width, bandHeight), tc[T::Win]);
        p->fillRect(QRect(r.left(), r.bottom() - width + 1, r.width(), width), tc[T::Win]);
        p->save();
        p->setPen((option->state & State_Active) ? tc[T::Line] : tc[T::Grid]);
        p->setBrush(Qt::NoBrush);
        p->drawRect(r.adjusted(0, 0, -1, -1));
        p->restore();
        return;
    }
    case PE_FrameGroupBox: {
        const QRectF r = crisp(option->rect);
        fillRounded(p, r, kCardRadius, tc[T::Surface]);
        strokeRounded(p, r, kCardRadius, tc[T::Line]);
        return;
    }
    case PE_IndicatorToolBarSeparator: {
        const QRect r = option->rect;
        if (option->state & State_Horizontal)
            p->fillRect(QRect(r.center().x(), r.center().y() - 10, 1, 20), tc[T::Line]);
        else
            p->fillRect(QRect(r.center().x() - 10, r.center().y(), 20, 1), tc[T::Line]);
        return;
    }
    case PE_PanelToolBar:
        return;
    case PE_PanelScrollAreaCorner:
        p->fillRect(option->rect, option->palette.base());
        return;
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, p, w);
}

// =============================================================================================
// drawControl

void FmStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *p,
                          const QWidget *w) const
{
    applyPreviewState(option, w);
    const ThemeColors &tc = colorsFor(w);

    switch (element) {
    case CE_PushButtonBevel:
        if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            drawButtonPanel(b, p, w);
            if (b->features & QStyleOptionButton::HasMenu) {
                const QRect ar(b->rect.right() - 20, b->rect.top(), 14, b->rect.height());
                drawChevron(p, QRectF(ar).center(), 3.5, Qt::DownArrow,
                            buttonTextColor(b->state, buttonRole(w), false, tc));
            }
        }
        return;
    case CE_PushButtonLabel:
        if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            const bool segment = !segmentOf(w).isEmpty();
            const ButtonRole role = buttonRole(w);
            QStyleOptionButton copy = *b;
            QColor text = buttonTextColor(b->state, role, segment, tc);
            if (role == ButtonRole::Link) {
                text = !(b->state & State_Enabled)                          ? tc[T::Fg3]
                     : (b->state & (State_MouseOver | State_Sunken))        ? tc[T::Accent]
                                                                            : tc[T::AccentFg];
            }
            copy.palette.setColor(QPalette::ButtonText, text);
            if (b->features & QStyleOptionButton::HasMenu)
                copy.rect.adjust(0, 0, -14, 0);
            // 키 칩(fmKeyHint): 글자(+아이콘) 뒤 8 px에 칩 — 글자와 칩을 한 덩어리로 가운데 둔다.
            const QString keys = keyHintOf(w);
            QRect chipRect;
            if (!keys.isEmpty()) {
                const QSize chip = keyChipSize(keys);
                int labelWidth = b->fontMetrics.horizontalAdvance(plainText(b->text));
                if (!b->icon.isNull())
                    labelWidth += b->iconSize.width() + (segment ? 4 : kButtonIconGap);
                const int total = labelWidth + kKeyChipGap + chip.width();
                const int left = copy.rect.left() + (copy.rect.width() - total) / 2;
                copy.rect = QRect(left, copy.rect.top(), labelWidth, copy.rect.height());
                chipRect = QRect(left + labelWidth + kKeyChipGap, copy.rect.center().y() - chip.height() / 2 + 1,
                                 chip.width(), chip.height());
            }
            const bool bold = segment && (b->state & State_On);
            p->save();
            if (bold) {
                QFont f = p->font();
                f.setWeight(QFont::DemiBold);
                p->setFont(f);
            }
            if (segment || role == ButtonRole::Link || !drawIconTextButtonLabel(this, copy, p, w))
                QProxyStyle::drawControl(element, &copy, p, w);
            p->restore();
            if (chipRect.isValid()) {
                const bool filled = (b->state & State_Enabled)
                                    && (role == ButtonRole::Primary || role == ButtonRole::Danger);
                paintKeyChip(p, chipRect, keys, tc, filled ? KeyChipLook::OnFill : KeyChipLook::Normal);
            }
        }
        return;
    case CE_CheckBoxLabel:
    case CE_RadioButtonLabel:
        // 키 칩(fmKeyHint): 글자 뒤 8 px — 삭제 대화상자의 '휴지통으로 이동(R) [Del]'
        if (const QString keys = keyHintOf(w); !keys.isEmpty()) {
            if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(option)) {
                QProxyStyle::drawControl(element, option, p, w);
                int textWidth = b->fontMetrics.horizontalAdvance(plainText(b->text));
                if (!b->icon.isNull())
                    textWidth += b->iconSize.width() + 4;
                const QSize chip = keyChipSize(keys);
                paintKeyChip(p, QRect(b->rect.left() + textWidth + kKeyChipGap, b->rect.center().y() - chip.height() / 2 + 1,
                                      chip.width(), chip.height()),
                             keys, tc);
                return;
            }
        }
        break;
    case CE_ToolButtonLabel:
        if (const auto *tb = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
            const bool segment = !segmentOf(w).isEmpty();
            QStyleOptionToolButton copy = *tb;
            copy.palette.setColor(QPalette::ButtonText, buttonTextColor(tb->state, buttonRole(w), segment, tc));
            const bool bold = segment && (tb->state & State_On);
            p->save();
            if (bold) {
                QFont f = p->font();
                f.setWeight(QFont::DemiBold);
                p->setFont(f);
            }
            QProxyStyle::drawControl(element, &copy, p, w);
            p->restore();
        }
        return;

    case CE_CheckBox:
        // QCommonStyle은 여기서 subElementRect를 proxy() 없이 불러 스위치 배치가 무시된다 — 직접 배치.
        if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(option); b && isSwitch(w)) {
            QStyleOptionButton sub = *b;
            sub.rect = subElementRect(SE_CheckBoxIndicator, b, w);
            proxy()->drawPrimitive(PE_IndicatorCheckBox, &sub, p, w);
            sub.rect = subElementRect(SE_CheckBoxContents, b, w);
            proxy()->drawControl(CE_CheckBoxLabel, &sub, p, w);
            if (b->state & State_HasFocus) {
                QStyleOptionFocusRect focus;
                focus.QStyleOption::operator=(*b);
                focus.rect = subElementRect(SE_CheckBoxFocusRect, b, w);
                proxy()->drawPrimitive(PE_FrameFocusRect, &focus, p, w);
            }
            return;
        }
        break;
    case CE_ProgressBar:
        if (const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            QStyleOptionProgressBar sub = *pb;
            sub.rect = subElementRect(SE_ProgressBarGroove, pb, w);
            drawProgress(CE_ProgressBarGroove, &sub, p, w);
            sub.rect = subElementRect(SE_ProgressBarContents, pb, w);
            drawProgress(CE_ProgressBarContents, &sub, p, w);
            if (pb->textVisible) {
                sub.rect = subElementRect(SE_ProgressBarLabel, pb, w);
                drawProgress(CE_ProgressBarLabel, &sub, p, w);
            }
        }
        return;

    case CE_TabBarTabShape:
        drawTabShape(option, p, w);
        return;
    case CE_TabBarTabLabel:
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {
            const bool selected = tab->state & State_Selected;
            QStyleOptionTab copy = *tab;
            const QColor fg = !(tab->state & State_Enabled) ? tc[T::Fg3] : selected ? tc[T::Fg] : tc[T::Fg2];
            copy.palette.setColor(QPalette::WindowText, fg);
            copy.palette.setColor(QPalette::ButtonText, fg);
            p->save();
            if (selected) {
                QFont f = p->font();
                f.setWeight(QFont::DemiBold);
                p->setFont(f);
            }
            QProxyStyle::drawControl(element, &copy, p, w);
            p->restore();
        }
        return;

    case CE_ToolBoxTabShape:
    case CE_ToolBoxTabLabel:
        drawToolBoxTab(element, option, p, w);
        return;

    case CE_Header:
        // QCommonStyle은 SE_HeaderLabel/SE_HeaderArrow를 proxy() 없이 불러 정렬 꺾쇠 위치가 무시된다.
        if (const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            p->save();
            p->setClipRect(option->rect);
            drawHeaderSection(h, p, w);
            QStyleOptionHeader sub = *h;
            sub.rect = subElementRect(SE_HeaderLabel, h, w);
            if (sub.rect.isValid())
                proxy()->drawControl(CE_HeaderLabel, &sub, p, w);
            if (h->sortIndicator != QStyleOptionHeader::None) {
                sub.rect = subElementRect(SE_HeaderArrow, h, w);
                proxy()->drawPrimitive(PE_IndicatorHeaderArrow, &sub, p, w);
            }
            p->restore();
        }
        return;
    case CE_HeaderSection:
        drawHeaderSection(option, p, w);
        return;
    case CE_HeaderLabel:
        if (const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            QStyleOptionHeader copy = *h;
            copy.palette.setColor(QPalette::ButtonText, tc[T::Fg2]);
            copy.palette.setColor(QPalette::WindowText, tc[T::Fg2]);
            QProxyStyle::drawControl(element, &copy, p, w);
        }
        return;
    case CE_HeaderEmptyArea:
        p->fillRect(option->rect, tc[T::Head]);
        p->fillRect(QRect(option->rect.left(), option->rect.bottom(), option->rect.width(), 1), tc[T::Line]);
        return;

    case CE_ItemViewItem:
        if (const auto *v = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            // 목업: 선택 행은 바탕만 바뀌고 글자색은 그대로 (역상 선택은 3단계에서 따로)
            QStyleOptionViewItem copy = *v;
            for (const auto group : {QPalette::Active, QPalette::Inactive})
                copy.palette.setColor(group, QPalette::HighlightedText, copy.palette.color(group, QPalette::Text));
            QProxyStyle::drawControl(element, &copy, p, w);
        }
        return;

    case CE_MenuBarItem:
        drawMenuBarItem(option, p, w);
        return;
    case CE_MenuBarEmptyArea:
    case CE_MenuEmptyArea:
    case CE_MenuHMargin:
    case CE_MenuVMargin:
        return;
    case CE_MenuItem:
        drawMenuItem(option, p, w);
        return;

    case CE_ProgressBarGroove:
    case CE_ProgressBarContents:
    case CE_ProgressBarLabel:
        drawProgress(element, option, p, w);
        return;

    case CE_ShapedFrame:
        drawShapedFrame(option, p, w);
        return;

    case CE_Splitter: {
        const QRect r = option->rect;
        if (option->state & State_Horizontal)
            p->fillRect(QRect(r.center().x(), r.top(), 1, r.height()), tc[T::Line]);
        else
            p->fillRect(QRect(r.left(), r.center().y(), r.width(), 1), tc[T::Line]);
        return;
    }
    case CE_ToolBar:
    case CE_FocusFrame:
        return;
    case CE_RubberBand: {
        p->save();
        p->setPen(tc[T::Accent]);
        p->setBrush(withAlpha(tc[T::Accent], 0.15));
        p->drawRect(option->rect.adjusted(0, 0, -1, -1));
        p->restore();
        return;
    }
    default:
        break;
    }
    QProxyStyle::drawControl(element, option, p, w);
}

// =============================================================================================
// drawComplexControl

void FmStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                 QPainter *p, const QWidget *w) const
{
    applyPreviewState(option, w);
    const ThemeColors &tc = colorsFor(w);

    switch (control) {
    case CC_ComboBox:
        if (const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            QStyle::State s = cb->state;
            if (s & State_On)  // 목록이 열려 있으면 포커스처럼 강조
                s |= State_HasFocus;
            drawInputFrame(p, cb->rect, s, tc, isInvalid(w));
            const QRect arrow = proxy()->subControlRect(CC_ComboBox, cb, SC_ComboBoxArrow, w);
            drawChevron(p, QRectF(arrow).center() - QPointF(2, 0), 3.5, Qt::DownArrow,
                        (cb->state & State_Enabled) ? tc[T::Fg3] : tc[T::BtnLine]);
        }
        return;

    case CC_SpinBox:
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            drawInputFrame(p, sb->rect, sb->state, tc, isInvalid(w));
            if (sb->buttonSymbols == QAbstractSpinBox::NoButtons)
                return;
            for (const SubControl sc : {SC_SpinBoxUp, SC_SpinBoxDown}) {
                const QRect r = proxy()->subControlRect(CC_SpinBox, sb, sc, w);
                const auto step = sc == SC_SpinBoxUp ? QAbstractSpinBox::StepUpEnabled
                                                     : QAbstractSpinBox::StepDownEnabled;
                const bool enabled = (sb->state & State_Enabled) && (sb->stepEnabled & step);
                if (enabled && (sb->activeSubControls & sc)) {
                    const bool pressed = sb->state & State_Sunken;
                    fillRounded(p, QRectF(r).adjusted(1, 1, -1, -1), 3.0,
                                overlay(tc[T::Fg], pressed ? 0.12 : 0.06));
                }
                drawChevron(p, QRectF(r).center(), 3.0, sc == SC_SpinBoxUp ? Qt::UpArrow : Qt::DownArrow,
                            enabled ? tc[T::Fg2] : tc[T::BtnLine]);
            }
        }
        return;

    case CC_ScrollBar:
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            // 바탕은 스크롤 영역 안쪽(목록이면 목록 바탕)과 같게
            const QAbstractScrollArea *area = scrollAreaOf(w);
            const QPalette::ColorRole role = area && area->viewport() ? area->viewport()->backgroundRole()
                                                                      : QPalette::Window;
            p->fillRect(sb->rect, sb->palette.brush(role));

            const QRect slider = proxy()->subControlRect(CC_ScrollBar, sb, SC_ScrollBarSlider, w);
            if (slider.isEmpty() || sb->minimum == sb->maximum)
                return;
            const bool horizontal = sb->orientation == Qt::Horizontal;
            const bool hover = sb->state & State_MouseOver;
            const bool pressed = (sb->activeSubControls & SC_ScrollBarSlider) && (sb->state & State_Sunken);
            const qreal thickness = (hover || pressed) ? 8.0 : 6.0;
            QRectF r(slider);
            if (horizontal) {
                r.setTop(r.center().y() - thickness / 2);
                r.setHeight(thickness);
                r.adjust(2, 0, -2, 0);
            } else {
                r.setLeft(r.center().x() - thickness / 2);
                r.setWidth(thickness);
                r.adjust(0, 2, 0, -2);
            }
            const QColor thumb = withAlpha(pressed ? tc[T::Fg2] : tc[T::Fg3], pressed ? 0.85 : hover ? 0.70 : 0.45);
            fillRounded(p, r, thickness / 2, thumb);
        }
        return;

    case CC_GroupBox:
        if (const auto *gb = qstyleoption_cast<const QStyleOptionGroupBox *>(option)) {
            if (gb->subControls & SC_GroupBoxFrame) {
                const QRect fr = proxy()->subControlRect(CC_GroupBox, gb, SC_GroupBoxFrame, w);
                const QRectF r = crisp(fr);
                fillRounded(p, r, kCardRadius, tc[T::Surface]);
                strokeRounded(p, r, kCardRadius, tc[T::Line]);
            }
            if ((gb->subControls & SC_GroupBoxLabel) && !gb->text.isEmpty()) {
                const QRect lr = proxy()->subControlRect(CC_GroupBox, gb, SC_GroupBoxLabel, w);
                int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextSingleLine;
                if (!proxy()->styleHint(SH_UnderlineShortcut, gb, w))
                    flags |= Qt::TextHideMnemonic;
                p->save();
                QFont f = p->font();
                f.setWeight(QFont::DemiBold);
                p->setFont(f);
                p->setPen((gb->state & State_Enabled) ? tc[T::Fg] : tc[T::Fg3]);
                p->drawText(lr, flags, gb->text);
                p->restore();
            }
            if (gb->subControls & SC_GroupBoxCheckBox) {
                QStyleOptionButton box;
                box.QStyleOption::operator=(*gb);
                box.rect = proxy()->subControlRect(CC_GroupBox, gb, SC_GroupBoxCheckBox, w);
                drawCheckIndicator(p, box.rect, box.state, tc);
            }
        }
        return;

    case CC_Slider:
        drawSlider(option, p, w);
        return;
    case CC_Dial:
        drawDial(option, p, w);
        return;
    case CC_TitleBar:
        drawTitleBar(option, p, w);
        return;
    case CC_MdiControls:
        drawMdiControls(option, p, w);
        return;

    default:
        break;
    }
    QProxyStyle::drawComplexControl(control, option, p, w);
}

// =============================================================================================
// 영역 · 크기

QRect FmStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *w) const
{
    switch (element) {
    case SE_PushButtonContents:
        return option->rect.adjusted(4, 2, -4, -2);
    case SE_PushButtonFocusRect:
        return option->rect;

    case SE_LineEditContents:
        if (const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(option); f && f->lineWidth > 0)
            return option->rect.adjusted(kInputPadX - 1, 1, -(kInputPadX - 1), -1);
        return option->rect;

    case SE_CheckBoxIndicator:
    case SE_CheckBoxContents:
    case SE_CheckBoxClickRect:
    case SE_CheckBoxFocusRect:
        if (isSwitch(w)) {
            const auto *b = qstyleoption_cast<const QStyleOptionButton *>(option);
            const QRect r = option->rect;
            const int iw = kSwitchWidth + 2 * kSwitchMargin;
            const int ih = kSwitchHeight + 2 * kSwitchMargin;
            const QRect indicator(r.right() - iw + 1, r.top() + (r.height() - ih) / 2, iw, ih);
            if (element == SE_CheckBoxIndicator || element == SE_CheckBoxFocusRect)
                return visualRect(option->direction, r, indicator);
            if (element == SE_CheckBoxContents) {
                int textWidth = 0;
                if (b) {
                    textWidth = option->fontMetrics.horizontalAdvance(b->text);
                    if (!b->icon.isNull())
                        textWidth += b->iconSize.width() + 4;
                }
                const int right = indicator.left() - kSwitchGap;
                return visualRect(option->direction, r, QRect(right - textWidth, r.top(), textWidth, r.height()));
            }
            return r;
        }
        if (element == SE_CheckBoxFocusRect)
            return option->rect;
        break;
    case SE_RadioButtonFocusRect:
        return option->rect;

    case SE_ProgressBarGroove:
    case SE_ProgressBarContents:
    case SE_ProgressBarLabel:
        if (const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            const bool horizontal = pb->state & State_Horizontal;
            const QRect r = pb->rect;
            int labelWidth = 0;
            if (pb->textVisible && horizontal)
                labelWidth = pb->fontMetrics.horizontalAdvance(u"100%"_s) + 8;
            if (element == SE_ProgressBarLabel)
                return QRect(r.right() - labelWidth + 1, r.top(), labelWidth, r.height());
            const QRect bar = r.adjusted(0, 0, -labelWidth, 0);
            const int thickness = progressThickness(w);
            if (horizontal)
                return QRect(bar.left(), bar.top() + (bar.height() - thickness) / 2, bar.width(), thickness);
            return QRect(bar.left() + (bar.width() - thickness) / 2, bar.top(), thickness, bar.height());
        }
        break;

    case SE_HeaderArrow:
        if (const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            // 목업: 정렬 꺾쇠는 이름 바로 뒤
            const int margin = proxy()->pixelMetric(PM_HeaderMargin, option, w);
            const int textWidth = option->fontMetrics.horizontalAdvance(h->text);
            const QRect r = h->rect;
            if (h->textAlignment & Qt::AlignRight)
                return QRect(r.right() - margin - textWidth - 14, r.top(), 10, r.height());
            return QRect(std::min(r.left() + margin + textWidth + 4, r.right() - 12), r.top(), 10, r.height());
        }
        break;
    case SE_HeaderLabel: {
        const int margin = proxy()->pixelMetric(PM_HeaderMargin, option, w);
        return option->rect.adjusted(margin, 0, -margin, 0);
    }
    default:
        break;
    }
    return QProxyStyle::subElementRect(element, option, w);
}

QRect FmStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                              SubControl sc, const QWidget *w) const
{
    switch (control) {
    case CC_ComboBox:
        if (const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            const QRect r = cb->rect;
            QRect res;
            switch (sc) {
            case SC_ComboBoxFrame:
            case SC_ComboBoxListBoxPopup:
                res = r;
                break;
            case SC_ComboBoxArrow:
                res = QRect(r.right() - kComboArrowWidth + 1, r.top(), kComboArrowWidth, r.height());
                break;
            case SC_ComboBoxEditField:
                res = QRect(r.left() + kInputPadX, r.top() + 2, r.width() - kInputPadX - kComboArrowWidth,
                            r.height() - 4);
                break;
            default:
                return QProxyStyle::subControlRect(control, option, sc, w);
            }
            return visualRect(cb->direction, r, res);
        }
        break;

    case CC_SpinBox:
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            const QRect r = sb->rect;
            const bool buttons = sb->buttonSymbols != QAbstractSpinBox::NoButtons;
            const int half = (r.height() - 4) / 2;
            QRect res;
            switch (sc) {
            case SC_SpinBoxFrame:
                res = r;
                break;
            case SC_SpinBoxEditField:
                res = QRect(r.left() + kInputPadX, r.top() + 2,
                            r.width() - kInputPadX - (buttons ? kSpinButtonWidth + 2 : kInputPadX), r.height() - 4);
                break;
            case SC_SpinBoxUp:
                if (!buttons)
                    return QRect();
                res = QRect(r.right() - kSpinButtonWidth - 1, r.top() + 2, kSpinButtonWidth, half);
                break;
            case SC_SpinBoxDown:
                if (!buttons)
                    return QRect();
                res = QRect(r.right() - kSpinButtonWidth - 1, r.top() + 2 + half, kSpinButtonWidth,
                            r.height() - 4 - half);
                break;
            default:
                return QProxyStyle::subControlRect(control, option, sc, w);
            }
            return visualRect(sb->direction, r, res);
        }
        break;

    case CC_ScrollBar:
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            // 화살표 단추 없이 홈 전체를 슬라이더가 움직인다.
            const QRect r = sb->rect;
            const bool horizontal = sb->orientation == Qt::Horizontal;
            const int length = horizontal ? r.width() : r.height();
            const qint64 range = qint64(sb->maximum) - qint64(sb->minimum);
            const int minLength = std::min(proxy()->pixelMetric(PM_ScrollBarSliderMin, sb, w), length);
            int sliderLength = length;
            if (range > 0) {
                sliderLength = int(qint64(sb->pageStep) * length / (range + sb->pageStep));
                sliderLength = std::clamp(sliderLength, minLength, length);
            }
            const int pos = sliderPositionFromValue(sb->minimum, sb->maximum, sb->sliderPosition,
                                                    length - sliderLength, sb->upsideDown);
            const auto part = [&](int start, int size) {
                return horizontal ? QRect(r.left() + start, r.top(), size, r.height())
                                  : QRect(r.left(), r.top() + start, r.width(), size);
            };
            switch (sc) {
            case SC_ScrollBarGroove:
                return r;
            case SC_ScrollBarSlider:
                return part(pos, sliderLength);
            case SC_ScrollBarSubPage:
                return part(0, pos);
            case SC_ScrollBarAddPage:
                return part(pos + sliderLength, length - pos - sliderLength);
            case SC_ScrollBarSubLine:
            case SC_ScrollBarAddLine:
            case SC_ScrollBarFirst:
            case SC_ScrollBarLast:
                return QRect();
            default:
                break;
            }
        }
        break;

    case CC_GroupBox:
        if (const auto *gb = qstyleoption_cast<const QStyleOptionGroupBox *>(option)) {
            const QRect r = gb->rect;
            const int lineHeight = gb->fontMetrics.height();
            const int titleHeight = gb->text.isEmpty() ? 0 : lineHeight + 8;
            const bool checkable = gb->subControls & SC_GroupBoxCheckBox;
            const int indicatorSpace = checkable ? kIndicator + 8 : 0;
            switch (sc) {
            case SC_GroupBoxCheckBox:
                return QRect(r.left(), r.top() + (lineHeight - kIndicator) / 2, kIndicator, kIndicator);
            case SC_GroupBoxLabel:
                return QRect(r.left() + indicatorSpace, r.top(),
                             gb->fontMetrics.horizontalAdvance(gb->text) + 8, lineHeight);
            case SC_GroupBoxFrame:
                return r.adjusted(0, titleHeight, 0, 0);
            case SC_GroupBoxContents:
                return r.adjusted(1, titleHeight + 1, -1, -1);
            default:
                break;
            }
        }
        break;

    case CC_Slider:
        if (const auto *sl = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            if (const QRect res = sliderSubControlRect(sl, sc, kSliderHandle, kSliderHandle); res.isValid())
                return res;
        }
        break;

    case CC_TitleBar:
        if (const auto *tb = qstyleoption_cast<const QStyleOptionTitleBar *>(option)) {
            // 오른쪽 끝부터 닫기 · 최대화(복원) · 최소화를 틈 없이 붙인다(Windows 11). 단추는 36 × 제목 표시줄 높이,
            // 바깥 1 px 틀 선 안쪽.
            const QRect r = tb->rect;
            const Qt::WindowFlags flags = tb->titleBarFlags;
            const bool isMin = tb->titleBarState & Qt::WindowMinimized;
            const bool isMax = tb->titleBarState & Qt::WindowMaximized;
            const bool hasClose = flags & Qt::WindowSystemMenuHint;
            const bool hasMax = flags & Qt::WindowMaximizeButtonHint;
            const bool hasMin = flags & Qt::WindowMinimizeButtonHint;
            const auto at = [&](int right) {
                return QRect(right - kCaptionButtonW + 1, r.top() + 1, kCaptionButtonW, r.height() - 1);
            };
            int right = r.right() - 1;
            const QRect close = hasClose ? at(right) : QRect();
            if (hasClose)
                right = close.left() - 1;
            const bool showMax = hasMax && !isMax;
            const bool showNormal = (isMax && hasMax) || (isMin && hasMin);
            const QRect maxOrNormal = (showMax || showNormal) ? at(right) : QRect();
            if (showMax || showNormal)
                right = maxOrNormal.left() - 1;
            const bool showMin = hasMin && !isMin;
            const QRect min = showMin ? at(right) : QRect();
            const int leftmost = showMin ? min.left() : (showMax || showNormal) ? maxOrNormal.left()
                                                        : hasClose ? close.left() : r.right();
            const QRect sysMenu = (flags & Qt::WindowSystemMenuHint)
                ? QRect(r.left() + 8, r.top() + (r.height() - 16) / 2, 16, 16) : QRect();
            QRect res;
            switch (sc) {
            case SC_TitleBarCloseButton: res = close; break;
            case SC_TitleBarMaxButton: res = showMax ? maxOrNormal : QRect(); break;
            case SC_TitleBarNormalButton: res = showNormal ? maxOrNormal : QRect(); break;
            case SC_TitleBarMinButton: res = min; break;
            case SC_TitleBarSysMenu: res = sysMenu; break;
            case SC_TitleBarLabel: {
                const int left = sysMenu.isValid() ? sysMenu.right() + 8 : r.left() + 8;
                res = QRect(left, r.top(), std::max(0, leftmost - 8 - left), r.height());
                break;
            }
            case SC_TitleBarShadeButton:
            case SC_TitleBarUnshadeButton:
            case SC_TitleBarContextHelpButton:
                return QRect();
            default:
                return QProxyStyle::subControlRect(control, option, sc, w);
            }
            return visualRect(tb->direction, r, res);
        }
        break;

    default:
        break;
    }
    return QProxyStyle::subControlRect(control, option, sc, w);
}

QSize FmStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &cs,
                                const QWidget *w) const
{
    switch (type) {
    case CT_PushButton:
        if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            const QString keys = keyHintOf(w);
            const int chip = keys.isEmpty() ? 0 : kKeyChipGap + keyChipSize(keys).width();
            if (buttonRole(w) == ButtonRole::Link)
                return QSize(cs.width() + chip + 4, kLinkHeight);
            const bool segment = !segmentOf(w).isEmpty();
            const bool compact = isSmall(w);
            const int smallHeight = density(w) == Density::Dialog ? kButtonHeightSmallDialog : kButtonHeightSmall;
            const int height = segment ? segmentHeight(w) : compact ? smallHeight : kButtonHeight;
            const int padX = segment ? segmentPadX(w) : compact ? kButtonPadXSmall : kButtonPadX;
            if (b->text.isEmpty() && !b->icon.isNull())
                return QSize(std::max(height, cs.width() + 16), height);
            const int minWidth = (segment || compact) ? 0 : kButtonMinWidth;
            // 세그먼트는 켜지면 굵은 글자가 되므로 여유 4 px
            // 아이콘 · 글자 간격 8(Qt가 넣어 준 4에 4를 더한다)
            const int iconGap = (!segment && !b->icon.isNull() && !b->text.isEmpty()) ? kButtonIconGap - 4 : 0;
            const int width = cs.width() + 2 * padX + (segment ? 4 : 0) + chip + iconGap;
            return QSize(std::max(minWidth, width), std::max(height, cs.height() + 8));
        }
        break;
    case CT_ToolButton: {
        const bool segment = !segmentOf(w).isEmpty();
        if (segment)
            return QSize(cs.width() + 2 * segmentPadX(w) + 4, segmentHeight(w));
        return QSize(std::max(kButtonHeight, cs.width() + 16), std::max(kButtonHeight, cs.height() + 16));
    }

    case CT_LineEdit:
        if (const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(option); f && f->lineWidth > 0)
            return QSize(cs.width() + 2 * kInputPadX, std::max(inputHeight(w), cs.height() + 2));
        return cs;
    case CT_SpinBox:
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            const bool buttons = sb->buttonSymbols != QAbstractSpinBox::NoButtons;
            return QSize(cs.width() + kInputPadX + (buttons ? kSpinButtonWidth + 2 : kInputPadX),
                         std::max(inputHeight(w), cs.height() + 4));
        }
        break;
    case CT_ComboBox:
        return QSize(cs.width() + kInputPadX + kComboArrowWidth + 4, std::max(inputHeight(w), cs.height() + 4));

    case CT_CheckBox:
        if (isSwitch(w)) {
            const int gap = cs.width() > 0 ? kSwitchGap : 0;
            return QSize(cs.width() + gap + kSwitchWidth + 2 * kSwitchMargin,
                         std::max(kSwitchHeight + 2 * kSwitchMargin, cs.height()));
        }
        [[fallthrough]];
    case CT_RadioButton:
        if (const QString keys = keyHintOf(w); !keys.isEmpty()) {
            QSize s = QProxyStyle::sizeFromContents(type, option, cs, w);
            s.rwidth() += kKeyChipGap + keyChipSize(keys).width();
            s.setHeight(std::max(s.height(), 20));
            return s;
        }
        break;

    case CT_TabBarTab:
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {
            // 선택 탭의 굵은 글자 여유 6. 세로 탭은 가로 · 세로가 바뀐다.
            if (isVerticalTab(tabSide(tab->shape)))
                return QSize(kTabHeight, cs.height() + 6);
            return QSize(cs.width() + 6, kTabHeight);
        }
        break;
    case CT_HeaderSection: {
        QSize s = QProxyStyle::sizeFromContents(type, option, cs, w);
        if (const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(option);
            h && h->orientation == Qt::Horizontal)
            s.setHeight(flatHeader(w) ? kHeaderHeightFlat : kHeaderHeight);
        return s;
    }
    case CT_ItemViewItem: {
        QSize s = QProxyStyle::sizeFromContents(type, option, cs, w);
        s.setHeight(std::max(s.height(), kItemMinHeight));
        return s;
    }

    case CT_MenuItem:
        if (const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            if (mi->menuItemType == QStyleOptionMenuItem::Separator)
                return QSize(cs.width(), 9);
            const bool subMenu = mi->menuItemType == QStyleOptionMenuItem::SubMenu;
            // 왼쪽 6 + 체크 열 20 + 간격 8 + 글자 + (단축키 앞 간격) + 오른쪽 12 + (하위 메뉴 꺾쇠)
            int width = 6 + 20 + 8 + cs.width() + 12 + (subMenu ? 12 : 0);
            if (mi->text.contains(u'\t') || mi->reservedShortcutWidth > 0)
                width += 28;
            return QSize(width, kMenuItemHeight);
        }
        break;
    case CT_MenuBarItem:
        return QSize(cs.width() + 20, kMenuBarItemHeight);

    case CT_ProgressBar:
        if (const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            if (pb->state & State_Horizontal)
                return QSize(cs.width(), pb->textVisible ? std::max(cs.height(), 16) : progressThickness(w));
            return QSize(pb->textVisible ? std::max(cs.width(), 16) : progressThickness(w), cs.height());
        }
        break;

    default:
        break;
    }
    return QProxyStyle::sizeFromContents(type, option, cs, w);
}

int FmStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *w) const
{
    switch (metric) {
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
        return 0;
    case PM_DefaultFrameWidth:
    case PM_ComboBoxFrameWidth:
    case PM_SpinBoxFrameWidth:
        return 1;
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return kIndicator;
    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing:
        return 8;
    case PM_ScrollBarExtent:
        return kScrollBarExtent;
    case PM_ScrollBarSliderMin:
        return 24;
    case PM_ScrollView_ScrollBarOverlap:
    case PM_ScrollView_ScrollBarSpacing:
        return 0;
    case PM_TabBarTabHSpace:
        return 2 * kTabPadX;
    case PM_TabBarTabVSpace:
        return 8;
    case PM_TabBarBaseOverlap:
        return 1;
    case PM_TabBarTabOverlap:
        return 0;
    case PM_MenuPanelWidth:
        return 1;
    case PM_MenuHMargin:
    case PM_MenuVMargin:
        return 4;
    case PM_MenuBarPanelWidth:
        return 0;
    case PM_MenuBarItemSpacing:
        return 2;
    case PM_MenuBarHMargin:
        return 6;
    case PM_MenuBarVMargin:
        return 2;
    case PM_SmallIconSize:
    case PM_ToolBarIconSize:
    case PM_ButtonIconSize:
    case PM_TabBarIconSize:
    case PM_ListViewIconSize:
        return 16;
    case PM_ToolBarItemSpacing:
        return 2;
    case PM_ToolBarSeparatorExtent:
        return 13;
    case PM_ToolBarFrameWidth:
    case PM_ToolBarItemMargin:
        return 0;
    case PM_HeaderMargin:
        return 8;
    case PM_FocusFrameHMargin:
    case PM_FocusFrameVMargin:
        return 2;
    case PM_ToolTipLabelFrameWidth:
        return 5;
    case PM_SplitterWidth:
        return 5;
    case PM_TabCloseIndicatorWidth:
    case PM_TabCloseIndicatorHeight:
        return kTabClose;
    case PM_SliderThickness:
    case PM_SliderControlThickness:
    case PM_SliderLength:
        return kSliderHandle;
    case PM_TitleBarHeight:
        return kTitleBarHeight;
    case PM_TitleBarButtonSize:
        return kMdiButton;
    default:
        break;
    }
    return QProxyStyle::pixelMetric(metric, option, w);
}

QIcon FmStyle::standardIcon(StandardPixmap standardIcon, const QStyleOption *option, const QWidget *w) const
{
    // 관리자 권한 방패 — 목업의 두 색 방패(--shield / --shield-2). Windows 시스템 방패 대신 쓴다.
    if (standardIcon == SP_VistaShield)
        return shieldIcon(colorsFor(w), 16);
    // 화살표 · 창 단추 · 확장 단추 등 단색 아이콘은 테마 색으로 (Fusion 그림은 검은색이라 다크에서 안 보인다).
    if (QIcon icon = themedStandardIcon(standardIcon, w, false); !icon.isNull())
        return icon;
    return QProxyStyle::standardIcon(standardIcon, option, w);
}

int FmStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *w,
                       QStyleHintReturn *returnData) const
{
    switch (hint) {
    case SH_UnderlineShortcut:
        return (m_alwaysMnemonics || m_altDown) ? 1 : 0;
    case SH_DialogButtonLayout:
        return QDialogButtonBox::WinLayout;
    case SH_DialogButtonBox_ButtonsHaveIcons:
        return 0;
    case SH_ItemView_ShowDecorationSelected:
        return 1;
    case SH_ItemView_ActivateItemOnSingleClick:
        return 0;
    case SH_ComboBox_Popup:
        return 0;
    case SH_ComboBox_ListMouseTracking:
    case SH_Menu_MouseTracking:
    case SH_MenuBar_MouseTracking:
    case SH_MenuBar_AltKeyNavigation:
    case SH_Menu_Scrollable:
    case SH_ScrollBar_MiddleClickAbsolutePosition:
        return 1;
    case SH_ScrollBar_Transient:
    case SH_EtchDisabledText:
    case SH_DitherDisabledText:
        return 0;
    case SH_Table_GridLineColor:
        return static_cast<int>(colorsFor(w)[T::Grid].rgba());
    case SH_ToolTipLabel_Opacity:
        return 255;
    case SH_TabBar_Alignment:
        return Qt::AlignLeft;
    case SH_TabBar_ElideMode:
        return Qt::ElideRight;
    case SH_LineEdit_PasswordCharacter:
        return 0x25CF;
    default:
        break;
    }
    return QProxyStyle::styleHint(hint, option, w, returnData);
}

} // namespace fm::style
