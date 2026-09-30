// Button · Switch · Card · ProgressBar — 스타일 속성을 켜 두는 얇은 서브클래스 (시안1 · 시안2 공통).

#include "fmwidgets/Button.h"
#include "fmwidgets/Card.h"
#include "fmwidgets/ProgressBar.h"
#include "fmwidgets/Switch.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>

#include <QEvent>
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
    QColor color = tc[T::Fg];
    switch (m_role) {
    case Primary: color = tc.isWatercolor() ? tc[T::Fg] : tc[T::OnAccent]; break;  // 시안2 기본 단추는 채움이 아님
    case Danger: color = tc[T::OnDanger]; break;
    case Link: color = tc[T::AccentFg]; break;
    case Normal:
    case Subtle: break;
    }
    const int px = m_role == Link ? 12 : 16;
    const fs::Glyph g = glyph::toStyle(m_glyph);
    setIcon(g == fs::Glyph::Shield ? fs::shieldIcon(tc, px) : fs::glyphIcon(g, color, px));
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
