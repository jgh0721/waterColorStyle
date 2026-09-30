// Button · Switch · Card · ProgressBar — 스타일 속성을 켜 두는 얇은 서브클래스 (시안1 · 시안2 공통).

#include "fmwidgets/Button.h"
#include "fmwidgets/Card.h"
#include "fmwidgets/ProgressBar.h"
#include "fmwidgets/Switch.h"

#include <fmstyle/StyleProps.h>

using namespace Qt::StringLiterals;

namespace fm::ui {

// ---------------------------------------------------------------------------------------------
// Button

Button::Button(QWidget *parent)
    : QPushButton(parent)
{
}

Button::Button(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
{
}

void Button::setRole(Role role)
{
    if (m_role == role)
        return;
    m_role = role;
    fm::style::ButtonRole r = fm::style::ButtonRole::Normal;
    switch (role) {
    case Primary: r = fm::style::ButtonRole::Primary; break;
    case Danger: r = fm::style::ButtonRole::Danger; break;
    case Subtle: r = fm::style::ButtonRole::Subtle; break;
    case Normal: break;
    }
    fm::style::setButtonRole(this, r);
}

void Button::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    fm::style::setSmall(this, compact);
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
    fm::style::setSwitch(this);
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
    fm::style::setCard(this);
}

// ---------------------------------------------------------------------------------------------
// ProgressBar

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
    fm::style::setSmall(this, compact);
}

void ProgressBar::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    fm::style::setProgressState(this, state == Paused ? u"paused"_s
                                    : state == Error  ? u"error"_s
                                                      : QString());
}

} // namespace fm::ui
