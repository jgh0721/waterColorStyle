// 도구 설명 — 시안1 Windows 11 설명 칸 · 시안2 XP 노란 칸과 풍선 도움말.

#include "fmwidgets/ToolTip.h"

#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeColors.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTabBar>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

constexpr char kAttached[] = "fmToolTipAttached";  // 붙인 ToolTip이 있는 대상 — 전역 처리기가 건너뛴다
constexpr int kTailLength = 7;    // 꼬리 높이(시안1) — 시안2 풍선은 10
constexpr int kTailWidth = 14;
constexpr int kGap = 4;           // 대상과 꼬리 끝 사이
constexpr int kFadeMs = 120;

bool animationsSupported()
{
    const QString platform = QGuiApplication::platformName();
    return platform != u"offscreen" && platform != u"minimal";
}

/// 항목 보기의 보기 영역이면 그 보기.
QAbstractItemView *viewOfViewport(QWidget *w)
{
    if (!w)
        return nullptr;
    auto *view = qobject_cast<QAbstractItemView *>(w->parentWidget());
    return view && view->viewport() == w ? view : nullptr;
}

/// 전역 처리기 — 앱 전체의 도움말 이벤트를 함께 쓰는 ToolTip으로.
class GlobalFilter : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::ToolTip: {
            auto *w = qobject_cast<QWidget *>(watched);
            if (!w || w->property(kAttached).toBool())
                return false;
            const auto *help = static_cast<QHelpEvent *>(event);
            QString text;
            m_index = QPersistentModelIndex();
            if (QAbstractItemView *view = viewOfViewport(w)) {
                m_index = view->indexAt(help->pos());
                text = m_index.data(Qt::ToolTipRole).toString();
            } else if (auto *tabs = qobject_cast<QTabBar *>(w)) {
                text = tabs->tabToolTip(tabs->tabAt(help->pos()));
            }
            if (text.isEmpty())
                text = w->toolTip();
            if (text.isEmpty()) {
                if (m_owner == w)
                    ToolTip::hideText();
                return false;  // 부모 위젯으로 넘어간다(그 위젯의 도구 설명)
            }
            m_owner = w;
            ToolTip::showText(help->globalPos(), text, w);
            event->accept();
            return true;
        }
        case QEvent::MouseMove:
            if (watched == m_owner && m_index.isValid()) {
                // 항목 보기: 다른 항목으로 가면 숨긴다(새 항목의 설명은 다음 도움말 이벤트에)
                if (QAbstractItemView *view = viewOfViewport(m_owner)) {
                    const auto *me = static_cast<QMouseEvent *>(event);
                    if (view->indexAt(me->position().toPoint()) != QModelIndex(m_index))
                        ToolTip::hideText();
                }
            }
            break;
        case QEvent::Leave:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::Wheel:
        case QEvent::KeyPress:
        case QEvent::Hide:
            if (watched == m_owner)
                ToolTip::hideText();
            break;
        case QEvent::ApplicationDeactivate:
            ToolTip::hideText();
            break;
        default:
            break;
        }
        return false;
    }

private:
    QPointer<QWidget> m_owner;
    QPersistentModelIndex m_index;
};

QPointer<ToolTip> g_shared;
GlobalFilter *g_filter = nullptr;

} // namespace

// ---------------------------------------------------------------------------------------------

ToolTip::ToolTip(QWidget *target)
    : QWidget(target, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(4);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    auto *header = new QHBoxLayout;
    header->setSpacing(6);
    m_icon = new QLabel(this);
    m_titleLabel = new QLabel(this);
    m_titleLabel->setTextFormat(Qt::PlainText);
    header->addWidget(m_icon, 0, Qt::AlignVCenter);
    header->addWidget(m_titleLabel, 1, Qt::AlignVCenter);
    layout->addLayout(header);
    m_body = new QLabel(this);
    m_body->setTextFormat(Qt::AutoText);
    layout->addWidget(m_body);

    m_showTimer = new QTimer(this);
    m_showTimer->setSingleShot(true);
    connect(m_showTimer, &QTimer::timeout, this, [this] { showAt(m_cursor); });
    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &ToolTip::hideTip);
    m_lifeTimer = new QTimer(this);
    m_lifeTimer->setSingleShot(true);
    connect(m_lifeTimer, &QTimer::timeout, this, &ToolTip::hideTip);

    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, [this] {
        rebuild();
        update();
    });
    setTarget(target);
    rebuild();
}

ToolTip::ToolTip(const QString &text, QWidget *target)
    : ToolTip(target)
{
    setText(text);
}

ToolTip::~ToolTip()
{
    if (m_target) {
        m_target->removeEventFilter(this);
        m_target->setProperty(kAttached, QVariant());
    }
}

ToolTip *ToolTip::attach(QWidget *target, const QString &text, const QString &title, glyph::Glyph glyph)
{
    auto *tip = new ToolTip(text, target);
    tip->setTitle(title);
    tip->setGlyph(glyph);
    return tip;
}

void ToolTip::installGlobal(QApplication &app)
{
    if (g_filter)
        return;
    g_filter = new GlobalFilter(&app);
    app.installEventFilter(g_filter);
}

void ToolTip::uninstallGlobal(QApplication &app)
{
    if (!g_filter)
        return;
    app.removeEventFilter(g_filter);
    delete g_filter;
    g_filter = nullptr;
    hideText();
}

bool ToolTip::isGlobalInstalled()
{
    return g_filter != nullptr;
}

ToolTip *ToolTip::shared()
{
    if (!g_shared)
        g_shared = new ToolTip;
    return g_shared;
}

void ToolTip::showText(const QPoint &globalPos, const QString &text, QWidget *owner)
{
    if (text.isEmpty()) {
        hideText();
        return;
    }
    ToolTip *tip = shared();
    // 주인 위젯과 같은 범위(ThemeScope) 색을 쓰도록 — 주인이 지워지면 null
    tip->m_scope = owner;
    tip->setText(text);
    tip->showAt(globalPos);
}

void ToolTip::hideText()
{
    if (g_shared)
        g_shared->hideTip();
}

// ---------------------------------------------------------------------------------------------
// 값

void ToolTip::setTarget(QWidget *target)
{
    if (m_target == target)
        return;
    if (m_target) {
        m_target->removeEventFilter(this);
        m_target->setProperty(kAttached, QVariant());
    }
    m_target = target;
    if (m_target) {
        m_target->installEventFilter(this);
        m_target->setProperty(kAttached, true);
        if (parentWidget() != m_target)
            setParent(m_target, windowFlags());
    }
}

void ToolTip::setText(const QString &text)
{
    m_text = text;
    rebuild();
}

void ToolTip::setTitle(const QString &title)
{
    m_title = title;
    rebuild();
}

void ToolTip::setGlyph(glyph::Glyph glyph)
{
    m_glyph = glyph;
    rebuild();
}

void ToolTip::setCustomWidget(QWidget *widget)
{
    if (m_custom == widget)
        return;
    if (m_custom)
        m_custom->deleteLater();
    m_custom = widget;
    if (widget) {
        widget->setParent(this);
        static_cast<QVBoxLayout *>(layout())->addWidget(widget);
    }
    rebuild();
}

void ToolTip::setPlacement(Placement placement)
{
    m_placement = placement;
    update();
}

void ToolTip::setTailVisible(bool visible)
{
    m_tail = visible;
    rebuild();
}

void ToolTip::setTail(Qt::Edge edge, int offset)
{
    m_tail = edge != 0;
    m_tailEdge = edge;
    rebuild();
    const QRect panel = panelRect();
    const int length = (edge == Qt::TopEdge || edge == Qt::BottomEdge) ? panel.width() : panel.height();
    m_tailOffset = offset >= 0 ? offset : length / 2;
    update();
}

void ToolTip::setShowDelay(int ms)
{
    m_showDelay = ms;
}

void ToolTip::setHideDelay(int ms)
{
    m_hideDelay = std::max(0, ms);
}

void ToolTip::setDuration(int ms)
{
    m_duration = ms;
}

void ToolTip::setMaximumTextWidth(int px)
{
    m_maxTextWidth = std::max(60, px);
    rebuild();
}

// ---------------------------------------------------------------------------------------------
// 모양

bool ToolTip::balloon() const
{
    // 시안2: 제목이나 꼬리가 있으면 XP 풍선 도움말(둥근 모서리 + 꼬리)
    return !m_title.isEmpty() || m_tail;
}

QMargins ToolTip::chromeMargins() const
{
    const fs::ThemeColors &tc = fs::themeColorsFor(colorSource());
    // 그림자 자리: 시안1 둘레 8(아래로 치우친 부드러운 그림자), 시안2 오른쪽 · 아래 3(XP)
    QMargins m = tc.isWatercolor() ? QMargins(1, 1, 4, 4) : QMargins(8, 6, 8, 10);
    const int tail = tc.isWatercolor() ? 10 : kTailLength;
    switch (m_tailEdge) {
    case Qt::TopEdge: m.setTop(m.top() + tail); break;
    case Qt::BottomEdge: m.setBottom(m.bottom() + tail); break;
    case Qt::LeftEdge: m.setLeft(m.left() + tail); break;
    case Qt::RightEdge: m.setRight(m.right() + tail); break;
    }
    return m;
}

QRect ToolTip::panelRect() const
{
    return rect().marginsRemoved(chromeMargins());
}

void ToolTip::applyColors()
{
    const QWidget *source = colorSource();
    const fs::ThemeColors &tc = fs::themeColorsFor(source);
    QColor fg = tc[T::Fg];
    if (tc.isWatercolor())
        fg = fs::watercolorChrome(tc.variant()).tipFg;
    for (QLabel *label : {m_titleLabel, m_body}) {
        QPalette pal = label->palette();
        pal.setColor(QPalette::WindowText, fg);
        pal.setColor(QPalette::Text, fg);
        pal.setColor(QPalette::Link, tc[T::AccentFg]);
        label->setPalette(pal);
    }
    m_body->setFont(fs::pixelFont(font(), 12));
    m_titleLabel->setFont(fs::pixelFont(font(), tc.isWatercolor() ? 12 : 12.5, tc.isWatercolor() ? QFont::Bold : QFont::DemiBold));
    if (m_glyph != glyph::None) {
        // 풍선 아이콘은 뜻 색: 정보 = 강조, 경고 = 주의, 확인 = 성공
        const fs::Glyph g = glyph::toStyle(m_glyph);
        const QColor c = m_glyph == glyph::Info      ? tc[T::Accent]
                       : m_glyph == glyph::Warning   ? tc[T::Warn]
                       : m_glyph == glyph::Check     ? tc[T::Ok]
                                                     : tc[T::Fg2];
        const qreal dpr = devicePixelRatioF();
        QPixmap pm(QSize(16, 16) * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        if (g == fs::Glyph::Shield)
            fs::paintGlyph(&p, g, QRectF(0, 0, 16, 16), tc[T::Shield], tc[T::Shield2]);
        else
            fs::paintGlyph(&p, g, QRectF(0, 0, 16, 16), c);
        p.end();
        m_icon->setPixmap(pm);
    }
}

void ToolTip::rebuild()
{
    const fs::ThemeColors &tc = fs::themeColorsFor(colorSource());
    const bool watercolor = tc.isWatercolor();
    const bool hasTitle = !m_title.isEmpty();
    m_icon->setVisible(m_glyph != glyph::None);
    m_titleLabel->setVisible(hasTitle);
    m_titleLabel->setText(m_title);
    const bool custom = m_custom != nullptr;
    m_body->setVisible(!custom && !m_text.isEmpty());
    m_body->setText(m_text);
    // 짧은 글은 한 줄, 길면 최대 폭에서 줄 바꿈
    const QFontMetrics fm(fs::pixelFont(font(), 12));
    const QString plain = Qt::mightBeRichText(m_text) ? QTextDocumentFragment::fromHtml(m_text).toPlainText() : m_text;
    int longest = 0;
    for (const QString &line : plain.split(u'\n'))
        longest = std::max(longest, fm.horizontalAdvance(line));
    m_body->setWordWrap(longest > m_maxTextWidth);
    m_body->setMaximumWidth(m_maxTextWidth);
    if (m_body->wordWrap())
        m_body->setMinimumWidth(std::min(m_maxTextWidth, longest));
    else
        m_body->setMinimumWidth(0);
    applyColors();

    // 여백: 그림자 · 꼬리 + 안쪽 여백(시안1 9 · 6, 시안2 칸 5 · 3, 풍선 10 · 8)
    const QMargins pad = watercolor ? (balloon() ? QMargins(10, 8, 10, 9) : QMargins(5, 3, 5, 3))
                                    : (hasTitle ? QMargins(12, 9, 12, 10) : QMargins(9, 6, 9, 7));
    const QMargins chrome = chromeMargins();
    layout()->setContentsMargins(chrome.left() + pad.left(), chrome.top() + pad.top(), chrome.right() + pad.right(),
                                 chrome.bottom() + pad.bottom());
    layout()->activate();
    adjustSize();
    update();
}

bool ToolTip::event(QEvent *event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        rebuild();
    return QWidget::event(event);
}

void ToolTip::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(colorSource());
    const bool watercolor = tc.isWatercolor();
    const QRectF panel = QRectF(panelRect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = watercolor ? (balloon() ? 7.0 : 0.0) : 6.0;

    // 풍선 모양 = 둥근 칸 + 꼬리
    QPainterPath shape;
    shape.addRoundedRect(panel, radius, radius);
    if (m_tailEdge != 0) {
        const qreal length = watercolor ? 10 : kTailLength;
        const qreal half = kTailWidth / 2.0;
        QPolygonF tail;
        const qreal o = m_tailOffset;
        switch (m_tailEdge) {
        case Qt::TopEdge:
            tail << QPointF(panel.left() + o - half, panel.top() + 1) << QPointF(panel.left() + o, panel.top() - length)
                 << QPointF(panel.left() + o + half, panel.top() + 1);
            break;
        case Qt::BottomEdge:
            tail << QPointF(panel.left() + o - half, panel.bottom() - 1) << QPointF(panel.left() + o, panel.bottom() + length)
                 << QPointF(panel.left() + o + half, panel.bottom() - 1);
            break;
        case Qt::LeftEdge:
            tail << QPointF(panel.left() + 1, panel.top() + o - half) << QPointF(panel.left() - length, panel.top() + o)
                 << QPointF(panel.left() + 1, panel.top() + o + half);
            break;
        case Qt::RightEdge:
            tail << QPointF(panel.right() - 1, panel.top() + o - half) << QPointF(panel.right() + length, panel.top() + o)
                 << QPointF(panel.right() - 1, panel.top() + o + half);
            break;
        }
        QPainterPath tailPath;
        tailPath.addPolygon(tail);
        tailPath.closeSubpath();
        shape = shape.united(tailPath).simplified();
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, !watercolor || balloon());
    p.setPen(Qt::NoPen);
    if (watercolor) {
        // XP: 오른쪽 아래로 3 px 밀린 반투명 그림자
        p.setBrush(QColor(0, 0, 0, 60));
        p.drawPath(shape.translated(3, 3));
        const fs::WatercolorChrome &x = fs::watercolorChrome(tc.variant());
        p.setBrush(x.tipBg);
        p.setPen(QPen(x.tipLine, 1));
        p.drawPath(shape);
        return;
    }
    // 시안1: 아래로 2 px 치우친 부드러운 그림자(넓은 링부터 겹쳐 가장자리로 갈수록 옅다) + Surface + 1 px 선
    QColor shadow = tc[T::Shadow];
    const qreal base = shadow.alphaF();
    const QPainterPath lifted = shape.translated(0, 2);
    for (int i = 6; i >= 1; --i) {
        QPainterPathStroker stroker;
        stroker.setWidth(i * 2.0);
        stroker.setJoinStyle(Qt::RoundJoin);
        shadow.setAlphaF(base * 0.12 * (7 - i) / 6.0);
        p.setBrush(shadow);
        p.drawPath(stroker.createStroke(lifted).united(lifted));
    }
    p.setBrush(tc[T::Surface]);
    p.setPen(QPen(tc[T::Line], 1));
    p.drawPath(shape);
}

// ---------------------------------------------------------------------------------------------
// 띄우기 · 숨기기

void ToolTip::showAt(const QPoint &globalPos)
{
    if (m_text.isEmpty() && !m_custom && m_title.isEmpty())
        return;
    m_hideTimer->stop();
    m_cursor = globalPos;
    placeNear(globalPos);
    if (!isVisible()) {
        if (animationsSupported()) {
            setWindowOpacity(0.0);
            show();
            fadeTo(1.0);
        } else {
            show();
        }
    }
    raise();
    if (m_duration >= 0) {
        const int plain = int(QTextDocumentFragment::fromHtml(m_text).toPlainText().size());
        m_lifeTimer->start(m_duration > 0 ? m_duration : 10000 + 40 * plain);
    } else {
        m_lifeTimer->stop();
    }
}

void ToolTip::hideTip()
{
    m_showTimer->stop();
    m_hideTimer->stop();
    m_lifeTimer->stop();
    if (!isVisible())
        return;
    if (animationsSupported() && windowOpacity() > 0.05)
        fadeTo(0.0);
    else
        hide();
}

void ToolTip::fadeTo(qreal opacity)
{
    if (m_fade)
        m_fade->stop();
    else {
        m_fade = new QVariantAnimation(this);
        m_fade->setDuration(kFadeMs);
        connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) { setWindowOpacity(v.toReal()); });
        connect(m_fade, &QVariantAnimation::finished, this, [this] {
            if (windowOpacity() < 0.05)
                hide();
        });
    }
    m_fade->setStartValue(windowOpacity());
    m_fade->setEndValue(opacity);
    m_fade->start();
}

void ToolTip::placeNear(const QPoint &globalPos)
{
    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen && m_target)
        screen = m_target->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    // 꼬리 없이 잰 풍선 크기로 쪽을 정하고, 꼬리 쪽 여백을 넣어 다시 잰다
    m_tailEdge = Qt::Edge(0);
    rebuild();
    QSize panel = panelRect().size();
    const QRect anchor = (m_placement == Cursor || !m_target)
                             ? QRect(globalPos, QSize(1, 1))
                             : QRect(m_target->mapToGlobal(QPoint(0, 0)), m_target->size());
    const int tail = m_tail ? (fs::themeColorsFor(colorSource()).isWatercolor() ? 10 : kTailLength) : 0;

    Placement where = m_placement;
    QPoint topLeft;  // 풍선(panel) 왼쪽 위
    const auto fitsBelow = [&] { return anchor.bottom() + kGap + tail + panel.height() <= avail.bottom(); };
    const auto fitsAbove = [&] { return anchor.top() - kGap - tail - panel.height() >= avail.top(); };
    const auto fitsRight = [&] { return anchor.right() + kGap + tail + panel.width() <= avail.right(); };
    const auto fitsLeft = [&] { return anchor.left() - kGap - tail - panel.width() >= avail.left(); };
    if (where == Below && !fitsBelow() && fitsAbove())
        where = Above;
    else if (where == Above && !fitsAbove() && fitsBelow())
        where = Below;
    else if (where == Right && !fitsRight() && fitsLeft())
        where = Left;
    else if (where == Left && !fitsLeft() && fitsRight())
        where = Right;

    if (where == Cursor) {
        // 시스템 도구 설명처럼 마우스 오른쪽 아래(2, 20). 아래가 모자라면 위로.
        const bool below = globalPos.y() + 20 + tail + panel.height() <= avail.bottom();
        topLeft = below ? QPoint(globalPos.x() + 2, globalPos.y() + 20 + tail)
                        : QPoint(globalPos.x() + 2, globalPos.y() - 6 - tail - panel.height());
        if (m_tail)
            m_tailEdge = below ? Qt::TopEdge : Qt::BottomEdge;
    } else if (where == Below || where == Above) {
        topLeft.setX(anchor.center().x() - panel.width() / 2);
        topLeft.setY(where == Below ? anchor.bottom() + 1 + kGap + tail : anchor.top() - kGap - tail - panel.height());
        if (m_tail)
            m_tailEdge = where == Below ? Qt::TopEdge : Qt::BottomEdge;
    } else {
        topLeft.setY(anchor.center().y() - panel.height() / 2);
        topLeft.setX(where == Right ? anchor.right() + 1 + kGap + tail : anchor.left() - kGap - tail - panel.width());
        if (m_tail)
            m_tailEdge = where == Right ? Qt::LeftEdge : Qt::RightEdge;
    }
    // 화면 안으로
    topLeft.setX(std::clamp(topLeft.x(), avail.left() + 2, std::max(avail.left() + 2, avail.right() - panel.width() - 2)));
    topLeft.setY(std::clamp(topLeft.y(), avail.top() + 2, std::max(avail.top() + 2, avail.bottom() - panel.height() - 2)));

    // 꼬리는 대상(또는 마우스) 가운데를 가리킨다 — 모서리 둥근 곳은 피한다
    const QPoint aim = where == Cursor ? globalPos : anchor.center();
    const bool horizontalEdge = m_tailEdge == Qt::TopEdge || m_tailEdge == Qt::BottomEdge;
    const int along = horizontalEdge ? aim.x() - topLeft.x() : aim.y() - topLeft.y();
    const int length = horizontalEdge ? panel.width() : panel.height();
    m_tailOffset = std::clamp(along, 8 + kTailWidth / 2, std::max(8 + kTailWidth / 2, length - 8 - kTailWidth / 2));

    rebuild();  // 꼬리 쪽 여백을 넣어 크기를 다시
    const QMargins m = chromeMargins();
    move(topLeft - QPoint(m.left(), m.top()));
}

// ---------------------------------------------------------------------------------------------
// 대상 이벤트

bool ToolTip::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_target)
        return QWidget::eventFilter(watched, event);
    switch (event->type()) {
    case QEvent::ToolTip:
        if (m_showDelay < 0)
            showAt(static_cast<QHelpEvent *>(event)->globalPos());
        return true;  // Qt 기본 도구 설명은 띄우지 않는다
    case QEvent::Enter:
        m_hideTimer->stop();
        if (m_showDelay >= 0) {
            m_cursor = QCursor::pos();
            m_showTimer->start(m_showDelay);
        }
        break;
    case QEvent::MouseMove:
        if (m_showTimer->isActive())
            m_cursor = QCursor::pos();
        break;
    case QEvent::Leave:
        m_showTimer->stop();
        if (isVisible()) {
            if (m_hideDelay > 0)
                m_hideTimer->start(m_hideDelay);
            else
                hideTip();
        }
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::Wheel:
    case QEvent::KeyPress:
    case QEvent::Hide:
    case QEvent::WindowDeactivate:
        hideTip();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace fm::ui
