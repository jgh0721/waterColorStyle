// 내용 대화상자 — 시안1 Windows 11 ContentDialog · 시안2 XP 대화상자.

#include "fmwidgets/ContentDialog.h"

#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeColors.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPushButton>
#include <QScreen>
#include <QStyleOptionTitleBar>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

constexpr qreal kCardRadius = 8.0;
constexpr int kMinCard = 320;      // WinUI ContentDialog 최소 · 최대 폭
constexpr int kMaxCard = 548;
constexpr int kPad = 24;           // 시안1 안쪽 여백
constexpr int kCaption = 27;       // 시안2 제목 표시줄(목업식 틀과 같다)
constexpr int kFrame = 3;          // 시안2 창 틀
constexpr int kOpenMs = 167;
constexpr int kCloseMs = 120;

bool animationsSupported()
{
    const QString platform = QGuiApplication::platformName();
    return platform != u"offscreen" && platform != u"minimal";
}

/// 부모 창을 덮는 층 — 대화상자가 떠 있는 동안 창이 쓸 수 없음을 보인다(WinUI SmokeFillColorDefault 30 %).
class SmokeLayer : public QWidget
{
public:
    SmokeLayer(QWidget *host, qreal alpha)
        : QWidget(host)
        , m_alpha(alpha)
    {
        setObjectName(u"fmContentDialogSmoke"_s);
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        host->installEventFilter(this);
        setGeometry(host->rect());
        raise();
    }

    void setLevel(qreal level)
    {
        m_level = level;
        update();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize)
            setGeometry(parentWidget()->rect());
        return false;
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(0, 0, 0, int(255 * m_alpha * m_level)));
    }
    // 덮은 창은 누를 수 없다(모달이 막지만 커서 모양 · 마우스 올림도 없게)
    void mousePressEvent(QMouseEvent *event) override { event->accept(); }

private:
    qreal m_alpha;
    qreal m_level = 1.0;
};

} // namespace

ContentDialog::ContentDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowModality(parent ? Qt::WindowModal : Qt::ApplicationModal);
    setMouseTracking(true);

    auto *outer = new QVBoxLayout(this);
    outer->setSpacing(0);
    outer->setSizeConstraint(QLayout::SetFixedSize);

    m_body = new QWidget(this);
    m_bodyLayout = new QVBoxLayout(m_body);
    m_titleLabel = new QLabel(m_body);
    m_titleLabel->setTextFormat(Qt::PlainText);
    m_titleLabel->setWordWrap(true);
    m_textLabel = new QLabel(m_body);
    m_textLabel->setWordWrap(true);
    m_textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    m_bodyLayout->addWidget(m_titleLabel);
    m_bodyLayout->addWidget(m_textLabel);
    outer->addWidget(m_body);

    m_footer = new QWidget(this);
    auto *buttons = new QHBoxLayout(m_footer);
    buttons->addStretch(0);
    m_primary = new QPushButton(m_footer);
    m_secondary = new QPushButton(m_footer);
    m_close = new QPushButton(tr("취소"), m_footer);
    for (QPushButton *b : {m_primary, m_secondary, m_close}) {
        b->setAutoDefault(false);
        buttons->addWidget(b);
    }
    outer->addWidget(m_footer);

    connect(m_primary, &QPushButton::clicked, this, [this] {
        Q_EMIT primaryButtonClicked();
        done(Primary);
    });
    connect(m_secondary, &QPushButton::clicked, this, [this] {
        Q_EMIT secondaryButtonClicked();
        done(Secondary);
    });
    connect(m_close, &QPushButton::clicked, this, [this] {
        Q_EMIT closeButtonClicked();
        done(None);
    });
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, &ContentDialog::applyDesign);
    updateButtons();
    applyDesign();
}

ContentDialog::~ContentDialog()
{
    if (m_smoke)
        m_smoke->deleteLater();
}

ContentDialog::Result ContentDialog::ask(QWidget *parent, const QString &title, const QString &text,
                                         const QString &primaryText, const QString &secondaryText,
                                         const QString &closeText, Button defaultButton)
{
    ContentDialog dialog(parent);
    dialog.setTitle(title);
    dialog.setText(text);
    dialog.setPrimaryButtonText(primaryText);
    dialog.setSecondaryButtonText(secondaryText);
    if (!closeText.isEmpty())
        dialog.setCloseButtonText(closeText);
    dialog.setDefaultButton(defaultButton);
    return Result(dialog.exec());
}

// ---------------------------------------------------------------------------------------------
// 값

void ContentDialog::setTitle(const QString &title)
{
    m_title = title;
    m_titleLabel->setText(title);
    setWindowTitle(title);
    applyDesign();
}

QString ContentDialog::text() const
{
    return m_textLabel->text();
}

void ContentDialog::setText(const QString &text)
{
    m_textLabel->setText(text);
    m_textLabel->setVisible(!m_content && !text.isEmpty());
}

void ContentDialog::setContentWidget(QWidget *widget)
{
    if (m_content == widget)
        return;
    if (m_content)
        m_content->deleteLater();
    m_content = widget;
    if (widget) {
        widget->setParent(m_body);
        m_bodyLayout->addWidget(widget);
    }
    m_textLabel->setVisible(!widget && !m_textLabel->text().isEmpty());
}

QString ContentDialog::primaryButtonText() const
{
    return m_primary->text();
}

void ContentDialog::setPrimaryButtonText(const QString &text)
{
    m_primary->setText(text);
    updateButtons();
}

QString ContentDialog::secondaryButtonText() const
{
    return m_secondary->text();
}

void ContentDialog::setSecondaryButtonText(const QString &text)
{
    m_secondary->setText(text);
    updateButtons();
}

QString ContentDialog::closeButtonText() const
{
    return m_close->text();
}

void ContentDialog::setCloseButtonText(const QString &text)
{
    m_close->setText(text);
    updateButtons();
}

void ContentDialog::setDefaultButton(Button button)
{
    m_default = button;
    updateButtons();
}

QPushButton *ContentDialog::button(Button which) const
{
    switch (which) {
    case PrimaryButton: return m_primary;
    case SecondaryButton: return m_secondary;
    case CloseButton: return m_close;
    case NoButton: break;
    }
    return nullptr;
}

void ContentDialog::setSmokeVisible(bool visible)
{
    m_smokeVisible = visible;
    if (!visible)
        showSmoke(false);
}

void ContentDialog::updateButtons()
{
    for (Button which : {PrimaryButton, SecondaryButton, CloseButton}) {
        QPushButton *b = button(which);
        b->setVisible(!b->text().isEmpty());
        // Enter는 기본 단추만 — 시안1에서는 기본 단추만 강조색(Windows 11), 시안2는 검은 테두리
        const bool isDefault = which == m_default && b->isVisibleTo(this);
        b->setDefault(isDefault);
        fs::setButtonRole(b, isDefault ? fs::ButtonRole::Primary : fs::ButtonRole::Normal);
    }
}

// ---------------------------------------------------------------------------------------------
// 모양

QMargins ContentDialog::chromeMargins() const
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    // 시안1: 그림자 자리(아래로 치우침), 시안2: 제목 표시줄 + 창 틀
    return tc.isWatercolor() ? QMargins(kFrame, kCaption, kFrame, kFrame) : QMargins(16, 12, 16, 24);
}

QRect ContentDialog::cardRect() const
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    return tc.isWatercolor() ? rect() : rect().marginsRemoved(chromeMargins());
}

QRect ContentDialog::captionRect() const
{
    if (!fs::themeColorsFor(this).isWatercolor())
        return {};
    return QRect(0, 0, width(), kCaption);
}

QRect ContentDialog::closeButtonRect() const
{
    const QRect caption = captionRect();
    if (caption.isEmpty())
        return {};
    QStyleOptionTitleBar opt;
    opt.initFrom(this);
    opt.rect = caption;
    opt.titleBarFlags = Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint;
    opt.subControls = QStyle::SC_TitleBarLabel | QStyle::SC_TitleBarCloseButton;
    return style()->subControlRect(QStyle::CC_TitleBar, &opt, QStyle::SC_TitleBarCloseButton, this);
}

void ContentDialog::applyDesign()
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    const bool watercolor = tc.isWatercolor();
    const QMargins chrome = chromeMargins();
    layout()->setContentsMargins(chrome);

    // 제목: 시안1 20 px 굵게(카드 안), 시안2는 제목 표시줄에
    m_titleLabel->setFont(fs::pixelFont(font(), 20, QFont::DemiBold));
    m_titleLabel->setVisible(!watercolor && !m_title.isEmpty());
    m_textLabel->setFont(fs::pixelFont(font(), watercolor ? 12 : 13));
    for (QLabel *label : {m_titleLabel, m_textLabel}) {
        QPalette pal = label->palette();
        pal.setColor(QPalette::WindowText, tc[T::Fg]);
        label->setPalette(pal);
    }
    m_bodyLayout->setContentsMargins(watercolor ? QMargins(12, 12, 12, 8) : QMargins(kPad, kPad, kPad, kPad));
    m_bodyLayout->setSpacing(watercolor ? 8 : 12);
    const int inner = watercolor ? 24 : 2 * kPad;
    m_textLabel->setMinimumWidth(kMinCard - inner);
    m_textLabel->setMaximumWidth(kMaxCard - inner);

    // 단추 줄: 시안1 같은 폭으로 채움(간격 8, 여백 24), 시안2 오른쪽에 붙음(최소 폭 75)
    auto *row = static_cast<QHBoxLayout *>(m_footer->layout());
    row->setContentsMargins(watercolor ? QMargins(12, 4, 12, 12) : QMargins(kPad, kPad, kPad, kPad));
    row->setSpacing(watercolor ? 6 : 8);
    row->setStretch(0, watercolor ? 1 : 0);
    for (int i = 1; i < row->count(); ++i)
        row->setStretch(i, watercolor ? 0 : 1);
    for (QPushButton *b : {m_primary, m_secondary, m_close})
        b->setMinimumWidth(watercolor ? 75 : 0);
    update();
}

void ContentDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange)
        applyDesign();
    if (event->type() == QEvent::ActivationChange)
        update();
    QDialog::changeEvent(event);
}

void ContentDialog::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QPainter p(this);
    if (tc.isWatercolor()) {
        // XP 창: 파란 틀 + 바깥선, 스타일이 그리는 제목 표시줄, Win 바탕
        const fs::WatercolorChrome &x = fs::watercolorChrome(tc.variant());
        p.fillRect(rect(), x.frame);
        p.fillRect(rect().marginsRemoved(chromeMargins()), tc[T::Win]);
        p.setPen(x.frameOuter);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        QStyleOptionTitleBar opt;
        opt.initFrom(this);
        opt.rect = captionRect();
        opt.text = m_title;
        opt.titleBarFlags = Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint;
        opt.titleBarState = 0;
        opt.state |= QStyle::State_Active;  // 모달 대화상자는 늘 앞 창
        opt.subControls = QStyle::SC_TitleBarLabel | QStyle::SC_TitleBarCloseButton;
        opt.activeSubControls = m_closeHot || m_closeDown ? QStyle::SC_TitleBarCloseButton : QStyle::SC_None;
        if (m_closeHot)
            opt.state |= QStyle::State_MouseOver;
        if (m_closeDown)
            opt.state |= QStyle::State_Sunken;
        style()->drawComplexControl(QStyle::CC_TitleBar, &opt, &p, this);
        return;
    }

    // Windows 11: 부드러운 그림자 + 둥근 카드(위 Surface · 아래 단추 줄 Win) + 1 px 선
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF card = QRectF(cardRect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath shape;
    shape.addRoundedRect(card, kCardRadius, kCardRadius);
    QColor shadow = tc[T::Shadow];
    const qreal base = shadow.alphaF();
    const QPainterPath lifted = shape.translated(0, 6);
    p.setPen(Qt::NoPen);
    for (int i = 12; i >= 1; --i) {
        QPainterPathStroker stroker;
        stroker.setWidth(i * 2.0);
        stroker.setJoinStyle(Qt::RoundJoin);
        shadow.setAlphaF(base * 0.07 * (13 - i) / 12.0);
        p.setBrush(shadow);
        p.drawPath(stroker.createStroke(lifted).united(lifted));
    }
    p.setBrush(tc[T::Surface]);
    p.drawPath(shape);
    if (m_footer->isVisible()) {
        const qreal top = m_footer->geometry().top();
        p.save();
        p.setClipPath(shape);
        p.fillRect(QRectF(card.left(), top, card.width(), card.bottom() - top + 1), tc[T::Win]);
        p.fillRect(QRectF(card.left(), top, card.width(), 1), tc[T::Line]);
        p.restore();
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(tc[T::Line], 1));
    p.drawPath(shape);
}

// ---------------------------------------------------------------------------------------------
// 띄우기 · 닫기

void ContentDialog::showEvent(QShowEvent *event)
{
    applyDesign();
    if (isWindow()) {
        adjustSize();
        // 부모 창 가운데(없으면 화면 가운데)
        QRect area;
        if (QWidget *host = parentWidget() ? parentWidget()->window() : nullptr)
            area = host->geometry();
        else if (QScreen *screen = QGuiApplication::primaryScreen())
            area = screen->availableGeometry();
        if (area.isValid())
            move(area.center() - QPoint(width() / 2, height() / 2));
        m_closing = false;
        if (m_smokeVisible)
            showSmoke(true);
        if (animationsSupported() && !fs::themeColorsFor(this).isWatercolor()) {
            setWindowOpacity(0.0);
            if (!m_fade) {
                m_fade = new QVariantAnimation(this);
                m_fade->setEasingCurve(QEasingCurve::OutCubic);
                connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
                    setWindowOpacity(v.toReal());
                    if (auto *smoke = static_cast<SmokeLayer *>(m_smoke.data()))
                        smoke->setLevel(v.toReal());
                });
            }
            m_fade->stop();
            m_fade->setDuration(kOpenMs);
            m_fade->setStartValue(0.0);
            m_fade->setEndValue(1.0);
            m_fade->start();
        }
    }
    QDialog::showEvent(event);
}

void ContentDialog::hideEvent(QHideEvent *event)
{
    showSmoke(false);
    QDialog::hideEvent(event);
}

void ContentDialog::done(int result)
{
    if (m_closing)
        return;
    if (isWindow() && isVisible() && m_fade && animationsSupported() && !fs::themeColorsFor(this).isWatercolor()) {
        m_closing = true;
        setResult(result);
        m_fade->stop();
        m_fade->setDuration(kCloseMs);
        m_fade->setStartValue(windowOpacity());
        m_fade->setEndValue(0.0);
        connect(m_fade, &QVariantAnimation::finished, this, [this, result] {
            QDialog::done(result);
            m_closing = false;
        }, Qt::SingleShotConnection);
        m_fade->start();
        return;
    }
    QDialog::done(result);
}

void ContentDialog::showSmoke(bool show)
{
    if (!show) {
        if (m_smoke)
            m_smoke->deleteLater();
        m_smoke = nullptr;
        return;
    }
    QWidget *host = parentWidget() ? parentWidget()->window() : nullptr;
    if (!host || m_smoke)
        return;
    const bool watercolor = fs::themeColorsFor(this).isWatercolor();
    auto *smoke = new SmokeLayer(host, watercolor ? 0.18 : 0.30);
    smoke->show();
    m_smoke = smoke;
}

// ---------------------------------------------------------------------------------------------
// 시안2 제목 표시줄 — 닫기 단추 · 끌어 옮기기

void ContentDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && captionRect().contains(event->position().toPoint())) {
        if (closeButtonRect().contains(event->position().toPoint())) {
            m_closeDown = true;
            update();
        } else {
            m_dragging = true;
            m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        }
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void ContentDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        return;
    }
    const bool hot = closeButtonRect().contains(event->position().toPoint());
    if (hot != m_closeHot) {
        m_closeHot = hot;
        update(captionRect());
    }
    QDialog::mouseMoveEvent(event);
}

void ContentDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_closeDown) {
        m_closeDown = false;
        update(captionRect());
        if (closeButtonRect().contains(event->position().toPoint())) {
            Q_EMIT closeButtonClicked();
            done(None);
        }
        return;
    }
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void ContentDialog::leaveEvent(QEvent *event)
{
    if (m_closeHot) {
        m_closeHot = false;
        update(captionRect());
    }
    QDialog::leaveEvent(event);
}

} // namespace fm::ui
