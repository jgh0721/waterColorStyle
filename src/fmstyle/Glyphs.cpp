#include "fmstyle/Glyphs.h"

#include "fmstyle/ThemeColors.h"

#include <QCache>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

#include <array>

using namespace Qt::StringLiterals;

namespace fm::style {

namespace {

// %1 = 첫 번째 색, %2 = 두 번째 색. 선 아이콘은 <g>의 stroke가 %1이다.
struct Def
{
    int viewBox;
    const char *body;
};

constexpr const char kFolderPath[] =
    "M1.5 4.2c0-.9.7-1.6 1.6-1.6h3.2l1.6 1.6h5c.9 0 1.6.7 1.6 1.6v6.4c0 .9-.7 1.6-1.6 1.6H3.1c-.9 0-1.6-.7-1.6-1.6z";

#define FM_STROKE(width) "<g fill='none' stroke='%1' stroke-width='" width "' stroke-linecap='round' stroke-linejoin='round'>"

const std::array<Def, std::size_t(Glyph::Count)> &defs()
{
    static const std::array<Def, std::size_t(Glyph::Count)> table{{
        {16, ""},  // None
        {16, "<rect x='1' y='2.5' width='6.2' height='11' rx='1.6' fill='%1'/>"
             "<rect x='8.8' y='2.5' width='6.2' height='11' rx='1.6' fill='%2'/>"},
        {16, "<path d='M8 1.2l5.8 2.1v4.3c0 3.4-2.4 6-5.8 7.2-3.4-1.2-5.8-3.8-5.8-7.2V3.3z' fill='%1'/>"
             "<path d='M8 1.2v13.6c3.4-1.2 5.8-3.8 5.8-7.2V3.3z' fill='%2'/>"},
        {16, FM_STROKE("1.3") "<rect x='5' y='5' width='8.5' height='8.5' rx='1.5'/>"
             "<path d='M11 5V3.5A1.5 1.5 0 0 0 9.5 2h-6A1.5 1.5 0 0 0 2 3.5v6A1.5 1.5 0 0 0 3.5 11H5'/></g>"},
        {16, FM_STROKE("1.3") "<path d='M10.5 2.5l3 3L5.5 13.5H2.5v-3z'/><path d='M9 4l3 3'/></g>"},
        {16, FM_STROKE("1.3") "<path d='M2.5 4h11M6 4V2.5h4V4M4 4l.7 9.5h6.6L12 4'/><path d='M6.8 6.5v4.5M9.2 6.5v4.5'/></g>"},
        {16, FM_STROKE("1.3") "<path d='M3.5 1.5h5.6l3.4 3.4v9.6h-9z'/><path d='M8 7v5M5.5 9.5h5'/></g>"},
        {16, nullptr},  // NewFolder — 폴더 경로를 끼워 만든다(아래)
        {16, FM_STROKE("1.6") "<path d='M5.5 3.5v9M10.5 3.5v9'/></g>"},
        {16, "<path d='M5 3l8 5-8 5z' fill='%1'/>"},
        {16, FM_STROKE("1.4") "<circle cx='8' cy='8' r='6'/><path d='M8 4.5V8l2.5 1.5'/></g>"},
        {16, "<g fill='%1'><circle cx='3.5' cy='8' r='1.2'/><circle cx='8' cy='8' r='1.2'/><circle cx='12.5' cy='8' r='1.2'/></g>"},
        {16, FM_STROKE("1.4") "<path d='M2.5 8h11M9.5 4l4 4-4 4'/></g>"},
        {16, FM_STROKE("2") "<path d='M3 8.5l3 3 7-7'/></g>"},
        {16, FM_STROKE("1.4") "<circle cx='8' cy='8' r='6.5'/><path d='M8 7.5v3.5M8 5v.2'/></g>"},
        {16, FM_STROKE("1.4") "<path d='M8 2L14.5 13.5h-13z'/><path d='M8 6.5v3.2M8 11.4v.2'/></g>"},
        {16, FM_STROKE("1.2") "<path d='M3.5 1.5h5.6l3.4 3.4v9.6h-9z'/><path d='M9 1.6v3.4h3.4'/></g>"
             "<rect x='5' y='8.6' width='6' height='3.6' rx='0.6' fill='%2'/>"},
        {16, nullptr},  // Folder
        {16, nullptr},  // FolderOutline
        {12, FM_STROKE("1.6") "<path d='M6 2v8M2 6h8'/></g>"},
        {10, FM_STROKE("1.3") "<path d='M2.5 4L5 6.5L7.5 4'/></g>"},
        {12, FM_STROKE("1.4") "<path d='M3 7.5L6 4.5L9 7.5'/></g>"},
        {16, FM_STROKE("1.4") "<path d='M5.5 3L2.5 6l3 3'/><path d='M2.5 6h7a4 4 0 0 1 0 8h-2'/></g>"},
        {16, FM_STROKE("1.4") "<path d='M4 3v5a2 2 0 0 0 2 2h7M10.5 7.5L13 10l-2.5 2.5'/></g>"},
        {10, "<g fill='none' stroke='%1' stroke-width='1'><path d='M0.5 0.5l9 9M9.5 0.5l-9 9'/></g>"},
    }};
    return table;
}

#undef FM_STROKE

QString bodyOf(Glyph glyph)
{
    switch (glyph) {
    case Glyph::NewFolder:
        return u"<g fill='none' stroke='%1' stroke-width='1.3' stroke-linecap='round' stroke-linejoin='round'>"
               "<path d='"_s + QLatin1StringView(kFolderPath) + u"'/><path d='M8 6.5v4.5M5.8 8.7h4.4'/></g>"_s;
    case Glyph::Folder:
        return u"<path d='"_s + QLatin1StringView(kFolderPath) + u"' fill='%1'/>"_s;
    case Glyph::FolderOutline:
        return u"<path d='"_s + QLatin1StringView(kFolderPath)
             + u"' fill='none' stroke='%1' stroke-width='1.3' stroke-linejoin='round'/>"_s;
    default:
        return QString::fromLatin1(defs()[std::size_t(glyph)].body);
    }
}

QColor defaultSecondary(Glyph glyph, const QColor &primary)
{
    QColor c = primary;
    if (glyph == Glyph::App)
        c.setAlphaF(primary.alphaF() * 0.5);
    return c;
}

// QtSvg(SVG Tiny 1.2)는 rgba()를 읽지 않으므로 #rrggbb + *-opacity 속성으로 바꿔 넣는다.
void putColor(QString &svg, const QString &slot, const QColor &c)
{
    const QString hex = c.name(QColor::HexRgb);
    const QString alpha = QString::number(c.alphaF(), 'f', 3);
    for (const QString attr : {u"fill"_s, u"stroke"_s})
        svg.replace(u"%1='%2'"_s.arg(attr, slot), u"%1='%2' %1-opacity='%3'"_s.arg(attr, hex, alpha));
}

QSvgRenderer *renderer(Glyph glyph, const QColor &primary, const QColor &secondary)
{
    static QCache<QString, QSvgRenderer> cache(256);
    const QString key = u"%1|%2|%3"_s.arg(int(glyph)).arg(primary.rgba()).arg(secondary.rgba());
    if (QSvgRenderer *r = cache.object(key))
        return r;
    const int vb = glyphViewBox(glyph);
    QString body = bodyOf(glyph);
    putColor(body, u"%1"_s, primary);
    putColor(body, u"%2"_s, secondary);
    const QString svg = u"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 %1 %1'>"_s.arg(vb) + body + u"</svg>"_s;
    auto *r = new QSvgRenderer(svg.toUtf8());
    cache.insert(key, r);
    return r;
}

} // namespace

int glyphViewBox(Glyph glyph) noexcept
{
    if (glyph == Glyph::Count)
        return 16;
    return defs()[std::size_t(glyph)].viewBox;
}

void paintGlyph(QPainter *painter, Glyph glyph, const QRectF &rect, const QColor &primary, const QColor &secondary)
{
    if (glyph == Glyph::None || glyph == Glyph::Count || !painter)
        return;
    const QColor second = secondary.isValid() ? secondary : defaultSecondary(glyph, primary);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    renderer(glyph, primary, second)->render(painter, rect);
    painter->restore();
}

QIcon glyphIcon(Glyph glyph, const QColor &primary, int px, const QColor &secondary)
{
    QIcon icon;
    if (glyph == Glyph::None || glyph == Glyph::Count)
        return icon;
    for (const qreal dpr : {1.0, 2.0}) {
        QPixmap pm(QSize(px, px) * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        paintGlyph(&p, glyph, QRectF(0, 0, px, px), primary, secondary);
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

QIcon shieldIcon(const ThemeColors &colors, int px)
{
    return glyphIcon(Glyph::Shield, colors[Token::Shield], px, colors[Token::Shield2]);
}

} // namespace fm::style
