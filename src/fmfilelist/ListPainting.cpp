#include "ListPainting_p.h"

#include <fmstyle/StyleProps.h>

#include <QPainter>
#include <QWidget>

namespace fm::filelist::detail {

using fm::style::Token;

bool paneActive(const QWidget *widget)
{
    for (const QWidget *w = widget; w; w = w->parentWidget()) {
        const QVariant v = w->property(fm::style::props::kPaneActive);
        if (v.isValid())
            return v.toBool();
    }
    return false;
}

RecordGeometry RecordGeometry::make(bool watercolor, bool twoLine, const ListAppearance &a)
{
    RecordGeometry g;
    g.watercolor = watercolor;
    g.twoLine = twoLine;
    g.nameBelow = twoLine && a.nameBelow;
    g.separator = twoLine ? a.twoLineSeparator : a.oneLineSeparator;
    const bool space = g.separator == RecordSeparator::Space;
    if (!twoLine) {
        // 1줄: 시안1 24 · 시안2 21, 여백 방식은 아래 틈 2 · 좌우 6 · 모서리 4
        const int row = watercolor ? 21 : 24;
        g.headerHeight = watercolor ? 22 : 27;
        if (space) {
            g.gap = 2;
            g.sideMargin = 6;
            g.radius = watercolor ? 0 : 4;
        }
        g.pitch = row + g.gap;
        return g;
    }
    // 2줄: 시안1 이름 22 + 메타 18(레코드 44), 시안2 20 + 16(38). 여백 방식 48/42 + 틈 2, 틴트 46/40.
    g.nameHeight = watercolor ? 20 : 22;
    g.metaHeight = watercolor ? 16 : 18;
    g.headerHeight = watercolor ? 40 : 45;
    const int content = g.nameHeight + g.metaHeight;
    int block = 0;
    switch (g.separator) {
    case RecordSeparator::Space:
        block = watercolor ? 42 : 48;
        g.gap = 2;
        g.sideMargin = 6;
        g.radius = watercolor ? 0 : 6;
        g.padTop = (block - content) / 2.0;
        break;
    case RecordSeparator::Tint:
        block = watercolor ? 40 : 46;
        g.padTop = (block - content) / 2.0;
        break;
    case RecordSeparator::None:
    case RecordSeparator::Zebra:
    case RecordSeparator::Line:
        block = watercolor ? 38 : 44;
        g.padTop = (block - 1 - content) / 2.0;  // 아래 1 px 구분선 자리
        break;
    }
    g.pitch = block + g.gap;
    return g;
}

QRectF RecordGeometry::block(const QRect &record) const
{
    return QRectF(record).adjusted(sideMargin, 0, -sideMargin, -gap);
}

QRectF RecordGeometry::nameLine(const QRect &record) const
{
    const QRectF b = block(record);
    if (!twoLine)
        return b;
    return QRectF(record.left(), b.top() + padTop + (nameBelow ? metaHeight : 0), record.width(), nameHeight);
}

QRectF RecordGeometry::metaLine(const QRect &record) const
{
    const QRectF b = block(record);
    if (!twoLine)
        return b;
    return QRectF(record.left(), b.top() + padTop + (nameBelow ? 0 : nameHeight), record.width(), metaHeight);
}

bool invertedCursor(const ListAppearance &a, const RecordState &s)
{
    return a.invertCursor && s.cursor && s.active;
}

TextColors textColors(const fm::style::ThemeColors &tc, const ListAppearance &a, const RecordState &s,
                      bool twoLine, RecordSeparator separator)
{
    TextColors c;
    const bool invCur = invertedCursor(a, s);
    const bool invSel = a.invertSelection && s.marked;
    const bool space2 = twoLine && separator == RecordSeparator::Space;
    if (invCur) {
        // TC 방식: 선택 위 역상 커서는 글자만 --inv-cur-sel, 아이콘 선은 --on-inv-cur
        const QColor text = s.marked ? tc[Token::InvCurSel] : tc[Token::OnInvCur];
        c.name = c.ext = c.meta = text;
        c.iconLine = tc[Token::OnInvCur];
    } else if (invSel) {
        c.name = c.ext = c.meta = c.iconLine = tc[Token::OnInvSel];
    } else if (tc.isWatercolor() && s.marked && s.active) {
        c.name = c.ext = c.meta = c.iconLine = QColor(0xFF, 0xFF, 0xFF);  // 시안2: 활성 패널 선택은 흰 글자
    } else if (tc.isWatercolor() && s.marked) {
        c.name = c.ext = c.meta = tc[Token::Fg];
        c.iconLine = tc[Token::Fg3];
    } else {
        c.name = s.hidden ? tc[Token::Fg3] : tc[Token::Fg];
        c.ext = tc[Token::Fg3];
        c.meta = twoLine ? (s.marked ? tc[Token::Fg2] : tc[Token::Fg3]) : tc[Token::Fg2];
        c.iconLine = tc[Token::Fg3];
    }
    if (tc.isWatercolor())
        c.bold = space2 && !s.marked;  // 시안2: 선택 이름은 굵게 하지 않는다
    else
        c.bold = space2 || (s.marked && a.boldSelection);
    return c;
}

void paintRecordBackground(QPainter *p, const QRect &record, const RecordGeometry &g, const ListAppearance &a,
                           const RecordState &s, bool alternate, const fm::style::ThemeColors &tc)
{
    p->save();
    p->fillRect(record, tc[Token::Surface]);
    if (g.separator == RecordSeparator::Zebra && alternate)
        p->fillRect(record, tc[Token::Alt]);

    const bool invCur = invertedCursor(a, s);
    QColor fill;
    if (invCur)
        fill = tc[Token::InvCur];
    else if (s.marked && a.invertSelection)
        fill = s.active ? tc[Token::InvSel] : tc[Token::InvSelIn];
    else if (s.marked)
        fill = s.active ? tc[Token::Sel] : tc[Token::SelIn];
    const QRectF block = g.block(record);
    if (fill.isValid()) {
        if (g.radius > 0) {
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(Qt::NoPen);
            p->setBrush(fill);
            p->drawRoundedRect(block, g.radius, g.radius);
        } else {
            p->fillRect(block, fill);
        }
    }

    if (g.separator == RecordSeparator::Tint) {
        QRectF band;
        qreal radius = 0;
        if (g.twoLine) {
            // 메타 줄 띠: 2열~끝, 위아래 1 · 왼쪽 2 안쪽, 모서리 4
            const QRectF meta = g.metaLine(record);
            const qreal left = record.left() + kIconBand;
            band = QRectF(left, meta.top(), record.right() + 1 - kRowGutter - left, meta.height()).adjusted(2, 1, 0, -1);
            radius = 4;
        } else {
            // 1줄: 메타 열(확장자~속성)에만, 위아래 2 · 왼쪽 2 안쪽, 모서리 3
            const qreal left = record.right() + 1 - (kExtWidth + kSizeWidth + kDateWidth1 + kAttrWidth1);
            band = QRectF(left, record.top(), record.right() + 1 - kRowGutter - left, record.height()).adjusted(2, 2, 0, -2);
            radius = 3;
        }
        const QColor tint = (invCur || (s.marked && a.invertSelection)) ? tc[Token::TintInv]
                          : s.marked                                    ? tc[Token::TintSel]
                                                                        : tc[Token::Tint];
        if (tc.isWatercolor())
            radius = 0;
        p->setRenderHint(QPainter::Antialiasing, radius > 0);
        p->setPen(Qt::NoPen);
        p->setBrush(tint);
        p->drawRoundedRect(band, radius, radius);
    }

    QColor line;
    switch (g.separator) {
    case RecordSeparator::Zebra:
        if (g.twoLine)
            line = tc[Token::Grid];
        break;
    case RecordSeparator::Line:
        line = g.twoLine ? tc[Token::Line] : tc[Token::Grid];
        break;
    case RecordSeparator::None:
    case RecordSeparator::Space:
    case RecordSeparator::Tint:
        break;
    }
    if (line.isValid())
        p->fillRect(QRect(record.left(), record.bottom(), record.width(), 1), line);
    p->restore();
}

QPen cursorPen(const fm::style::ThemeColors &tc, InactiveCursor inactiveCursor, const RecordState &s)
{
    const bool asActive = s.active || inactiveCursor == InactiveCursor::SameAsActive;
    if (tc.isWatercolor()) {
        // 시안2: 1 px 점선 --fg(선택 위 흰색), 비활성 패널은 없음
        if (!asActive)
            return QPen(Qt::NoPen);
        // 점선은 장치 픽셀 기준(cosmetic)으로 — 125 % 같은 배율에서 1 px 무늬가 뭉개져 실선처럼 보이지 않게
        QPen pen(s.marked && s.active ? QColor(0xFF, 0xFF, 0xFF) : tc[Token::Fg], 1.0);
        pen.setCosmetic(true);
        pen.setDashPattern({1.0, 1.0});
        return pen;
    }
    if (asActive)
        return QPen(tc[Token::Focus], 1.0);
    if (inactiveCursor == InactiveCursor::Hidden)
        return QPen(Qt::NoPen);
    QPen pen(tc[Token::Fg3], 1.0);
    pen.setCosmetic(true);
    pen.setDashPattern({3.0, 3.0});
    return pen;
}

void paintRecordCursor(QPainter *p, const QRect &record, const RecordGeometry &g, const ListAppearance &a,
                       const RecordState &s, const fm::style::ThemeColors &tc)
{
    if (!s.cursor || invertedCursor(a, s))
        return;
    const QPen pen = cursorPen(tc, a.inactiveCursor, s);
    if (pen.style() == Qt::NoPen)
        return;
    p->save();
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    if (g.radius > 0) {
        p->setRenderHint(QPainter::Antialiasing);
        p->drawRoundedRect(g.block(record).adjusted(0.5, 0.5, -0.5, -0.5), g.radius, g.radius);
    } else {
        p->setRenderHint(QPainter::Antialiasing, false);
        const QRect r = g.block(record).toRect();
        p->drawRect(r.adjusted(0, 0, -1, -1));
    }
    p->restore();
}

} // namespace fm::filelist::detail
