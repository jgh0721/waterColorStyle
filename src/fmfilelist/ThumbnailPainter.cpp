#include "fmfilelist/ThumbnailPainter.h"

#include "fmfilelist/FileIconPainter.h"
#include "ListPainting_p.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeColors.h>

#include <QImage>
#include <QLinearGradient>
#include <QModelIndex>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTextLayout>

#include <cmath>

using namespace Qt::StringLiterals;

namespace fm::filelist::ThumbnailPainter {

using fm::style::Token;

namespace {

constexpr int kPad = 6;
constexpr int kGap = 4;
constexpr int kNameLine = 16;
constexpr int kInfoLine = 15;

int nameLineCount(const ThumbnailAppearance &a)
{
    return a.nameLines > 0 ? a.nameLines : 3;
}

int captionHeight(const ThumbnailAppearance &a)
{
    return 1 + nameLineCount(a) * kNameLine + (a.info == ThumbnailAppearance::Info::None ? 0 : 1 + kInfoLine) + 2;
}

QRectF fitRect(const QRectF &box, qreal aspect)
{
    if (aspect <= 0)
        return box;
    const qreal s = box.width();
    QSizeF size = aspect >= 1 ? QSizeF(s, std::round(s / aspect)) : QSizeF(std::round(s * aspect), s);
    return QRectF(box.center().x() - size.width() / 2.0, box.center().y() - size.height() / 2.0, size.width(), size.height());
}

// 그림자 0 0 0 1px rgba(0,0,0,.14), 0 1px 3px rgba(0,0,0,.12) 근사
void paintArtShadow(QPainter *p, const QRectF &r, qreal radius)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 12));
    p->drawRoundedRect(r.adjusted(-1, 0, 1, 2.5), radius + 1, radius + 1);
    p->setBrush(QColor(0, 0, 0, 36));
    p->drawRoundedRect(r.adjusted(-1, -1, 1, 1), radius + 1, radius + 1);
    p->restore();
}

QString infoText(const QModelIndex &index, ThumbnailAppearance::Info info)
{
    if (index.data(IsUpRole).toBool())
        return QString();
    const bool dir = index.data(IsDirRole).toBool();
    const QString size = dir ? u"파일 폴더"_s : index.data(SizeTextRole).toString();  // 모델의 표시 형식
    const QString date = index.data(DateTextRole).toString();
    switch (info) {
    case ThumbnailAppearance::Info::None:     return QString();
    case ThumbnailAppearance::Info::Size:     return size;
    case ThumbnailAppearance::Info::Date:     return date;
    case ThumbnailAppearance::Info::SizeDate: return dir ? date : size + u" · "_s + date;
    }
    return QString();
}

// 아무 곳에서나 줄을 바꾸고 maxLines를 넘으면 마지막 줄 끝을 줄인다.
QStringList wrapName(const QString &name, const QFont &font, int width, int maxLines)
{
    QStringList lines;
    QList<int> starts;
    QTextLayout layout(name, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(width);
        starts.append(line.textStart());
        lines.append(name.mid(line.textStart(), line.textLength()));
    }
    layout.endLayout();
    if (maxLines > 0 && lines.size() > maxLines) {
        const QString rest = name.mid(starts.at(maxLines - 1));
        lines = lines.mid(0, maxLines - 1);
        lines.append(QFontMetrics(font).elidedText(rest, Qt::ElideRight, width));
    }
    return lines;
}

} // namespace

int tileHeight(const ThumbnailAppearance &a)
{
    return kPad + a.size + kGap + captionHeight(a) + kPad;
}

void paintMockArt(QPainter *p, const QRectF &r, Art art)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    const qreal w = r.width();
    const qreal h = r.height();
    auto at = [&](qreal x, qreal y, qreal ww, qreal hh) { return QRectF(r.left() + x * w, r.top() + y * h, ww * w, hh * h); };
    switch (art) {
    case Art::Shot: {
        p->fillRect(r, QColor(0xFF, 0xFF, 0xFF));
        for (qreal y = 0.06; y < 1.0; y += 0.06)
            p->fillRect(QRectF(r.left(), r.top() + y * h, w, 1), QColor(0xE9, 0xEC, 0xF0));
        p->fillRect(at(0, 0, 1, 0.13), QColor(0xDD, 0xE1, 0xE7));
        p->fillRect(at(0, 0.13, 1, 0.03), QColor(0x1F, 0x5F, 0xD1));
        p->fillRect(QRectF(r.center().x(), r.top() + 0.16 * h, 1, h * 0.84), QColor(0xC9, 0xCE, 0xD6));
        p->fillRect(at(0.06, 0.44, 0.38, 0.08), QColor(0xCF, 0xE0, 0xFA));
        p->fillRect(at(0.56, 0.60, 0.38, 0.08), QColor(0xCF, 0xE0, 0xFA));
        break;
    }
    case Art::Video: {
        QLinearGradient bg(r.topLeft(), r.bottomRight());
        bg.setColorAt(0, QColor(0x1C, 0x23, 0x40));
        bg.setColorAt(1, QColor(0x3B, 0x2B, 0x55));
        p->fillRect(r, bg);
        QRadialGradient glow(r.left() + 0.82 * w, r.top() + 0.2 * h, 0.55 * w);
        glow.setColorAt(0, QColor(120, 140, 235, 191));
        glow.setColorAt(1, QColor(120, 140, 235, 0));
        p->fillRect(r, glow);
        p->setOpacity(0.92);
        const QRectF slide = at(0.08, 0.18, 0.5, 0.64);
        p->fillRect(slide, QColor(0xF7, 0xF9, 0xFC));
        p->fillRect(QRectF(slide.left() + slide.width() * 0.1, slide.top() + slide.height() * 0.12,
                           slide.width() * 0.6, slide.height() * 0.1), QColor(0x1F, 0x5F, 0xD1));
        for (int i = 0; i < 4; ++i)
            p->fillRect(QRectF(slide.left() + slide.width() * 0.1, slide.top() + slide.height() * (0.36 + i * 0.14),
                               slide.width() * (i == 3 ? 0.5 : 0.8), std::max(1.0, slide.height() * 0.06)),
                        QColor(0xB9, 0xC2, 0xD4));
        break;
    }
    case Art::Pdf: {
        p->fillRect(r, QColor(0xFF, 0xFF, 0xFF));
        p->fillRect(at(0.1, 0.08, 0.58, 0.08), QColor(0xC4, 0x38, 0x2D));
        p->fillRect(at(0.1, 0.22, 0.8, 0.03), QColor(0x9A, 0xA1, 0xAC));
        p->fillRect(at(0.1, 0.28, 0.6, 0.03), QColor(0x9A, 0xA1, 0xAC));
        for (qreal y = 0.38; y < 0.92; y += 0.06)
            p->fillRect(at(0.1, y, y > 0.8 ? 0.5 : 0.8, 0.025), QColor(0xD5, 0xD9, 0xDF));
        break;
    }
    case Art::Photo: {
        QLinearGradient sky(r.topLeft(), r.bottomLeft());
        sky.setColorAt(0, QColor(0x86, 0xBF, 0xE6));
        sky.setColorAt(1, QColor(0xDC, 0xEF, 0xFB));
        p->fillRect(r, sky);
        p->setBrush(QColor(0xFF, 0xE9, 0xA8));
        p->drawEllipse(QPointF(r.left() + 0.75 * w, r.top() + 0.28 * h), 0.08 * w, 0.08 * w);
        QPainterPath far;
        far.moveTo(r.left(), r.top() + 0.7 * h);
        far.quadTo(r.left() + 0.3 * w, r.top() + 0.45 * h, r.left() + 0.65 * w, r.top() + 0.68 * h);
        far.lineTo(r.right(), r.top() + 0.6 * h);
        far.lineTo(r.right(), r.bottom());
        far.lineTo(r.left(), r.bottom());
        far.closeSubpath();
        p->setBrush(QColor(0x5E, 0x8F, 0x66));
        p->drawPath(far);
        QPainterPath near;
        near.moveTo(r.left(), r.top() + 0.85 * h);
        near.quadTo(r.left() + 0.55 * w, r.top() + 0.66 * h, r.right(), r.top() + 0.82 * h);
        near.lineTo(r.right(), r.bottom());
        near.lineTo(r.left(), r.bottom());
        near.closeSubpath();
        p->setBrush(QColor(0x35, 0x5F, 0x45));
        p->drawPath(near);
        break;
    }
    case Art::None:
    case Art::Loading:
    case Art::Image:
        break;
    }
    p->restore();
}

void paintTile(QPainter *p, const QRect &rect, const QModelIndex &index, const TileState &state,
               const ThumbnailAppearance &a, const fm::style::ThemeColors &tc, const QFont &baseFont)
{
    const int ts = a.size;
    const int tw = a.tileWidth();
    const int th = tileHeight(a);
    const QRect tile(rect.left() + (rect.width() - tw) / 2, rect.top(), tw, th);
    const bool wc = tc.isWatercolor();
    const qreal radius = wc ? 0 : 6;

    detail::RecordState s;
    s.active = state.active;
    s.cursor = state.cursor;
    s.marked = index.data(MarkedRole).toBool();
    s.hidden = index.data(HiddenRole).toBool();
    const bool invCur = a.invertCursor && s.cursor && s.active;
    const bool invSel = a.invertSelection && s.marked;

    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    // 타일 바탕: 선택 = 타일 전체(역상 선택은 캡션만)
    if (s.marked && !invSel) {
        p->setPen(Qt::NoPen);
        p->setBrush(s.active ? tc[Token::Sel] : tc[Token::SelIn]);
        p->drawRoundedRect(QRectF(tile), radius, radius);
    }
    // 커서: 타일 테두리(역상 커서는 캡션 바탕으로 대신)
    if (s.cursor && !invCur) {
        const QPen pen = detail::cursorPen(tc, a.inactiveCursor, s);
        if (pen.style() != Qt::NoPen) {
            p->setPen(pen);
            p->setBrush(Qt::NoBrush);
            if (radius > 0) {
                p->drawRoundedRect(QRectF(tile).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
            } else {
                p->setRenderHint(QPainter::Antialiasing, false);
                p->drawRect(tile.adjusted(0, 0, -1, -1));
                p->setRenderHint(QPainter::Antialiasing);
            }
        }
    }

    // 그림 상자
    const QRectF box(tile.center().x() + 0.5 - ts / 2.0, tile.top() + kPad, ts, ts);
    const Art art = Art(index.data(ArtRole).toInt());
    const qreal artRadius = wc ? 0 : 2;
    p->save();
    if (s.hidden)
        p->setOpacity(0.55);
    QRectF artRect;
    switch (art) {
    case Art::Image: {
        const QImage image = index.data(ThumbnailRole).value<QImage>();
        if (image.isNull())
            break;
        const qreal aspect = qreal(image.width()) / qreal(image.height());
        artRect = a.fill ? box : fitRect(box, aspect);
        paintArtShadow(p, artRect, artRadius);
        QPainterPath clip;
        clip.addRoundedRect(artRect, artRadius, artRadius);
        p->setClipPath(clip);
        QRectF source(0, 0, image.width(), image.height());
        if (a.fill) {
            const qreal side = std::min(image.width(), image.height());
            source = QRectF((image.width() - side) / 2.0, (image.height() - side) / 2.0, side, side);
        }
        p->setRenderHint(QPainter::SmoothPixmapTransform);
        p->drawImage(artRect, image, source);
        p->setClipping(false);
        break;
    }
    case Art::Shot:
    case Art::Video:
    case Art::Pdf:
    case Art::Photo: {
        const qreal aspect = index.data(AspectRole).toReal();
        artRect = a.fill ? box : fitRect(box, aspect);
        paintArtShadow(p, artRect, artRadius);
        QPainterPath clip;
        clip.addRoundedRect(artRect, artRadius, artRadius);
        p->setClipPath(clip);
        // 채움: 비율대로 그린 그림의 가운데를 정사각형으로 자른다
        const QRectF full = a.fill ? QRectF(box.center().x() - ts * std::max(1.0, aspect) / 2.0,
                                            box.center().y() - ts * std::max(1.0, 1.0 / std::max(0.01, aspect)) / 2.0,
                                            ts * std::max(1.0, aspect), ts * std::max(1.0, 1.0 / std::max(0.01, aspect)))
                                   : artRect;
        paintMockArt(p, full, art);
        p->setClipping(false);
        break;
    }
    case Art::Loading: {
        const QRectF r(box.center().x() - ts / 2.0, box.center().y() - std::round(ts * 0.75) / 2.0, ts, std::round(ts * 0.75));
        p->setPen(Qt::NoPen);
        p->setBrush(tc[Token::Grid]);
        p->drawRoundedRect(r, wc ? 0 : 4, wc ? 0 : 4);
        const bool label = ts >= 96;
        const QFont small = fm::style::pixelFont(baseFont, 11);
        const qreal spinnerTop = r.center().y() - (label ? (18 + 4 + 14) / 2.0 : 9);
        p->setPen(QPen(tc[Token::Fg3], 2, Qt::SolidLine, Qt::RoundCap));
        p->setBrush(Qt::NoBrush);
        p->drawArc(QRectF(r.center().x() - 7, spinnerTop + 2, 14, 14), 90 * 16, -270 * 16);
        if (label) {
            p->setFont(small);
            p->drawText(QRectF(r.left(), spinnerTop + 22, r.width(), 14), Qt::AlignCenter, u"만드는 중"_s);
        }
        break;
    }
    case Art::None:
        break;
    }
    if (artRect.isNull() && art != Art::Loading) {
        // 미리보기 없음: 글리프(폴더 · 파일 = 크기 × .62, 상위 폴더 = 크기 × .46)
        const Kind kind = Kind(index.data(KindRole).toInt());
        const qreal g = std::round(ts * (kind == Kind::Up ? 0.46 : 0.62));
        FileIconPainter::paint(p, QRectF(box.center().x() - g / 2.0, box.center().y() - g / 2.0, g, g), kind,
                               tc[Token::Fg3], tc);
    }
    p->restore();

    // 배지: 그림 오른쪽 아래 3 px, 높이 15, 좌우 4, 모서리 3, rgba(0,0,0,.66), 흰 고정폭 10 px — 보통 크기 이상
    const QString badge = index.data(BadgeRole).toString();
    if (a.badges && ts >= 96 && !badge.isEmpty() && !artRect.isNull()) {
        const QFont font = fm::style::monoFont(10);
        const int bw = QFontMetrics(font).horizontalAdvance(badge) + 8;
        const QRectF br(artRect.right() - 3 - bw, artRect.bottom() - 3 - 15, bw, 15);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 168));
        p->drawRoundedRect(br, wc ? 0 : 3, wc ? 0 : 3);
        p->setFont(font);
        p->setPen(QColor(0xFF, 0xFF, 0xFF));
        p->drawText(br, Qt::AlignCenter, badge);
    }

    // 캡션: 폭 100 %, 안쪽 1 4 2, 모서리 4
    const QRect caption(tile.left() + kPad, int(box.bottom()) + kGap, tw - 2 * kPad, captionHeight(a));
    QColor nameColor = s.hidden ? tc[Token::Fg3] : tc[Token::Fg];
    QColor infoColor = tc[Token::Fg3];
    QColor captionFill;
    if (invCur) {
        captionFill = tc[Token::InvCur];
        nameColor = infoColor = s.marked ? tc[Token::InvCurSel] : tc[Token::OnInvCur];
    } else if (invSel) {
        captionFill = s.active ? tc[Token::InvSel] : tc[Token::InvSelIn];
        nameColor = infoColor = tc[Token::OnInvSel];
    } else if (wc && s.marked && s.active) {
        nameColor = infoColor = QColor(0xFF, 0xFF, 0xFF);
    }
    if (captionFill.isValid()) {
        p->setPen(Qt::NoPen);
        p->setBrush(captionFill);
        p->drawRoundedRect(QRectF(caption), wc ? 0 : 4, wc ? 0 : 4);
    }
    const bool bold = !wc && s.marked && a.boldSelection;
    const QFont nameFont = fm::style::pixelFont(baseFont, 12.5, bold ? QFont::DemiBold : QFont::Normal);
    const int textWidth = caption.width() - 8;
    const QStringList lines = wrapName(index.data(FullNameRole).toString(), nameFont, textWidth, a.nameLines);
    p->setFont(nameFont);
    p->setPen(nameColor);
    int y = caption.top() + 1;
    const int maxLines = nameLineCount(a);
    for (int i = 0; i < lines.size() && i < maxLines; ++i) {
        p->drawText(QRect(caption.left() + 4, y, textWidth, kNameLine), Qt::AlignHCenter | Qt::AlignVCenter, lines.at(i));
        y += kNameLine;
    }
    const QString info = infoText(index, a.info);
    if (!info.isEmpty()) {
        const QFont infoFont = fm::style::withTabularNumbers(fm::style::pixelFont(baseFont, 11.5));
        p->setFont(infoFont);
        p->setPen(infoColor);
        const int infoTop = caption.top() + 1 + std::min<int>(int(lines.size()), maxLines) * kNameLine + 1;
        p->drawText(QRect(caption.left() + 4, infoTop, textWidth, kInfoLine), Qt::AlignHCenter | Qt::AlignVCenter,
                    QFontMetrics(infoFont).elidedText(info, Qt::ElideRight, textWidth));
    }
    p->restore();
}

} // namespace fm::filelist::ThumbnailPainter
