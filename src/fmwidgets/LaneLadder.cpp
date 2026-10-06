// 사다리 — 레인 사이를 오간 자취 (시안1 · 시안2 공통).

#include "fmwidgets/LaneLadder.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeColors.h>
#include <fmstyle/ThemeManager.h>

#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

constexpr int kHeadPadY = 5;
constexpr int kTextPadX = 8;
constexpr qreal kTimePx = 11.0;
constexpr qreal kTextPx = 12.5;
constexpr qreal kLanePx = 11.5;
constexpr qreal kDetailPx = 10.5;
constexpr qreal kHead = 7.0;   // 화살촉 길이
constexpr qreal kLoopW = 18.0; // Self 고리 폭
constexpr qreal kLoopH = 9.0;

struct LineLook
{
    QColor color;
    qreal width;
    Qt::PenStyle style;
    bool boldText;
    bool italicText;
    QColor textColor;
};

LineLook lookFor(LaneLadder::Kind kind, const fs::ThemeColors &tc)
{
    switch (kind) {
    case LaneLadder::Inferred:
        // 보지 못한 구간이다 — 본 것처럼 그리지 않는다.
        return {tc[T::Fg3], 1.2, Qt::DashLine, false, true, tc[T::Fg3]};
    case LaneLadder::Warn:
        return {tc[T::Warn], 1.6, Qt::SolidLine, true, false, tc[T::Warn]};
    case LaneLadder::Failed:
        return {tc[T::Danger], 1.8, Qt::SolidLine, true, false, tc[T::Danger]};
    case LaneLadder::Stream:
        return {tc[T::Accent], 2.4, Qt::DotLine, false, false, tc[T::Fg]};
    case LaneLadder::Self:
        return {tc[T::Fg3], 1.2, Qt::SolidLine, false, false, tc[T::Fg3]};
    case LaneLadder::Divider:
        return {tc[T::Line], 1.0, Qt::SolidLine, true, false, tc[T::Fg]};
    case LaneLadder::Seen:
        break;
    }
    return {tc[T::Fg2], 1.5, Qt::SolidLine, false, false, tc[T::Fg]};
}

/// x0 → x1 가로 화살표. 화살촉은 가는 쪽에도 보이게 선 색으로 채운다.
void drawArrow(QPainter *p, qreal x0, qreal x1, qreal y, const LineLook &look)
{
    QPen pen(look.color, look.width, look.style, Qt::FlatCap);
    p->setPen(pen);
    const qreal dir = x1 >= x0 ? 1.0 : -1.0;
    p->drawLine(QPointF(x0, y), QPointF(x1 - dir * kHead * 0.6, y));

    QPainterPath head;
    head.moveTo(x1, y);
    head.lineTo(x1 - dir * kHead, y - kHead * 0.5);
    head.lineTo(x1 - dir * kHead, y + kHead * 0.5);
    head.closeSubpath();
    p->setPen(Qt::NoPen);
    p->setBrush(look.color);
    p->drawPath(head);
}

/// 한 레인 안에서 일어난 일 — 오른쪽으로 나갔다 돌아오는 작은 고리.
void drawLoop(QPainter *p, qreal x, qreal y, const LineLook &look)
{
    QPainterPath path;
    path.moveTo(x, y - kLoopH * 0.5);
    path.cubicTo(x + kLoopW, y - kLoopH * 1.4, x + kLoopW, y + kLoopH * 1.4, x, y + kLoopH * 0.5);
    p->setPen(QPen(look.color, look.width, look.style, Qt::FlatCap));
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);

    QPainterPath head;
    head.moveTo(x, y + kLoopH * 0.5);
    head.lineTo(x + kHead, y + kLoopH * 0.5 - kHead * 0.5);
    head.lineTo(x + kHead, y + kLoopH * 0.5 + kHead * 0.5);
    head.closeSubpath();
    p->setPen(Qt::NoPen);
    p->setBrush(look.color);
    p->drawPath(head);
}

} // namespace

// ---------------------------------------------------------------------------------------------

LaneLadder::LaneLadder(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, qOverload<>(&QWidget::update));
}

void LaneLadder::setLanes(const QList<Lane> &lanes)
{
    m_lanes = lanes;
    updateGeometry();
    update();
}

void LaneLadder::addRung(int from, int to, const QString &time, const QString &text, Kind kind)
{
    m_rungs.append(Rung{from, to, time, text, kind});
    updateGeometry();
    update();
}

void LaneLadder::addDivider(const QString &time, const QString &text)
{
    m_rungs.append(Rung{-1, -1, time, text, Divider});
    updateGeometry();
    update();
}

void LaneLadder::setRungs(const QList<Rung> &rungs)
{
    m_rungs = rungs;
    if (m_current >= m_rungs.size())
        setCurrentRow(-1);
    updateGeometry();
    update();
}

void LaneLadder::clear()
{
    m_rungs.clear();
    m_hover = -1;
    setCurrentRow(-1);
    updateGeometry();
    update();
}

int LaneLadder::firstFailure() const
{
    for (int i = 0; i < m_rungs.size(); ++i) {
        if (m_rungs.at(i).kind == Failed)
            return i;
    }
    return -1;
}

void LaneLadder::setLaneWidth(int px)
{
    m_laneWidth = std::max(60, px);
    updateGeometry();
    update();
}

void LaneLadder::setTimeWidth(int px)
{
    m_timeWidth = std::max(0, px);
    updateGeometry();
    update();
}

void LaneLadder::setCurrentRow(int row)
{
    const int r = (row >= 0 && row < m_rungs.size()) ? row : -1;
    if (r == m_current)
        return;
    m_current = r;
    Q_EMIT currentRowChanged(m_current);
    update();
}

// ---------------------------------------------------------------------------------------------
// 자리

int LaneLadder::headerHeight() const
{
    if (m_lanes.isEmpty())
        return 0;
    const QFontMetrics fn(fs::pixelFont(font(), kLanePx, QFont::DemiBold));
    const QFontMetrics fd(fs::monoFont(kDetailPx));
    const bool anyDetail = std::any_of(m_lanes.cbegin(), m_lanes.cend(), [](const Lane &l) { return !l.detail.isEmpty(); });
    return 2 * kHeadPadY + fn.height() + (anyDetail ? fd.height() : 0) + 1;
}

QRect LaneLadder::rowRect(int row) const
{
    return QRect(0, headerHeight() + row * kRowHeight, width(), kRowHeight);
}

int LaneLadder::rowAt(const QPoint &pos) const
{
    const int h = headerHeight();
    if (pos.y() < h)
        return -1;
    const int r = (pos.y() - h) / kRowHeight;
    return r >= 0 && r < m_rungs.size() ? r : -1;
}

qreal LaneLadder::laneCenter(int lane) const
{
    const int n = std::max(1, static_cast<int>(m_lanes.size()));
    const qreal w = static_cast<qreal>(m_laneWidth) / n;
    return m_timeWidth + w * (lane + 0.5);
}

QSize LaneLadder::sizeHint() const
{
    const QFontMetrics ft(fs::pixelFont(font(), kTextPx));
    int textW = 160;
    for (const Rung &r : m_rungs)
        textW = std::max(textW, ft.horizontalAdvance(r.text) + 2 * kTextPadX);
    return QSize(m_timeWidth + m_laneWidth + textW, headerHeight() + m_rungs.size() * kRowHeight);
}

QSize LaneLadder::minimumSizeHint() const
{
    return QSize(m_timeWidth + m_laneWidth + 80, headerHeight() + kRowHeight);
}

// ---------------------------------------------------------------------------------------------
// 그리기

void LaneLadder::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QFont fTime = fs::withTabularNumbers(fs::monoFont(kTimePx));
    const QFont fText = fs::pixelFont(font(), kTextPx);
    const QFont fLane = fs::pixelFont(font(), kLanePx, QFont::DemiBold);
    const QFont fDetail = fs::monoFont(kDetailPx);

    const int head = headerHeight();
    const int textX = m_timeWidth + m_laneWidth;

    // 머리 — 레인 이름과 곁글
    if (head > 0) {
        p.fillRect(QRect(0, 0, width(), head), tc[T::Head]);
        p.setPen(QPen(tc[T::Line], 1.0));
        p.drawLine(0, head - 1, width(), head - 1);

        const int n = static_cast<int>(m_lanes.size());
        const qreal w = static_cast<qreal>(m_laneWidth) / std::max(1, n);
        const QFontMetrics fn(fLane);
        for (int i = 0; i < n; ++i) {
            const QRectF cell(m_timeWidth + w * i, kHeadPadY, w, fn.height());
            p.setFont(fLane);
            p.setPen(tc[T::Fg]);
            p.drawText(cell, Qt::AlignHCenter | Qt::AlignVCenter, m_lanes.at(i).name);
            if (!m_lanes.at(i).detail.isEmpty()) {
                const QFontMetrics fd(fDetail);
                const QRectF sub(cell.left() + 2, cell.bottom(), w - 4, fd.height());
                p.setFont(fDetail);
                p.setPen(tc[T::Fg3]);
                p.drawText(sub, Qt::AlignHCenter | Qt::AlignVCenter,
                           fd.elidedText(m_lanes.at(i).detail, Qt::ElideRight, static_cast<int>(sub.width())));
            }
        }
    }

    // 레인 안내선 — 화살표가 어느 자리에서 떠나 어디에 닿는지 눈으로 잇게 한다
    if (!m_lanes.isEmpty() && !m_rungs.isEmpty()) {
        p.setPen(QPen(tc[T::Grid], 1.0, Qt::SolidLine));
        const int bottom = head + static_cast<int>(m_rungs.size()) * kRowHeight;
        for (int i = 0; i < m_lanes.size(); ++i) {
            const qreal x = std::round(laneCenter(i)) + 0.5;
            p.drawLine(QPointF(x, head), QPointF(x, bottom));
        }
    }

    for (int i = 0; i < m_rungs.size(); ++i) {
        const Rung &r = m_rungs.at(i);
        const QRect rr = rowRect(i);
        const LineLook look = lookFor(r.kind, tc);

        if (r.kind == Divider)
            p.fillRect(rr, tc[T::Head]);
        else if (i % 2 == 1)
            p.fillRect(rr, tc[T::Alt]);
        if (i == m_current)
            p.fillRect(rr, tc[T::Sel]);
        else if (i == m_hover)
            p.fillRect(rr, tc[T::AccentSoft]);

        // 시각
        if (m_timeWidth > 0 && !r.time.isEmpty()) {
            p.setFont(fTime);
            p.setPen(tc[T::Fg3]);
            p.drawText(QRect(0, rr.top(), m_timeWidth - 6, rr.height()), Qt::AlignRight | Qt::AlignVCenter, r.time);
        }

        // 그림
        const qreal y = rr.center().y() + 0.5;
        if (r.kind == Divider) {
            p.setPen(QPen(tc[T::Line], 1.0, Qt::SolidLine));
            p.drawLine(QPointF(m_timeWidth + 4, y), QPointF(textX - 4, y));
        } else if (!m_lanes.isEmpty() && r.from >= 0 && r.from < m_lanes.size() && r.to >= 0 && r.to < m_lanes.size()) {
            if (r.from == r.to)
                drawLoop(&p, laneCenter(r.from), y, look);
            else
                drawArrow(&p, laneCenter(r.from), laneCenter(r.to), y, look);
        }

        // 글
        p.setFont(r.kind == Divider || look.boldText ? fs::pixelFont(font(), kTextPx, QFont::DemiBold)
                                                     : (look.italicText ? [&] { QFont f = fText; f.setItalic(true); return f; }()
                                                                        : fText));
        p.setPen(look.textColor);
        const QRect tr(textX + kTextPadX, rr.top(), std::max(10, width() - textX - 2 * kTextPadX), rr.height());
        p.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(p.font()).elidedText(r.text, Qt::ElideRight, tr.width()));
    }
}

// ---------------------------------------------------------------------------------------------
// 입력

void LaneLadder::mousePressEvent(QMouseEvent *event)
{
    const int r = rowAt(event->pos());
    if (r >= 0) {
        setCurrentRow(r);
        setFocus(Qt::MouseFocusReason);
    }
}

void LaneLadder::mouseDoubleClickEvent(QMouseEvent *event)
{
    const int r = rowAt(event->pos());
    if (r >= 0) {
        setCurrentRow(r);
        Q_EMIT rowActivated(r);
    }
}

void LaneLadder::mouseMoveEvent(QMouseEvent *event)
{
    const int r = rowAt(event->pos());
    if (r != m_hover) {
        m_hover = r;
        update();
    }
}

void LaneLadder::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    if (m_hover != -1) {
        m_hover = -1;
        update();
    }
}

void LaneLadder::keyPressEvent(QKeyEvent *event)
{
    if (m_rungs.isEmpty()) {
        QWidget::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Down:
        setCurrentRow(std::min(m_current + 1, static_cast<int>(m_rungs.size()) - 1));
        return;
    case Qt::Key_Up:
        setCurrentRow(std::max(m_current - 1, 0));
        return;
    case Qt::Key_Home:
        setCurrentRow(0);
        return;
    case Qt::Key_End:
        setCurrentRow(static_cast<int>(m_rungs.size()) - 1);
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_current >= 0)
            Q_EMIT rowActivated(m_current);
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void LaneLadder::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange
        || event->type() == QEvent::FontChange) {
        updateGeometry();
        update();
    }
}

} // namespace fm::ui
