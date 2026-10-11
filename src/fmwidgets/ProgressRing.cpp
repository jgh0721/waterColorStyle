// 진행 고리 — 시안1 Windows 11 고리 · 시안2 XP 진행 막대 칸을 고리로.

#include "fmwidgets/ProgressRing.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeColors.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Qt::StringLiterals;

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

constexpr int kHint = 48;
constexpr int kSpinMs = 2000;       // 바쁨 한 주기 — 호가 두 바퀴 돌며 한 번 늘었다 준다
constexpr int kChunk = 7;           // 시안2 칸 · 틈(진행 막대와 같은 7 + 2)
constexpr int kChunkGap = 2;

/// 고리 모양 조각(annular sector). 각도는 Qt 규칙(3시 0도, 시계 반대 방향 +).
QPainterPath sector(const QPointF &c, qreal outer, qreal inner, qreal startDeg, qreal spanDeg)
{
    QPainterPath path;
    const QRectF o(c.x() - outer, c.y() - outer, 2 * outer, 2 * outer);
    const QRectF i(c.x() - inner, c.y() - inner, 2 * inner, 2 * inner);
    path.arcMoveTo(o, startDeg);
    path.arcTo(o, startDeg, spanDeg);
    path.arcTo(i, startDeg + spanDeg, -spanDeg);
    path.closeSubpath();
    return path;
}

} // namespace

ProgressRing::ProgressRing(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, qOverload<>(&QWidget::update));
}

ProgressRing::~ProgressRing() = default;

void ProgressRing::setMinimum(int minimum)
{
    setRange(minimum, std::max(minimum, m_maximum));
}

void ProgressRing::setMaximum(int maximum)
{
    setRange(std::min(m_minimum, maximum), maximum);
}

void ProgressRing::setRange(int minimum, int maximum)
{
    const bool wasBusy = isBusy();
    m_minimum = minimum;
    m_maximum = std::max(minimum, maximum);
    setValue(std::clamp(m_value, m_minimum, m_maximum));
    if (wasBusy != isBusy()) {
        updateAnimation();
        Q_EMIT busyChanged(isBusy());
    }
    update();
}

void ProgressRing::setValue(int value)
{
    value = std::clamp(value, m_minimum, m_maximum);
    if (value == m_value)
        return;
    m_value = value;
    update();
    Q_EMIT valueChanged(value);
}

qreal ProgressRing::fraction() const
{
    if (m_maximum <= m_minimum)
        return 0;
    return qreal(m_value - m_minimum) / qreal(m_maximum - m_minimum);
}

void ProgressRing::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    const bool was = isBusy();
    m_busy = busy;
    updateAnimation();
    update();
    if (was != isBusy())
        Q_EMIT busyChanged(isBusy());
}

void ProgressRing::setTextVisible(bool visible)
{
    m_textVisible = visible;
    update();
}

void ProgressRing::setValueDisplay(ValueDisplay display)
{
    m_display = display;
    update();
}

QString ProgressRing::text() const
{
    if (isBusy())
        return {};
    if (m_display == Actual)
        return QString::number(m_value);
    return u"%1%"_s.arg(qRound(fraction() * 100));
}

void ProgressRing::setState(State state)
{
    m_state = state;
    update();
}

void ProgressRing::setTrackVisible(bool visible)
{
    m_trackVisible = visible;
    update();
}

void ProgressRing::setThickness(int px)
{
    m_thickness = std::max(0, px);
    update();
}

QSize ProgressRing::sizeHint() const
{
    return {kHint, kHint};
}

QSize ProgressRing::minimumSizeHint() const
{
    return {16, 16};
}

void ProgressRing::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateAnimation();
}

void ProgressRing::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    updateAnimation();
}

void ProgressRing::updateAnimation()
{
    const bool run = isBusy() && isVisible();
    if (!run) {
        if (m_spin)
            m_spin->stop();
        return;
    }
    if (!m_spin) {
        m_spin = new QVariantAnimation(this);
        m_spin->setStartValue(0.0);
        m_spin->setEndValue(1.0);
        m_spin->setDuration(kSpinMs);
        m_spin->setLoopCount(-1);
        connect(m_spin, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_phase = v.toReal();
            update();
        });
    }
    if (m_spin->state() != QAbstractAnimation::Running)
        m_spin->start();
}

int ProgressRing::ringThickness(qreal diameter) const
{
    if (m_thickness > 0)
        return m_thickness;
    return std::max(2, int(std::lround(diameter / 10.0)));
}

void ProgressRing::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    const bool enabled = isEnabled();
    QColor color = m_state == Paused ? tc[T::Paused] : m_state == Error ? tc[T::Danger] : tc[T::Accent];
    if (!enabled)
        color = tc[T::Fg3];

    const qreal side = std::min(width(), height());
    const QPointF c(width() / 2.0, height() / 2.0);
    const int t = ringThickness(side);
    const bool busy = isBusy();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (!tc.isWatercolor()) {
        // 시안1: 홈 고리 + 둥근 끝 호
        const qreal r = side / 2.0 - t / 2.0 - 1;
        const QRectF ring(c.x() - r, c.y() - r, 2 * r, 2 * r);
        if (m_trackVisible) {
            p.setPen(QPen(enabled ? tc[T::Line] : tc[T::Grid], t));
            p.drawEllipse(ring);
        }
        p.setPen(QPen(color, t, Qt::SolidLine, Qt::RoundCap));
        if (busy) {
            // 두 바퀴 도는 동안 호가 20°에서 270°까지 한 번 늘었다 준다(WinUI 무한 고리)
            const qreal grow = 0.5 - 0.5 * std::cos(2 * std::numbers::pi * m_phase);
            const qreal span = 20 + 250 * grow;
            const qreal start = 90 - m_phase * 720 - grow * 90;
            p.drawArc(ring, int(start * 16), int(-span * 16));
        } else if (const qreal f = fraction(); f > 0) {
            if (f >= 1.0)
                p.drawEllipse(ring);
            else
                p.drawArc(ring, 90 * 16, int(-f * 360 * 16));
        }
    } else {
        // 시안2: 들어간 홈 고리 + XP 진행 막대 칸
        const fs::WatercolorChrome &x = fs::watercolorChrome(tc.variant());
        const qreal outer = side / 2.0 - 1;
        const qreal inner = std::max(2.0, outer - std::max(t, 6) - 2);
        if (m_trackVisible) {
            QPainterPath track;
            track.addEllipse(c, outer, outer);
            track.addEllipse(c, inner, inner);
            p.setPen(Qt::NoPen);
            p.setBrush(enabled ? x.trough : tc[T::Win]);
            p.drawPath(track);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(x.fieldOuter, 1));
            p.drawEllipse(c, outer - 0.5, outer - 0.5);
            p.setPen(QPen(x.fieldBright, 1));
            p.drawEllipse(c, inner + 0.5, inner + 0.5);
        }
        const qreal blockOuter = outer - 2;
        const qreal blockInner = inner + 2;
        const qreal mid = (blockOuter + blockInner) / 2;
        const int count = std::clamp(int(2 * std::numbers::pi * mid / (kChunk + kChunkGap)), 8, 60);
        const qreal step = 360.0 / count;
        const qreal gapDeg = std::min(step * 0.4, kChunkGap / mid * 180.0 / std::numbers::pi);
        // 칸은 관 모양으로 — 가운데가 밝다(XP 막대의 세로 그라데이션)
        QRadialGradient tube(c, blockOuter);
        tube.setColorAt(blockInner / blockOuter, color.darker(108));
        tube.setColorAt(mid / blockOuter, color.lighter(140));
        tube.setColorAt(1.0, color.darker(108));
        p.setPen(Qt::NoPen);
        const auto block = [&](int i, qreal alpha) {
            p.setOpacity(alpha);
            p.setBrush(tube);
            // 12시에서 시계 방향으로 i번째 칸
            const qreal start = 90 - i * step - gapDeg / 2;
            p.drawPath(sector(c, blockOuter, blockInner, start, -(step - gapDeg)));
        };
        if (busy) {
            // 밝은 칸 셋이 돈다(XP 마키)
            const int head = int(m_phase * count * 2) % count;
            for (int k = 0; k < 3; ++k)
                block((head - k + count) % count, 1.0 - k * 0.3);
        } else {
            const int filled = int(std::lround(fraction() * count));
            for (int i = 0; i < filled; ++i)
                block(i, 1.0);
        }
        p.setOpacity(1.0);
    }

    if (m_textVisible && !busy && side >= 28) {
        p.setPen(enabled ? tc[T::Fg] : tc[T::Fg3]);
        p.setFont(fs::pixelFont(font(), std::clamp(side * 0.24, 9.0, 22.0), QFont::DemiBold));
        p.drawText(QRectF(c.x() - side / 2, c.y() - side / 2, side, side), Qt::AlignCenter, text());
    }
}

} // namespace fm::ui
