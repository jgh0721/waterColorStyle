// 대화상자 공통 부품 — DialogFooter · KeyChip · Tag · Banner · setupDialogChrome (시안1 · 시안2 공통).

#include "fmwidgets/Banner.h"
#include "fmwidgets/DialogChrome.h"
#include "fmwidgets/DialogFooter.h"
#include "fmwidgets/KeyChip.h"
#include "fmwidgets/Tag.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>

#include <QDialog>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

void repaintOnThemeChange(QWidget *w)
{
    QObject::connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, w, qOverload<>(&QWidget::update));
}

} // namespace

// ---------------------------------------------------------------------------------------------
// DialogFooter

DialogFooter::DialogFooter(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setLineWidth(1);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    fs::setFooter(this);
}

// ---------------------------------------------------------------------------------------------
// KeyChip

KeyChip::KeyChip(QWidget *parent)
    : KeyChip(QString(), parent)
{
}

KeyChip::KeyChip(const QString &keys, QWidget *parent)
    : QWidget(parent)
    , m_keys(keys)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    repaintOnThemeChange(this);
}

void KeyChip::setKeys(const QString &keys)
{
    m_keys = keys;
    updateGeometry();
    update();
}

QSize KeyChip::sizeHint() const
{
    return fs::keyChipSize(m_keys);
}

void KeyChip::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QSize s = sizeHint();
    const QRectF r(0, (height() - s.height()) / 2.0, s.width(), s.height());
    fs::paintKeyChip(&p, r, m_keys, fs::themeColorsFor(this));
}

// ---------------------------------------------------------------------------------------------
// Tag

namespace {

fs::Tone toStyleTone(Tag::Tone tone)
{
    switch (tone) {
    case Tag::Ok: return fs::Tone::Ok;
    case Tag::Info: return fs::Tone::Info;
    case Tag::Mute: return fs::Tone::Mute;
    case Tag::Bad: return fs::Tone::Bad;
    case Tag::Warn: return fs::Tone::Warn;
    case Tag::Danger: return fs::Tone::Danger;
    }
    return fs::Tone::Mute;
}

} // namespace

Tag::Tag(QWidget *parent)
    : Tag(QString(), Mute, parent)
{
}

Tag::Tag(const QString &text, Tone tone, QWidget *parent)
    : QWidget(parent)
    , m_text(text)
    , m_tone(tone)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    repaintOnThemeChange(this);
}

void Tag::setText(const QString &text)
{
    m_text = text;
    setAccessibleName(text);
    updateGeometry();
    update();
}

void Tag::setTone(Tone tone)
{
    m_tone = tone;
    updateGeometry();
    update();
}

void Tag::setGlyph(glyph::Glyph glyph)
{
    m_glyph = glyph;
    updateGeometry();
    update();
}

void Tag::setCompact(bool compact)
{
    m_compact = compact;
    updateGeometry();
    update();
}

QSize Tag::sizeHint() const
{
    return fs::tagSize(m_text, toStyleTone(m_tone), m_compact, m_glyph != glyph::None);
}

void Tag::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    const fs::Tone tone = toStyleTone(m_tone);
    QIcon icon;
    if (m_glyph != glyph::None)
        icon = fs::glyphIcon(glyph::toStyle(m_glyph), fs::toneColors(tone, tc).foreground, 12);
    QPainter p(this);
    const QSize s = sizeHint();
    fs::paintTag(&p, QRectF(0, (height() - s.height()) / 2.0, s.width(), s.height()), m_text, tone, tc, m_compact, icon);
}

// ---------------------------------------------------------------------------------------------
// Banner

namespace {

constexpr int kBannerPadX = 12;
constexpr int kBannerPadY = 10;
constexpr int kBannerIcon = 16;
constexpr int kBannerGap = 10;

fs::Tone toStyleTone(Banner::Tone tone)
{
    switch (tone) {
    case Banner::Info: return fs::Tone::Info;
    case Banner::Warn: return fs::Tone::Warn;
    case Banner::Danger: return fs::Tone::Danger;
    case Banner::Ok: return fs::Tone::Ok;
    }
    return fs::Tone::Info;
}

} // namespace

Banner::Banner(QWidget *parent)
    : Banner(Info, glyph::Info, QString(), parent)
{
}

Banner::Banner(Tone tone, glyph::Glyph glyph, const QString &text, QWidget *parent)
    : QFrame(parent)
    , m_tone(tone)
    , m_glyph(glyph)
    , m_label(new QLabel(text, this))
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(kBannerPadX + kBannerIcon + kBannerGap, kBannerPadY, kBannerPadX, kBannerPadY);
    layout->setSpacing(0);
    m_label->setWordWrap(true);
    m_label->setTextFormat(Qt::AutoText);
    m_label->setFont(fs::pixelFont(QFont(), 12.5));
    layout->addWidget(m_label, 1);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    setMinimumHeight(kBannerIcon + 2 * kBannerPadY);
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, [this] {
        applyTextColor();
        update();
    });
    applyTextColor();
}

void Banner::setTone(Tone tone)
{
    m_tone = tone;
    update();
}

void Banner::setGlyph(glyph::Glyph glyph)
{
    m_glyph = glyph;
    update();
}

QString Banner::text() const
{
    return m_label->text();
}

void Banner::setText(const QString &text)
{
    m_label->setText(text);
}

void Banner::setIconAlignment(IconAlignment alignment)
{
    m_iconAlign = alignment;
    update();
}

void Banner::applyTextColor()
{
    if (m_applying)
        return;
    m_applying = true;
    QPalette pal = m_label->palette();
    pal.setColor(QPalette::WindowText, fs::themeColorsFor(this)[T::Fg]);
    m_label->setPalette(pal);
    m_applying = false;
}

void Banner::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyTextColor();
}

void Banner::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    const fs::Tone tone = toStyleTone(m_tone);
    QPainter p(this);
    fs::paintBannerFrame(&p, QRectF(rect()), tone, tc);
    if (m_glyph == glyph::None)
        return;
    const qreal y = m_iconAlign == IconTop ? kBannerPadY + 1 : (height() - kBannerIcon) / 2.0;
    const QRectF ir(kBannerPadX, y, kBannerIcon, kBannerIcon);
    const fs::Glyph g = glyph::toStyle(m_glyph);
    if (g == fs::Glyph::Shield)
        fs::paintGlyph(&p, g, ir, tc[T::Shield], tc[T::Shield2]);
    else
        fs::paintGlyph(&p, g, ir, fs::toneColors(tone, tc).foreground);
}

// ---------------------------------------------------------------------------------------------
// setupDialogChrome

void setupDialogChrome(QDialog *dialog, const DialogChromeOptions &options)
{
    if (!dialog)
        return;
    Qt::WindowFlags flags = Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                          | Qt::WindowCloseButtonHint;
    if (options.minimizeButton)
        flags |= Qt::WindowMinimizeButtonHint;
    if (options.maximizeButton)
        flags |= Qt::WindowMaximizeButtonHint;
    dialog->setWindowFlags(flags);
    fs::setDensity(dialog, fs::Density::Dialog);

    const auto refreshIcon = [dialog, icon = options.icon] {
        const fs::ThemeColors &tc = fs::themeColorsFor(dialog);
        const fs::Glyph g = glyph::toStyle(icon);
        dialog->setWindowIcon(g == fs::Glyph::Shield ? fs::shieldIcon(tc, 16)
                                                     : fs::glyphIcon(g, tc[T::Accent], 16));
    };
    refreshIcon();
    QObject::connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, dialog, refreshIcon);

    if (options.fixedSize)
        dialog->setFixedSize(dialog->size());
}

} // namespace fm::ui
