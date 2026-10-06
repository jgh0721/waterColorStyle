// 두 줄 시간 흐름 — 같은 자 위의 두 쪽 자취 (시안1 · 시안2 공통).

#include "fmwidgets/PairedTimeline.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeColors.h>
#include <fmstyle/ThemeManager.h>

#include <QEvent>
#include <QFontMetrics>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

constexpr int kPadX = 10;
constexpr int kPadTop = 18;     // 자 눈금 글이 들어가는 자리
constexpr int kPadBottom = 8;
constexpr int kRowGap = 34;     // 두 줄 사이
constexpr qreal kTickH = 9.0;
constexpr qreal kFirstH = 14.0;
constexpr qreal kLabelPx = 11.5;
constexpr qreal kAxisPx = 10.5;
constexpr qreal kNearPx = 6.0;
constexpr int kMinHeight = 86;

QColor alpha(QColor c, qreal a)
{
    c.setAlphaF(a);
    return c;
}

QString msText(qint64 ms)
{
    if (ms < 1000)
        return QString::number(ms) + u" ms"_s;
    return QLocale().toString(ms / 1000.0, 'f', ms < 10000 ? 2 : 1) + u" s"_s;
}

} // namespace

// ---------------------------------------------------------------------------------------------

PairedTimeline::PairedTimeline(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, qOverload<>(&QWidget::update));
}

void PairedTimeline::setRows(const Row &top, const Row &bottom)
{
    m_rows[0] = top;
    m_rows[1] = bottom;
    update();
}

PairedTimeline::Row PairedTimeline::row(int index) const
{
    return (index == kTopRow || index == kBottomRow) ? m_rows[index] : Row{};
}

void PairedTimeline::addMark(int row, qint64 atMs, Mark kind, int pairId, const QString &note)
{
    if (row != kTopRow && row != kBottomRow)
        return;
    auto &list = m_points[row];
    const Point pt{atMs, kind, pairId, note};
    // 들어온 차례가 뒤죽박죽일 수 있다 — 시각 차례로 끼워 넣는다.
    const auto at = std::lower_bound(list.begin(), list.end(), pt,
                                     [](const Point &a, const Point &b) { return a.atMs < b.atMs; });
    list.insert(at, pt);
    update();
}

void PairedTimeline::addGap(int row, qint64 fromMs, qint64 toMs)
{
    if (row != kTopRow && row != kBottomRow)
        return;
    if (toMs <= fromMs)
        return;
    m_gaps[row].append(Gap{fromMs, toMs});
    update();
}

void PairedTimeline::clear()
{
    for (int r = 0; r < 2; ++r) {
        m_points[r].clear();
        m_gaps[r].clear();
    }
    m_hoverRow = -1;
    m_hoverIndex = -1;
    update();
}

int PairedTimeline::markCount(int row) const
{
    return (row == kTopRow || row == kBottomRow) ? static_cast<int>(m_points[row].size()) : 0;
}

qint64 PairedTimeline::firstMs(int row) const
{
    if (row != kTopRow && row != kBottomRow)
        return -1;
    return m_points[row].isEmpty() ? -1 : m_points[row].first().atMs;
}

qint64 PairedTimeline::lastMs(int row) const
{
    if (row != kTopRow && row != kBottomRow)
        return -1;
    return m_points[row].isEmpty() ? -1 : m_points[row].last().atMs;
}

QList<PairedTimeline::Gap> PairedTimeline::effectiveGaps(int row) const
{
    QList<Gap> out = m_gaps[row];
    // 손으로 주지 않아도 상한을 넘는 빈 자리는 스스로 띠가 된다 — 「여기서 멎었다」가 눈에 걸리게.
    if (m_gapMs > 0) {
        const auto &pts = m_points[row];
        for (int i = 1; i < pts.size(); ++i) {
            const qint64 d = pts.at(i).atMs - pts.at(i - 1).atMs;
            if (d <= m_gapMs)
                continue;
            const Gap g{pts.at(i - 1).atMs, pts.at(i).atMs};
            const bool already = std::any_of(m_gaps[row].cbegin(), m_gaps[row].cend(), [&](const Gap &x) {
                return x.fromMs <= g.fromMs && x.toMs >= g.toMs;
            });
            if (!already)
                out.append(g);
        }
    }
    return out;
}

int PairedTimeline::gapCount(int row) const
{
    return (row == kTopRow || row == kBottomRow) ? static_cast<int>(effectiveGaps(row).size()) : 0;
}

qint64 PairedTimeline::spanMs() const
{
    if (m_span > 0)
        return m_span;
    qint64 last = 0;
    for (int r = 0; r < 2; ++r) {
        if (!m_points[r].isEmpty())
            last = std::max(last, m_points[r].last().atMs);
        for (const Gap &g : m_gaps[r])
            last = std::max(last, g.toMs);
    }
    return std::max<qint64>(last, 1);
}

void PairedTimeline::setSpanMs(qint64 ms)
{
    m_span = std::max<qint64>(0, ms);
    update();
}

void PairedTimeline::setGapThresholdMs(qint64 ms)
{
    m_gapMs = std::max<qint64>(0, ms);
    update();
}

void PairedTimeline::setLabelWidth(int px)
{
    m_labelWidth = std::max(0, px);
    updateGeometry();
    update();
}

// ---------------------------------------------------------------------------------------------
// 자리

QRectF PairedTimeline::plotRect() const
{
    return QRectF(m_labelWidth + kPadX, kPadTop, std::max(20, width() - m_labelWidth - 2 * kPadX),
                  std::max(20, height() - kPadTop - kPadBottom));
}

qreal PairedTimeline::xFor(qint64 ms) const
{
    const QRectF r = plotRect();
    const qreal f = std::clamp(static_cast<qreal>(ms) / static_cast<qreal>(spanMs()), 0.0, 1.0);
    return r.left() + r.width() * f;
}

qreal PairedTimeline::baselineY(int row) const
{
    const QRectF r = plotRect();
    const qreal mid = r.center().y();
    return row == kTopRow ? mid - kRowGap / 2.0 : mid + kRowGap / 2.0;
}

int PairedTimeline::markNear(int row, qreal x) const
{
    int best = -1;
    qreal bestD = kNearPx;
    for (int i = 0; i < m_points[row].size(); ++i) {
        const qreal d = std::abs(xFor(m_points[row].at(i).atMs) - x);
        if (d <= bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

QSize PairedTimeline::sizeHint() const
{
    return QSize(m_labelWidth + 320, kMinHeight);
}

QSize PairedTimeline::minimumSizeHint() const
{
    return QSize(m_labelWidth + 120, kMinHeight);
}

// ---------------------------------------------------------------------------------------------
// 그리기

void PairedTimeline::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF plot = plotRect();
    const QFont fLabel = fs::pixelFont(font(), kLabelPx, QFont::DemiBold);
    const QFont fDetail = fs::pixelFont(font(), kAxisPx);
    const QFont fAxis = fs::withTabularNumbers(fs::monoFont(kAxisPx));

    // 자 — 양 끝과 가운데만 적는다. 눈금이 촘촘하면 글이 서로를 가린다.
    p.setFont(fAxis);
    p.setPen(tc[T::Fg3]);
    const qint64 span = spanMs();
    p.drawText(QRectF(plot.left(), 0, 60, kPadTop), Qt::AlignLeft | Qt::AlignVCenter, u"0"_s);
    p.drawText(QRectF(plot.center().x() - 30, 0, 60, kPadTop), Qt::AlignHCenter | Qt::AlignVCenter, msText(span / 2));
    p.drawText(QRectF(plot.right() - 60, 0, 60, kPadTop), Qt::AlignRight | Qt::AlignVCenter, msText(span));

    for (int r = 0; r < 2; ++r) {
        const qreal y = baselineY(r);

        // 이름 · 곁글
        p.setFont(fLabel);
        p.setPen(tc[T::Fg]);
        p.drawText(QRectF(kPadX, y - 16, m_labelWidth - 4, 16), Qt::AlignLeft | Qt::AlignBottom, m_rows[r].name);
        if (!m_rows[r].detail.isEmpty()) {
            p.setFont(fDetail);
            p.setPen(tc[T::Fg3]);
            p.drawText(QRectF(kPadX, y, m_labelWidth - 4, 16), Qt::AlignLeft | Qt::AlignTop,
                       QFontMetrics(fDetail).elidedText(m_rows[r].detail, Qt::ElideRight, m_labelWidth - 4));
        }

        // 빈 자리 띠 — 먼저 깔아 눈금이 그 위에 온다
        for (const Gap &g : effectiveGaps(r)) {
            const qreal x0 = xFor(g.fromMs);
            const qreal x1 = xFor(g.toMs);
            p.fillRect(QRectF(x0, y - kFirstH, x1 - x0, kFirstH * 2), alpha(tc[T::Paused], 0.16));
            p.setPen(QPen(alpha(tc[T::Paused], 0.55), 1.0, Qt::DotLine));
            p.drawLine(QPointF(x0, y - kFirstH), QPointF(x0, y + kFirstH));
            p.drawLine(QPointF(x1, y - kFirstH), QPointF(x1, y + kFirstH));
        }

        // 바탕 줄
        p.setPen(QPen(tc[T::Line], 1.0));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    // 짝 잇기 — 기울기가 곧 지연이다. 눈금보다 먼저 그려 눈금이 위에 온다
    for (const Point &a : m_points[kTopRow]) {
        if (a.pairId < 0)
            continue;
        const auto it = std::find_if(m_points[kBottomRow].cbegin(), m_points[kBottomRow].cend(),
                                     [&](const Point &b) { return b.pairId == a.pairId; });
        if (it == m_points[kBottomRow].cend())
            continue;
        const qint64 lag = it->atMs - a.atMs;
        const bool late = m_gapMs > 0 && lag > m_gapMs / 4;
        p.setPen(QPen(late ? alpha(tc[T::Warn], 0.75) : alpha(tc[T::Fg3], 0.45), late ? 1.4 : 1.0, Qt::SolidLine));
        p.drawLine(QPointF(xFor(a.atMs), baselineY(kTopRow) + kTickH * 0.5),
                   QPointF(xFor(it->atMs), baselineY(kBottomRow) - kTickH * 0.5));
    }

    // 눈금
    for (int r = 0; r < 2; ++r) {
        const qreal y = baselineY(r);
        for (int i = 0; i < m_points[r].size(); ++i) {
            const Point &pt = m_points[r].at(i);
            const qreal x = xFor(pt.atMs);
            const bool hot = r == m_hoverRow && i == m_hoverIndex;

            switch (pt.kind) {
            case Cut: {
                // 끊긴 자리 — 가위표로 「여기서 끝났다」를 못 박는다
                p.setPen(QPen(tc[T::Danger], 1.8, Qt::SolidLine, Qt::FlatCap));
                const qreal h = kFirstH * 0.6;
                p.drawLine(QPointF(x - h, y - h), QPointF(x + h, y + h));
                p.drawLine(QPointF(x - h, y + h), QPointF(x + h, y - h));
                break;
            }
            case First: {
                p.setPen(QPen(tc[T::Accent], 2.2, Qt::SolidLine, Qt::FlatCap));
                p.drawLine(QPointF(x, y - kFirstH), QPointF(x, y + kFirstH));
                break;
            }
            case Note: {
                p.setPen(Qt::NoPen);
                p.setBrush(tc[T::Fg3]);
                p.drawEllipse(QPointF(x, y), 2.6, 2.6);
                break;
            }
            case Tick:
            default: {
                p.setPen(QPen(hot ? tc[T::Accent] : tc[T::Fg2], hot ? 2.0 : 1.4, Qt::SolidLine, Qt::FlatCap));
                p.drawLine(QPointF(x, y - kTickH * 0.5), QPointF(x, y + kTickH * 0.5));
                break;
            }
            }
        }
    }

    if (m_points[kTopRow].isEmpty() && m_points[kBottomRow].isEmpty()) {
        p.setFont(fDetail);
        p.setPen(tc[T::Fg3]);
        p.drawText(plot, Qt::AlignCenter, tr("조각이 없다"));
    }
}

// ---------------------------------------------------------------------------------------------
// 입력

void PairedTimeline::mouseMoveEvent(QMouseEvent *event)
{
    const qreal x = event->position().x();
    const qreal y = event->position().y();
    const int r = y < plotRect().center().y() ? kTopRow : kBottomRow;
    const int i = markNear(r, x);

    if (r != m_hoverRow || i != m_hoverIndex) {
        m_hoverRow = i >= 0 ? r : -1;
        m_hoverIndex = i;
        Q_EMIT markHovered(m_hoverRow, m_hoverIndex, i >= 0 ? m_points[r].at(i).atMs : -1);
        update();
    }
    if (i >= 0) {
        const Point &pt = m_points[r].at(i);
        QToolTip::showText(event->globalPosition().toPoint(),
                           pt.note.isEmpty() ? msText(pt.atMs) : u"%1 · %2"_s.arg(msText(pt.atMs), pt.note), this);
    } else {
        QToolTip::hideText();
    }
}

void PairedTimeline::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    if (m_hoverRow != -1 || m_hoverIndex != -1) {
        m_hoverRow = -1;
        m_hoverIndex = -1;
        Q_EMIT markHovered(-1, -1, -1);
        update();
    }
}

void PairedTimeline::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange
        || event->type() == QEvent::FontChange) {
        updateGeometry();
        update();
    }
}

} // namespace fm::ui
