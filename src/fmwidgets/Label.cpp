#include "fmwidgets/Label.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeManager.h>

#include <QEvent>
#include <QPainter>
#include <QStyle>

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

struct RoleSpec
{
    qreal px;
    QFont::Weight weight;
    T color;
    bool wrap;
    bool tnum;
};

RoleSpec specOf(Label::TextRole role)
{
    switch (role) {
    case Label::Body:        return {13, QFont::Normal, T::Fg, false, false};
    case Label::FieldLabel:  return {12, QFont::DemiBold, T::Fg2, false, false};
    case Label::Help:        return {12, QFont::Normal, T::Fg3, true, false};
    case Label::Meta:        return {12, QFont::Normal, T::Fg2, false, false};
    case Label::PathMeta:    return {12, QFont::Normal, T::Fg3, false, false};
    case Label::StatLabel:   return {12, QFont::Normal, T::Fg3, false, false};
    case Label::StatValue:   return {15, QFont::DemiBold, T::Fg, false, true};
    case Label::BigNumber:   return {26, QFont::DemiBold, T::Fg, false, true};
    case Label::Caption:     return {12, QFont::DemiBold, T::Fg2, false, false};
    case Label::Minor:       return {12, QFont::Normal, T::Fg3, false, false};
    case Label::Summary:     return {12.5, QFont::Normal, T::Fg2, false, true};
    case Label::Heading:     return {15, QFont::DemiBold, T::Fg, true, false};
    case Label::Description: return {13, QFont::Normal, T::Fg2, true, false};
    case Label::PageTitle:   return {20, QFont::DemiBold, T::Fg, false, false};
    case Label::SectionTitle: return {13, QFont::DemiBold, T::Fg, false, false};
    }
    return {13, QFont::Normal, T::Fg, false, false};
}

} // namespace

Label::Label(QWidget *parent)
    : Label(QString(), Body, parent)
{
}

Label::Label(const QString &text, QWidget *parent)
    : Label(text, Body, parent)
{
}

Label::Label(const QString &text, TextRole role, QWidget *parent)
    : QLabel(text, parent)
    , m_role(role)
{
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, &Label::apply);
    apply();
}

void Label::setTextRole(TextRole role)
{
    if (m_role == role)
        return;
    m_role = role;
    apply();
}

void Label::setTone(Tone tone)
{
    if (m_tone == tone)
        return;
    m_tone = tone;
    apply();
}

void Label::setMonospace(bool on)
{
    if (m_mono == on)
        return;
    m_mono = on;
    apply();
}

void Label::setTabularNumbers(bool on)
{
    if (m_tnum == on)
        return;
    m_tnum = on;
    apply();
}

void Label::setElideMode(Qt::TextElideMode mode)
{
    if (m_elide == mode)
        return;
    m_elide = mode;
    if (mode != Qt::ElideNone)
        setWordWrap(false);
    updateGeometry();
    update();
}

void Label::apply()
{
    if (m_applying)
        return;
    m_applying = true;
    const RoleSpec spec = specOf(m_role);
    QFont f = m_mono ? fs::monoFont(spec.px, spec.weight) : fs::pixelFont(QFont(), spec.px, spec.weight);
    if (spec.tnum || m_tnum)
        f = fs::withTabularNumbers(f);
    setFont(f);
    if (m_elide == Qt::ElideNone)
        setWordWrap(spec.wrap);

    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QColor color = tc[spec.color];
    switch (m_tone) {
    case Default: break;
    case Secondary: color = tc[T::Fg2]; break;
    case Muted: color = tc[T::Fg3]; break;
    case Accent: color = tc[T::AccentFg]; break;
    case Danger: color = tc[T::Danger]; break;
    case Warn: color = tc[T::Warn]; break;
    case Ok: color = tc[T::Ok]; break;
    }
    QPalette pal = palette();
    for (const auto group : {QPalette::Active, QPalette::Inactive})
        pal.setColor(group, QPalette::WindowText, color);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, tc[T::Fg3]);
    setPalette(pal);
    m_applying = false;
}

QSize Label::minimumSizeHint() const
{
    if (m_elide == Qt::ElideNone)
        return QLabel::minimumSizeHint();
    // 줄여 그릴 수 있으므로 폭은 조금만 요구한다.
    return QSize(fontMetrics().horizontalAdvance(u'…') * 3, QLabel::minimumSizeHint().height());
}

void Label::changeEvent(QEvent *event)
{
    QLabel::changeEvent(event);
    // 상위의 ThemeScope가 바뀌면(팔레트 전파) 토큰 색을 다시 읽는다.
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange
        || event->type() == QEvent::ParentChange)
        apply();
}

void Label::paintEvent(QPaintEvent *event)
{
    if (m_elide == Qt::ElideNone || textFormat() == Qt::RichText) {
        QLabel::paintEvent(event);
        return;
    }
    QPainter p(this);
    const QRect r = contentsRect().adjusted(margin(), margin(), -margin(), -margin());
    const QString full = text();
    const QString shown = fontMetrics().elidedText(full, m_elide, r.width());
    // 줄였을 때만 전체 글을 도구 설명으로
    const QString wantedTip = shown != full ? full : QString();
    if (toolTip() != wantedTip)
        setToolTip(wantedTip);
    style()->drawItemText(&p, r, int(alignment()), palette(), isEnabled(), shown, foregroundRole());
}

} // namespace fm::ui
