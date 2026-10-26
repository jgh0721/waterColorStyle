// Button · Switch · Card · ProgressBar — 스타일 속성을 켜 두는 얇은 서브클래스 (시안1 · 시안2 공통).

#include "fmwidgets/Button.h"
#include "fmwidgets/Card.h"
#include "fmwidgets/ProgressBar.h"
#include "fmwidgets/Switch.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

// ---------------------------------------------------------------------------------------------
// Button

Button::Button(QWidget *parent)
    : Button(QString(), parent)
{
}

Button::Button(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
{
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, &Button::refreshIcon);
}

void Button::setRole(Role role)
{
    if (m_role == role)
        return;
    const bool wasLink = m_role == Link;
    m_role = role;
    fs::ButtonRole r = fs::ButtonRole::Normal;
    switch (role) {
    case Primary: r = fs::ButtonRole::Primary; break;
    case Danger: r = fs::ButtonRole::Danger; break;
    case Subtle: r = fs::ButtonRole::Subtle; break;
    case Link: r = fs::ButtonRole::Link; break;
    case Normal: break;
    }
    fs::setButtonRole(this, r);
    // 링크 단추는 목업 .linkbtn: 12.5 px 글자, 아이콘 12
    if (role == Link) {
        setFont(fs::pixelFont(font(), 12.5));
        setIconSize(QSize(12, 12));
        setAutoDefault(false);
    } else if (wasLink) {
        setFont(QFont());
        setAttribute(Qt::WA_SetFont, false);
        setIconSize(QSize(16, 16));
    }
    refreshIcon();
}

void Button::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    fs::setSmall(this, compact);
}

void Button::setKeyHint(const QString &keys)
{
    if (m_keyHint == keys)
        return;
    m_keyHint = keys;
    fs::setKeyHint(this, keys);
}

void Button::setGlyph(glyph::Glyph glyph)
{
    if (m_glyph == glyph)
        return;
    m_glyph = glyph;
    refreshIcon();
}

void Button::refreshIcon()
{
    if (m_glyph == glyph::None)
        return;
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    const bool wc = tc.isWatercolor();
    // 아이콘은 글자색을 따른다 — 시안2는 마우스 올림 · 누름에 --x-hfg, 사용 안 함은 --x-dfg(06 §6.4 D2)
    fs::GlyphStateColors colors;
    colors.normal = tc[T::Fg];
    switch (m_role) {
    case Primary: colors.normal = wc ? tc[T::Fg] : tc[T::OnAccent]; break;  // 시안2 기본 단추는 채움이 아님
    case Danger: colors.normal = tc[T::OnDanger]; break;
    case Link: colors.normal = tc[T::AccentFg]; colors.active = tc[T::Accent]; break;
    case Normal:
    case Subtle: break;
    }
    if (wc) {
        const fs::WatercolorChrome &x = fs::watercolorChrome(tc.variant());
        colors.disabled = x.disFg;
        if (m_role != Link)
            colors.active = m_role == Danger ? tc[T::OnDanger] : x.hoverFg;
    } else {
        colors.disabled = tc[T::Fg3];
    }
    const int px = m_role == Link ? 12 : 16;
    const fs::Glyph g = glyph::toStyle(m_glyph);
    setIcon(g == fs::Glyph::Shield ? fs::shieldIcon(tc, px) : fs::glyphIcon(g, colors, px));
}

void Button::changeEvent(QEvent *event)
{
    QPushButton::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        refreshIcon();
}

// ---------------------------------------------------------------------------------------------
// Switch

Switch::Switch(QWidget *parent)
    : Switch(QString(), parent)
{
}

Switch::Switch(const QString &text, QWidget *parent)
    : QCheckBox(text, parent)
{
    fs::setSwitch(this);
    connect(this, &QCheckBox::toggled, this, &Switch::syncText);
}

void Switch::setOnText(const QString &text)
{
    m_onText = text;
    syncText();
}

void Switch::setOffText(const QString &text)
{
    m_offText = text;
    syncText();
}

void Switch::checkStateSet()
{
    QCheckBox::checkStateSet();
    syncText();
}

void Switch::syncText()
{
    if (m_onText.isEmpty() || m_offText.isEmpty())
        return;
    setText(isChecked() ? m_onText : m_offText);
}

// ---------------------------------------------------------------------------------------------
// Card

Card::Card(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setLineWidth(1);
    fs::setCard(this);
}

Card::Card(const QString &title, QWidget *parent)
    : Card(parent)
{
    setTitle(title);
}

void Card::setTitle(const QString &title)
{
    if (m_title == title)
        return;
    m_title = title;
    applyTitleMargins();
    update();
}

// QFrame은 틀 두께가 바뀔 때(스타일 적용 · 글꼴 변경) 내용 여백을 틀 두께로 다시 쓴다 — 그 뒤에 띠 높이를 다시 건다.
void Card::applyTitleMargins()
{
    const int f = frameWidth();
    if (m_title.isEmpty())
        setContentsMargins(f, f, f, f);
    else
        setContentsMargins(f, kHeaderHeight, f, f);
}

bool Card::event(QEvent *event)
{
    const bool handled = QFrame::event(event);
    switch (event->type()) {
    case QEvent::Polish:
    case QEvent::StyleChange:
    case QEvent::FontChange:
    case QEvent::Show:
        if (!m_title.isEmpty())
            applyTitleMargins();
        break;
    default:
        break;
    }
    return handled;
}

void Card::setMeta(const QString &meta)
{
    m_meta = meta;
    update();
}

void Card::setHeaderWidget(QWidget *widget)
{
    m_headerWidget = widget;
    if (widget) {
        widget->setParent(this);
        widget->show();
        placeHeaderWidget();
    }
}

void Card::placeHeaderWidget()
{
    if (!m_headerWidget)
        return;
    // 제목 오른쪽 남는 폭을 모두 쓸 수 있다(보조 글자는 그 사이에서 줄어든다)
    const int titleWidth = QFontMetrics(fs::pixelFont(font(), 13, QFont::DemiBold)).horizontalAdvance(m_title);
    const QSize s = m_headerWidget->sizeHint().boundedTo(QSize(std::max(0, width() - 14 - titleWidth - 20), kHeaderHeight - 4));
    m_headerWidget->setGeometry(width() - 10 - s.width(), (kHeaderHeight - s.height()) / 2, s.width(), s.height());
}

void Card::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    if (!m_title.isEmpty())
        applyTitleMargins();
    placeHeaderWidget();
}

void Card::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    if (m_title.isEmpty())
        return;
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QPainter p(this);
    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QRectF band(card.left(), card.top(), card.width(), kHeaderHeight);
    if (tc.isPaperFluent()) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath clip;
        clip.addRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), 7.5, 7.5);
        p.setClipPath(clip);
        QLinearGradient wash(band.topLeft(), band.bottomLeft());
        wash.setColorAt(0, tc[T::Alt]);
        wash.setColorAt(1, tc[T::Surface]);
        p.fillRect(band.adjusted(1, 1, -1, 0), wash);
        p.restore();
    }
    p.fillRect(QRectF(1, kHeaderHeight - 1, width() - 2, 1), tc[T::Grid]);
    const int right = m_headerWidget ? m_headerWidget->x() - 8 : width() - 14;
    QFont f = fs::pixelFont(font(), 13, QFont::DemiBold);
    p.setFont(f);
    p.setPen(isEnabled() ? tc[T::Fg] : tc[T::Fg3]);
    const QFontMetrics tm(f);
    const int titleWidth = std::min(tm.horizontalAdvance(m_title), std::max(0, right - 14));
    p.drawText(QRect(14, 0, titleWidth, kHeaderHeight - 1), Qt::AlignLeft | Qt::AlignVCenter, tm.elidedText(m_title, Qt::ElideRight, titleWidth));
    if (!m_meta.isEmpty()) {
        const QFont mf = fs::pixelFont(font(), 11.5);
        p.setFont(mf);
        p.setPen(tc[T::Fg3]);
        const int left = 14 + titleWidth + 12;
        p.drawText(QRect(left, 0, std::max(0, right - left), kHeaderHeight - 1), (m_headerWidget ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter,
                   QFontMetrics(mf).elidedText(m_meta, Qt::ElideRight, std::max(0, right - left)));
    }
}

// ---------------------------------------------------------------------------------------------
// ProgressBar

namespace {
constexpr int kBusyIntervalMs = 33;      // 약 30 fps
constexpr qreal kBusyCycleMs = 1600.0;   // 한 번 가로지르는 시간
} // namespace

ProgressBar::ProgressBar(QWidget *parent)
    : QProgressBar(parent)
{
    setTextVisible(false);
}

void ProgressBar::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    applySize();
}

void ProgressBar::setThickness(Thickness thickness)
{
    if (m_thickness == thickness)
        return;
    m_thickness = thickness;
    applySize();
}

void ProgressBar::applySize()
{
    fs::SizeVariant v = fs::SizeVariant::Normal;
    if (m_thickness == ThinBar)
        v = fs::SizeVariant::Thin;
    else if (m_thickness == ThickBar)
        v = fs::SizeVariant::Thick;
    else if (m_compact)
        v = fs::SizeVariant::Small;
    fs::setSizeVariant(this, v);
}

void ProgressBar::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    fs::setProgressState(this, state == Paused ? u"paused"_s
                             : state == Error  ? u"error"_s
                                               : QString());
}

void ProgressBar::paintEvent(QPaintEvent *event)
{
    // setRange는 가상 함수가 아니므로 그릴 때 진행률 모름 상태를 확인해 움직임을 켜고 끈다.
    const bool busy = minimum() == maximum();
    if (busy && isVisible()) {
        if (!m_busyTimer) {
            m_busyTimer = new QTimer(this);
            m_busyTimer->setInterval(kBusyIntervalMs);
            connect(m_busyTimer, &QTimer::timeout, this, &ProgressBar::tickBusy);
        }
        if (!m_busyTimer->isActive())
            m_busyTimer->start();
        setProperty(fs::props::kBusyPhase, m_busyPhase);
    } else if (m_busyTimer && m_busyTimer->isActive()) {
        m_busyTimer->stop();
        setProperty(fs::props::kBusyPhase, QVariant());
    }
    QProgressBar::paintEvent(event);
}

void ProgressBar::hideEvent(QHideEvent *event)
{
    if (m_busyTimer)
        m_busyTimer->stop();
    QProgressBar::hideEvent(event);
}

void ProgressBar::tickBusy()
{
    if (minimum() != maximum()) {
        m_busyTimer->stop();
        setProperty(fs::props::kBusyPhase, QVariant());
        update();
        return;
    }
    m_busyPhase += kBusyIntervalMs / kBusyCycleMs;
    if (m_busyPhase > 1.0)
        m_busyPhase -= 1.0;
    update();
}

} // namespace fm::ui
