#include "fmstyle/WatercolorStyle.h"

#include "fmstyle/Glyphs.h"
#include "fmstyle/StylePaint.h"

#include "fmstyle/StyleProps.h"
#include "fmstyle/ThemeManager.h"

#include "ColorMath_p.h"
#include "StyleCommon_p.h"
#include "StyleShared_p.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLinearGradient>
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

using namespace Qt::StringLiterals;

namespace fm::style {

using T = Token;
using X = WatercolorChrome;
using namespace detail;

namespace {

// ---------------------------------------------------------------------------------------------
// 크기 — 캔버스 시안2의 CSS 값 (장치 독립 픽셀). 시안1보다 촘촘하되 XP처럼 빽빽하지는 않게.

constexpr int kButtonHeight = 27;
constexpr int kButtonHeightSmall = 24;
constexpr int kButtonMinWidth = 80;
constexpr int kButtonPadX = 12;
constexpr int kButtonPadXSmall = 8;
constexpr int kSegmentPadX = 10;
constexpr int kInputHeight = 25;
constexpr int kInputPadX = 6;
constexpr int kDropButtonWidth = 16;     // 콤보 · 스핀 상자 안쪽 단추
constexpr int kMenuButtonIndicator = 13; // 분할 도구 단추의 메뉴 칸(XP)
constexpr int kToolDropDown = 12;        // 메뉴 달린 도구 단추(분할 아님)의 삼각형 자리
constexpr int kIndicator = 13;           // 체크 상자 · 라디오
constexpr int kSwitchWidth = 52;
constexpr int kSwitchHeight = 22;
constexpr int kSwitchMargin = 3;         // 점선 포커스 자리 (2 px 떨어져 1 px)
constexpr int kSwitchGap = 10;
constexpr int kTabHeight = 26;           // 선택 탭. 나머지는 위에서 3 px 내려 23 px
constexpr int kTabDrop = 3;
constexpr int kTabPadX = 14;
constexpr int kHeaderHeight = 22;
constexpr int kItemMinHeight = 21;
constexpr int kMenuItemHeight = 24;
constexpr int kMenuBarItemHeight = 22;
constexpr int kScrollBarExtent = 17;
constexpr int kProgressHeight = 16;
constexpr int kProgressThin = 12;        // fmSize=small
constexpr int kChunk = 7;                // 진행 블록 7 px + 틈 2 px
constexpr int kChunkGap = 2;
constexpr int kToolButton = 29;
constexpr int kTitleBarHeight = 27;
constexpr int kCaptionButtonW = 21;
constexpr int kCaptionButtonH = 20;
constexpr int kKeyChipGap = 7;            // 글자와 키 칩 사이 (시안2 간격 7)
constexpr int kLinkHeight = 24;           // fmRole=link
constexpr int kHeaderHeightFlat = 28;     // 대화상자 · 설정 표 머리글 — 시안2에서도 평면
// Qt 표준 위젯 — 목업이 없어 XP 컨트롤을 워터컬러 입체 색으로 옮겼다.
constexpr int kTabClose = 16;             // 탭 닫기 단추 자리 (입체 단추는 14)
constexpr int kSliderLength = 11;         // 트랙 막대 손잡이: 진행 방향 11 × 가로지르는 방향 21
constexpr int kSliderThickness = 21;
constexpr int kSliderTrack = 4;           // 들어간 홈 두께
// 도크(07 §4.3) — 제목 줄 21~22(글자 높이 + 위아래 2), 캡션 단추 16(= 2 × 3 + Qt가 줄인 아이콘 10), 분할선 4, 떠 있는 틀 4
constexpr int kDockTitleMargin = 2;
constexpr int kDockButtonMargin = 3;
constexpr int kDockButton = 2 * kDockButtonMargin + 10;
constexpr int kDockSeparator = 4;
constexpr int kDockFrame = 4;
// 세그먼트 높이는 메인 주소 줄만 24이고, 대화상자 · 설정은 시안1 높이를 쓴다(캔버스 워터컬러 보드).
constexpr int kSegmentHeightDialog = 32;
constexpr int kSegmentHeightSettings = 30;
constexpr int kSegmentHeightSmall = 26;
constexpr int kSegmentHeightMini = 20;

int segmentHeight(const QWidget *w)
{
    switch (sizeVariant(w)) {
    case SizeVariant::Mini: return kSegmentHeightMini;
    case SizeVariant::Small: return kSegmentHeightSmall;
    case SizeVariant::Compact: return kButtonHeightSmall;  // 메인 창 주소 줄 24
    case SizeVariant::Normal:
    case SizeVariant::Thin:
    case SizeVariant::Thick: break;
    }
    switch (density(w)) {
    case Density::Dialog: return kSegmentHeightDialog;
    case Density::Settings: return kSegmentHeightSettings;
    case Density::Normal: break;
    }
    return kButtonHeightSmall;
}

int segmentPadX(const QWidget *w)
{
    switch (sizeVariant(w)) {
    case SizeVariant::Mini: return 8;
    case SizeVariant::Small: return 12;
    case SizeVariant::Compact: return 10;
    case SizeVariant::Normal:
    case SizeVariant::Thin:
    case SizeVariant::Thick: break;
    }
    return density(w) == Density::Dialog ? 6 : kSegmentPadX;
}

int progressThickness(const QWidget *w)
{
    const SizeVariant v = sizeVariant(w);
    return (v == SizeVariant::Small || v == SizeVariant::Thin) ? kProgressThin : kProgressHeight;
}

// ---------------------------------------------------------------------------------------------
// 입체 그리기 도우미 — 모두 1 px 선을 정수 좌표에 칠한다 (안티앨리어싱 없음).

void hLine(QPainter *p, int x1, int x2, int y, const QColor &c)
{
    if (x2 >= x1)
        p->fillRect(QRect(x1, y, x2 - x1 + 1, 1), c);
}

void vLine(QPainter *p, int x, int y1, int y2, const QColor &c)
{
    if (y2 >= y1)
        p->fillRect(QRect(x, y1, 1, y2 - y1 + 1), c);
}

void outline(QPainter *p, const QRect &r, const QColor &c)
{
    hLine(p, r.left(), r.right(), r.top(), c);
    hLine(p, r.left(), r.right(), r.bottom(), c);
    vLine(p, r.left(), r.top() + 1, r.bottom() - 1, c);
    vLine(p, r.right(), r.top() + 1, r.bottom() - 1, c);
}

// CSS: inset 1px 0 hi, inset 0 1px hi 아래에 inset -1px 0 lo, inset 0 -1px lo — 오른쪽 · 아래가 위에 온다.
void bevel(QPainter *p, const QRect &r, const QColor &hi, const QColor &lo)
{
    hLine(p, r.left(), r.right(), r.top(), hi);
    vLine(p, r.left(), r.top(), r.bottom(), hi);
    vLine(p, r.right(), r.top(), r.bottom(), lo);
    hLine(p, r.left(), r.right(), r.bottom(), lo);
}

// linear-gradient(to bottom, a 5%, b 20%, b 70%, c 90%)
QLinearGradient vGradient(const QRect &r, const QColor &a, const QColor &b, const QColor &c,
                          qreal s1 = 0.05, qreal s2 = 0.20, qreal s3 = 0.70, qreal s4 = 0.90)
{
    QLinearGradient g(QPointF(0, r.top()), QPointF(0, r.bottom() + 1));
    g.setColorAt(0.0, a);
    g.setColorAt(s1, a);
    g.setColorAt(s2, b);
    g.setColorAt(s3, b);
    g.setColorAt(s4, c);
    g.setColorAt(1.0, c);
    return g;
}

enum class Look { Normal, Hover, Pressed, Disabled };

// 입체 버튼 몸통: 1 px 테두리 + 그라데이션 + 빗면. r은 테두리를 포함한 바깥 사각형.
void raised(QPainter *p, const QRect &r, const X &x, Look look, const QColor &border)
{
    if (r.width() < 2 || r.height() < 2)
        return;
    outline(p, r, border);
    const QRect in = r.adjusted(1, 1, -1, -1);
    switch (look) {
    case Look::Disabled:
        p->fillRect(in, x.g2);
        return;
    case Look::Pressed:
        // linear-gradient(to top, p1 5%, p2 20%, p2 70%, p3 90%) — 빗면 없음
        p->fillRect(in, vGradient(in, x.p3, x.p2, x.p1, 0.10, 0.30, 0.80, 0.95));
        return;
    case Look::Hover:
        p->fillRect(in, vGradient(in, x.h1, x.h2, x.h3));
        bevel(p, in, x.hoverHi, x.hoverLo);
        return;
    case Look::Normal:
        p->fillRect(in, vGradient(in, x.g1, x.g2, x.g3));
        bevel(p, in, x.hi, x.lo);
        return;
    }
}

// 들어간 칸 (입력 · 목록 · 진행 막대 홈): 위 · 왼쪽 짙게, 아래 · 오른쪽 밝게, 안쪽 위 · 왼쪽 선.
void sunken(QPainter *p, const QRect &r, const X &x, const QColor &fill)
{
    if (r.width() < 2 || r.height() < 2)
        return;
    p->fillRect(r, fill);
    hLine(p, r.left(), r.right() - 1, r.top(), x.fieldOuter);
    vLine(p, r.left(), r.top(), r.bottom() - 1, x.fieldOuter);
    hLine(p, r.left(), r.right(), r.bottom(), x.fieldBright);
    vLine(p, r.right(), r.top(), r.bottom(), x.fieldBright);
    hLine(p, r.left() + 1, r.right() - 1, r.top() + 1, x.fieldInner);
    vLine(p, r.left() + 1, r.top() + 1, r.bottom() - 1, x.fieldInner);
}

// 얕게 들어간 칸 (상태 표시줄 칸): 위 · 왼쪽 lo, 아래 · 오른쪽 hi.
void thinSunken(QPainter *p, const QRect &r, const X &x)
{
    hLine(p, r.left(), r.right() - 1, r.top(), x.lo);
    vLine(p, r.left(), r.top(), r.bottom() - 1, x.lo);
    hLine(p, r.left(), r.right(), r.bottom(), x.hi);
    vLine(p, r.right(), r.top(), r.bottom(), x.hi);
}

// Windows 점선 포커스 (1 px 켜고 1 px 끔).
void dottedRect(QPainter *p, const QRect &r, const QColor &c)
{
    if (r.width() < 2 || r.height() < 2)
        return;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);
    QPen pen(c, 1);
    pen.setDashPattern({1.0, 1.0});
    pen.setCapStyle(Qt::FlatCap);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5));
    p->restore();
}

// 채운 삼각형 화살표 (XP 방식). w는 밑변, 방향 쪽 높이는 w/2.
void triangle(QPainter *p, const QPointF &c, qreal w, Qt::ArrowType dir, const QColor &color)
{
    const qreal h = w / 2.0;
    QPolygonF poly;
    switch (dir) {
    case Qt::DownArrow:
        poly << QPointF(c.x() - h, c.y() - h / 2) << QPointF(c.x() + h, c.y() - h / 2)
             << QPointF(c.x(), c.y() + h / 2);
        break;
    case Qt::UpArrow:
        poly << QPointF(c.x() - h, c.y() + h / 2) << QPointF(c.x() + h, c.y() + h / 2)
             << QPointF(c.x(), c.y() - h / 2);
        break;
    case Qt::RightArrow:
        poly << QPointF(c.x() - h / 2, c.y() - h) << QPointF(c.x() - h / 2, c.y() + h)
             << QPointF(c.x() + h / 2, c.y());
        break;
    case Qt::LeftArrow:
        poly << QPointF(c.x() + h / 2, c.y() - h) << QPointF(c.x() + h / 2, c.y() + h)
             << QPointF(c.x() - h / 2, c.y());
        break;
    default:
        return;
    }
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    p->setBrush(color);
    p->drawPolygon(poly);
    p->restore();
}

void checkMark(QPainter *p, const QRectF &box, const QColor &color)
{
    // CSS: 4 × 8 ㄴ자를 40° 돌린 모양
    QPainterPath path;
    path.moveTo(box.left() + box.width() * 0.22, box.top() + box.height() * 0.50);
    path.lineTo(box.left() + box.width() * 0.42, box.top() + box.height() * 0.70);
    path.lineTo(box.left() + box.width() * 0.78, box.top() + box.height() * 0.26);
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, 2.0);
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::MiterJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);
    p->restore();
}

Look lookOf(QStyle::State s, bool on = false)
{
    if (!(s & QStyle::State_Enabled))
        return Look::Disabled;
    if ((s & QStyle::State_Sunken) || on)
        return Look::Pressed;
    if (s & QStyle::State_MouseOver)
        return Look::Hover;
    return Look::Normal;
}

QColor buttonTextColor(QStyle::State s, ButtonRole role, const ThemeColors &tc, const X &x)
{
    if (!(s & QStyle::State_Enabled))
        return x.disFg;
    if (role == ButtonRole::Danger)
        return tc[T::OnDanger];
    if (s & (QStyle::State_Sunken | QStyle::State_On | QStyle::State_MouseOver))
        return x.hoverFg;
    return tc[T::Fg];
}

// 위험 버튼: 붉은 그라데이션, 반투명 빗면.
void dangerPanel(QPainter *p, const QRect &r, const X &x, Look look)
{
    outline(p, r, x.dangerOuter);
    const QRect in = r.adjusted(1, 1, -1, -1);
    QColor a = x.danger1, b = x.danger2, c = x.danger3;
    if (look == Look::Hover) {
        a = mix(a, QColor(0xFF, 0xFF, 0xFF), 0.10);
        b = mix(b, QColor(0xFF, 0xFF, 0xFF), 0.10);
        c = mix(c, QColor(0xFF, 0xFF, 0xFF), 0.10);
    }
    if (look == Look::Pressed) {
        p->fillRect(in, vGradient(in, c, b, a, 0.08, 0.30, 0.75, 0.95));
        return;
    }
    p->fillRect(in, vGradient(in, a, b, c, 0.05, 0.25, 0.70, 0.92));
    bevel(p, in, QColor(255, 255, 255, 89), QColor(0, 0, 0, 56));
}

QColor progressColor(const QStyleOption *opt, const QWidget *w, const ThemeColors &tc)
{
    const QString state = stringProp(opt, w, props::kProgressState);
    if (state == u"paused")
        return tc[T::Paused];
    if (state == u"error")
        return tc[T::Danger];
    return tc[T::Accent];
}

// 제목 표시줄 단추 모양 (최소화 · 최대화 · 복원 · 닫기)
enum class Caption { Minimize, Maximize, Restore, Close };

void captionGlyph(QPainter *p, const QRect &r, Caption kind, const QColor &c)
{
    const QPoint o = r.center();
    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);
    switch (kind) {
    case Caption::Minimize:
        p->fillRect(QRect(o.x() - 4, o.y() + 2, 8, 2), c);
        break;
    case Caption::Maximize:
        p->fillRect(QRect(o.x() - 4, o.y() - 4, 9, 2), c);
        outline(p, QRect(o.x() - 4, o.y() - 4, 9, 9), c);
        break;
    case Caption::Restore:
        p->fillRect(QRect(o.x() - 2, o.y() - 5, 7, 2), c);
        outline(p, QRect(o.x() - 2, o.y() - 5, 7, 6), c);
        p->fillRect(QRect(o.x() - 5, o.y() - 2, 7, 2), c);
        outline(p, QRect(o.x() - 5, o.y() - 2, 7, 7), c);
        break;
    case Caption::Close: {
        p->setRenderHint(QPainter::Antialiasing, true);
        QPen pen(c, 2.0);
        pen.setCapStyle(Qt::FlatCap);
        p->setPen(pen);
        const QPointF m(o.x() + 0.5, o.y() + 0.5);
        p->drawLine(m + QPointF(-3.5, -3.5), m + QPointF(3.5, 3.5));
        p->drawLine(m + QPointF(3.5, -3.5), m + QPointF(-3.5, 3.5));
        break;
    }
    }
    p->restore();
}

// 워터컬러 제목 표시줄 바탕: 가로 그라데이션 + 위 · 아래 선 + 오른쪽 모자이크.
void captionBackground(QPainter *p, const QRect &r, const X &x, bool active, int mosaicRight)
{
    if (!active) {
        p->fillRect(r, x.titleInactive);
        hLine(p, r.left(), r.right(), r.top(), x.titleInactiveHi);
        return;
    }
    // linear-gradient(90deg, edge 0, ttl 2px, ttl calc(100% - 150px), cap calc(100% - 112px))
    const qreal w = std::max(1, r.width());
    QLinearGradient g(QPointF(r.left(), 0), QPointF(r.right() + 1, 0));
    g.setColorAt(0.0, x.titleEdge);
    g.setColorAt(std::min(0.49, 2.0 / w), x.title);
    g.setColorAt(std::clamp((w - 150.0) / w, 0.5, 0.98), x.title);
    g.setColorAt(std::clamp((w - 112.0) / w, 0.51, 0.99), x.titleCap);
    g.setColorAt(1.0, x.titleCap);
    p->fillRect(r, g);
    hLine(p, r.left(), r.right(), r.top(), x.titleHi);
    hLine(p, r.left(), r.right(), r.bottom() - 1, x.titleLo2);
    hLine(p, r.left(), r.right(), r.bottom(), x.titleLo);

    // 모자이크: 30 × 20 안의 4 px 네모 12개
    if (mosaicRight > r.left() + 40) {
        struct Tile { int x, y, c; };
        static constexpr Tile kTiles[] = {{0, 0, 1},  {5, 0, 2},   {15, 0, 3},  {5, 5, 1},
                                          {10, 5, 2}, {20, 5, 3},  {0, 10, 2},  {10, 10, 1},
                                          {15, 10, 3}, {5, 15, 1}, {15, 15, 2}, {25, 15, 3}};
        const int left = mosaicRight - 30;
        const int top = r.top() + (r.height() - 20) / 2;
        for (const Tile &t : kTiles) {
            const QColor &c = t.c == 1 ? x.mosaic1 : t.c == 2 ? x.mosaic2 : x.mosaic3;
            p->fillRect(QRect(left + t.x, top + t.y, 4, 4), c);
        }
    }
}

// 캡션 단추 바탕 · 테두리를 그리고 기호 색을 돌려준다(닫기에 마우스를 올리면 붉은 단추).
QColor captionButtonFace(QPainter *p, const QRect &r, const X &x, bool close, bool active, bool hover, bool pressed)
{
    QColor fill = active ? x.capFill : x.titleInactive;
    QColor line = active ? x.capLine : x.capInactiveLine;
    QColor glyph = active ? x.capGlyph : x.capInactiveGlyph;
    if (close && (hover || pressed)) {
        fill = pressed ? QColor(0xB0, 0x4A, 0x4A) : QColor(0xC9, 0x62, 0x62);
        line = QColor(0xDF, 0xA2, 0xA2);
        glyph = QColor(0xFF, 0xFF, 0xFF);
    } else if (pressed) {
        fill = mix(fill, QColor(0, 0, 0), 0.18);
        line = x.capHoverLine;
    } else if (hover) {
        line = x.capHoverLine;
    }
    p->fillRect(r, fill);
    outline(p, r, line);
    return glyph;
}

void captionButton(QPainter *p, const QRect &r, const X &x, Caption kind, bool active, bool hover,
                   bool pressed)
{
    captionGlyph(p, r, kind, captionButtonFace(p, r, x, kind == Caption::Close, active, hover, pressed));
}

// 새긴 점 세 개(분할 손잡이) — 긴 쪽 가운데에 4 px 간격.
void gripDots(QPainter *p, const QRect &r, const X &x)
{
    const bool vertical = r.width() < r.height();
    const QPoint c = r.center();
    for (int i = -1; i <= 1; ++i) {
        const QPoint d = vertical ? QPoint(c.x() - 1, c.y() + 4 * i - 1) : QPoint(c.x() + 4 * i - 1, c.y() - 1);
        p->fillRect(QRect(d, QSize(2, 2)), x.lo);
        p->fillRect(QRect(d + QPoint(1, 1), QSize(1, 1)), x.hi);
    }
}

} // namespace

// =============================================================================================

WatercolorStyle::WatercolorStyle()
    : QProxyStyle(u"Fusion"_s)
{
    setObjectName(u"WatercolorStyle"_s);
}

WatercolorStyle::~WatercolorStyle() = default;

const ThemeColors &WatercolorStyle::colorsFor(const QWidget *widget)
{
    // 범위에 걸린 색이 다른 디자인 것이면 같은 변형의 워터컬러 색을 쓴다.
    const ThemeColors *scoped = ThemeScope::find(widget);
    if (scoped && scoped->design() == Design::Watercolor)
        return *scoped;
    const ThemeManager &manager = ThemeManager::instance();
    return manager.colors(Design::Watercolor, scoped ? scoped->variant() : manager.effectiveVariant());
}

const WatercolorChrome &WatercolorStyle::chromeFor(const QWidget *widget)
{
    return watercolorChrome(colorsFor(widget).variant());
}

void WatercolorStyle::setAlwaysShowMnemonics(bool on)
{
    if (m_alwaysMnemonics == on)
        return;
    m_alwaysMnemonics = on;
    const auto windows = QApplication::topLevelWidgets();
    for (QWidget *w : windows)
        w->update();
}

QPalette WatercolorStyle::standardPalette() const
{
    const ThemeManager &manager = ThemeManager::instance();
    return manager.colors(Design::Watercolor, manager.effectiveVariant()).toPalette();
}

void WatercolorStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    if (qobject_cast<QAbstractButton *>(widget) || qobject_cast<QComboBox *>(widget)
        || qobject_cast<QAbstractSpinBox *>(widget) || qobject_cast<QLineEdit *>(widget)
        || qobject_cast<QTabBar *>(widget) || qobject_cast<QScrollBar *>(widget)
        || qobject_cast<QHeaderView *>(widget) || qobject_cast<QMenuBar *>(widget)) {
        widget->setAttribute(Qt::WA_Hover, true);
    }
    // 목업: 열 머리글 · 탭 · 도구 설명은 12 px. 앱에서 따로 글꼴을 준 위젯은 건드리지 않는다
    // (스타일이 준 글꼴은 fmStyledFont로 표시해 두어 디자인을 바꿀 때 다시 건다).
    if ((qobject_cast<QHeaderView *>(widget) || qobject_cast<QTabBar *>(widget)
         || widget->inherits("QTipLabel"))
        && (!widget->testAttribute(Qt::WA_SetFont) || boolProp(widget, props::kStyledFont))) {
        QFont f = widget->font();
        f.setPixelSize(12);
        widget->setFont(f);
        widget->setProperty(props::kStyledFont, true);
    }
    polishItemView(widget);
    polishCalendarPart(widget);
    polishDockButton(widget);
}

void WatercolorStyle::unpolish(QWidget *widget)
{
    if (boolProp(widget, props::kStyledFont)) {
        widget->setProperty(props::kStyledFont, QVariant());
        widget->setFont(QFont());
        widget->setAttribute(Qt::WA_SetFont, false);
    }
    unpolishItemView(widget);
    unpolishCalendarPart(widget);
    QProxyStyle::unpolish(widget);
}

void WatercolorStyle::polish(QApplication *app)
{
    QProxyStyle::polish(app);
    app->installEventFilter(this);
}

void WatercolorStyle::unpolish(QApplication *app)
{
    app->removeEventFilter(this);
    QProxyStyle::unpolish(app);
}

void WatercolorStyle::setAltDown(bool down, QObject *source)
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

bool WatercolorStyle::eventFilter(QObject *watched, QEvent *event)
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

void WatercolorStyle::drawButtonPanel(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    if (!segmentOf(w).isEmpty()) {
        drawSegment(option, p, w);
        return;
    }
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const QStyle::State s = option->state;
    const bool enabled = s & State_Enabled;
    const ButtonRole role = buttonRole(w);
    const QRect r = option->rect;
    const auto *b = qstyleoption_cast<const QStyleOptionButton *>(option);
    const bool isDefault = b && (b->features & QStyleOptionButton::DefaultButton);

    if (role == ButtonRole::Link) {
        // 링크 단추: 패널 없음. 키보드 포커스만 점선.
        if (enabled && keyboardFocus(s))
            dottedRect(p, r.adjusted(1, 1, -1, -1), tc[T::Focus]);
        return;
    }
    // 납작한 단추(QPushButton::setFlat)는 은은한 단추처럼 — 마우스 올림 · 누름 · 켬에만 입체
    const bool flat = b && (b->features & QStyleOptionButton::Flat);
    if (role == ButtonRole::Subtle || (flat && role == ButtonRole::Normal)) {
        if (enabled && (s & (State_Sunken | State_On)))
            raised(p, r, x, Look::Pressed, x.out);
        else if (enabled && (s & State_MouseOver))
            raised(p, r, x, Look::Hover, x.out);
        else if (!enabled && (s & State_On))
            raised(p, r, x, Look::Pressed, x.disLine);
    } else if (role == ButtonRole::Danger && enabled) {
        dangerPanel(p, r, x, lookOf(s));
    } else if (!enabled && (s & State_On)) {
        // 켠 토글 · 사용 안 함: 들어간 모양은 남기고 테두리 · 글자만 흐리게
        raised(p, r, x, Look::Pressed, x.disLine);
    } else {
        const Look look = lookOf(s, s.testFlag(State_On));
        // 기본 버튼: 1 px 바깥 테두리를 하나 더 (CSS outline) — Windows처럼 기본 단추(Enter)에도 붙인다.
        if (enabled && (role == ButtonRole::Primary || isDefault)) {
            outline(p, r, x.out);
            raised(p, r.adjusted(1, 1, -1, -1), x, look, x.out);
        } else {
            raised(p, r, x, look, enabled ? x.out : x.disLine);
        }
    }
    if (enabled && keyboardFocus(s))
        dottedRect(p, r.adjusted(3, 3, -3, -3), role == ButtonRole::Danger ? tc[T::OnDanger] : tc[T::Focus]);
}

// 토글 단추 묶음: 조각마다 입체 단추, 이웃과 테두리 1 px를 같이 쓴다 (CSS margin-left: -1px).
void WatercolorStyle::drawSegment(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const QString pos = segmentOf(w);
    const bool first = pos == u"first" || pos == u"only";
    const QStyle::State s = option->state;
    const bool enabled = s & State_Enabled;
    const Look look = lookOf(s, s.testFlag(State_On));
    const QColor border = enabled ? x.out : x.disLine;

    const QRect r = option->rect;
    p->save();
    p->setClipRect(r, Qt::IntersectClip);
    raised(p, first ? r : r.adjusted(-1, 0, 0, 0), x, look, border);
    p->restore();
    if (enabled && keyboardFocus(s))
        dottedRect(p, r.adjusted(first ? 3 : 2, 3, -3, -3), tc[T::Focus]);
}

void WatercolorStyle::drawToolPanel(const QStyleOption *option, QPainter *p, const QWidget *w) const
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
    const X &x = watercolorChrome(tc.variant());
    const QStyle::State s = option->state;
    if (!(s & State_Enabled))
        return;
    if (s & (State_On | State_Sunken))
        raised(p, option->rect, x, Look::Pressed, x.out);
    else if (s & State_MouseOver)
        raised(p, option->rect, x, Look::Hover, x.out);
    if (keyboardFocus(s))
        dottedRect(p, option->rect.adjusted(3, 3, -3, -3), tc[T::Focus]);
}

// 도구 단추: XP 도구 모음처럼 분할이면 두 칸이 각각 입체(마우스 올림이면 둘 다, 눌린 칸만 들어감), 메뉴 달린 단추
// (분할 아님)는 오른쪽에 작은 삼각형 — Qt 기본(오른쪽 아래 작은 화살표) 대신.
void WatercolorStyle::drawToolButton(const QStyleOptionComplex *option, QPainter *p, const QWidget *w) const
{
    const auto *tb = qstyleoption_cast<const QStyleOptionToolButton *>(option);
    if (!tb || !dockButtonKind(w).isEmpty() || !segmentOf(w).isEmpty()) {
        QProxyStyle::drawComplexControl(CC_ToolButton, option, p, w);
        return;
    }
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const ToolButtonParts parts = toolButtonParts(proxy(), tb, w, kToolDropDown);
    const bool enabled = tb->state & State_Enabled;

    QStyleOptionToolButton panel = *tb;
    if (parts.split) {
        panel.rect = parts.button;
        panel.state = parts.buttonState;
        drawToolPanel(&panel, p, w);
        panel.rect = parts.menu;
        panel.state = parts.menuState;
        drawToolPanel(&panel, p, w);
    } else {
        panel.state = parts.buttonState;
        drawToolPanel(&panel, p, w);
    }

    QStyleOptionToolButton label = *tb;
    const int fw = proxy()->pixelMetric(PM_DefaultFrameWidth, option, w);
    label.rect = parts.button.adjusted(fw, fw, -fw, -fw);
    label.state = parts.buttonState;
    proxy()->drawControl(CE_ToolButtonLabel, &label, p, w);

    if (parts.split || parts.dropDown) {
        // 눌린 메뉴 칸은 XP처럼 1 px 내려간다
        const QPointF shift = (parts.menuState & State_Sunken) ? QPointF(1, 1) : QPointF(parts.dropDown ? -1 : 0, 0);
        triangle(p, QRectF(parts.indicator).center() + shift, 6, Qt::DownArrow, enabled ? tc[T::Fg] : x.disFg);
    }
}

void WatercolorStyle::drawFocus(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const ThemeColors &tc = colorsFor(w);
    if (isComboPopupView(w))
        return;  // 콤보 펼친 목록에는 커서 점선이 없다(XP 드롭다운 목록)
    // 목록의 커서: 활성 패널에서만 점선 (선택 행 위에서는 흰 점선)
    if (qobject_cast<const QAbstractItemView *>(w)) {
        if (!paneActive(option, w))
            return;
        dottedRect(p, option->rect, (option->state & State_Selected) ? tc[T::OnAccent] : tc[T::Focus]);
        return;
    }
    if (!keyboardFocus(option->state))
        return;
    // 버튼 종류는 패널에서 이미 그렸다
    if (qobject_cast<const QAbstractButton *>(w) && !qobject_cast<const QCheckBox *>(w)
        && !qobject_cast<const QRadioButton *>(w))
        return;
    if (isSwitch(w)) {
        dottedRect(p, option->rect.adjusted(kSwitchMargin - 2, kSwitchMargin - 2, 2 - kSwitchMargin,
                                            2 - kSwitchMargin),
                   tc[T::Focus]);
        return;
    }
    dottedRect(p, option->rect, tc[T::Focus]);
}

// ---------------------------------------------------------------------------------------------
// 탭 · 머리글

namespace {

// 위쪽 탭 하나. r은 원점에서 시작하는 위쪽 기준 사각형(세로 0이 바깥).
void drawNorthTab(QPainter *p, const QRect &r, const QStyleOptionTab &tab, bool hover, bool paneActive,
                  const ThemeColors &tc, const X &x)
{
    if (tab.state & QStyle::State_Selected) {
        // 선택 탭: 창 바탕색, 26 px, 아래 선을 덮어 내용과 이어진다.
        p->fillRect(r, tc[T::Win]);
        hLine(p, r.left(), r.right(), r.top(), x.tabLine);
        vLine(p, r.left(), r.top(), r.bottom(), x.tabLine);
        vLine(p, r.right(), r.top(), r.bottom(), x.tabLine);
        const QRect in = r.adjusted(1, 1, -1, 0);
        vLine(p, in.left(), in.top(), in.bottom(), x.tabHi);
        vLine(p, in.right(), in.top(), in.bottom(), x.tabHi);
        // 활성 패널: 안쪽 위 2 px 강조색 (목업 .pane.on .tab.is-on)
        if (paneActive)
            p->fillRect(QRect(in.left(), in.top(), in.width(), 2), tc[T::Accent]);
        else
            hLine(p, in.left(), in.right(), in.top(), x.tabHi);
        return;
    }
    // 나머지: 3 px 내려 23 px. 맨 아래 한 줄은 탭 줄의 밑선 자리.
    const QRect box = r.adjusted(0, kTabDrop, 0, -1);
    p->fillRect(box, hover ? x.tabHover : x.tab);
    hLine(p, box.left(), box.right(), box.top(), x.tabLine);
    // 이웃과 테두리를 겹친다 (CSS margin-right: -1px): 앞 탭이 선택이면 왼쪽 선은 선택 탭이 그린다.
    if (tab.selectedPosition != QStyleOptionTab::PreviousIsSelected)
        vLine(p, box.left(), box.top(), box.bottom(), x.tabLine);
    if (tab.position == QStyleOptionTab::End || tab.position == QStyleOptionTab::OnlyOneTab)
        vLine(p, box.right(), box.top(), box.bottom(), x.tabLine);
}

} // namespace

void WatercolorStyle::drawTabShape(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
    if (!tab)
        return;
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const bool hover = (tab->state & State_MouseOver) && (tab->state & State_Enabled);
    // 위쪽 탭 모양으로 그리고 아래 · 왼쪽 · 오른쪽 탭은 좌표를 뒤집거나 돌린다(바깥 쪽으로 내려앉는다).
    const TabSide side = tabSide(tab->shape);
    p->save();
    p->setTransform(tabTransform(side, tab->rect), true);
    drawNorthTab(p, QRect(QPoint(0, 0), tabLocalSize(side, tab->rect)), *tab, hover, paneActiveProperty(w), tc, x);
    p->restore();
}

void WatercolorStyle::drawHeaderSection(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(option);
    if (!h)
        return;
    const X &x = chromeFor(w);
    const bool pressed = h->state & State_Sunken;
    const bool hover = (h->state & State_MouseOver) && (h->state & State_Enabled);
    const QRect r = h->rect;
    if (flatHeader(w)) {
        // 대화상자 · 설정 표 머리글은 입체가 아니다 — --head 바탕 · 아래 --line · 칸 사이 --grid
        const ThemeColors &tc = colorsFor(w);
        QColor bg = tc[T::Head];
        if (pressed)
            bg = mix(bg, tc[T::Fg], 0.08);
        else if (hover)
            bg = mix(bg, tc[T::Fg], 0.04);
        p->fillRect(r, bg);
        if (h->orientation == Qt::Horizontal) {
            hLine(p, r.left(), r.right(), r.bottom(), tc[T::Line]);
            if (h->position != QStyleOptionHeader::End && h->position != QStyleOptionHeader::OnlyOneSection)
                vLine(p, r.right(), r.top(), r.bottom() - 1, tc[T::Grid]);
        } else {
            vLine(p, r.right(), r.top(), r.bottom(), tc[T::Line]);
        }
        return;
    }
    p->fillRect(r, pressed ? x.g3 : hover ? x.g1 : x.g2);
    // 오른쪽 · 아래 1 px 테두리, 안쪽 빗면 (CSS .hc)
    vLine(p, r.right(), r.top(), r.bottom(), x.out);
    hLine(p, r.left(), r.right(), r.bottom(), x.out);
    if (!pressed)
        bevel(p, r.adjusted(0, 0, -1, -1), x.hi, x.lo);
}

// ---------------------------------------------------------------------------------------------
// 메뉴

void WatercolorStyle::drawMenuBarItem(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if (!mi)
        return;
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const bool enabled = mi->state & State_Enabled;
    const bool lit = enabled && (mi->state & (State_Selected | State_Sunken));
    const QRect r = mi->rect;
    if (lit) {
        const int top = r.top() + (r.height() - kMenuBarItemHeight) / 2;
        p->fillRect(QRect(r.left(), top, r.width(), kMenuBarItemHeight), tc[T::Accent]);
    }
    int flags = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
    if (!proxy()->styleHint(SH_UnderlineShortcut, mi, w))
        flags |= Qt::TextHideMnemonic;
    p->save();
    p->setPen(!enabled ? x.disFg : lit ? tc[T::OnAccent] : tc[T::Fg]);
    p->drawText(r, flags, mi->text);
    p->restore();
}

void WatercolorStyle::drawMenuItem(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if (!mi)
        return;
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const QRect r = mi->rect;

    if (mi->menuItemType == QStyleOptionMenuItem::Separator) {
        // 새긴 선 2 px (위 lo · 아래 hi), 위아래 4 px
        const int y = r.top() + (r.height() - 2) / 2;
        hLine(p, r.left() + 3, r.right() - 3, y, x.lo);
        hLine(p, r.left() + 3, r.right() - 3, y + 1, x.hi);
        return;
    }
    if (mi->menuItemType == QStyleOptionMenuItem::EmptyArea)
        return;

    const bool enabled = mi->state & State_Enabled;
    const bool selected = (mi->state & State_Selected) && enabled;
    if (selected) {
        p->fillRect(r, x.menuHover);
        outline(p, r, x.menuHoverLine);
    }
    const QColor fg = enabled ? tc[T::Fg] : x.disFg;

    // 체크 · 아이콘 열 (왼쪽 4 px 뒤 20 px)
    const QRect column(r.left() + 4, r.top(), 20, r.height());
    const bool checkable = mi->checkType != QStyleOptionMenuItem::NotCheckable;
    if (checkable && mi->checked) {
        if (mi->checkType == QStyleOptionMenuItem::Exclusive) {
            p->save();
            p->setRenderHint(QPainter::Antialiasing, true);
            p->setPen(Qt::NoPen);
            p->setBrush(fg);
            p->drawEllipse(QRectF(column).center(), 2.5, 2.5);
            p->restore();
        } else {
            QRectF box(0, 0, 11, 11);
            box.moveCenter(QRectF(column).center());
            checkMark(p, box, fg);
        }
    } else if (!mi->icon.isNull()) {
        const QIcon::Mode mode = enabled ? QIcon::Normal : QIcon::Disabled;
        mi->icon.paint(p, QRect(column.center().x() - 8, column.center().y() - 8, 16, 16),
                       Qt::AlignCenter, mode);
    }

    QString text = mi->text;
    QString shortcut;
    if (const qsizetype tab = text.indexOf(u'\t'); tab >= 0) {
        shortcut = text.mid(tab + 1);
        text = text.left(tab);
    }
    const bool subMenu = mi->menuItemType == QStyleOptionMenuItem::SubMenu;
    const int left = column.right() + 1 + 6;
    const QRect textRect(left, r.top(), r.right() - 14 - left - (subMenu ? 10 : 0), r.height());

    int flags = Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine | Qt::TextShowMnemonic;
    if (!proxy()->styleHint(SH_UnderlineShortcut, mi, w))
        flags |= Qt::TextHideMnemonic;

    p->save();
    p->setFont(mi->font);
    p->setPen(fg);
    p->drawText(textRect, flags, text);
    if (!shortcut.isEmpty()) {
        p->setPen(enabled ? withAlpha(tc[T::Fg], 0.7) : x.disFg);
        p->drawText(textRect, Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine, shortcut);
    }
    p->restore();

    if (subMenu)
        triangle(p, QPointF(r.right() - 8.5, QRectF(r).center().y()), 7, Qt::RightArrow, fg);
}

// ---------------------------------------------------------------------------------------------
// 진행 막대 · 틀

void WatercolorStyle::drawProgress(ControlElement element, const QStyleOption *option, QPainter *p,
                                   const QWidget *w) const
{
    const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(option);
    if (!pb)
        return;
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const bool horizontal = pb->state & State_Horizontal;
    const QRect g = pb->rect;

    if (element == CE_ProgressBarGroove) {
        sunken(p, g, x, x.trough);
        return;
    }
    if (element == CE_ProgressBarLabel) {
        if (!pb->textVisible || pb->text.isEmpty())
            return;
        p->save();
        p->setPen((pb->state & State_Enabled) ? tc[T::Fg2] : x.disFg);
        p->drawText(pb->rect, Qt::AlignVCenter | Qt::AlignRight, pb->text);
        p->restore();
        return;
    }

    // CE_ProgressBarContents — 테두리 1 px + 안쪽 여백(보통 2 px, 얇으면 1 px) 안에 7 px 블록
    const int pad = isSmall(w) ? 1 : 2;
    const QRect area = g.adjusted(1 + pad, 1 + pad, -1 - pad, -1 - pad);
    if (area.isEmpty())
        return;
    const QColor color = (pb->state & State_Enabled) ? progressColor(pb, w, tc) : x.disLine;
    const int length = horizontal ? area.width() : area.height();

    int from = 0;
    int to = 0;
    if (pb->minimum == pb->maximum) {
        // 진행률을 알 수 없음 — 가운데에 블록 세 개 (움직임은 다음 단계)
        const int span = 3 * (kChunk + kChunkGap) - kChunkGap;
        from = std::max(0, (length - span) / 2);
        to = std::min(length, from + span);
    } else {
        const double range = double(pb->maximum) - double(pb->minimum);
        const double fraction = std::clamp((double(pb->progress) - double(pb->minimum)) / range, 0.0, 1.0);
        to = int(std::lround(length * fraction));
    }
    if (to <= from)
        return;

    const bool reversed = horizontal ? (pb->invertedAppearance != (pb->direction == Qt::RightToLeft))
                                     : (pb->invertedAppearance == pb->bottomToTop);
    p->save();
    p->setClipRect(area, Qt::IntersectClip);
    for (int at = from; at < to; at += kChunk + kChunkGap) {
        const int size = std::min(kChunk, to - at);
        QRect block;
        if (horizontal) {
            block = reversed ? QRect(area.right() - at - size + 1, area.top(), size, area.height())
                             : QRect(area.left() + at, area.top(), size, area.height());
        } else {
            block = reversed ? QRect(area.left(), area.top() + at, area.width(), size)
                             : QRect(area.left(), area.bottom() - at - size + 1, area.width(), size);
        }
        p->fillRect(block, color);
    }
    p->restore();
}

void WatercolorStyle::drawShapedFrame(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(option);
    if (!f)
        return;
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const QRect r = f->rect;
    if (isFooter(w)) {
        // 대화상자 버튼 영역 — 시안2는 위 선이 없고 --foot = --win이라 본문과 이어진다.
        p->fillRect(r, tc[T::Foot]);
        return;
    }
    if (boolProp(w, props::kCard)) {
        p->fillRect(r, tc[T::Surface]);
        outline(p, r, tc[T::Line]);
        return;
    }
    const bool sunkenShadow = f->state & State_Sunken;
    const bool raisedShadow = f->state & State_Raised;
    switch (f->frameShape) {
    case QFrame::NoFrame:
        return;
    case QFrame::HLine: {
        const int y = r.top() + (r.height() - (sunkenShadow || raisedShadow ? 2 : 1)) / 2;
        if (sunkenShadow || raisedShadow) {
            hLine(p, r.left(), r.right(), y, sunkenShadow ? x.lo : x.hi);
            hLine(p, r.left(), r.right(), y + 1, sunkenShadow ? x.hi : x.lo);
        } else {
            hLine(p, r.left(), r.right(), y, tc[T::Line]);
        }
        return;
    }
    case QFrame::VLine: {
        const int xx = r.left() + (r.width() - (sunkenShadow || raisedShadow ? 2 : 1)) / 2;
        if (sunkenShadow || raisedShadow) {
            vLine(p, xx, r.top(), r.bottom(), sunkenShadow ? x.lo : x.hi);
            vLine(p, xx + 1, r.top(), r.bottom(), sunkenShadow ? x.hi : x.lo);
        } else {
            vLine(p, xx, r.top(), r.bottom(), tc[T::Line]);
        }
        return;
    }
    case QFrame::Box:
        if (sunkenShadow || raisedShadow) {
            // 새긴 테두리
            outline(p, r.adjusted(0, 0, -1, -1), sunkenShadow ? x.lo : x.hi);
            outline(p, r.adjusted(1, 1, 0, 0), sunkenShadow ? x.hi : x.lo);
        } else {
            outline(p, r, tc[T::Line]);
        }
        return;
    default:
        if (sunkenShadow) {
            // 목록 · 편집기 틀: 입력 칸과 같은 들어간 테두리. 오른쪽 · 아래 안쪽 1 px는 바탕색으로 메운다.
            const QColor base = f->palette.color(QPalette::Base);
            hLine(p, r.left() + 1, r.right() - 1, r.bottom() - 1, base);
            vLine(p, r.right() - 1, r.top() + 1, r.bottom() - 1, base);
            hLine(p, r.left(), r.right() - 1, r.top(), x.fieldOuter);
            vLine(p, r.left(), r.top(), r.bottom() - 1, x.fieldOuter);
            hLine(p, r.left(), r.right(), r.bottom(), x.fieldBright);
            vLine(p, r.right(), r.top(), r.bottom(), x.fieldBright);
            hLine(p, r.left() + 1, r.right() - 1, r.top() + 1, x.fieldInner);
            vLine(p, r.left() + 1, r.top() + 1, r.bottom() - 1, x.fieldInner);
        } else if (raisedShadow) {
            bevel(p, r, x.hi, x.lo);
        } else if (f->lineWidth > 0) {
            outline(p, r, tc[T::Line]);
        }
        return;
    }
}

// ---------------------------------------------------------------------------------------------
// 스크롤 막대 — 17 px, 양 끝 화살표 단추, 흰 손잡이에 잡는 줄 네 개

void WatercolorStyle::drawScrollBar(const QStyleOptionComplex *option, QPainter *p, const QWidget *w) const
{
    const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(option);
    if (!sb)
        return;
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const bool horizontal = sb->orientation == Qt::Horizontal;
    const bool enabled = (sb->state & State_Enabled) && sb->minimum != sb->maximum;
    const bool sunkenState = sb->state & State_Sunken;

    p->fillRect(sb->rect, x.sbTrack);

    // 누르고 있는 쪽 페이지 영역은 조금 어둡게
    for (const SubControl page : {SC_ScrollBarSubPage, SC_ScrollBarAddPage}) {
        if (enabled && sunkenState && (sb->activeSubControls & page))
            p->fillRect(proxy()->subControlRect(CC_ScrollBar, sb, page, w), mix(x.sbTrack, x.lo, 0.35));
    }

    for (const SubControl arrow : {SC_ScrollBarSubLine, SC_ScrollBarAddLine}) {
        const QRect r = proxy()->subControlRect(CC_ScrollBar, sb, arrow, w);
        if (r.isEmpty())
            continue;
        const bool active = enabled && (sb->activeSubControls & arrow);
        if (active && sunkenState)
            raised(p, r, x, Look::Pressed, x.sbThumbLine);
        else if (active && (sb->state & State_MouseOver))
            raised(p, r, x, Look::Hover, x.sbThumbLine);
        const Qt::ArrowType dir = horizontal ? (arrow == SC_ScrollBarSubLine ? Qt::LeftArrow : Qt::RightArrow)
                                             : (arrow == SC_ScrollBarSubLine ? Qt::UpArrow : Qt::DownArrow);
        triangle(p, QRectF(r).center(), 7, dir, enabled ? x.sbArrow : x.disFg);
    }

    const QRect slider = proxy()->subControlRect(CC_ScrollBar, sb, SC_ScrollBarSlider, w);
    if (!enabled || slider.isEmpty())
        return;
    const bool active = sb->activeSubControls & SC_ScrollBarSlider;
    const bool pressed = active && sunkenState;
    const bool hover = active && (sb->state & State_MouseOver);
    p->fillRect(slider, pressed ? x.g3 : x.sbThumb);
    outline(p, slider, hover || pressed ? x.h3 : x.sbThumbLine);

    // 잡는 줄: 1 px 줄 네 개 (7 × 6)
    const QPoint c = slider.center();
    if (horizontal && slider.width() >= 14) {
        for (int i = 0; i < 4; ++i)
            vLine(p, c.x() - 3 + 2 * i, c.y() - 2, c.y() + 3, x.sbGrip);
    } else if (!horizontal && slider.height() >= 14) {
        for (int i = 0; i < 4; ++i)
            hLine(p, c.x() - 2, c.x() + 3, c.y() - 3 + 2 * i, x.sbGrip);
    }
}

// ---------------------------------------------------------------------------------------------
// MDI 창의 제목 표시줄

void WatercolorStyle::drawTitleBar(const QStyleOptionComplex *option, QPainter *p, const QWidget *w) const
{
    const auto *tb = qstyleoption_cast<const QStyleOptionTitleBar *>(option);
    if (!tb)
        return;
    const X &x = chromeFor(w);
    const bool active = tb->state & State_Active;
    const QRect label = proxy()->subControlRect(CC_TitleBar, tb, SC_TitleBarLabel, w);

    captionBackground(p, tb->rect, x, active, label.right() - 5);

    if ((tb->subControls & SC_TitleBarSysMenu) && !tb->icon.isNull()) {
        const QRect ir = proxy()->subControlRect(CC_TitleBar, tb, SC_TitleBarSysMenu, w);
        tb->icon.paint(p, ir, Qt::AlignCenter, active ? QIcon::Normal : QIcon::Disabled);
    }
    if ((tb->subControls & SC_TitleBarLabel) && !tb->text.isEmpty()) {
        p->save();
        QFont f = p->font();
        f.setPixelSize(12);
        f.setWeight(QFont::Bold);
        p->setFont(f);
        const QRect tr = label.adjusted(0, 0, active ? -44 : 0, 0);
        const QString text = QFontMetrics(f).elidedText(tb->text, Qt::ElideRight, tr.width());
        p->setPen(QColor(0, 0, 0, active ? 115 : 64));
        p->drawText(tr.translated(1, 1), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);
        p->setPen(active ? QColor(0xFF, 0xFF, 0xFF) : QColor(0xF8, 0xF8, 0xF8));
        p->drawText(tr, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);
        p->restore();
    }

    struct Btn { SubControl sc; Caption kind; };
    const Btn buttons[] = {{SC_TitleBarMinButton, Caption::Minimize},
                           {SC_TitleBarMaxButton, Caption::Maximize},
                           {SC_TitleBarNormalButton, Caption::Restore},
                           {SC_TitleBarCloseButton, Caption::Close}};
    for (const Btn &b : buttons) {
        if (!(tb->subControls & b.sc))
            continue;
        const QRect r = proxy()->subControlRect(CC_TitleBar, tb, b.sc, w);
        if (r.isEmpty())
            continue;
        const bool on = tb->activeSubControls & b.sc;
        captionButton(p, r, x, b.kind, active, on && (tb->state & State_MouseOver),
                      on && (tb->state & State_Sunken));
    }
}

// 최대화한 MDI 창이 메뉴 막대에 두는 단추 묶음 — 제목 표시줄 단추와 같은 파란 입체 단추.
void WatercolorStyle::drawMdiControls(const QStyleOptionComplex *option, QPainter *p, const QWidget *w) const
{
    const X &x = chromeFor(w);
    const bool enabled = option->state & State_Enabled;
    struct Btn { SubControl sc; Caption kind; };
    const Btn buttons[] = {{SC_MdiMinButton, Caption::Minimize},
                           {SC_MdiNormalButton, Caption::Restore},
                           {SC_MdiCloseButton, Caption::Close}};
    for (const Btn &b : buttons) {
        if (!(option->subControls & b.sc))
            continue;
        const QRect r = proxy()->subControlRect(CC_MdiControls, option, b.sc, w);
        if (r.isEmpty())
            continue;
        const bool on = enabled && (option->activeSubControls & b.sc);
        captionButton(p, r, x, b.kind, enabled, on && (option->state & State_MouseOver),
                      on && (option->state & State_Sunken));
    }
}

// 도크 제목 줄(07 §4.3): MDI 제목 표시줄의 축소판 — 활성은 파란 그라데이션(모자이크 없음), 비활성은 흐린 띠,
// 굵은 12 px 흰 제목 + 그림자. 세로 제목 줄은 돌려 그린다(글자는 아래에서 위로).
void WatercolorStyle::drawDockTitle(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const auto *dw = qstyleoption_cast<const QStyleOptionDockWidget *>(option);
    if (!dw)
        return;
    const X &x = chromeFor(w);
    const bool active = dockActive(w);
    QRect r = dw->rect;
    QRect text = proxy()->subElementRect(SE_DockWidgetTitleBarText, dw, w);
    p->save();
    if (dw->verticalTitleBar) {
        text = QRect(r.bottom() - text.bottom(), text.left() - r.left(), text.height(), text.width());
        p->translate(r.left(), r.bottom() + 1);
        p->rotate(-90);
        r = QRect(0, 0, r.height(), r.width());
    }
    if (active) {
        // 제목 표시줄 색을 왼쪽 ttl → 오른쪽 cap으로 고르게(좁은 도크에서 캡션 단추 자리 끊김이 생기지 않게)
        QLinearGradient g(QPointF(r.left(), 0), QPointF(r.right() + 1, 0));
        g.setColorAt(0.0, x.title);
        g.setColorAt(1.0, x.titleCap);
        p->fillRect(r, g);
        hLine(p, r.left(), r.right(), r.top(), x.titleHi);
        hLine(p, r.left(), r.right(), r.bottom() - 1, x.titleLo2);
        hLine(p, r.left(), r.right(), r.bottom(), x.titleLo);
    } else {
        captionBackground(p, r, x, false, r.left());
    }
    if (!dw->title.isEmpty() && text.width() > 0) {
        QFont f = p->font();
        f.setPixelSize(12);
        f.setWeight(QFont::Bold);
        p->setFont(f);
        const QString title = QFontMetrics(f).elidedText(dw->title, Qt::ElideRight, text.width());
        const int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextHideMnemonic;
        p->setPen(QColor(0, 0, 0, active ? 115 : 64));
        p->drawText(text.translated(1, 1), flags, title);
        p->setPen(active ? QColor(0xFF, 0xFF, 0xFF) : QColor(0xF8, 0xF8, 0xF8));
        p->drawText(text, flags, title);
    }
    p->restore();
}

// 도크 제목 줄 단추: 16 × 16 캡션 단추(제목 표시줄 단추와 같은 색 · 닫기 올림은 붉게). 닫기 · 떼어 내기는 캡션 기호,
// 압정 · 메뉴는 굵은 선 기호.
void WatercolorStyle::drawDockButton(const QStyleOption *option, QPainter *p, const QWidget *w) const
{
    const X &x = chromeFor(w);
    const QStyle::State s = option->state;
    const bool enabled = s & State_Enabled;
    const bool pressed = enabled && (s & (State_Sunken | State_On));
    const bool hover = enabled && (s & (State_Raised | State_MouseOver));
    const QString kind = dockButtonKind(w);
    const QRect r = option->rect;
    const QColor glyph = captionButtonFace(p, r, x, kind == u"close", dockActive(w) && enabled, hover, pressed);
    if (kind == u"close")
        captionGlyph(p, r, Caption::Close, glyph);
    else if (kind == u"float")
        captionGlyph(p, r, Caption::Restore, glyph);
    else
        paintChromeGlyph(p, dockButtonGlyph(kind), QRectF(r), glyph, true);
}

// ---------------------------------------------------------------------------------------------
// Qt 표준 위젯 — 목업이 없어 XP 컨트롤을 워터컬러 입체 색으로 옮겼다.

// 도구 상자: 입체 머리(열린 칸은 굵은 글자), 왼쪽 채운 삼각형(열림 = 아래).
void WatercolorStyle::drawToolBoxTab(ControlElement element, const QStyleOption *option, QPainter *p,
                                     const QWidget *w) const
{
    const auto *tb = qstyleoption_cast<const QStyleOptionToolBox *>(option);
    if (!tb)
        return;
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const QStyle::State s = tb->state;
    const bool enabled = s & State_Enabled;
    const bool selected = s & State_Selected;
    const QRect r = tb->rect;

    if (element == CE_ToolBoxTabShape) {
        raised(p, r, x, lookOf(s), enabled ? x.out : x.disLine);
        if (enabled && keyboardFocus(s))
            dottedRect(p, r.adjusted(3, 3, -3, -3), tc[T::Focus]);
        return;
    }

    const QColor fg = buttonTextColor(s, ButtonRole::Normal, tc, x);
    const QRect arrow = visualRect(tb->direction, r, QRect(r.left() + 6, r.top(), 12, r.height()));
    const Qt::ArrowType closed = tb->direction == Qt::RightToLeft ? Qt::LeftArrow : Qt::RightArrow;
    triangle(p, QRectF(arrow).center(), 7, selected ? Qt::DownArrow : closed, fg);
    int left = r.left() + 24;
    if (!tb->icon.isNull()) {
        const int icon = proxy()->pixelMetric(PM_SmallIconSize, tb, w);
        const QRect ir = visualRect(tb->direction, r, QRect(left, r.top() + (r.height() - icon) / 2, icon, icon));
        tb->icon.paint(p, ir, Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled);
        left += icon + 5;
    }
    const QRect textRect = visualRect(tb->direction, r, QRect(left, r.top(), r.right() - 6 - left, r.height()));
    int flags = Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextShowMnemonic
              | (tb->direction == Qt::RightToLeft ? Qt::AlignRight : Qt::AlignLeft);
    if (!proxy()->styleHint(SH_UnderlineShortcut, tb, w))
        flags |= Qt::TextHideMnemonic;
    p->save();
    QFont f = p->font();
    if (selected)
        f.setWeight(QFont::Bold);
    p->setFont(f);
    p->setPen(fg);
    p->drawText(textRect, flags, QFontMetrics(f).elidedText(tb->text, Qt::ElideRight, textRect.width()));
    p->restore();
}

// 트랙 막대(XP): 들어간 4 px 홈, 11 × 21 입체 손잡이, 눈금은 보조 글자색. 키보드 포커스는 전체 점선.
void WatercolorStyle::drawSlider(const QStyleOptionComplex *option, QPainter *p, const QWidget *w) const
{
    const auto *sl = qstyleoption_cast<const QStyleOptionSlider *>(option);
    if (!sl)
        return;
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const bool enabled = sl->state & State_Enabled;
    const bool horizontal = sl->orientation == Qt::Horizontal;
    const QRect handle = proxy()->subControlRect(CC_Slider, sl, SC_SliderHandle, w);
    const int handleLength = horizontal ? handle.width() : handle.height();
    const QRect r = sl->rect;

    if ((sl->subControls & SC_SliderTickmarks) && sl->tickPosition != QSlider::NoTicks) {
        const QColor tick = enabled ? tc[T::Fg2] : x.disFg;
        const bool mirrored = horizontal && sl->direction == Qt::RightToLeft;
        for (const int pos : sliderTickPositions(sl, handleLength)) {
            if (horizontal) {
                const int xx = mirrored ? r.right() - pos : r.left() + pos;
                if (sl->tickPosition & QSlider::TicksAbove)
                    vLine(p, xx, handle.top() - 4, handle.top() - 2, tick);
                if (sl->tickPosition & QSlider::TicksBelow)
                    vLine(p, xx, handle.bottom() + 2, handle.bottom() + 4, tick);
            } else {
                const int y = r.top() + pos;
                if (sl->tickPosition & QSlider::TicksLeft)
                    hLine(p, handle.left() - 4, handle.left() - 2, y, tick);
                if (sl->tickPosition & QSlider::TicksRight)
                    hLine(p, handle.right() + 2, handle.right() + 4, y, tick);
            }
        }
    }

    // 홈: 손잡이 가운데가 움직이는 구간보다 양쪽으로 2 px 더
    const int inset = handleLength / 2 - 2;
    const QRect groove = horizontal
        ? QRect(r.left() + inset, handle.top() + (handle.height() - kSliderTrack) / 2, r.width() - 2 * inset, kSliderTrack)
        : QRect(handle.left() + (handle.width() - kSliderTrack) / 2, r.top() + inset, kSliderTrack, r.height() - 2 * inset);
    sunken(p, groove, x, enabled ? x.trough : tc[T::Win]);

    const bool active = sl->activeSubControls & SC_SliderHandle;
    const Look look = !enabled                                      ? Look::Disabled
                    : active && (sl->state & State_Sunken)          ? Look::Pressed
                    : active && (sl->state & State_MouseOver)       ? Look::Hover
                                                                    : Look::Normal;
    raised(p, handle, x, look, enabled ? x.out : x.disLine);
    if (enabled && keyboardFocus(sl->state))
        dottedRect(p, r, tc[T::Focus]);
}

// 다이얼: 위가 밝은 입체 원 손잡이 + 값 위치의 강조색 점, 바깥 눈금. 키보드 포커스는 점선.
void WatercolorStyle::drawDial(const QStyleOptionComplex *option, QPainter *p, const QWidget *w) const
{
    const auto *d = qstyleoption_cast<const QStyleOptionSlider *>(option);
    if (!d)
        return;
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());
    const bool enabled = d->state & State_Enabled;
    const bool hover = enabled && (d->state & State_MouseOver);
    const QRectF r(d->rect);
    const QPointF c = r.center();
    const bool notches = d->subControls & SC_DialTickmarks;
    const qreal outer = std::min(r.width(), r.height()) / 2.0 - 1.0;
    const qreal knob = outer - (notches ? 6.0 : 1.0);
    if (knob < 5.0)
        return;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    if (notches) {
        p->setPen(QPen(enabled ? tc[T::Fg2] : x.disFg, 1.0));
        for (const DialNotch &n : dialNotches(d)) {
            const QPointF dir(std::cos(n.angle), -std::sin(n.angle));
            p->drawLine(c + dir * (outer - (n.major ? 4.0 : 2.0)), c + dir * outer);
        }
    }
    const QRectF body(c.x() - knob + 0.5, c.y() - knob + 0.5, 2 * knob - 1, 2 * knob - 1);
    QLinearGradient g(body.topLeft(), body.bottomLeft());
    if (enabled) {
        g.setColorAt(0.0, hover ? x.h1 : x.g1);
        g.setColorAt(0.5, hover ? x.h2 : x.g2);
        g.setColorAt(1.0, hover ? x.h3 : x.g3);
    } else {
        g.setColorAt(0.0, x.g2);
        g.setColorAt(1.0, x.g2);
    }
    p->setPen(QPen(enabled ? x.out : x.disLine, 1.0));
    p->setBrush(g);
    p->drawEllipse(body);
    // 빗면: 위 · 왼쪽 반원은 밝게, 아래 · 오른쪽 반원은 어둡게
    const QRectF bevelRect = body.adjusted(1, 1, -1, -1);
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(hover ? x.hoverHi : x.hi, 1.0));
    p->drawArc(bevelRect, 45 * 16, 180 * 16);
    p->setPen(QPen(hover ? x.hoverLo : x.lo, 1.0));
    p->drawArc(bevelRect, 225 * 16, 180 * 16);
    // 값 위치 표시: 들어간 점
    const qreal angle = dialAngle(d);
    const QPointF dot = c + QPointF(std::cos(angle), -std::sin(angle)) * (knob * 0.62);
    p->setPen(QPen(x.lo, 1.0));
    p->setBrush(enabled ? tc[T::Accent] : x.disFg);
    p->drawEllipse(dot, 3.0, 3.0);
    p->restore();
    if (enabled && keyboardFocus(d->state))
        dottedRect(p, d->rect, tc[T::Focus]);
}

// ---------------------------------------------------------------------------------------------
// 체크 상자 · 라디오 · 스위치 · 입력 칸

namespace {

// 13 × 13 클래식 체크 상자: 바깥 위·왼 co / 아래·오른 cb2, 안쪽 위·왼 ci / 아래·오른 cl.
void drawCheckIndicator(QPainter *p, const QRect &rect, QStyle::State s, const ThemeColors &tc, const X &x)
{
    const bool enabled = s & QStyle::State_Enabled;
    const bool pressed = s & QStyle::State_Sunken;
    QRect b(0, 0, kIndicator, kIndicator);
    b.moveCenter(rect.center());
    p->fillRect(b, !enabled ? tc[T::Win] : pressed ? x.g3 : tc[T::Field]);
    hLine(p, b.left(), b.right() - 1, b.top(), x.checkOuter);
    vLine(p, b.left(), b.top(), b.bottom() - 1, x.checkOuter);
    hLine(p, b.left(), b.right(), b.bottom(), x.checkBright);
    vLine(p, b.right(), b.top(), b.bottom(), x.checkBright);
    const QRect in = b.adjusted(1, 1, -1, -1);
    hLine(p, in.left(), in.right() - 1, in.top(), x.checkInner);
    vLine(p, in.left(), in.top(), in.bottom() - 1, x.checkInner);
    hLine(p, in.left(), in.right(), in.bottom(), x.checkLight);
    vLine(p, in.right(), in.top(), in.bottom(), x.checkLight);

    const QColor mark = enabled ? tc[T::Fg] : x.disFg;
    if (s & QStyle::State_NoChange)
        p->fillRect(QRect(b.left() + 4, b.top() + 4, 5, 5), withAlpha(mark, 0.6));
    else if (s & QStyle::State_On)
        checkMark(p, QRectF(in).adjusted(0.5, 0.5, -0.5, -0.5), mark);
}

void drawRadioIndicator(QPainter *p, const QRect &rect, QStyle::State s, const ThemeColors &tc, const X &x)
{
    const bool enabled = s & QStyle::State_Enabled;
    const bool pressed = s & QStyle::State_Sunken;
    QRectF b(0, 0, kIndicator, kIndicator);
    b.moveCenter(QRectF(rect).center());
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    p->setBrush(!enabled ? tc[T::Win] : pressed ? x.g3 : tc[T::Field]);
    p->drawEllipse(b.adjusted(1, 1, -1, -1));
    // 빗면: 위 · 왼쪽 반원은 짙게, 아래 · 오른쪽 반원은 밝게
    const auto ring = [p](const QRectF &r, const QColor &topLeft, const QColor &bottomRight) {
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(topLeft, 1.0));
        p->drawArc(r, 45 * 16, 180 * 16);
        p->setPen(QPen(bottomRight, 1.0));
        p->drawArc(r, 225 * 16, 180 * 16);
    };
    ring(b.adjusted(0.5, 0.5, -0.5, -0.5), x.checkOuter, x.checkBright);
    ring(b.adjusted(1.5, 1.5, -1.5, -1.5), x.checkInner, x.checkLight);
    if (s & QStyle::State_On) {
        p->setPen(Qt::NoPen);
        p->setBrush(enabled ? tc[T::Fg] : x.disFg);
        p->drawEllipse(b.center(), 2.3, 2.3);
    }
    p->restore();
}

// 스위치 — 캔버스 .swt: 52 × 22 들어간 홈, 반쪽 크기의 입체 손잡이. 켜지면 홈이 강조색.
void drawSwitch(QPainter *p, const QRect &rect, QStyle::State s, const ThemeColors &tc, const X &x)
{
    const bool enabled = s & QStyle::State_Enabled;
    const bool on = s & QStyle::State_On;
    QRect track(0, 0, kSwitchWidth, kSwitchHeight);
    track.moveCenter(rect.center());

    p->save();
    if (!enabled)
        p->setOpacity(0.5);
    sunken(p, track, x, on ? tc[T::Accent] : x.trough);
    const QRect in = track.adjusted(1, 1, -1, -1);
    const int half = in.width() / 2;
    const QRect knob = on ? QRect(in.left() + in.width() - half, in.top(), half, in.height())
                          : QRect(in.left(), in.top(), half, in.height());
    const Look look = !enabled ? Look::Normal
                    : (s & QStyle::State_Sunken) ? Look::Pressed
                    : (s & QStyle::State_MouseOver) ? Look::Hover
                                                     : Look::Normal;
    raised(p, knob, x, look, x.out);
    p->restore();
}

// 들어간 입력 칸. 포커스면 강조색 2 px 테두리 (CSS: border accent + inset 0 0 0 1px accent).
// 들어간 칸. 포커스는 2 px 강조 테두리, 오류(fmInvalid)는 2 px 위험 테두리.
/// 들어간 입력 칸. 읽기 전용(State_ReadOnly)은 XP처럼 창 바탕(3D 면)에 글자는 그대로 검다.
void drawInputFrame(QPainter *p, const QRect &r, QStyle::State s, const ThemeColors &tc, const X &x,
                    bool invalid = false)
{
    const bool enabled = s & QStyle::State_Enabled;
    const bool readOnly = s & QStyle::State_ReadOnly;
    sunken(p, r, x, enabled && !readOnly ? tc[T::Field] : tc[T::Win]);
    if (enabled && (invalid || (s & QStyle::State_HasFocus))) {
        const QColor c = invalid ? tc[T::Danger] : tc[T::Accent];
        outline(p, r, c);
        outline(p, r.adjusted(1, 1, -1, -1), c);
    }
}

// 콤보 · 스핀 상자 안의 작은 입체 단추.
void drawInnerButton(QPainter *p, const QRect &r, const X &x, bool enabled, bool hover, bool pressed)
{
    raised(p, r, x, !enabled ? Look::Disabled : pressed ? Look::Pressed : hover ? Look::Hover : Look::Normal,
           enabled ? x.out : x.disLine);
}

// 스핀 상자 ± 기호(QAbstractSpinBox::PlusMinus) — 삼각형처럼 굵게(2 px), 8 px.
void plusMinus(QPainter *p, const QRect &r, bool plus, const QColor &color)
{
    const QPoint o = r.center();
    p->fillRect(QRect(o.x() - 3, o.y(), 8, 2), color);
    if (plus)
        p->fillRect(QRect(o.x(), o.y() - 3, 2, 8), color);
}

// 트리의 가지 표시(XP 방식): 점선 연결선 + 하위가 있으면 9 × 9 상자에 + / −.
// 선은 QWindowsStyle과 같은 규칙 — 항목이면 가운데에서 바깥쪽 가로선, 아래 형제가 있으면 아래로 이어지는 세로선,
// 위로는 언제나 가운데(상자가 있으면 상자 위)까지.
void drawBranch(QPainter *p, const QRect &rect, QStyle::State s, Qt::LayoutDirection direction, const ThemeColors &tc)
{
    constexpr int kBox = 9;
    const bool children = s & QStyle::State_Children;
    const int midH = rect.x() + rect.width() / 2;
    const int midV = rect.y() + rect.height() / 2;
    const int half = children ? kBox / 2 : 0;
    // 선 · 상자 테두리는 보조 글자색 — 입체 그림자색(lo)은 다크 바탕에서 보이지 않는다.
    const QColor line = tc[T::Fg3];
    const QBrush dotted(line, Qt::Dense4Pattern);
    if (s & QStyle::State_Item) {
        if (direction == Qt::RightToLeft)
            p->fillRect(QRect(rect.left(), midV, midH - half - rect.left(), 1), dotted);
        else
            p->fillRect(QRect(midH + half, midV, rect.right() - midH - half + 1, 1), dotted);
    }
    if (s & QStyle::State_Sibling)
        p->fillRect(QRect(midH, midV + half, 1, rect.bottom() - midV - half + 1), dotted);
    if (s & (QStyle::State_Open | QStyle::State_Children | QStyle::State_Item | QStyle::State_Sibling))
        p->fillRect(QRect(midH, rect.y(), 1, midV - half - rect.y()), dotted);
    if (!children)
        return;
    const QRect b(midH - half, midV - half, kBox, kBox);
    p->fillRect(b, tc[T::Field]);
    outline(p, b, line);
    const QColor sign = tc[T::Fg];
    hLine(p, b.left() + 2, b.right() - 2, midV, sign);
    if (!(s & QStyle::State_Open))
        vLine(p, midH, b.top() + 2, b.bottom() - 2, sign);
}

} // namespace

// =============================================================================================
// drawPrimitive

void WatercolorStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *p,
                                    const QWidget *w) const
{
    applyPreviewState(option, w);
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());

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
        if (!dockButtonKind(w).isEmpty())
            drawDockButton(option, p, w);
        else
            drawToolPanel(option, p, w);
        return;
    case PE_FrameDockWidget: {
        // 떠 있는 도크(사용자 제목 줄) · 자동 숨김 펼친 창의 틀: MDI 창 틀과 같은 바깥 1 px + 파란 띠(비활성은 흐린 띠)
        const bool active = dockActive(w);
        const QRect r = option->rect;
        const QColor band = active ? x.frame : x.titleInactive;
        p->fillRect(r.adjusted(kDockFrame, kDockFrame, -kDockFrame, -kDockFrame), tc[T::Win]);
        p->fillRect(QRect(r.left(), r.top(), r.width(), kDockFrame), band);
        p->fillRect(QRect(r.left(), r.bottom() - kDockFrame + 1, r.width(), kDockFrame), band);
        p->fillRect(QRect(r.left(), r.top(), kDockFrame, r.height()), band);
        p->fillRect(QRect(r.right() - kDockFrame + 1, r.top(), kDockFrame, r.height()), band);
        outline(p, r, active ? x.frameOuter : x.titleInactiveHi);
        return;
    }
    case PE_IndicatorDockWidgetResizeHandle:
        // 도크 분할선: 평소 비움, 마우스 올림이면 새긴 점 셋
        if (option->state & State_MouseOver)
            gripDots(p, option->rect, x);
        return;
    case PE_FrameFocusRect:
        drawFocus(option, p, w);
        return;

    case PE_PanelLineEdit:
        if (const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
            if (f->lineWidth > 0) {
                drawInputFrame(p, f->rect, f->state, tc, x, isInvalid(w));
            } else if (isItemViewEditor(w)) {
                // 항목 보기의 칸 편집기: 입력 바탕 + 강조색 1 px 테두리. 글자 자리는 칸과 같게 둔다.
                p->fillRect(f->rect, tc[T::Field]);
                outline(p, f->rect, tc[T::Accent]);
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
            drawSwitch(p, option->rect, option->state, tc, x);
        else
            drawCheckIndicator(p, option->rect, option->state, tc, x);
        return;
    case PE_IndicatorItemViewItemCheck:
        drawCheckIndicator(p, option->rect, option->state, tc, x);
        return;
    case PE_IndicatorRadioButton:
        drawRadioIndicator(p, option->rect, option->state, tc, x);
        return;

    case PE_IndicatorArrowUp:
    case PE_IndicatorArrowDown:
    case PE_IndicatorArrowLeft:
    case PE_IndicatorArrowRight: {
        const Qt::ArrowType dir = element == PE_IndicatorArrowUp     ? Qt::UpArrow
                                : element == PE_IndicatorArrowDown   ? Qt::DownArrow
                                : element == PE_IndicatorArrowLeft   ? Qt::LeftArrow
                                                                     : Qt::RightArrow;
        const qreal size = std::clamp(std::min(option->rect.width(), option->rect.height()) / 2.0, 5.0, 8.0);
        triangle(p, QRectF(option->rect).center(), size, dir,
                 (option->state & State_Enabled) ? tc[T::Fg] : x.disFg);
        return;
    }
    case PE_IndicatorHeaderArrow:
        if (const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            if (h->sortIndicator == QStyleOptionHeader::None)
                return;
            // QHeaderView는 오름차순을 SortDown으로 넘긴다. 목업은 오름차순에 위쪽 화살표.
            // 색은 머리글 글자색(--fg, watercolor.css .hc)을 따른다.
            triangle(p, QRectF(option->rect).center(), 7,
                     h->sortIndicator == QStyleOptionHeader::SortDown ? Qt::UpArrow : Qt::DownArrow,
                     tc[T::Fg]);
        }
        return;
    case PE_IndicatorBranch:
        drawBranch(p, option->rect, option->state, option->direction, tc);
        return;

    case PE_PanelItemViewItem:
        if (const auto *v = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            if (v->state & State_Selected) {
                p->fillRect(v->rect, paneActive(v, w) ? tc[T::Sel] : tc[T::SelIn]);
                return;
            }
            if (v->backgroundBrush.style() != Qt::NoBrush)
                p->fillRect(v->rect, v->backgroundBrush);
            // 마우스 올림(XP 핫 트래킹): 목록 바탕에 강조색 12 %. 트리 행 바탕과 겹쳐도 되게 불투명.
            if (itemHover(v, w))
                p->fillRect(v->rect, mix(v->palette.color(QPalette::Base), tc[T::Accent], 0.12));
        }
        return;
    case PE_PanelItemViewRow:
        if (const auto *v = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            if ((v->state & State_Selected)
                && proxy()->styleHint(SH_ItemView_ShowDecorationSelected, option, w))
                p->fillRect(v->rect, paneActive(v, w) ? tc[T::Sel] : tc[T::SelIn]);
            else if (itemHover(v, w))
                p->fillRect(v->rect, mix(v->palette.color(QPalette::Base), tc[T::Accent], 0.12));
            else if (v->features & QStyleOptionViewItem::Alternate)
                p->fillRect(v->rect, tc[T::Alt]);
        }
        return;
    case PE_IndicatorItemViewItemDrop: {
        // 끌어 놓기 위치: 항목 사이는 강조색 2 px 선 + 양 끝 세로 막대(XP), 항목 위는 강조색 2 px 테두리.
        const QRect r = option->rect;
        if (r.height() == 0) {
            p->fillRect(QRect(r.left(), r.top() - 1, r.width(), 2), tc[T::Accent]);
            p->fillRect(QRect(r.left(), r.top() - 3, 2, 6), tc[T::Accent]);
            p->fillRect(QRect(r.right() - 1, r.top() - 3, 2, 6), tc[T::Accent]);
        } else if (r.width() == 0) {
            p->fillRect(QRect(r.left() - 1, r.top(), 2, r.height()), tc[T::Accent]);
            p->fillRect(QRect(r.left() - 3, r.top(), 6, 2), tc[T::Accent]);
            p->fillRect(QRect(r.left() - 3, r.bottom() - 1, 6, 2), tc[T::Accent]);
        } else {
            outline(p, r, tc[T::Accent]);
            outline(p, r.adjusted(1, 1, -1, -1), tc[T::Accent]);
        }
        return;
    }
    case PE_IndicatorColumnViewArrow:
        // 열 보기의 하위 항목 표시 — 채운 삼각형
        if (const auto *v = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            triangle(p, QRectF(v->rect).center(), 7, v->direction == Qt::RightToLeft ? Qt::LeftArrow : Qt::RightArrow,
                     (v->state & State_Enabled) ? tc[T::Fg] : x.disFg);
        }
        return;

    case PE_PanelMenu:
        p->fillRect(option->rect, tc[T::Surface]);
        return;
    case PE_FrameMenu:
        outline(p, option->rect, x.menuLine);
        return;
    case PE_PanelMenuBar:
        return;
    case PE_PanelTipLabel:
        p->fillRect(option->rect, x.tipBg);
        outline(p, option->rect, x.tipLine);
        return;

    case PE_FrameTabBarBase:
        if (const auto *tb = qstyleoption_cast<const QStyleOptionTabBarBase *>(option)) {
            p->fillRect(tabBaseLine(tabSide(tb->shape), tb->rect), x.tabLine);
            return;
        }
        break;
    case PE_FrameTabWidget:
        p->fillRect(option->rect, tc[T::Win]);
        outline(p, option->rect, x.tabLine);
        return;
    case PE_IndicatorTabClose: {
        // 탭 닫기 단추: 굵은 X, 마우스 올림 · 누름이면 14 px 입체 단추.
        const QStyle::State s = option->state;
        const bool enabled = s & State_Enabled;
        const bool pressed = enabled && (s & State_Sunken);
        const bool hover = enabled && (s & (State_Raised | State_MouseOver));
        QRect box(0, 0, 14, 14);
        box.moveCenter(option->rect.center());
        if (pressed)
            raised(p, box, x, Look::Pressed, x.out);
        else if (hover)
            raised(p, box, x, Look::Hover, x.out);
        const QColor color = !enabled ? x.disFg : (pressed || hover) ? x.hoverFg : tc[T::Fg];
        paintChromeGlyph(p, ChromeGlyph::Close, QRectF(box).adjusted(1, 1, -1, -1), color, true);
        return;
    }
    case PE_Frame:
        if (option->state & State_Sunken) {
            QStyleOptionFrame f;
            f.QStyleOption::operator=(*option);
            f.frameShape = QFrame::StyledPanel;
            drawShapedFrame(&f, p, w);
        } else {
            outline(p, option->rect, tc[T::Line]);
        }
        return;
    case PE_FrameWindow: {
        // MDI 창 틀: 바깥 1 px + 파란 틀. QMdiSubWindow는 제목 표시줄을 그린 뒤 틀을 그리므로 띠만 칠한다.
        const bool active = option->state & State_Active;
        const QRect r = option->rect;
        int width = kSwitchMargin + 1;  // 기본 4 px
        if (const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(option); f && f->lineWidth > 0)
            width = f->lineWidth;
        const QColor band = active ? x.frame : x.titleInactive;
        p->fillRect(QRect(r.left(), r.top(), r.width(), width), band);
        p->fillRect(QRect(r.left(), r.bottom() - width + 1, r.width(), width), band);
        p->fillRect(QRect(r.left(), r.top(), width, r.height()), band);
        p->fillRect(QRect(r.right() - width + 1, r.top(), width, r.height()), band);
        outline(p, r, active ? x.frameOuter : x.titleInactiveHi);
        return;
    }
    case PE_FrameGroupBox:
        p->fillRect(option->rect, tc[T::Surface]);
        outline(p, option->rect, tc[T::Line]);
        return;
    case PE_FrameStatusBarItem:
        thinSunken(p, option->rect, x);
        return;
    case PE_IndicatorToolBarSeparator: {
        // 새긴 세로줄 2 px × 22 px
        const QRect r = option->rect;
        if (qobject_cast<const QComboBox *>(w)) {
            // 콤보 펼친 목록의 구분선(insertSeparator) — 폭 전체(좌우 2 안쪽) 1 px
            hLine(p, r.left() + 2, r.right() - 2, r.center().y(), tc[T::Line]);
            return;
        }
        if (option->state & State_Horizontal) {
            const int cx = r.center().x();
            const int top = r.center().y() - 11;
            vLine(p, cx, top, top + 21, x.lo);
            vLine(p, cx + 1, top, top + 21, x.hi);
        } else {
            const int cy = r.center().y();
            const int left = r.center().x() - 11;
            hLine(p, left, left + 21, cy, x.lo);
            hLine(p, left, left + 21, cy + 1, x.hi);
        }
        return;
    }
    case PE_IndicatorToolBarHandle: {
        // 도드라진 막대 하나
        const QRect r = option->rect;
        if (option->state & State_Horizontal)
            bevel(p, QRect(r.center().x() - 1, r.top() + 3, 3, r.height() - 6), x.hi, x.lo);
        else
            bevel(p, QRect(r.left() + 3, r.center().y() - 1, r.width() - 6, 3), x.hi, x.lo);
        return;
    }
    case PE_PanelToolBar:
        return;
    case PE_PanelScrollAreaCorner:
        p->fillRect(option->rect, tc[T::Win]);
        return;
    case PE_PanelStatusBar:
        p->fillRect(option->rect, tc[T::Win]);
        return;
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, p, w);
}

// =============================================================================================
// drawControl

void WatercolorStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *p,
                                  const QWidget *w) const
{
    applyPreviewState(option, w);
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());

    switch (element) {
    case CE_PushButtonBevel:
        if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            drawButtonPanel(b, p, w);
            if (b->features & QStyleOptionButton::HasMenu) {
                const QRect ar(b->rect.right() - 18, b->rect.top(), 12, b->rect.height());
                triangle(p, QRectF(ar).center(), 7, Qt::DownArrow,
                         buttonTextColor(b->state, buttonRole(w), tc, x));
            }
        }
        return;
    case CE_PushButtonLabel:
        if (const auto *b = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            const ButtonRole role = buttonRole(w);
            QStyleOptionButton copy = *b;
            QColor text = buttonTextColor(b->state, role, tc, x);
            if (role == ButtonRole::Link) {
                text = !(b->state & State_Enabled)                   ? x.disFg
                     : (b->state & (State_MouseOver | State_Sunken)) ? tc[T::Accent]
                                                                     : tc[T::AccentFg];
            }
            copy.palette.setColor(QPalette::ButtonText, text);
            // 아이콘은 글자색을 따른다(06 §6.4 D2): 마우스 올림 · 누름이면 QIcon::Active(흰 글자색) 그림을 쓰도록
            // Fusion이 Active를 고르는 조건(HasFocus)을 바꿔 넘긴다. 포커스 틀은 여기서 그리지 않는다.
            if ((b->state & State_Enabled) && (b->state & (State_MouseOver | State_Sunken)))
                copy.state |= State_HasFocus;
            else
                copy.state &= ~State_HasFocus;
            if (b->features & QStyleOptionButton::HasMenu)
                copy.rect.adjust(0, 0, -12, 0);
            // 키 칩(fmKeyHint): 글자 뒤 7 px — 글자와 칩을 한 덩어리로 가운데
            const QString keys = keyHintOf(w);
            QRect chipRect;
            if (!keys.isEmpty()) {
                const QSize chip = keyChipSize(keys);
                int labelWidth = b->fontMetrics.horizontalAdvance(plainText(b->text));
                if (!b->icon.isNull())
                    labelWidth += b->iconSize.width() + kButtonIconGap;
                const int total = labelWidth + kKeyChipGap + chip.width();
                const int left = copy.rect.left() + (copy.rect.width() - total) / 2;
                copy.rect = QRect(left, copy.rect.top(), labelWidth, copy.rect.height());
                chipRect = QRect(left + labelWidth + kKeyChipGap, copy.rect.center().y() - chip.height() / 2 + 1,
                                 chip.width(), chip.height());
            }
            if (!segmentOf(w).isEmpty() || role == ButtonRole::Link || !drawIconTextButtonLabel(this, copy, p, w))
                QProxyStyle::drawControl(element, &copy, p, w);
            if (chipRect.isValid()) {
                // 워터컬러의 기본 단추는 채움이 아니므로 위험 단추만 채움 위 칩
                const bool filled = (b->state & State_Enabled) && role == ButtonRole::Danger;
                paintKeyChip(p, chipRect, keys, tc, filled ? KeyChipLook::OnFill : KeyChipLook::Normal);
            }
        }
        return;
    case CE_CheckBoxLabel:
    case CE_RadioButtonLabel:
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
        if (!dockButtonKind(w).isEmpty())
            return;  // 도크 제목 줄 단추는 PE_PanelButtonTool에서 기호까지 그렸다
        if (const auto *tb = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
            QStyleOptionToolButton copy = *tb;
            QStyle::State s = tb->state;
            if (tb->activeSubControls & SC_ToolButton)
                s |= (tb->state & State_Sunken);
            copy.palette.setColor(QPalette::ButtonText, buttonTextColor(s, buttonRole(w), tc, x));
            QProxyStyle::drawControl(element, &copy, p, w);
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
            QStyleOptionTab copy = *tab;
            const QColor fg = (tab->state & State_Enabled) ? tc[T::Fg] : x.disFg;
            copy.palette.setColor(QPalette::WindowText, fg);
            copy.palette.setColor(QPalette::ButtonText, fg);
            if (!(tab->state & State_Selected))
                copy.rect = adjustTabRect(tabSide(tab->shape), copy.rect, kTabDrop, 1);  // 23 px 상자 가운데
            QProxyStyle::drawControl(element, &copy, p, w);
        }
        return;

    case CE_ToolBoxTabShape:
    case CE_ToolBoxTabLabel:
        drawToolBoxTab(element, option, p, w);
        return;

    case CE_DockWidgetTitle:
        drawDockTitle(option, p, w);
        return;

    case CE_Header:
        // QCommonStyle은 SE_HeaderLabel/SE_HeaderArrow를 proxy() 없이 불러 정렬 화살표 위치가 무시된다.
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
            const QColor fg = !(h->state & State_Enabled) ? x.disFg : flatHeader(w) ? tc[T::Fg2] : tc[T::Fg];
            copy.palette.setColor(QPalette::ButtonText, fg);
            copy.palette.setColor(QPalette::WindowText, fg);
            QProxyStyle::drawControl(element, &copy, p, w);
        }
        return;
    case CE_HeaderEmptyArea:
        p->fillRect(option->rect, x.g2);
        hLine(p, option->rect.left(), option->rect.right(), option->rect.bottom(), x.out);
        return;

    case CE_ItemViewItem:
        if (const auto *v = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            // 목업: 활성 패널의 선택 행은 흰 글자, 비활성 패널은 글자색 그대로
            QStyleOptionViewItem copy = *v;
            const QColor text = paneActive(v, w) ? tc[T::OnAccent] : copy.palette.color(QPalette::Text);
            for (const auto group : {QPalette::Active, QPalette::Inactive})
                copy.palette.setColor(group, QPalette::HighlightedText, text);
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

    case CE_ToolBar:
        if (const auto *tb = qstyleoption_cast<const QStyleOptionToolBar *>(option)) {
            // 위 · 아래 새긴 선 (CSS .toolbar의 inset 그림자 네 줄)
            const QRect r = tb->rect;
            if (tb->state & State_Horizontal) {
                hLine(p, r.left(), r.right(), r.top(), x.lo);
                hLine(p, r.left(), r.right(), r.top() + 1, x.hi);
                hLine(p, r.left(), r.right(), r.bottom() - 1, x.lo);
                hLine(p, r.left(), r.right(), r.bottom(), x.hi);
            } else {
                vLine(p, r.left(), r.top(), r.bottom(), x.lo);
                vLine(p, r.left() + 1, r.top(), r.bottom(), x.hi);
                vLine(p, r.right() - 1, r.top(), r.bottom(), x.lo);
                vLine(p, r.right(), r.top(), r.bottom(), x.hi);
            }
        }
        return;
    case CE_Splitter:
        // 분할 손잡이: 평소 비움(XP), 마우스 올림 · 끌기 중이면 새긴 점 셋(00 #9)
        if (option->state & (State_MouseOver | State_Sunken))
            gripDots(p, option->rect, x);
        return;
    case CE_FocusFrame:
        return;
    case CE_RubberBand:
        p->save();
        p->setPen(tc[T::Accent]);
        p->setBrush(withAlpha(tc[T::Accent], 0.18));
        p->drawRect(option->rect.adjusted(0, 0, -1, -1));
        p->restore();
        return;
    case CE_ColumnViewGrip: {
        // 열 보기의 열 크기 손잡이: 입체 칸에 새긴 세로줄 두 개
        const QRect r = option->rect;
        raised(p, r, x, Look::Normal, x.out);
        const int h = std::max(4, r.height() * 2 / 5);
        const int top = r.top() + (r.height() - h) / 2;
        const int cx = r.center().x();
        for (const int xx : {cx - 2, cx + 1}) {
            vLine(p, xx, top, top + h - 1, x.lo);
            vLine(p, xx + 1, top, top + h - 1, x.hi);
        }
        return;
    }
    default:
        break;
    }
    QProxyStyle::drawControl(element, option, p, w);
}

// =============================================================================================
// drawComplexControl

void WatercolorStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                         QPainter *p, const QWidget *w) const
{
    applyPreviewState(option, w);
    const ThemeColors &tc = colorsFor(w);
    const X &x = watercolorChrome(tc.variant());

    switch (control) {
    case CC_ComboBox:
        if (const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            const bool enabled = cb->state & State_Enabled;
            // 편집 여부와 상관없이 포커스는 입력과 같은 2 px 강조 테두리(캔버스 .input:focus 규칙).
            // 틀 없음(setFrame(false))이면 들어간 틀 없이 화살표 단추만.
            if (cb->frame)
                drawInputFrame(p, cb->rect, cb->state, tc, x, isInvalid(w));
            else if (enabled && (cb->state & State_HasFocus) && !cb->editable)
                dottedRect(p, proxy()->subControlRect(CC_ComboBox, cb, SC_ComboBoxEditField, w), tc[T::Focus]);
            const QRect arrow = proxy()->subControlRect(CC_ComboBox, cb, SC_ComboBoxArrow, w);
            const bool open = cb->state & State_On;
            const bool pressed = open || ((cb->activeSubControls & SC_ComboBoxArrow) && (cb->state & State_Sunken));
            drawInnerButton(p, arrow, x, enabled, cb->state.testFlag(State_MouseOver), pressed);
            triangle(p, QRectF(arrow).center() + QPointF(0, 0.5), 8, Qt::DownArrow, enabled ? tc[T::Fg] : x.disFg);
        }
        return;

    case CC_SpinBox:
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            QStyle::State s = sb->state;
            if (const auto *spin = qobject_cast<const QAbstractSpinBox *>(w); spin && spin->isReadOnly())
                s |= State_ReadOnly;
            if (sb->frame)
                drawInputFrame(p, sb->rect, s, tc, x, isInvalid(w));
            if (sb->buttonSymbols == QAbstractSpinBox::NoButtons)
                return;
            for (const SubControl sc : {SC_SpinBoxUp, SC_SpinBoxDown}) {
                const QRect r = proxy()->subControlRect(CC_SpinBox, sb, sc, w);
                const auto step = sc == SC_SpinBoxUp ? QAbstractSpinBox::StepUpEnabled
                                                     : QAbstractSpinBox::StepDownEnabled;
                const bool enabled = (sb->state & State_Enabled) && (sb->stepEnabled & step);
                const bool active = sb->activeSubControls & sc;
                drawInnerButton(p, r, x, enabled, active && (sb->state & State_MouseOver),
                                active && (sb->state & State_Sunken));
                const QColor glyph = enabled ? tc[T::Fg] : x.disFg;
                if (sb->buttonSymbols == QAbstractSpinBox::PlusMinus)
                    plusMinus(p, r, sc == SC_SpinBoxUp, glyph);
                else
                    triangle(p, QRectF(r).center(), 6, sc == SC_SpinBoxUp ? Qt::UpArrow : Qt::DownArrow, glyph);
            }
        }
        return;

    case CC_ToolButton:
        drawToolButton(option, p, w);
        return;

    case CC_ScrollBar:
        drawScrollBar(option, p, w);
        return;

    case CC_GroupBox:
        if (const auto *gb = qstyleoption_cast<const QStyleOptionGroupBox *>(option)) {
            if (gb->subControls & SC_GroupBoxFrame) {
                const QRect fr = proxy()->subControlRect(CC_GroupBox, gb, SC_GroupBoxFrame, w);
                if (gb->features & QStyleOptionFrame::Flat) {
                    // 납작(setFlat): 칸 없이 제목 아래 새긴 선(어두운 선 + 밝은 선)만
                    p->fillRect(QRect(fr.left(), fr.top(), fr.width(), 1), tc[T::Line]);
                    p->fillRect(QRect(fr.left(), fr.top() + 1, fr.width(), 1), x.hi);
                } else {
                    p->fillRect(fr, tc[T::Surface]);
                    outline(p, fr, tc[T::Line]);
                }
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
                p->setPen((gb->state & State_Enabled) ? tc[T::Fg] : x.disFg);
                p->drawText(lr, flags, gb->text);
                p->restore();
            }
            if (gb->subControls & SC_GroupBoxCheckBox) {
                QStyleOptionButton box;
                box.QStyleOption::operator=(*gb);
                box.rect = proxy()->subControlRect(CC_GroupBox, gb, SC_GroupBoxCheckBox, w);
                drawCheckIndicator(p, box.rect, box.state, tc, x);
            }
        }
        return;

    case CC_TitleBar:
        drawTitleBar(option, p, w);
        return;
    case CC_MdiControls:
        drawMdiControls(option, p, w);
        return;
    case CC_Slider:
        drawSlider(option, p, w);
        return;
    case CC_Dial:
        drawDial(option, p, w);
        return;

    default:
        break;
    }
    QProxyStyle::drawComplexControl(control, option, p, w);
}

// =============================================================================================
// 영역 · 크기

QRect WatercolorStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *w) const
{
    switch (element) {
    case SE_PushButtonContents:
        return option->rect.adjusted(4, 2, -4, -2);
    case SE_PushButtonFocusRect:
        return option->rect.adjusted(3, 3, -3, -3);

    case SE_LineEditContents:
        if (const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(option); f && f->lineWidth > 0)
            return option->rect.adjusted(kInputPadX - 1, 2, -(kInputPadX - 1), -2);
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
        break;

    case SE_ProgressBarGroove:
    case SE_ProgressBarContents:
    case SE_ProgressBarLabel:
        if (const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            const bool horizontal = pb->state & State_Horizontal;
            const QRect r = pb->rect;
            const int thickness = progressThickness(w);
            int labelWidth = 0;
            if (pb->textVisible && horizontal)
                labelWidth = pb->fontMetrics.horizontalAdvance(u"100%"_s) + 8;
            if (element == SE_ProgressBarLabel)
                return QRect(r.right() - labelWidth + 1, r.top(), labelWidth, r.height());
            const QRect bar = r.adjusted(0, 0, -labelWidth, 0);
            if (horizontal)
                return QRect(bar.left(), bar.top() + (bar.height() - thickness) / 2, bar.width(), thickness);
            return QRect(bar.left() + (bar.width() - thickness) / 2, bar.top(), thickness, bar.height());
        }
        break;

    case SE_HeaderArrow:
        if (const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            // 목업: 정렬 화살표는 이름 바로 뒤 (표 머리글은 가운데 맞춤이 기본)
            return headerArrowRect(h, proxy()->pixelMetric(PM_HeaderMargin, option, w), h->rect.height() - 1);
        }
        break;
    case SE_HeaderLabel: {
        const int margin = proxy()->pixelMetric(PM_HeaderMargin, option, w);
        return option->rect.adjusted(margin, 0, -margin, -1);
    }
    case SE_DockWidgetCloseButton:
    case SE_DockWidgetFloatButton:
    case SE_DockWidgetTitleBarText:
    case SE_DockWidgetIcon:
        // 오른쪽 2 px 안쪽부터 닫기 · 떼어 내기(사이 1), 글자는 왼쪽 6
        return dockTitleSubRect(element, option, w, kDockButton, 1, 2, 6);
    default:
        break;
    }
    return QProxyStyle::subElementRect(element, option, w);
}

QRect WatercolorStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option,
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
                // 테두리 1 px 안쪽 오른쪽에 16 px 단추 (CSS .combo::after)
                res = QRect(r.right() - kDropButtonWidth, r.top() + 1, kDropButtonWidth, r.height() - 2);
                break;
            case SC_ComboBoxEditField:
                res = QRect(r.left() + kInputPadX, r.top() + 3, r.width() - kInputPadX - kDropButtonWidth - 3,
                            r.height() - 6);
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
            const int inner = r.height() - 2;
            const int half = inner / 2;
            QRect res;
            switch (sc) {
            case SC_SpinBoxFrame:
                res = r;
                break;
            case SC_SpinBoxEditField:
                res = QRect(r.left() + kInputPadX, r.top() + 2,
                            r.width() - kInputPadX - (buttons ? kDropButtonWidth + 3 : kInputPadX), r.height() - 4);
                break;
            case SC_SpinBoxUp:
                if (!buttons)
                    return QRect();
                res = QRect(r.right() - kDropButtonWidth, r.top() + 1, kDropButtonWidth, half);
                break;
            case SC_SpinBoxDown:
                if (!buttons)
                    return QRect();
                res = QRect(r.right() - kDropButtonWidth, r.top() + 1 + half, kDropButtonWidth, inner - half);
                break;
            default:
                return QProxyStyle::subControlRect(control, option, sc, w);
            }
            return visualRect(sb->direction, r, res);
        }
        break;

    case CC_ScrollBar:
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            // 양 끝 화살표 단추 + 가운데 홈
            const QRect r = sb->rect;
            const bool horizontal = sb->orientation == Qt::Horizontal;
            const int length = horizontal ? r.width() : r.height();
            const int thick = horizontal ? r.height() : r.width();
            const int button = std::min(thick, length / 2);
            const int groove = std::max(0, length - 2 * button);
            const qint64 range = qint64(sb->maximum) - qint64(sb->minimum);
            const int minLength = std::min(proxy()->pixelMetric(PM_ScrollBarSliderMin, sb, w), groove);
            int sliderLength = groove;
            if (range > 0) {
                sliderLength = int(qint64(sb->pageStep) * groove / (range + sb->pageStep));
                sliderLength = std::clamp(sliderLength, minLength, groove);
            }
            const int pos = sliderPositionFromValue(sb->minimum, sb->maximum, sb->sliderPosition,
                                                    groove - sliderLength, sb->upsideDown);
            const auto part = [&](int start, int size) {
                return horizontal ? QRect(r.left() + start, r.top(), size, r.height())
                                  : QRect(r.left(), r.top() + start, r.width(), size);
            };
            QRect res;
            switch (sc) {
            case SC_ScrollBarSubLine:
                res = part(0, button);
                break;
            case SC_ScrollBarAddLine:
                res = part(length - button, button);
                break;
            case SC_ScrollBarGroove:
                res = part(button, groove);
                break;
            case SC_ScrollBarSlider:
                res = part(button + pos, sliderLength);
                break;
            case SC_ScrollBarSubPage:
                res = part(button, pos);
                break;
            case SC_ScrollBarAddPage:
                res = part(button + pos + sliderLength, groove - pos - sliderLength);
                break;
            case SC_ScrollBarFirst:
            case SC_ScrollBarLast:
                return QRect();
            default:
                return QProxyStyle::subControlRect(control, option, sc, w);
            }
            return horizontal ? visualRect(sb->direction, r, res) : res;
        }
        break;

    case CC_GroupBox:
        if (const auto *gb = qstyleoption_cast<const QStyleOptionGroupBox *>(option)) {
            const QRect r = gb->rect;
            const int lineHeight = gb->fontMetrics.height();
            const int titleHeight = gb->text.isEmpty() ? 0 : lineHeight + 8;
            switch (sc) {
            case SC_GroupBoxCheckBox:
            case SC_GroupBoxLabel:
                // 제목 줄(체크 + 글자)은 정렬(setAlignment)을 따른다
                return groupBoxTitleRect(gb, sc, kIndicator, 7, 8);
            case SC_GroupBoxFrame:
                return r.adjusted(0, titleHeight, 0, 0);
            case SC_GroupBoxContents:
                if (gb->features & QStyleOptionFrame::Flat)
                    return r.adjusted(0, titleHeight + 2, 0, 0);
                return r.adjusted(1, titleHeight + 1, -1, -1);
            default:
                break;
            }
        }
        break;

    case CC_TitleBar:
        if (const auto *tb = qstyleoption_cast<const QStyleOptionTitleBar *>(option)) {
            // 오른쪽부터 닫기 — 3 px — 최대화(복원) · 최소화(1 px 겹침). 단추는 21 × 20.
            const QRect r = tb->rect;
            const Qt::WindowFlags flags = tb->titleBarFlags;
            const bool isMin = tb->titleBarState & Qt::WindowMinimized;
            const bool isMax = tb->titleBarState & Qt::WindowMaximized;
            const bool hasClose = flags & Qt::WindowSystemMenuHint;
            const bool hasMax = flags & Qt::WindowMaximizeButtonHint;
            const bool hasMin = flags & Qt::WindowMinimizeButtonHint;
            const int top = r.top() + (r.height() - kCaptionButtonH) / 2;
            const auto at = [&](int right) { return QRect(right - kCaptionButtonW + 1, top, kCaptionButtonW, kCaptionButtonH); };

            int right = r.right() - 3;
            const QRect close = hasClose ? at(right) : QRect();
            if (hasClose)
                right = close.left() - 4;
            const bool showMax = hasMax && !isMax;
            const bool showNormal = (isMax && hasMax) || (isMin && hasMin);
            const QRect maxOrNormal = (showMax || showNormal) ? at(right) : QRect();
            if (showMax || showNormal)
                right = maxOrNormal.left();  // 최소화는 1 px 겹친다
            const bool showMin = hasMin && !isMin;
            const QRect min = showMin ? at(right) : QRect();
            const int leftmost = showMin ? min.left() : (showMax || showNormal) ? maxOrNormal.left()
                                                        : hasClose ? close.left() : r.right();
            const QRect sysMenu = (flags & Qt::WindowSystemMenuHint)
                ? QRect(r.left() + 6, r.top() + (r.height() - 16) / 2, 16, 16) : QRect();
            QRect res;
            switch (sc) {
            case SC_TitleBarCloseButton: res = close; break;
            case SC_TitleBarMaxButton: res = showMax ? maxOrNormal : QRect(); break;
            case SC_TitleBarNormalButton: res = showNormal ? maxOrNormal : QRect(); break;
            case SC_TitleBarMinButton: res = min; break;
            case SC_TitleBarSysMenu: res = sysMenu; break;
            case SC_TitleBarLabel: {
                const int left = sysMenu.isValid() ? sysMenu.right() + 6 : r.left() + 6;
                res = QRect(left, r.top(), std::max(0, leftmost - 6 - left), r.height());
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

    case CC_Slider:
        if (const auto *sl = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            if (const QRect res = sliderSubControlRect(sl, sc, kSliderLength, kSliderThickness); res.isValid())
                return res;
        }
        break;

    default:
        break;
    }
    return QProxyStyle::subControlRect(control, option, sc, w);
}

QSize WatercolorStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &cs,
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
            const int height = segment ? segmentHeight(w) : compact ? kButtonHeightSmall : kButtonHeight;
            const int padX = segment ? segmentPadX(w) : compact ? kButtonPadXSmall : kButtonPadX;
            if (b->text.isEmpty() && !b->icon.isNull())
                return QSize(std::max(height, cs.width() + 11), height);  // 정사각 27 × 27
            // 캔버스 워터컬러: 작은 단추 · 칩 단추도 최소 폭 80(watercolor.css가 .btn 최소 폭을 덮어씀 — 보드 그대로)
            const int minWidth = segment ? 0 : kButtonMinWidth;
            const int iconGap = (!segment && !b->icon.isNull() && !b->text.isEmpty()) ? kButtonIconGap - 4 : 0;
            return QSize(std::max(minWidth, cs.width() + 2 * padX + chip + iconGap), std::max(height, cs.height() + 6));
        }
        break;
    case CT_ToolButton: {
        if (!segmentOf(w).isEmpty())
            return QSize(cs.width() + 2 * segmentPadX(w), segmentHeight(w));
        const int dropDown = toolButtonHasDropDown(option) ? kToolDropDown - 2 : 0;
        return QSize(std::max(kToolButton, cs.width() + 13) + dropDown, std::max(kToolButton, cs.height() + 13));
    }

    case CT_LineEdit:
        if (const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(option); f && f->lineWidth > 0)
            return QSize(cs.width() + 2 * kInputPadX, std::max(kInputHeight, cs.height() + 2));
        return cs;
    case CT_SpinBox:
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            const bool buttons = sb->buttonSymbols != QAbstractSpinBox::NoButtons;
            return QSize(cs.width() + kInputPadX + (buttons ? kDropButtonWidth + 3 : kInputPadX),
                         std::max(kInputHeight, cs.height() + 2));
        }
        break;
    case CT_ComboBox:
        return QSize(cs.width() + kInputPadX + kDropButtonWidth + 8, std::max(kInputHeight, cs.height() + 2));

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
            if (isVerticalTab(tabSide(tab->shape)))
                return QSize(kTabHeight, cs.height());
            return QSize(cs.width(), kTabHeight);
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
                return QSize(cs.width(), 10);
            const bool subMenu = mi->menuItemType == QStyleOptionMenuItem::SubMenu;
            // 왼쪽 4 + 체크 열 20 + 간격 6 + 글자 + (단축키 앞 간격) + 오른쪽 14 + (하위 메뉴 화살표)
            int width = 4 + 20 + 6 + cs.width() + 14 + (subMenu ? 10 : 0);
            if (mi->text.contains(u'\t') || mi->reservedShortcutWidth > 0)
                width += 28;
            return QSize(width, kMenuItemHeight);
        }
        break;
    case CT_MenuBarItem:
        return QSize(cs.width() + 18, kMenuBarItemHeight);

    case CT_ProgressBar:
        if (const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            const int thickness = progressThickness(w);
            if (pb->state & State_Horizontal)
                return QSize(cs.width(), std::max(thickness, pb->textVisible ? cs.height() : 0));
            return QSize(std::max(thickness, pb->textVisible ? cs.width() : 0), cs.height());
        }
        break;

    default:
        break;
    }
    return QProxyStyle::sizeFromContents(type, option, cs, w);
}

int WatercolorStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *w) const
{
    switch (metric) {
    case PM_MenuButtonIndicator:
        // 분할 도구 단추의 메뉴 칸만 — 메뉴 달린 누름 단추의 폭은 그대로 둔다
        if (qstyleoption_cast<const QStyleOptionToolButton *>(option))
            return kMenuButtonIndicator;
        break;
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
        return 1;  // XP처럼 누르면 글자가 1 px 내려간다
    case PM_ButtonDefaultIndicator:
        return 0;
    case PM_DefaultFrameWidth:
    case PM_ComboBoxFrameWidth:
    case PM_SpinBoxFrameWidth:
        return 2;
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return kIndicator;
    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing:
        return 7;
    case PM_ScrollBarExtent:
        return kScrollBarExtent;
    case PM_ScrollBarSliderMin:
        return 17;
    case PM_ScrollView_ScrollBarOverlap:
    case PM_ScrollView_ScrollBarSpacing:
        return 0;
    case PM_TabBarTabHSpace:
        return 2 * kTabPadX;
    case PM_TabBarTabVSpace:
        return 6;
    case PM_TabBarBaseOverlap:
        return 1;
    case PM_TabBarTabOverlap:
    case PM_TabBarTabShiftVertical:
    case PM_TabBarTabShiftHorizontal:
        return 0;
    case PM_MenuPanelWidth:
        return 1;
    case PM_MenuHMargin:
    case PM_MenuVMargin:
        return 3;
    case PM_MenuBarPanelWidth:
    case PM_MenuBarItemSpacing:
        return 0;
    case PM_MenuBarHMargin:
        return 3;
    case PM_MenuBarVMargin:
        return 1;
    case PM_SmallIconSize:
    case PM_ToolBarIconSize:
    case PM_ButtonIconSize:
    case PM_TabBarIconSize:
    case PM_ListViewIconSize:
        return 16;
    case PM_ToolBarItemSpacing:
        return 1;
    case PM_ToolBarSeparatorExtent:
        return 8;
    case PM_ToolBarFrameWidth:
    case PM_ToolBarItemMargin:
        return 2;
    case PM_ToolBarHandleExtent:
        return 9;
    case PM_HeaderMargin:
        return 6;
    case PM_FocusFrameHMargin:
    case PM_FocusFrameVMargin:
        return 2;
    case PM_ToolTipLabelFrameWidth:
        return 3;
    case PM_SplitterWidth:
        return 4;
    case PM_TitleBarHeight:
        return kTitleBarHeight;
    case PM_TitleBarButtonSize:
        return kCaptionButtonH;
    case PM_MdiSubWindowFrameWidth:
        return 4;
    case PM_TabCloseIndicatorWidth:
    case PM_TabCloseIndicatorHeight:
        return kTabClose;
    case PM_SliderThickness:
    case PM_SliderControlThickness:
        return kSliderThickness;
    case PM_SliderLength:
        return kSliderLength;
    case PM_DockWidgetTitleMargin:
        return kDockTitleMargin;
    case PM_DockWidgetTitleBarButtonMargin:
        return kDockButtonMargin;
    case PM_DockWidgetFrameWidth:
        return kDockFrame;
    case PM_DockWidgetSeparatorExtent:
        return kDockSeparator;
    default:
        break;
    }
    return QProxyStyle::pixelMetric(metric, option, w);
}

QIcon WatercolorStyle::standardIcon(StandardPixmap standardIcon, const QStyleOption *option, const QWidget *w) const
{
    // 관리자 권한 방패 — 목업의 두 색 방패(--shield / --shield-2)
    if (standardIcon == SP_VistaShield)
        return shieldIcon(colorsFor(w), 16);
    // 화살표 · 창 단추 · 확장 단추 등 단색 아이콘은 테마 색으로 (Fusion 그림은 검은색이라 다크에서 안 보인다).
    if (QIcon icon = themedStandardIcon(standardIcon, w, true); !icon.isNull())
        return icon;
    return QProxyStyle::standardIcon(standardIcon, option, w);
}

QPixmap WatercolorStyle::generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap,
                                             const QStyleOption *option) const
{
    // 선택한 항목의 아이콘은 그대로 — Qt 기본은 강조색을 30 % 덮어 목록 아이콘이 파랗게 물든다.
    if (iconMode == QIcon::Selected)
        return pixmap;
    return QProxyStyle::generatedIconPixmap(iconMode, pixmap, option);
}

int WatercolorStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *w,
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
    case SH_TitleBar_NoBorder:
        return 0;
    case SH_DockWidget_ButtonsHaveFrame:
        return 1;  // 캡션 단추 바탕을 늘 그리게(PE_PanelButtonTool) — 기호도 스타일이 그린다
    default:
        break;
    }
    return QProxyStyle::styleHint(hint, option, w, returnData);
}

} // namespace fm::style
