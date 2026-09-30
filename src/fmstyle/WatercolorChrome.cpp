#include "fmstyle/WatercolorChrome.h"

namespace fm::style {

namespace {

QColor rgb(QRgb v) { return QColor::fromRgb(v); }
QColor rgba(int r, int g, int b, qreal a) { return QColor(r, g, b, qRound(a * 255)); }

WatercolorChrome makeLight()
{
    WatercolorChrome c;
    c.out = rgb(0x000000);
    c.hi = rgb(0xF9F9F7);
    c.lo = rgb(0xA1A18C);
    c.g1 = rgb(0xF5F5F1);
    c.g2 = rgb(0xEBEBE4);
    c.g3 = rgb(0xE1E1D7);
    c.h1 = rgb(0xA9C8E9);
    c.h2 = rgb(0x9CBEE3);
    c.h3 = rgb(0x8FB4DD);
    c.hoverHi = rgb(0xE1ECF7);
    c.hoverLo = rgb(0x4F7BAA);
    c.hoverFg = rgb(0x000000);
    c.p1 = rgb(0x8FB4DD);
    c.p2 = rgb(0x83ABD6);
    c.p3 = rgb(0x77A1CF);
    c.disLine = rgb(0xBEBEB8);
    c.disFg = rgb(0x767672);
    c.fieldOuter = rgb(0x000000);
    c.fieldBright = rgb(0xF9F9F7);
    c.fieldInner = rgb(0xA1A18C);
    c.checkOuter = rgb(0x9A9286);
    c.checkInner = rgb(0x403E3B);
    c.checkBright = rgb(0xFFFFFF);
    c.checkLight = rgb(0xD4CFC7);
    c.title = rgb(0x629DF6);
    c.titleHi = rgb(0x85B2F5);
    c.titleLo = rgb(0x578CDD);
    c.titleLo2 = rgb(0x5E97ED);
    c.titleEdge = rgb(0x88B5F8);
    c.titleCap = rgb(0x3573D6);
    c.frame = rgb(0x3470D1);
    c.frameOuter = rgb(0x6896E0);
    c.capFill = rgb(0x3573D6);
    c.capLine = rgb(0x86ABE6);
    c.capGlyph = rgb(0xCCDCF5);
    c.capHoverLine = rgb(0xCCDCF5);
    c.mosaic1 = rgb(0x8FBCFF);
    c.mosaic2 = rgb(0x78A9FF);
    c.mosaic3 = rgb(0x5E94EA);
    c.titleInactive = rgb(0x2C60B2);
    c.titleInactiveHi = rgb(0x567DBA);
    c.capInactiveLine = rgb(0x80A0D1);
    c.capInactiveGlyph = rgb(0xCAD7EC);
    c.tab = rgb(0xD7D7CA);
    c.tabLine = rgb(0xC2C2B1);
    c.tabHover = rgb(0x9CBEE3);
    c.tabHi = rgb(0xFFFFFF);
    c.trough = rgb(0xEBEBE4);
    c.menuLine = rgb(0xA1A18C);
    c.menuHover = rgba(36, 72, 132, 0.40);
    c.menuHoverLine = rgba(36, 72, 132, 0.60);
    c.danger1 = rgb(0xD27272);
    c.danger2 = rgb(0xB84A4A);
    c.danger3 = rgb(0xA53D3D);
    c.dangerOuter = rgb(0x7A2020);
    c.sbTrack = rgb(0xE1E1D7);
    c.sbThumb = rgb(0xFFFFFF);
    c.sbThumbLine = rgb(0xC2C2B1);
    c.sbGrip = rgb(0xC5C5BD);
    c.sbArrow = rgb(0x000000);
    c.tipBg = rgb(0xFFFFBF);
    c.tipLine = rgb(0x000000);
    c.tipFg = rgb(0x000000);
    return c;
}

// 다크: 캔버스의 .fm.dark — 파란 창 틀은 그대로, 몸통만 어둡게.
WatercolorChrome makeDark()
{
    WatercolorChrome c;
    c.out = rgb(0x8C8C82);
    c.hi = rgb(0x5C5C55);
    c.lo = rgb(0x1A1A17);
    c.g1 = rgb(0x4A4A44);
    c.g2 = rgb(0x3F3F3A);
    c.g3 = rgb(0x363631);
    c.h1 = rgb(0x5282C8);
    c.h2 = rgb(0x4574BA);
    c.h3 = rgb(0x3C69AC);
    c.hoverHi = rgb(0x7EA3DA);
    c.hoverLo = rgb(0x1F3F72);
    c.hoverFg = rgb(0xFFFFFF);
    c.p1 = rgb(0x2F5796);
    c.p2 = rgb(0x2A4F8A);
    c.p3 = rgb(0x25477E);
    c.disLine = rgb(0x55554E);
    c.disFg = rgb(0x7C7C74);
    c.fieldOuter = rgb(0x0B0B0A);
    c.fieldBright = rgb(0x6A6A62);
    c.fieldInner = rgb(0x111110);
    c.checkOuter = rgb(0x0E0E0C);
    c.checkInner = rgb(0x050504);
    c.checkBright = rgb(0x8A8A80);
    c.checkLight = rgb(0x4E4E47);
    c.title = rgb(0x3F70C4);
    c.titleHi = rgb(0x5A89D8);
    c.titleLo = rgb(0x2F5CA8);
    c.titleLo2 = rgb(0x355FAE);
    c.titleEdge = rgb(0x5F8FDE);
    c.titleCap = rgb(0x2A55A0);
    c.frame = rgb(0x2A56A6);
    c.frameOuter = rgb(0x4D74BA);
    c.capFill = rgb(0x2A55A0);
    c.capLine = rgb(0x6A8CC8);
    c.capGlyph = rgb(0xD2DEF2);
    c.capHoverLine = rgb(0xD2DEF2);
    c.mosaic1 = rgb(0x6795E2);
    c.mosaic2 = rgb(0x5585D6);
    c.mosaic3 = rgb(0x3F6DBC);
    c.titleInactive = rgb(0x33568F);
    c.titleInactiveHi = rgb(0x4A6AA0);
    c.capInactiveLine = rgb(0x5A78AA);
    c.capInactiveGlyph = rgb(0xB8C6DE);
    c.tab = rgb(0x3A3A35);
    c.tabLine = rgb(0x55554D);
    c.tabHover = rgb(0x3F5F8E);
    c.tabHi = rgb(0x5C5C55);
    c.trough = rgb(0x262623);
    c.menuLine = rgb(0x6C6C63);
    c.menuHover = rgba(80, 130, 210, 0.38);
    c.menuHoverLine = rgba(120, 165, 230, 0.60);
    c.danger1 = rgb(0xC46060);
    c.danger2 = rgb(0xB04848);
    c.danger3 = rgb(0x9C3E3E);
    c.dangerOuter = rgb(0xD89090);
    c.sbTrack = rgb(0x363631);
    c.sbThumb = rgb(0x4A4A44);
    c.sbThumbLine = rgb(0x6C6C63);
    c.sbGrip = rgb(0x6C6C63);
    c.sbArrow = rgb(0xECECE6);
    c.tipBg = rgb(0x3A3826);
    c.tipLine = rgb(0x8C8C82);
    c.tipFg = rgb(0xECECE6);
    return c;
}

} // namespace

const WatercolorChrome &watercolorChrome(Variant variant) noexcept
{
    static const WatercolorChrome light = makeLight();
    static const WatercolorChrome dark = makeDark();
    return variant == Variant::Light ? light : dark;
}

} // namespace fm::style
