#include "fmfilelist/FileIconPainter.h"

#include <fmstyle/ThemeColors.h>

#include <QPainter>
#include <QPainterPath>

namespace fm::filelist::FileIconPainter {

namespace {

// M1.5 4.2c0-.9.7-1.6 1.6-1.6h3.2l1.6 1.6h5c.9 0 1.6.7 1.6 1.6v6.4c0 .9-.7 1.6-1.6 1.6H3.1c-.9 0-1.6-.7-1.6-1.6z
QPainterPath folderPath()
{
    QPainterPath p;
    p.moveTo(1.5, 4.2);
    p.cubicTo(1.5, 3.3, 2.2, 2.6, 3.1, 2.6);
    p.lineTo(6.3, 2.6);
    p.lineTo(7.9, 4.2);
    p.lineTo(12.9, 4.2);
    p.cubicTo(13.8, 4.2, 14.5, 4.9, 14.5, 5.8);
    p.lineTo(14.5, 12.2);
    p.cubicTo(14.5, 13.1, 13.8, 13.8, 12.9, 13.8);
    p.lineTo(3.1, 13.8);
    p.cubicTo(2.2, 13.8, 1.5, 13.1, 1.5, 12.2);
    p.closeSubpath();
    return p;
}

// 화면 px 기준 굵기를 viewBox 단위로 바꾼다.
qreal strokeUnits(qreal px, qreal viewBoxUnitsAt16)
{
    if (px >= 40)
        return 1.5 * 16.0 / px;  // 큰 글리프는 화면 1.5 px
    if (px >= 18)
        return 1.0 * viewBoxUnitsAt16 / 1.2;  // 20 px: 파일 윤곽 1.0
    return viewBoxUnitsAt16;
}

} // namespace

void paint(QPainter *painter, const QRectF &rect, Kind kind, const QColor &line,
           const fm::style::ThemeColors &colors, qreal opacity)
{
    const qreal px = std::min(rect.width(), rect.height());
    if (px <= 0)
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setOpacity(painter->opacity() * opacity);
    painter->translate(rect.center().x() - px / 2.0, rect.center().y() - px / 2.0);
    painter->scale(px / 16.0, px / 16.0);

    switch (kind) {
    case Kind::Up: {
        // 위 화살표 M8 13V3.5M4.5 7L8 3.5L11.5 7, 선 1.4
        painter->setPen(QPen(line, strokeUnits(px, 1.4), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath p;
        p.moveTo(8, 13);
        p.lineTo(8, 3.5);
        p.moveTo(4.5, 7);
        p.lineTo(8, 3.5);
        p.lineTo(11.5, 7);
        painter->drawPath(p);
        break;
    }
    case Kind::Folder:
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors[fm::style::Token::Folder]);
        painter->drawPath(folderPath());
        break;
    default: {
        // 윤곽 M3.5 1.5h5.6l3.4 3.4v9.6h-9z + 접힌 귀 M9 1.6v3.4h3.4 + 종류 띠 rect 5,8.6 6×3.6 rx0.6
        painter->setPen(QPen(line, strokeUnits(px, 1.2), Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        QPainterPath page;
        page.moveTo(3.5, 1.5);
        page.lineTo(9.1, 1.5);
        page.lineTo(12.5, 4.9);
        page.lineTo(12.5, 14.5);
        page.lineTo(3.5, 14.5);
        page.closeSubpath();
        painter->drawPath(page);
        painter->drawPolyline(QPolygonF{{9.0, 1.6}, {9.0, 5.0}, {12.4, 5.0}});
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors[kindToken(kind)]);
        painter->drawRoundedRect(QRectF(5.0, 8.6, 6.0, 3.6), 0.6, 0.6);
        break;
    }
    }
    painter->restore();
}

} // namespace fm::filelist::FileIconPainter
