#include "fmwidgets/BarListCard.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QEvent>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QLocale>
#include <QPainter>
#include <QStringList>
#include <QToolTip>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

// 목업 .cats — 안쪽 여백 6 10 8 · 줄 간격 3 · 칸 34 / 132 / 1fr / 44 · 칸 사이 8 · 막대 높이 12
constexpr int kPadTop = 6;
constexpr int kPadX = 10;
constexpr int kPadBottom = 8;
constexpr int kRow = 18;
constexpr int kRowGap = 3;
constexpr int kColGap = 8;
constexpr int kBarHeight = 12;
constexpr int kCountMin = 44;
constexpr int kLabelMin = 60;
constexpr int kBarMin = 40;
// 목업 .shn — 최소 폭 16 · 좌우 4 · 왼쪽 간격 4 · 11 px
constexpr int kBadgeHeight = 16;
constexpr int kBadgeMin = 16;
constexpr int kBadgePadX = 4;
constexpr int kBadgeGap = 4;
// 목업 .legend — 위 간격 6 · 항목 간격 12 · 색 상자 10 + 4
constexpr int kLegendGap = 6;
constexpr int kLegendRow = 16;
constexpr int kLegendItemGap = 12;
constexpr int kSwatch = 10;
constexpr int kSwatchGap = 4;

QColor mix(const QColor &base, const QColor &over, qreal amount)
{
    return QColor::fromRgbF(base.redF() + (over.redF() - base.redF()) * amount,
                            base.greenF() + (over.greenF() - base.greenF()) * amount,
                            base.blueF() + (over.blueF() - base.blueF()) * amount);
}

QFont labelFont(const QFont &base) { return fs::pixelFont(base, 12.5); }
QFont countFont(const QFont &base) { return fs::withTabularNumbers(fs::pixelFont(base, 12.5)); }
QFont badgeFont(const QFont &base) { return fs::withTabularNumbers(fs::pixelFont(base, 11, QFont::DemiBold)); }
QFont legendFont(const QFont &base) { return fs::pixelFont(base, 11.5); }

int badgeWidth(const QFont &font, const QString &text)
{
    return std::max(kBadgeMin, QFontMetrics(font).horizontalAdvance(text) + 2 * kBadgePadX);
}

void paintBadge(QPainter *p, const QRectF &r, const QString &text, const fs::ThemeColors &tc)
{
    const fs::ToneColors c = fs::toneColors(fs::Tone::Warn, tc);
    const qreal radius = fs::squareCorners(tc) ? 0.0 : 2.0;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, radius > 0);
    p->setPen(c.border.alpha() > 0 ? QPen(c.border, 1.0) : Qt::NoPen);
    p->setBrush(c.background);
    p->drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    p->setPen(c.foreground);
    p->drawText(r, Qt::AlignCenter, text);
    p->restore();
}

QString numberText(double v)
{
    if (std::abs(v - std::round(v)) < 1e-9)
        return QLocale().toString(qlonglong(std::llround(v)));
    return QLocale().toString(v, 'f', 1);
}

} // namespace

BarListCard::BarListCard(QWidget *parent)
    : Card(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QObject::connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, qOverload<>(&QWidget::update));
}

void BarListCard::setSeries(const QList<Series> &series)
{
    m_series = series;
    changed();
}

void BarListCard::addRow(const Row &row)
{
    m_rows.append(row);
    changed();
}

void BarListCard::setRows(const QList<Row> &rows)
{
    m_rows = rows;
    changed();
}

void BarListCard::clear()
{
    setRows({});
}

void BarListCard::setCodeWidth(int width)
{
    m_codeWidth = std::max(0, width);
    changed();
}

void BarListCard::setLabelWidth(int width)
{
    m_labelWidth = std::max(0, width);
    changed();
}

void BarListCard::setMaximum(double maximum)
{
    m_maximum = std::max(0.0, maximum);
    update();
}

void BarListCard::setLegendVisible(bool visible)
{
    m_legendVisible = visible;
    changed();
}

void BarListCard::setBadgeLegend(const QString &text)
{
    m_badgeLegend = text;
    changed();
}

void BarListCard::changed()
{
    refreshAccessibility();
    updateGeometry();
    update();
}

// ---------------------------------------------------------------------------------------------
// 치수

int BarListCard::effectiveCodeWidth() const
{
    // 코드가 하나도 없으면 칸을 두지 않는다
    const QFontMetrics fm(fs::monoFont(11.5));
    int widest = -1;
    for (const Row &row : m_rows)
        if (!row.code.isEmpty())
            widest = std::max(widest, fm.horizontalAdvance(row.code));
    return widest < 0 ? 0 : std::max(m_codeWidth, widest);
}

int BarListCard::countColumnWidth() const
{
    const QFontMetrics fm(countFont(font()));
    int widest = kCountMin;
    for (const Row &row : m_rows)
        widest = std::max(widest, fm.horizontalAdvance(countOf(row)));
    return widest;
}

BarListCard::Columns BarListCard::columns() const
{
    const QFont badge = badgeFont(font());
    int badgeColumn = 0;
    for (const Row &row : m_rows)
        if (!row.badgeText.isEmpty())
            badgeColumn = std::max(badgeColumn, kBadgeGap + badgeWidth(badge, row.badgeText));

    Columns c;
    const int codeWidth = effectiveCodeWidth();
    c.code = 1 + kPadX;
    c.label = c.code + (codeWidth > 0 ? codeWidth + kColGap : 0);
    c.bar = c.label + m_labelWidth + kColGap;
    c.right = width() - 1 - kPadX;
    c.count = c.right - badgeColumn - countColumnWidth();
    c.barWidth = std::max<qreal>(0.0, c.count - kColGap - c.bar);
    return c;
}

double BarListCard::rowTotal(const Row &row) const
{
    double total = 0.0;
    for (double v : row.values)
        total += std::max(0.0, v);
    return total;
}

double BarListCard::effectiveMaximum() const
{
    if (m_maximum > 0.0)
        return m_maximum;
    double widest = 0.0;
    for (const Row &row : m_rows)
        widest = std::max(widest, rowTotal(row));
    return widest > 0.0 ? widest : 1.0;
}

QString BarListCard::countOf(const Row &row) const
{
    return row.countText.isEmpty() ? numberText(rowTotal(row)) : row.countText;
}

QColor BarListCard::seriesColor(int index, const fs::ThemeColors &tc) const
{
    // 구간을 정하지 않은 값 — 강조 · 옅은 강조 · 경고 · 흐림 순
    static const Series kFallback[] = {{QString(), T::Accent, 1.0, {}}, {QString(), T::Accent, 0.47, {}},
                                       {QString(), T::Warn, 0.6, {}}, {QString(), T::Fg3, 0.5, {}}};
    const Series &s = index < m_series.size() ? m_series.at(index) : kFallback[index % 4];
    if (s.color.isValid())
        return s.color;
    const QColor c = tc[s.token];
    return s.strength >= 1.0 ? c : mix(tc[T::Surface], c, std::clamp(s.strength, 0.0, 1.0));
}

int BarListCard::legendHeight() const
{
    if (!m_legendVisible || (m_series.isEmpty() && m_badgeLegend.isEmpty()))
        return 0;
    return kLegendGap + kLegendRow;
}

QRectF BarListCard::barRect(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    const Columns c = columns();
    const qreal top = 1 + kPadTop + row * (kRow + kRowGap);
    return QRectF(c.bar, top + (kRow - kBarHeight) / 2.0, c.barWidth, kBarHeight);
}

qreal BarListCard::segmentWidth(int row, int series) const
{
    if (row < 0 || row >= m_rows.size() || series < 0 || series >= m_rows.at(row).values.size())
        return 0.0;
    const qreal inner = std::max<qreal>(0.0, barRect(row).width() - 2);
    return inner * std::max(0.0, m_rows.at(row).values.at(series)) / effectiveMaximum();
}

QSize BarListCard::sizeHint() const
{
    const int n = int(m_rows.size());
    const int rows = n > 0 ? n * kRow + (n - 1) * kRowGap : 0;
    return QSize(420, 2 + kPadTop + rows + legendHeight() + kPadBottom);
}

QSize BarListCard::minimumSizeHint() const
{
    const int codeWidth = effectiveCodeWidth();
    const int w = 2 + 2 * kPadX + (codeWidth > 0 ? codeWidth + kColGap : 0) + std::min(m_labelWidth, kLabelMin) + kColGap + kBarMin
                + kColGap + countColumnWidth();
    return QSize(w, sizeHint().height());
}

// ---------------------------------------------------------------------------------------------
// 그리기

void BarListCard::paintEvent(QPaintEvent *event)
{
    Card::paintEvent(event);  // 카드 바탕 · 테두리(스타일)
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    const bool watercolor = tc.isWatercolor();
    const fs::WatercolorChrome &x = fs::watercolorChrome(tc.variant());
    QPainter p(this);

    const Columns c = columns();
    const QFont code = fs::monoFont(11.5);
    const QFont label = labelFont(font());
    const QFont count = countFont(font());
    const QFont badge = badgeFont(font());
    const double max = effectiveMaximum();
    const int codeWidth = effectiveCodeWidth();
    const QColor fg = isEnabled() ? tc[T::Fg] : tc[T::Fg3];

    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &row = m_rows.at(i);
        const qreal top = 1 + kPadTop + i * (kRow + kRowGap);

        if (codeWidth > 0) {
            p.setFont(code);
            p.setPen(tc[T::Fg3]);
            p.drawText(QRectF(c.code, top, codeWidth, kRow), Qt::AlignLeft | Qt::AlignVCenter, row.code);
        }
        p.setFont(label);
        p.setPen(fg);
        p.drawText(QRectF(c.label, top, m_labelWidth, kRow), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(label).elidedText(row.label, Qt::ElideRight, m_labelWidth));

        // 홈 — 시안2는 얕게 들어간 칸(위 · 왼쪽 어두운 빗면), 시안1은 옅은 바탕 + 구분선
        const QRectF bar = barRect(i);
        if (bar.width() >= 4) {
            const QRect track = bar.toAlignedRect();
            if (watercolor) {
                p.fillRect(track, x.trough);
                p.fillRect(QRect(track.left(), track.top(), track.width() - 1, 1), x.lo);
                p.fillRect(QRect(track.left(), track.top(), 1, track.height() - 1), x.lo);
                p.fillRect(QRect(track.left(), track.bottom(), track.width(), 1), x.hi);
                p.fillRect(QRect(track.right(), track.top(), 1, track.height()), x.hi);
            } else {
                p.fillRect(track, tc[T::Alt]);
                p.fillRect(QRect(track.left(), track.top(), track.width(), 1), tc[T::Line]);
                p.fillRect(QRect(track.left(), track.bottom(), track.width(), 1), tc[T::Line]);
                p.fillRect(QRect(track.left(), track.top(), 1, track.height()), tc[T::Line]);
                p.fillRect(QRect(track.right(), track.top(), 1, track.height()), tc[T::Line]);
            }
            const QRectF inner = QRectF(track).adjusted(1, 1, -1, -1);
            p.save();
            p.setClipRect(inner);
            qreal at = inner.left();
            for (int s = 0; s < row.values.size(); ++s) {
                const qreal w = inner.width() * std::max(0.0, row.values.at(s)) / max;
                if (w <= 0)
                    continue;
                p.fillRect(QRectF(at, inner.top(), w, inner.height()), seriesColor(s, tc));
                at += w;
            }
            p.restore();
        }

        // 수 · 배지 — 수는 같은 칸에서 오른쪽 맞춤(숫자 폭 고정), 배지는 그 뒤 칸
        p.setFont(count);
        p.setPen(isEnabled() ? tc[T::Fg2] : tc[T::Fg3]);
        p.drawText(QRectF(c.count, top, countColumnWidth(), kRow), Qt::AlignRight | Qt::AlignVCenter, countOf(row));
        if (!row.badgeText.isEmpty()) {
            p.setFont(badge);
            const int bw = badgeWidth(badge, row.badgeText);
            paintBadge(&p, QRectF(c.count + countColumnWidth() + kBadgeGap, top + (kRow - kBadgeHeight) / 2.0, bw, kBadgeHeight),
                       row.badgeText, tc);
        }
    }

    // 범례
    if (legendHeight() > 0) {
        const int n = int(m_rows.size());
        const qreal top = 1 + kPadTop + (n > 0 ? n * kRow + (n - 1) * kRowGap : 0) + kLegendGap;
        const QFont legend = legendFont(font());
        const QFontMetrics fm(legend);
        const QColor swatchLine = tc.isDark() ? QColor(255, 255, 255, 51) : QColor(0, 0, 0, 51);
        qreal at = 1 + kPadX;
        for (int s = 0; s < m_series.size(); ++s) {
            const QRectF box(at, top + (kLegendRow - kSwatch) / 2.0, kSwatch, kSwatch);
            p.fillRect(box, seriesColor(s, tc));
            p.setPen(QPen(swatchLine, 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawRect(box.adjusted(0.5, 0.5, -0.5, -0.5));
            at += kSwatch + kSwatchGap;
            p.setFont(legend);
            p.setPen(tc[T::Fg3]);
            const QString &name = m_series.at(s).name;
            p.drawText(QRectF(at, top, fm.horizontalAdvance(name) + 1, kLegendRow), Qt::AlignLeft | Qt::AlignVCenter, name);
            at += fm.horizontalAdvance(name) + kLegendItemGap;
        }
        if (!m_badgeLegend.isEmpty()) {
            p.setFont(badge);
            const QString sample = u"n"_s;
            const int bw = badgeWidth(badge, sample);
            paintBadge(&p, QRectF(at, top + (kLegendRow - kBadgeHeight) / 2.0, bw, kBadgeHeight), sample, tc);
            at += bw + kBadgeGap;
            p.setFont(legend);
            p.setPen(tc[T::Fg3]);
            p.drawText(QRectF(at, top, std::max<qreal>(0, width() - 1 - kPadX - at), kLegendRow), Qt::AlignLeft | Qt::AlignVCenter,
                       fm.elidedText(m_badgeLegend, Qt::ElideRight, std::max(0, int(width() - 1 - kPadX - at))));
        }
    }
}

bool BarListCard::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        for (int i = 0; i < m_rows.size(); ++i) {
            const qreal top = 1 + kPadTop + i * (kRow + kRowGap);
            if (help->pos().y() < top || help->pos().y() >= top + kRow)
                continue;
            const Row &row = m_rows.at(i);
            QString text = row.toolTip;
            if (text.isEmpty()) {
                QStringList lines{row.label};
                for (int s = 0; s < row.values.size() && s < m_series.size(); ++s)
                    lines.append(m_series.at(s).name + u": "_s + numberText(row.values.at(s)));
                text = lines.join(u'\n');
            }
            QToolTip::showText(help->globalPos(), text, this);
            return true;
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    return Card::event(event);
}

void BarListCard::refreshAccessibility()
{
    QStringList lines;
    for (const Row &row : std::as_const(m_rows)) {
        QStringList parts;
        for (int s = 0; s < row.values.size() && s < m_series.size(); ++s)
            parts.append(m_series.at(s).name + u' ' + numberText(row.values.at(s)));
        QString line = (row.code.isEmpty() ? QString() : row.code + u' ') + row.label + u": "_s + countOf(row);
        if (!parts.isEmpty())
            line += u" ("_s + parts.join(u", "_s) + u')';
        if (!row.badgeText.isEmpty())
            line += u" · "_s + row.badgeText;
        lines.append(line);
    }
    setAccessibleDescription(lines.join(u'\n'));
}

} // namespace fm::ui
