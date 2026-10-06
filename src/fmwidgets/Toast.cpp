// 알림(토스트) — 일을 끝낸 뒤 잠깐 떠 있다 스스로 사라지는 쪽지 (시안1 · 시안2 공통).

#include "fmwidgets/Toast.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeColors.h>
#include <fmstyle/ThemeManager.h>

#include <QEasingCurve>
#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTimerEvent>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

constexpr int kPadX = 12;
constexpr int kPadY = 10;
constexpr int kIcon = 16;
constexpr int kGap = 10;
constexpr int kTextPx = 12;      // 12.5 px — 글꼴은 pixelFont로 소수 크기를 준다
constexpr qreal kTextPxF = 12.5;
constexpr int kMaxWidth = 420;
constexpr int kMinWidth = 220;
constexpr int kStackGap = 8;
constexpr int kBottomGap = 16;
constexpr int kFadeInMs = 180;
constexpr int kFadeOutMs = 220;
constexpr qreal kRise = 6.0;
constexpr int kActionPadX = 10;
constexpr int kActionGap = 10;

/// 글이 길면 더 오래 둔다 — 60 자마다 1.2 초, 10 초까지.
int durationFor(const QString &text)
{
    const int extra = static_cast<int>(text.size() / 60) * 1200;
    return std::min(Toast::kDefaultMs + extra, 10000);
}

fs::Tone toStyleTone(Toast::Tone tone)
{
    switch (tone) {
    case Toast::Ok: return fs::Tone::Ok;
    case Toast::Warn: return fs::Tone::Warn;
    case Toast::Danger: return fs::Tone::Danger;
    case Toast::Info: break;
    }
    return fs::Tone::Info;
}

glyph::Glyph defaultGlyphFor(Toast::Tone tone)
{
    switch (tone) {
    case Toast::Ok: return glyph::Check;
    case Toast::Warn: return glyph::Warning;
    case Toast::Danger: return glyph::Warning;
    case Toast::Info: break;
    }
    return glyph::Info;
}

/// 리치 텍스트일 수 있으므로 QTextDocument로 재서 그린다 — <b>가 들어간 글의 폭이 맞게.
QTextDocument *documentFor(const QString &text, const QFont &font, int width, const QColor &color)
{
    auto *doc = new QTextDocument;
    doc->setDefaultFont(font);
    doc->setDocumentMargin(0);
    doc->setTextWidth(width);
    doc->setHtml(u"<span style=\"color:%1\">%2</span>"_s.arg(color.name(QColor::HexRgb), text));
    return doc;
}

} // namespace

// ---------------------------------------------------------------------------------------------

Toast::Toast(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, qOverload<>(&QWidget::update));
}

Toast::Toast(Tone tone, glyph::Glyph glyph, const QString &text, QWidget *parent)
    : Toast(parent)
{
    m_tone = tone;
    m_glyph = glyph;
    m_text = text;
}

Toast::~Toast() = default;

// ---------------------------------------------------------------------------------------------
// 띄우기

Toast *Toast::showOver(QWidget *host, Tone tone, const QString &text, int ms)
{
    return showOver(host, tone, defaultGlyphFor(tone), text, ms);
}

Toast *Toast::showOver(QWidget *host, Tone tone, glyph::Glyph glyph, const QString &text, int ms)
{
    if (!host)
        return nullptr;

    auto *t = new Toast(tone, glyph, text, host);
    t->setAttribute(Qt::WA_DeleteOnClose);
    t->setDuration(ms == 0 ? durationFor(text) : ms);
    t->showOn(host);
    return t;
}

QList<Toast *> Toast::toastsOn(QWidget *host)
{
    QList<Toast *> out;
    if (!host)
        return out;
    // 자식 차례가 곧 띄운 차례다 — 아래에서 위로 쌓으려면 그 차례를 지킨다.
    const auto children = host->findChildren<Toast *>(QString(), Qt::FindDirectChildrenOnly);
    for (Toast *t : children) {
        if (!t->m_closing)
            out.append(t);
    }
    return out;
}

void Toast::dismissAll(QWidget *host)
{
    const auto list = toastsOn(host);
    for (Toast *t : list)
        t->dismiss();
}

void Toast::showOn(QWidget *host)
{
    QWidget *h = host ? host : parentWidget();
    if (!h)
        return;
    if (parentWidget() != h)
        setParent(h);

    if (m_host && m_host != h)
        m_host->removeEventFilter(this);
    m_host = h;
    m_host->installEventFilter(this);

    // 상한을 넘으면 가장 오래된 것부터 닫는다 — 쌓인 쪽지가 창을 덮지 않게.
    const auto others = toastsOn(m_host);
    for (int i = 0; i + kMaxStack < others.size() + 1; ++i) {
        if (others.at(i) != this)
            others.at(i)->dismiss();
    }

    relayout();
    show();
    raise();
    startTimer();
    fadeTo(1.0, kFadeInMs, false);
    restack(m_host);
}

void Toast::dismiss()
{
    if (m_closing)
        return;
    m_closing = true;
    if (m_timerId != 0) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    Q_EMIT dismissed();
    QPointer<QWidget> host(m_host);
    fadeTo(0.0, kFadeOutMs, true);
    if (host)
        restack(host);
}

void Toast::restack(QWidget *host)
{
    if (!host)
        return;
    const auto list = toastsOn(host);
    int y = host->height() - kBottomGap;
    // 새것이 아래 — 뒤에서부터 올라가며 쌓는다.
    for (int i = list.size() - 1; i >= 0; --i) {
        Toast *t = list.at(i);
        const QSize s = t->sizeHint();
        y -= s.height();
        t->setGeometry((host->width() - s.width()) / 2, y, s.width(), s.height());
        t->raise();
        y -= kStackGap;
    }
}

// ---------------------------------------------------------------------------------------------
// 값

void Toast::setTone(Tone tone)
{
    m_tone = tone;
    update();
}

void Toast::setGlyph(glyph::Glyph glyph)
{
    m_glyph = glyph;
    update();
}

void Toast::setText(const QString &text)
{
    m_text = text;
    relayout();
    update();
}

void Toast::setActionText(const QString &text)
{
    m_actionText = text;
    // 단추가 있으면 스스로 닫지 않는다 — 누를 것이 있는데 사라지면 누를 수 없다.
    if (!m_actionText.isEmpty() && m_timerId != 0) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    relayout();
    update();
}

void Toast::setDuration(int ms)
{
    m_duration = ms;
    if (isVisible())
        startTimer();
}

void Toast::startTimer()
{
    if (m_timerId != 0) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    if (m_duration <= 0 || !m_actionText.isEmpty())
        return;
    m_timerId = QObject::startTimer(m_duration);
}

void Toast::relayout()
{
    if (!m_host)
        return;
    const QSize s = sizeHint();
    resize(s);
    restack(m_host);
}

void Toast::fadeTo(qreal target, int ms, bool thenDelete)
{
    if (m_fade) {
        m_fade->stop();
        m_fade->deleteLater();
        m_fade = nullptr;
    }
    m_fade = new QVariantAnimation(this);
    m_fade->setDuration(ms);
    m_fade->setStartValue(m_opacity);
    m_fade->setEndValue(target);
    m_fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this, target](const QVariant &v) {
        m_opacity = v.toReal();
        // 들어올 때만 아래에서 올라온다. 나갈 때는 자리를 지킨다 — 다른 쪽지가 밀려 보이지 않게.
        m_rise = target > 0.5 ? kRise * (1.0 - m_opacity) : 0.0;
        update();
    });
    if (thenDelete) {
        connect(m_fade, &QVariantAnimation::finished, this, &QWidget::close);
    }
    m_fade->start(QAbstractAnimation::DeleteWhenStopped);
}

// ---------------------------------------------------------------------------------------------
// 크기

int Toast::wrappedTextHeight(int width) const
{
    const QFont f = fs::pixelFont(font(), kTextPxF);
    const QColor c = fs::themeColorsFor(this)[T::Fg];
    QTextDocument *doc = documentFor(m_text, f, width, c);
    const int h = static_cast<int>(std::ceil(doc->size().height()));
    delete doc;
    return std::max(h, kIcon);
}

QSize Toast::sizeHint() const
{
    const QFont f = fs::pixelFont(font(), kTextPxF);
    const QFontMetrics fm(f);

    int actionW = 0;
    if (!m_actionText.isEmpty())
        actionW = kActionGap + fm.horizontalAdvance(m_actionText) + 2 * kActionPadX;

    const int chrome = 2 * kPadX + kIcon + kGap + actionW;
    const int wantText = fm.horizontalAdvance(QTextDocumentFragment::fromHtml(m_text).toPlainText());
    int w = std::clamp(chrome + wantText, kMinWidth, kMaxWidth);
    if (m_host)
        w = std::min(w, std::max(kMinWidth, m_host->width() - 2 * kBottomGap));

    const int textW = std::max(40, w - chrome);
    const int h = 2 * kPadY + wrappedTextHeight(textW);
    return QSize(w, h);
}

QSize Toast::minimumSizeHint() const
{
    return QSize(kMinWidth, 2 * kPadY + kIcon);
}

QRect Toast::actionRect() const
{
    if (m_actionText.isEmpty())
        return {};
    const QFontMetrics fm(fs::pixelFont(font(), kTextPxF));
    const int w = fm.horizontalAdvance(m_actionText) + 2 * kActionPadX;
    const int h = kIcon + 6;
    return QRect(width() - kPadX - w, (height() - h) / 2, w, h);
}

QRect Toast::textRect() const
{
    const QRect ar = actionRect();
    const int right = ar.isNull() ? width() - kPadX : ar.left() - kActionGap;
    return QRect(kPadX + kIcon + kGap, kPadY, std::max(10, right - kPadX - kIcon - kGap), height() - 2 * kPadY);
}

// ---------------------------------------------------------------------------------------------
// 그리기 · 입력

void Toast::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    const fs::Tone tone = toStyleTone(m_tone);
    const fs::ToneColors tcol = fs::toneColors(tone, tc);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setOpacity(m_opacity);
    p.translate(0.0, m_rise);

    // 쪽지는 창 위에 떠 있다 — 배너와 달리 그림자와 또렷한 바탕이 있어야 글이 읽힌다.
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = fs::squareCorners(tc) ? 0.0 : 6.0;
    QColor shadow = tc[T::Shadow];
    shadow.setAlphaF(shadow.alphaF() * 0.55);
    p.setPen(Qt::NoPen);
    p.setBrush(shadow);
    p.drawRoundedRect(r.translated(0, 2), radius, radius);

    fs::paintBannerFrame(&p, r, tone, tc);
    // 배너 바탕은 반투명일 수 있다 — 창 위에서는 한 번 더 깔아 글이 흐려지지 않게 한다.
    if (tcol.background.alpha() < 255) {
        p.setPen(Qt::NoPen);
        p.setBrush(tc[T::Surface]);
        p.drawRoundedRect(r, radius, radius);
        fs::paintBannerFrame(&p, r, tone, tc);
    }

    if (m_glyph != glyph::None) {
        const QRectF ir(kPadX, (height() - kIcon) / 2.0, kIcon, kIcon);
        const fs::Glyph g = glyph::toStyle(m_glyph);
        if (g == fs::Glyph::Shield)
            fs::paintGlyph(&p, g, ir, tc[T::Shield], tc[T::Shield2]);
        else
            fs::paintGlyph(&p, g, ir, tcol.foreground);
    }

    const QRect tr = textRect();
    QTextDocument *doc = documentFor(m_text, fs::pixelFont(font(), kTextPxF), tr.width(), tc[T::Fg]);
    p.save();
    p.translate(tr.left(), tr.top() + std::max(0.0, (tr.height() - doc->size().height()) / 2.0));
    doc->drawContents(&p);
    p.restore();
    delete doc;

    if (const QRect ar = actionRect(); !ar.isNull()) {
        p.setPen(QPen(tc[T::BtnLine], 1.0));
        p.setBrush(m_actionHot ? tc[T::Alt] : tc[T::Btn]);
        p.drawRoundedRect(QRectF(ar).adjusted(0.5, 0.5, -0.5, -0.5), radius ? 4.0 : 0.0, radius ? 4.0 : 0.0);
        p.setPen(tc[T::Fg]);
        p.setFont(fs::pixelFont(font(), kTextPxF, QFont::DemiBold));
        p.drawText(ar, Qt::AlignCenter, m_actionText);
    }
}

void Toast::mousePressEvent(QMouseEvent *event)
{
    if (const QRect ar = actionRect(); !ar.isNull() && ar.contains(event->pos())) {
        Q_EMIT actionTriggered();
        dismiss();
        return;
    }
    dismiss();
}

void Toast::mouseMoveEvent(QMouseEvent *event)
{
    const QRect ar = actionRect();
    const bool hot = !ar.isNull() && ar.contains(event->pos());
    if (hot != m_actionHot) {
        m_actionHot = hot;
        update();
    }
    setCursor(hot ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void Toast::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_hovering = true;
    // 읽는 중에 사라지지 않게 — 시계를 멈춘다.
    if (m_timerId != 0) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    update();
}

void Toast::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hovering = false;
    m_actionHot = false;
    if (!m_closing)
        startTimer();
    update();
}

void Toast::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        update();
}

bool Toast::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_host && (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest)) {
        relayout();
    }
    return QWidget::eventFilter(watched, event);
}

void Toast::timerEvent(QTimerEvent *event)
{
    if (event->timerId() != m_timerId) {
        QWidget::timerEvent(event);
        return;
    }
    killTimer(m_timerId);
    m_timerId = 0;
    if (!m_hovering)
        dismiss();
}

} // namespace fm::ui
