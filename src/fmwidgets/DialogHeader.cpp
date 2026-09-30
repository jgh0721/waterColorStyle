#include "fmwidgets/DialogHeader.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeManager.h>

#include <QEvent>
#include <QPainter>
#include <QTextLayout>

using namespace Qt::StringLiterals;

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

constexpr int kBadge = 40;
constexpr int kTitleLine = 20;
constexpr int kSubtitleLine = 17;     // .sub — 12 px
constexpr int kSubtitleGap = 2;
constexpr int kDescLine = 19;         // .desc — 13 px (여러 줄)
constexpr int kDescGap = 4;

fs::Tone toStyleTone(DialogHeader::Tone tone)
{
    switch (tone) {
    case DialogHeader::Info: return fs::Tone::Info;
    case DialogHeader::Danger: return fs::Tone::Danger;
    case DialogHeader::Mute: return fs::Tone::Mute;
    case DialogHeader::Warn: return fs::Tone::Warn;
    case DialogHeader::Ok: return fs::Tone::Ok;
    }
    return fs::Tone::Info;
}

/// 여러 줄 설명(.desc) — 목업 줄높이 19에 맞춰 줄마다 19 px 칸의 가운데에 놓는다.
/// painter가 null이면 줄 수만 센다.
int layoutDescription(QPainter *painter, const QString &text, const QFont &font, const QRect &rect)
{
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    int lines = 0;
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(std::max(1, rect.width()));
        line.setPosition(QPointF(0, lines * kDescLine + (kDescLine - line.height()) / 2.0));
        ++lines;
    }
    layout.endLayout();
    if (painter)
        layout.draw(painter, rect.topLeft());
    return lines;
}

} // namespace

DialogHeader::DialogHeader(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, qOverload<>(&QWidget::update));
}

void DialogHeader::setTitle(const QString &title)
{
    m_title = title;
    refreshAccessibility();
    update();
}

void DialogHeader::setTitleTo(const QString &titleTo)
{
    m_titleTo = titleTo;
    refreshAccessibility();
    update();
}

void DialogHeader::setSubtitle(const QString &subtitle)
{
    m_subtitle = subtitle;
    refreshAccessibility();
    updateGeometry();
    update();
}

void DialogHeader::setGlyph(glyph::Glyph glyph)
{
    m_glyph = glyph;
    update();
}

void DialogHeader::setTone(Tone tone)
{
    m_tone = tone;
    update();
}

void DialogHeader::setSubtitleElide(Qt::TextElideMode mode)
{
    m_elide = mode;
    update();
}

void DialogHeader::setSubtitleWrap(bool wrap)
{
    if (m_wrap == wrap)
        return;
    m_wrap = wrap;
    QSizePolicy sp = sizePolicy();
    sp.setHeightForWidth(wrap);
    setSizePolicy(sp);
    updateGeometry();
    update();
}

QFont DialogHeader::titleFont() const
{
    return fs::pixelFont(font(), 15, QFont::DemiBold);
}

QFont DialogHeader::subtitleFont() const
{
    return fs::pixelFont(font(), m_wrap ? 13 : 12);
}

int DialogHeader::heightForWidth(int width) const
{
    int text = kTitleLine;
    if (!m_subtitle.isEmpty()) {
        if (m_wrap) {
            const int w = std::max(1, width - textLeft());
            const int lines = std::max(1, layoutDescription(nullptr, m_subtitle, subtitleFont(), QRect(0, 0, w, 0)));
            text += kDescGap + lines * kDescLine;
        } else {
            text += kSubtitleGap + kSubtitleLine;
        }
    }
    return std::max(kBadge, text);
}

QSize DialogHeader::sizeHint() const
{
    return QSize(480, heightForWidth(480));
}

QSize DialogHeader::minimumSizeHint() const
{
    return QSize(textLeft() + 120, m_wrap ? heightForWidth(textLeft() + 240) : heightForWidth(0));
}

void DialogHeader::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QPainter p(this);

    // 배지
    const QRectF badge(0, 0, kBadge, kBadge);
    const fs::Tone tone = toStyleTone(m_tone);
    fs::paintBadge(&p, badge, tone, tc);
    if (m_glyph != glyph::None) {
        const fs::Glyph g = glyph::toStyle(m_glyph);
        // 권한 배지: 방패 24(두 색), 위험 삼각형 22, 나머지 20 (docs/specs/03 §1)
        const qreal px = g == fs::Glyph::Shield ? 24 : (g == fs::Glyph::Warning || g == fs::Glyph::LockKeyhole) ? 22 : 20;
        QRectF ir(0, 0, px, px);
        ir.moveCenter(badge.center());
        if (g == fs::Glyph::Shield)
            fs::paintGlyph(&p, g, ir, tc[T::Shield], tc[T::Shield2]);
        else
            fs::paintGlyph(&p, g, ir, fs::badgeColors(tone, tc).icon);
    }

    // 제목 (+ "→ 대상")
    const int left = textLeft();
    const int width = std::max(0, this->width() - left);
    p.setFont(titleFont());
    p.setPen(tc[T::Fg]);
    const QFontMetrics tfm(p.font());
    const QRect titleRect(left, 0, width, kTitleLine);
    if (m_titleTo.isEmpty()) {
        p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                   tfm.elidedText(m_title, Qt::ElideRight, titleRect.width()));
    } else {
        const int arrow = 14 + 2 * 8;
        const int half = std::max(0, (width - arrow) / 2);
        const QString from = tfm.elidedText(m_title, Qt::ElideMiddle, std::max(half, width - arrow - tfm.horizontalAdvance(m_titleTo)));
        const int fromW = tfm.horizontalAdvance(from);
        p.drawText(QRect(left, 0, fromW, kTitleLine), Qt::AlignLeft | Qt::AlignVCenter, from);
        fs::paintGlyph(&p, fs::Glyph::ArrowRight, QRectF(left + fromW + 8, 3, 14, 14), tc[T::Fg3]);
        const int toLeft = left + fromW + arrow;
        p.drawText(QRect(toLeft, 0, std::max(0, left + width - toLeft), kTitleLine), Qt::AlignLeft | Qt::AlignVCenter,
                   tfm.elidedText(m_titleTo, Qt::ElideMiddle, std::max(0, left + width - toLeft)));
    }

    // 부제
    if (m_subtitle.isEmpty())
        return;
    p.setFont(subtitleFont());
    p.setPen(tc[T::Fg2]);
    if (m_wrap) {
        layoutDescription(&p, m_subtitle, p.font(), QRect(left, kTitleLine + kDescGap, width, 0));
        return;
    }
    const QFontMetrics sfm(p.font());
    const QRect r(left, kTitleLine + kSubtitleGap, width, kSubtitleLine);
    const QString shown = sfm.elidedText(m_subtitle, m_elide, r.width());
    p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, shown);
    const QString wantedTip = shown != m_subtitle ? m_subtitle : QString();
    if (toolTip() != wantedTip)
        setToolTip(wantedTip);
}

void DialogHeader::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        updateGeometry();
}

void DialogHeader::refreshAccessibility()
{
    setAccessibleName(m_titleTo.isEmpty() ? m_title : m_title + u" → "_s + m_titleTo);
    setAccessibleDescription(m_subtitle);
}

} // namespace fm::ui
