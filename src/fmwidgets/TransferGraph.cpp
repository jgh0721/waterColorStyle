#include "fmwidgets/TransferGraph.h"

#include <fmstyle/ThemeColors.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QEasingCurve>
#include <QEvent>
#include <QLinearGradient>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace fm::ui {

using fm::style::Token;

namespace {

constexpr int kMaxSamples = 1200;       // 넘으면 이웃 두 개를 하나로 합친다
constexpr double kSmoothingMs = 1200.0; // 속도 평활 시간 상수
constexpr qint64 kMinTimeWindowMs = 10'000;
constexpr int kLineAnimMs = 420;
constexpr int kScaleAnimMs = 600;

constexpr const char *kUnits[] = {"B/s", "KB/s", "MB/s", "GB/s", "TB/s"};

QColor alpha(QColor c, qreal a)
{
    c.setAlphaF(a);
    return c;
}

int unitIndexFor(double bytesPerSecond)
{
    int i = 0;
    double v = bytesPerSecond;
    while (v >= 1024.0 && i < 4) {
        v /= 1024.0;
        ++i;
    }
    return i;
}

QString formatIn(double bytesPerSecond, int unit, int decimals)
{
    const double v = bytesPerSecond / std::pow(1024.0, unit);
    return QLocale().toString(v, 'f', decimals) + u" "_s + QString::fromLatin1(kUnits[unit]);
}

/// 1 · 2 · 2.5 · 5 × 10ⁿ 가운데 raw 이상인 가장 작은 값.
double niceStep(double raw)
{
    if (raw <= 0.0)
        return 1.0;
    const double e = std::pow(10.0, std::floor(std::log10(raw)));
    const double f = raw / e;
    const double n = f <= 1.0 ? 1.0 : f <= 2.0 ? 2.0 : f <= 2.5 ? 2.5 : f <= 5.0 ? 5.0 : 10.0;
    return n * e;
}

/// 눈금 간격 (바이트/초). 한 화면에 2 ~ 3줄.
double gridStep(double scale)
{
    const int unit = unitIndexFor(scale);
    const double unitSize = std::pow(1024.0, unit);
    return niceStep(scale / unitSize / 2.6) * unitSize;
}

QString formatTime(qint64 ms)
{
    const qlonglong s = ms / 1000;
    const QChar zero(u'0');
    if (s >= 3600)
        return u"%1:%2:%3"_s.arg(s / 3600).arg((s / 60) % 60, 2, 10, zero).arg(s % 60, 2, 10, zero);
    return u"%1:%2"_s.arg(s / 60, 2, 10, zero).arg(s % 60, 2, 10, zero);
}

void pill(QPainter *p, const QPointF &anchorRight, const QString &text, const QColor &bg,
          const QColor &fg, const QRectF &bounds, qreal radius)
{
    const QFontMetricsF fm(p->font());
    const qreal w = fm.horizontalAdvance(text) + 10;
    const qreal h = 16;
    QRectF r(anchorRight.x() - w, anchorRight.y() - h / 2, w, h);
    if (r.top() < bounds.top())
        r.moveTop(bounds.top());
    if (r.bottom() > bounds.bottom())
        r.moveBottom(bounds.bottom());
    p->setPen(Qt::NoPen);
    p->setBrush(bg);
    p->drawRoundedRect(r, radius, radius);
    p->setPen(fg);
    p->drawText(r, Qt::AlignCenter, text);
}

} // namespace

TransferGraph::TransferGraph(QWidget *parent)
    : QWidget(parent)
    , m_lineAnim(new QVariantAnimation(this))
    , m_scaleAnim(new QVariantAnimation(this))
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(tr("처리 속도 그래프"));

    m_lineAnim->setDuration(kLineAnimMs);
    m_lineAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_lineAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_lineShown = v.toDouble();
        update();
    });
    m_scaleAnim->setDuration(kScaleAnimMs);
    m_scaleAnim->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_scaleAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_scaleShown = v.toDouble();
        update();
    });
}

TransferGraph::~TransferGraph() = default;

void TransferGraph::start(qint64 totalBytes)
{
    m_samples.clear();
    m_total = std::max<qint64>(0, totalBytes);
    m_activeMs = 0;
    m_current = m_peak = 0.0;
    m_lineAnim->stop();
    m_scaleAnim->stop();
    m_lineShown = 0.0;
    m_scaleShown = 0.0;
    update();
}

void TransferGraph::addSample(qint64 bytesDone, qint64 elapsedMs)
{
    double raw = 0.0;
    qint64 dt = 0;
    if (!m_samples.isEmpty()) {
        const Sample &last = m_samples.constLast();
        dt = elapsedMs - last.ms;
        if (dt <= 0)
            return;
        raw = double(bytesDone - last.bytes) * 1000.0 / double(dt);
    }
    if (m_paused)
        raw = 0.0;
    raw = std::max(0.0, raw);

    if (m_samples.isEmpty()) {
        m_current = 0.0;
    } else {
        const double a = 1.0 - std::exp(-double(dt) / kSmoothingMs);
        m_current += a * (raw - m_current);
        if (!m_paused)
            m_activeMs += dt;
    }
    if (m_current > m_peak) {
        m_peak = m_current;
        Q_EMIT peakChanged(m_peak);
    }
    m_samples.append({bytesDone, elapsedMs, m_current, m_paused});
    decimate();
    retarget();

    const double avg = averageSpeed();
    // 앱(또는 .ui)에서 직접 넣은 설명은 덮어쓰지 않는다
    if (accessibleDescription().isEmpty() || accessibleDescription() == m_autoDescription) {
        m_autoDescription = tr("현재 %1, 평균 %2").arg(formatRate(m_current), formatRate(avg));
        setAccessibleDescription(m_autoDescription);
    }
    Q_EMIT speedChanged(m_current, avg);
    update();
}

void TransferGraph::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    retarget();
    update();
}

void TransferGraph::setSpeedLimit(double bytesPerSecond)
{
    m_limit = std::max(0.0, bytesPerSecond);
    retarget();
    update();
}

void TransferGraph::setAxis(Axis axis)
{
    if (m_axis == axis)
        return;
    m_axis = axis;
    update();
}

void TransferGraph::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void TransferGraph::setShowAverage(bool on)
{
    m_showAverage = on;
    update();
}

void TransferGraph::setFramed(bool on)
{
    m_framed = on;
    update();
}

void TransferGraph::setHeaderVisible(bool on)
{
    if (m_headerVisible == on)
        return;
    m_headerVisible = on;
    update();
}

double TransferGraph::averageSpeed() const noexcept
{
    if (m_samples.isEmpty() || m_activeMs <= 0)
        return 0.0;
    const double moved = double(m_samples.constLast().bytes - m_samples.constFirst().bytes);
    return moved * 1000.0 / double(m_activeMs);
}

qint64 TransferGraph::bytesDone() const noexcept
{
    return m_samples.isEmpty() ? 0 : m_samples.constLast().bytes;
}

QString TransferGraph::formatRate(double bytesPerSecond)
{
    const int unit = unitIndexFor(bytesPerSecond);
    const double v = bytesPerSecond / std::pow(1024.0, unit);
    return formatIn(bytesPerSecond, unit, (unit == 0 || v >= 100.0) ? 0 : 1);
}

QSize TransferGraph::sizeHint() const
{
    return {560, 140};
}

QSize TransferGraph::minimumSizeHint() const
{
    return {240, 96};
}

TransferGraph::Axis TransferGraph::effectiveAxis() const noexcept
{
    return (m_axis == Axis::Progress && m_total > 0) ? Axis::Progress : Axis::Time;
}

QRectF TransferGraph::plotRect() const
{
    if (!m_headerVisible) {
        // 머리 줄을 밖에 둔 진행 창(목업 CopyProgress): 위 4 · 아래 6, 테두리가 없으면 좌우 여백도 없다
        const qreal side = m_framed ? 12 : 0;
        return QRectF(side, 4, width() - 2 * side, height() - 4 - 6);
    }
    const qreal header = 24;
    return QRectF(12, 8 + header, width() - 24, height() - 8 - header - 10);
}

double TransferGraph::xFraction(const Sample &s) const
{
    if (effectiveAxis() == Axis::Progress)
        return std::clamp(double(s.bytes) / double(m_total), 0.0, 1.0);
    const qint64 span = std::max(kMinTimeWindowMs, m_samples.isEmpty() ? 0 : m_samples.constLast().ms);
    return std::clamp(double(s.ms) / double(span), 0.0, 1.0);
}

void TransferGraph::decimate()
{
    if (m_samples.size() <= kMaxSamples)
        return;
    // 처음과 마지막은 그대로 두고 가운데 이웃을 둘씩 합친다.
    QList<Sample> merged;
    merged.reserve(m_samples.size() / 2 + 2);
    merged.append(m_samples.constFirst());
    for (qsizetype i = 1; i + 1 < m_samples.size() - 1; i += 2) {
        const Sample &a = m_samples.at(i);
        const Sample &b = m_samples.at(i + 1);
        merged.append({b.bytes, b.ms, (a.speed + b.speed) / 2.0, a.paused || b.paused});
    }
    merged.append(m_samples.constLast());
    m_samples = std::move(merged);
}

double TransferGraph::targetScale() const
{
    double top = std::max({m_peak, m_current, m_limit, 1024.0 * 1024.0});
    top *= 1.15;
    const double step = gridStep(top);
    return std::ceil(top / step) * step;
}

void TransferGraph::retarget()
{
    const double lineTarget = m_paused ? 0.0 : m_current;
    // 보이기 전(기록을 한꺼번에 넣을 때)에는 움직임 없이 바로 맞춘다.
    if (!isVisible()) {
        m_lineAnim->stop();
        m_scaleAnim->stop();
        m_lineShown = lineTarget;
        m_scaleShown = targetScale();
        return;
    }
    if (m_samples.size() <= 1) {
        m_lineShown = lineTarget;
    } else {
        m_lineAnim->stop();
        m_lineAnim->setStartValue(m_lineShown);
        m_lineAnim->setEndValue(lineTarget);
        m_lineAnim->start();
    }

    const double scale = targetScale();
    if (m_scaleShown <= 0.0) {
        m_scaleShown = scale;
    } else if (std::abs(scale - m_scaleShown) > 1.0
               && !(m_scaleAnim->state() == QAbstractAnimation::Running
                    && m_scaleAnim->endValue().toDouble() == scale)) {
        m_scaleAnim->stop();
        m_scaleAnim->setStartValue(m_scaleShown);
        m_scaleAnim->setEndValue(scale);
        m_scaleAnim->start();
    }
}

int TransferGraph::sampleNear(qreal x) const
{
    const QRectF plot = plotRect();
    if (m_samples.isEmpty() || plot.width() <= 0)
        return -1;
    const double f = (x - plot.left()) / plot.width();
    int best = -1;
    double bestDist = 1e9;
    for (qsizetype i = 0; i < m_samples.size(); ++i) {
        const double d = std::abs(xFraction(m_samples.at(i)) - f);
        if (d < bestDist) {
            bestDist = d;
            best = int(i);
        }
    }
    // 진행률 축에서 아직 가지 않은 곳은 값이 없다
    if (best >= 0 && f > xFraction(m_samples.constLast()) + 0.01)
        return -1;
    return best;
}

void TransferGraph::mouseMoveEvent(QMouseEvent *event)
{
    m_hoverX = event->position().x();
    update();
    QWidget::mouseMoveEvent(event);
}

void TransferGraph::leaveEvent(QEvent *event)
{
    m_hoverX = -1.0;
    update();
    QWidget::leaveEvent(event);
}

void TransferGraph::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange
        || event->type() == QEvent::FontChange)
        update();
    QWidget::changeEvent(event);
}

void TransferGraph::paintEvent(QPaintEvent *)
{
    const auto &tc = fm::style::themeColorsFor(this);
    // 시안2(워터컬러)는 모서리가 모두 네모나고 도구 설명이 노란 칸이다.
    const bool square = tc.isWatercolor();
    const qreal round = square ? 0.0 : 1.0;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_framed) {
        const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(QPen(tc[Token::Line], 1.0));
        p.setBrush(tc[Token::Surface]);
        p.drawRoundedRect(frame, 6 * round, 6 * round);
    }

    QFont small = font();
    small.setPixelSize(11);
    QFont header = font();
    header.setPixelSize(12);

    // 곡선이 글자를 지나가도 읽히도록 글자 뒤에 바탕색을 반투명하게 깐다
    const QColor backdrop = m_framed ? tc[Token::Surface] : palette().color(backgroundRole());
    const auto haloText = [&](const QRectF &r, int flags, const QString &text, const QColor &color) {
        const QRectF br = p.boundingRect(r, flags, text).adjusted(-2, 0, 2, 0);
        p.setPen(Qt::NoPen);
        p.setBrush(alpha(backdrop, 0.82));
        p.drawRoundedRect(br, 2 * round, 2 * round);
        p.setPen(color);
        p.drawText(r, flags, text);
    };

    const Axis axis = effectiveAxis();
    const QRectF plot = plotRect();
    const double scale = m_scaleShown > 0 ? m_scaleShown : targetScale();
    const auto yOf = [&](double v) {
        return plot.bottom() - std::clamp(v / scale, 0.0, 1.04) * plot.height();
    };

    // 머리 줄(밖에 두었으면 그리지 않는다)
    if (m_headerVisible) {
        p.setFont(header);
        p.setPen(tc[Token::Fg3]);
        const QString title = !m_title.isEmpty() ? m_title
                            : axis == Axis::Progress ? tr("처리 속도 · 진행률 기준")
                                                     : tr("처리 속도 · 경과 시간 기준");
        const QRectF headerRect(plot.left(), 8, plot.width(), 18);
        p.drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter, title);
        if (m_peak > 0)
            p.drawText(headerRect, Qt::AlignRight | Qt::AlignVCenter, tr("최대 %1").arg(formatRate(m_peak)));
    }

    // 눈금 선 (글자는 평균 · 제한 글자와 겹치지 않는 것만 나중에)
    p.setFont(small);
    const double step = gridStep(scale);
    const int unit = unitIndexFor(step * 2);
    struct GridLabel { QRectF rect; QString text; };
    QList<GridLabel> gridLabels;
    for (double v = step; v < scale * 0.999; v += step) {
        const qreal y = std::round(yOf(v)) + 0.5;
        p.setPen(QPen(tc[Token::Grid], 1.0));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        const double inUnit = v / std::pow(1024.0, unit);
        const int decimals = std::abs(inUnit - std::round(inUnit)) < 1e-6 ? 0 : 1;
        const QString text = formatIn(v, unit, decimals);
        gridLabels.append({QRectF(plot.left() + 2, y - 15, QFontMetricsF(small).horizontalAdvance(text) + 2, 14), text});
    }
    const auto drawGridLabels = [&](const QList<QRectF> &avoid) {
        p.setFont(small);
        p.setPen(tc[Token::Fg3]);
        for (const GridLabel &g : std::as_const(gridLabels)) {
            const bool hit = std::any_of(avoid.cbegin(), avoid.cend(),
                                         [&](const QRectF &r) { return r.intersects(g.rect.adjusted(0, -2, 0, 2)); });
            if (!hit && g.rect.top() >= plot.top() - 2)
                haloText(g.rect, Qt::AlignLeft | Qt::AlignBottom, g.text, tc[Token::Fg3]);
        }
    };
    p.setPen(QPen(tc[Token::Line], 1.0));
    p.drawLine(QPointF(plot.left(), std::round(plot.bottom()) + 0.5),
               QPointF(plot.right(), std::round(plot.bottom()) + 0.5));

    if (m_samples.size() < 2) {
        drawGridLabels({});
        p.setPen(tc[Token::Fg3]);
        p.drawText(plot, Qt::AlignCenter, tr("속도를 재는 중…"));
        return;
    }

    const auto xOf = [&](const Sample &s) { return plot.left() + xFraction(s) * plot.width(); };

    // 일시 정지 구간
    for (qsizetype i = 1; i < m_samples.size(); ++i) {
        if (!m_samples.at(i).paused)
            continue;
        const qreal x0 = xOf(m_samples.at(i - 1));
        qreal x1 = xOf(m_samples.at(i));
        if (axis == Axis::Progress)  // 진행률 축에서는 정지 중 x가 움직이지 않으므로 폭을 준다
            x1 = std::max(x1, x0 + 3.0);
        p.fillRect(QRectF(x0, plot.top(), x1 - x0, plot.height()), alpha(tc[Token::Paused], 0.16));
    }

    // 면적과 선
    QPainterPath line;
    line.moveTo(xOf(m_samples.constFirst()), yOf(m_samples.constFirst().speed));
    for (qsizetype i = 1; i < m_samples.size(); ++i)
        line.lineTo(xOf(m_samples.at(i)), yOf(m_samples.at(i).speed));
    QPainterPath area = line;
    area.lineTo(xOf(m_samples.constLast()), plot.bottom());
    area.lineTo(xOf(m_samples.constFirst()), plot.bottom());
    area.closeSubpath();

    QLinearGradient fill(plot.topLeft(), plot.bottomLeft());
    fill.setColorAt(0.0, alpha(tc[Token::Accent], tc.isDark() ? 0.42 : 0.30));
    fill.setColorAt(1.0, alpha(tc[Token::Accent], 0.04));
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawPath(area);
    QPen linePen(tc[Token::Accent], 1.5);
    linePen.setJoinStyle(Qt::RoundJoin);
    p.setPen(linePen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(line);

    // 진행률 축: 지금 위치
    if (axis == Axis::Progress) {
        const qreal x = std::round(xOf(m_samples.constLast())) + 0.5;
        QPen nowPen(alpha(tc[Token::Fg3], 0.6), 1.0);
        nowPen.setStyle(Qt::DotLine);
        p.setPen(nowPen);
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }

    QList<QRectF> taken;  // 글자가 차지한 자리

    // 속도 제한
    if (m_limit > 0) {
        const qreal y = std::round(yOf(m_limit)) + 0.5;
        QPen pen(tc[Token::Warn], 1.0);
        pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        QString limitText = formatRate(m_limit);
        limitText.replace(QLocale().decimalPoint() + u"0 "_s, u" "_s);  // 50.0 MB/s → 50 MB/s
        const QRectF r(plot.left(), y - 15, plot.width() - 4, 14);
        haloText(r, Qt::AlignRight | Qt::AlignBottom, tr("제한 %1").arg(limitText), tc[Token::Warn]);
        taken.append(QRectF(r.right() - 110, r.top(), 110, r.height()));
    }

    // 평균 속도
    const double avg = averageSpeed();
    const qreal yLine = yOf(m_lineShown);
    if (m_showAverage && avg > 0) {
        const qreal y = std::round(yOf(avg)) + 0.5;
        QPen pen(tc[Token::Fg2], 1.0);
        pen.setDashPattern({3.0, 3.0});
        p.setPen(pen);
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        // 현재 속도 선과 겹치면 글자를 선 아래로
        const bool below = std::abs(y - yLine) < 16 && y >= yLine;
        const QRectF label = below ? QRectF(plot.left() + 2, y + 1, 200, 14)
                                   : QRectF(plot.left() + 2, y - 15, 200, 14);
        const QString avgText = tr("평균 %1").arg(formatRate(avg));
        haloText(label, Qt::AlignLeft | (below ? Qt::AlignTop : Qt::AlignBottom), avgText, tc[Token::Fg2]);
        taken.append(QRectF(label.left(), label.top(), QFontMetricsF(small).horizontalAdvance(avgText) + 4,
                            label.height()));
    }
    drawGridLabels(taken);

    // 현재 속도 — 속도에 따라 움직이는 가로선
    {
        const QColor c = m_paused ? tc[Token::Paused] : tc[Token::Accent];
        const qreal y = std::round(yLine) + 0.5;
        p.setPen(QPen(c, 1.0));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        const QString text = m_paused ? tr("일시 정지") : formatRate(m_current);
        pill(&p, QPointF(plot.right() - 2, y), text, c, m_paused ? tc[Token::Surface] : tc[Token::OnAccent],
             plot.adjusted(0, 1, 0, -1), 3 * round);
    }

    // 마우스 읽기 (미리보기 속성 fmPreviewHover: 0 ~ 1)
    qreal hoverX = m_hoverX;
    if (hoverX < 0) {
        const QVariant preview = property("fmPreviewHover");
        if (preview.isValid())
            hoverX = plot.left() + preview.toDouble() * plot.width();
    }
    if (hoverX >= plot.left() && hoverX <= plot.right()) {
        const int i = sampleNear(hoverX);
        if (i >= 0) {
            const Sample &s = m_samples.at(i);
            const QPointF pt(xOf(s), yOf(s.speed));
            p.setPen(QPen(alpha(tc[Token::Fg], 0.35), 1.0));
            p.drawLine(QPointF(std::round(pt.x()) + 0.5, plot.top()), QPointF(std::round(pt.x()) + 0.5, plot.bottom()));
            p.setPen(QPen(tc[Token::Surface], 2.0));
            p.setBrush(tc[Token::Accent]);
            p.drawEllipse(pt, 3.5, 3.5);

            const QString where = axis == Axis::Progress
                ? QLocale().toString(100.0 * double(s.bytes) / double(m_total), 'f', 0) + u" %"_s
                : formatTime(s.ms);
            const QString text = where + u"  ·  "_s + (s.paused ? tr("일시 정지") : formatRate(s.speed));
            const QFontMetricsF fm(small);
            const QSizeF box(fm.horizontalAdvance(text) + 14, 20);
            QRectF tip(pt.x() + 8, plot.top() + 2, box.width(), box.height());
            if (tip.right() > plot.right())
                tip.moveRight(pt.x() - 8);
            if (square) {
                const auto &x = fm::style::watercolorChrome(tc.variant());
                p.setPen(QPen(x.tipLine, 1.0));
                p.setBrush(x.tipBg);
                p.drawRect(tip.toAlignedRect().adjusted(0, 0, -1, -1));
                p.setPen(x.tipFg);
            } else {
                p.setPen(QPen(tc[Token::Line], 1.0));
                p.setBrush(tc[Token::Surface]);
                p.drawRoundedRect(tip, 4, 4);
                p.setPen(tc[Token::Fg]);
            }
            p.drawText(tip, Qt::AlignCenter, text);
        }
    }
}

} // namespace fm::ui
