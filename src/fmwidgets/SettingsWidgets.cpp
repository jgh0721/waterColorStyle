#include "fmwidgets/SettingsWidgets.h"

#include "fmwidgets/Label.h"

#include <fmstyle/ColorScheme.h>
#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeManager.h>

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QColorDialog>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpressionValidator>
#include <QStandardItemModel>
#include <QStyleOptionComboBox>
#include <QStyleOptionFocusRect>
#include <QStylePainter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

using namespace Qt::StringLiterals;

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

QColor swatchLine(const fs::ThemeColors &tc)
{
    return tc.isDark() ? QColor(255, 255, 255, 51) : QColor(16, 20, 28, 46);
}

void repaintOnThemeChange(QWidget *w)
{
    QObject::connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, w, qOverload<>(&QWidget::update));
}

bool showMnemonic(const QWidget *w)
{
    QStyleOption option;
    option.initFrom(w);
    return w->style()->styleHint(QStyle::SH_UnderlineShortcut, &option, w);
}

} // namespace

// ---------------------------------------------------------------------------------------------
// 그리기 도우미

QSize chipSize(const QString &text, qreal px, int padX, int height)
{
    const QFontMetricsF fm(fs::pixelFont(QFont(), px, QFont::DemiBold));
    return QSize(int(std::ceil(fm.horizontalAdvance(text))) + 2 * padX, height);
}

void paintChip(QPainter *p, const QRectF &rect, const QString &text, ChipKind kind, const fs::ThemeColors &tc, qreal radius,
               qreal px, bool bold)
{
    QColor bg = Qt::transparent;
    QColor fg = tc[T::Fg3];
    QColor border = Qt::transparent;
    Qt::PenStyle borderStyle = Qt::SolidLine;
    switch (kind) {
    case ChipKind::Base:     bg = tc[T::Grid]; fg = tc[T::Fg3]; break;
    case ChipKind::Link:     border = tc[T::Line]; fg = tc[T::AccentFg]; break;
    case ChipKind::Changed:  bg = tc[T::AccentSoft]; border = tc[T::Accent]; fg = tc[T::AccentFg]; break;
    case ChipKind::Override: bg = tc[T::WarnBg]; border = tc[T::WarnLine]; fg = tc[T::Warn]; break;
    case ChipKind::Fixed:
    case ChipKind::Ok:       bg = tc[T::OkBg]; fg = tc[T::Ok]; break;
    case ChipKind::Warn:     bg = tc[T::WarnBg]; fg = tc[T::Warn]; break;
    case ChipKind::Mute:     bg = tc[T::Grid]; fg = tc[T::Fg2]; break;
    case ChipKind::Accent:   bg = tc[T::AccentSoft]; fg = tc[T::AccentFg]; break;
    case ChipKind::Dashed:   border = tc[T::Line]; fg = tc[T::Fg3]; borderStyle = Qt::DashLine; break;
    }
    const qreal r = tc.isWatercolor() ? 0 : radius;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, r > 0);
    QPen pen(border, 1.0, borderStyle);
    if (borderStyle == Qt::DashLine) {
        pen.setCosmetic(true);
        pen.setDashPattern({3, 2});
    }
    p->setPen(border.alpha() ? pen : QPen(Qt::NoPen));
    p->setBrush(bg);
    p->drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    p->setPen(fg);
    p->setFont(fs::pixelFont(QFont(), px, bold ? QFont::DemiBold : QFont::Normal));
    p->drawText(rect, Qt::AlignCenter, text);
    p->restore();
}

QSize keyCapSize(const QString &text)
{
    const QFontMetricsF fm(fs::monoFont(11.5));
    return QSize(int(std::ceil(fm.horizontalAdvance(text))) + 12, 20);
}

void paintKeyCap(QPainter *p, const QRectF &rect, const QString &text, const fs::ThemeColors &tc, bool dim, bool hot)
{
    const qreal r = tc.isWatercolor() ? 0 : 4;
    const QColor border = hot ? tc[T::Accent] : dim ? tc[T::Grid] : tc[T::Line];
    const QColor fill = hot ? tc[T::AccentSoft] : tc[T::Surface];
    const QColor fg = hot ? tc[T::AccentFg] : dim ? tc[T::Fg3] : tc[T::Fg];
    p->save();
    p->setRenderHint(QPainter::Antialiasing, r > 0);
    // 아래 테두리 2 px — 바깥 사각을 테두리 색으로 칠하고 안을 1 px 줄여 채운다
    p->setPen(Qt::NoPen);
    p->setBrush(border);
    p->drawRoundedRect(rect, r, r);
    p->setBrush(fill);
    p->drawRoundedRect(rect.adjusted(1, 1, -1, -2), std::max<qreal>(0, r - 1), std::max<qreal>(0, r - 1));
    p->setPen(fg);
    p->setFont(fs::monoFont(11.5));
    p->drawText(rect.adjusted(0, 0, 0, -1), Qt::AlignCenter, text);
    p->restore();
}

void paintSwatch(QPainter *p, const QRectF &rect, const QColor &color, const fs::ThemeColors &tc, qreal radius, const QColor &base,
                 bool shadow)
{
    const qreal r = tc.isWatercolor() ? 0 : radius;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRectF inner = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    if (shadow) {
        // 그림자 견본 — 목록 바탕 사각에 2단 그림자
        p->setPen(Qt::NoPen);
        QColor s1 = color;
        s1.setAlphaF(color.alphaF() * 0.6);
        p->setBrush(s1);
        p->drawRoundedRect(inner.translated(0, 2).adjusted(1, 1, -1, 0), r, r);
        p->setBrush(tc[T::Surface]);
        p->drawRoundedRect(inner.adjusted(1, 0, -1, -2), r, r);
        p->restore();
        return;
    }
    if (color.alpha() < 255) {
        p->setPen(Qt::NoPen);
        p->setBrush(base.isValid() ? base : tc[T::Surface]);
        p->drawRoundedRect(inner, r, r);
    }
    p->setPen(QPen(swatchLine(tc), 1.0));
    p->setBrush(color);
    p->drawRoundedRect(inner, r, r);
    p->restore();
}

// ---------------------------------------------------------------------------------------------
// SettingRow

SettingRow::SettingRow(QWidget *parent)
    : SettingRow(QString(), QString(), parent)
{
}

SettingRow::SettingRow(const QString &title, const QString &description, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 8, 14, 8);
    layout->setSpacing(16);
    m_textBox = new QWidget(this);
    m_textBox->setObjectName(u"settingRowText"_s);
    auto *text = new QVBoxLayout(m_textBox);
    text->setContentsMargins(0, 0, 0, 0);
    text->setSpacing(2);
    m_title = new Label(title, Label::Body, m_textBox);
    m_description = new Label(description, Label::Minor, m_textBox);
    m_description->setElideMode(Qt::ElideRight);
    m_description->setVisible(!description.isEmpty());
    text->addWidget(m_title);
    text->addWidget(m_description);
    m_textBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_textBox, 1);
    m_controls = new QHBoxLayout();
    m_controls->setSpacing(8);
    layout->addLayout(m_controls);
    setMinimumHeight(m_rowHeight);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setAccessibleName(QString(title).remove(u'&'));
    repaintOnThemeChange(this);
}

QString SettingRow::title() const
{
    return m_title->text();
}

void SettingRow::setTitle(const QString &title)
{
    m_title->setText(title);
    setAccessibleName(QString(title).remove(u'&'));
}

QString SettingRow::description() const
{
    return m_description->text();
}

void SettingRow::setDescription(const QString &description)
{
    m_description->setText(description);
    m_description->setVisible(!description.isEmpty());
    setAccessibleDescription(description);
}

void SettingRow::setModified(bool modified)
{
    if (m_modified == modified)
        return;
    m_modified = modified;
    // 점 6 + 간격 6
    m_title->setContentsMargins(modified ? 12 : 0, 0, 0, 0);
    m_title->setToolTip(modified ? tr("기본값과 다름") : QString());
    update();
}

void SettingRow::setRowHeight(int height)
{
    m_rowHeight = height;
    setMinimumHeight(height);
}

QLabel *SettingRow::titleLabel() const
{
    return m_title;
}

void SettingRow::addControl(QWidget *widget)
{
    if (widget->parentWidget() != this)
        widget->setParent(this);
    m_controls->addWidget(widget);
    // "목록 글꼴(&F)"처럼 니모닉이 있으면 첫 컨트롤을 버디로
    if (m_title->text().contains(u'&') && !m_title->buddy() && widget->focusPolicy() != Qt::NoFocus)
        m_title->setBuddy(widget);
    if (widget->accessibleName().isEmpty())
        widget->setAccessibleName(QString(m_title->text()).remove(u'&'));
}

void SettingRow::adoptChildren()
{
    for (QObject *child : children()) {
        auto *w = qobject_cast<QWidget *>(child);
        if (!w || w == m_textBox || w->isWindow())
            continue;
        if (layout()->indexOf(w) >= 0 || m_controls->indexOf(w) >= 0)
            continue;
        addControl(w);
    }
}

bool SettingRow::event(QEvent *event)
{
    if (event->type() == QEvent::Polish)
        adoptChildren();
    return QWidget::event(event);
}

bool SettingRow::isFirstRow() const
{
    const QWidget *parent = parentWidget();
    if (!parent || !parent->layout())
        return true;
    const QLayout *l = parent->layout();
    for (int i = 0; i < l->count(); ++i) {
        const QWidget *w = l->itemAt(i)->widget();
        if (w == this)
            return true;
        if (qobject_cast<const SettingRow *>(w) && !w->isHidden())
            return false;
    }
    return true;
}

void SettingRow::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QPainter p(this);
    if (!isFirstRow())
        p.fillRect(QRect(0, 0, width(), 1), tc[T::Grid]);
    if (m_modified) {
        const QRect title = m_title->geometry().translated(m_textBox->pos());
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(isEnabled() ? tc[T::Accent] : tc[T::Fg3]);
        p.drawEllipse(QRectF(title.left(), title.center().y() - 2.5, 6, 6));
    }
}

// ---------------------------------------------------------------------------------------------
// SearchField

SearchField::SearchField(QWidget *parent)
    : QLineEdit(parent)
{
    setClearButtonEnabled(true);
    setFont(fs::pixelFont(font(), 12.5));
    m_icon = addAction(QIcon(), QLineEdit::LeadingPosition);
    refreshIcon();
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, &SearchField::refreshIcon);
}

void SearchField::refreshIcon()
{
    m_icon->setIcon(fs::glyphIcon(fs::Glyph::Search, fs::themeColorsFor(this)[T::Fg3], 14));
}

void SearchField::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QLineEdit::keyPressEvent(event);
        event->accept();  // 대화상자 기본 단추(확인)로 새지 않게
        return;
    }
    if (event->key() == Qt::Key_Escape && !text().isEmpty()) {
        clear();
        event->accept();
        return;
    }
    QLineEdit::keyPressEvent(event);
}

void SearchField::changeEvent(QEvent *event)
{
    QLineEdit::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        refreshIcon();
}

// ---------------------------------------------------------------------------------------------
// ThemeModeCard

ThemeModeCard::ThemeModeCard(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setAutoExclusive(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    repaintOnThemeChange(this);
}

void ThemeModeCard::setMode(Mode mode)
{
    m_mode = mode;
    update();
}

void ThemeModeCard::setAccent(const QColor &accent)
{
    m_accent = accent;
    update();
}

QSize ThemeModeCard::sizeHint() const
{
    return QSize(136, 6 + 78 + 8 + 20 + 6);
}

QSize ThemeModeCard::minimumSizeHint() const
{
    return QSize(96, sizeHint().height());
}

void ThemeModeCard::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    const bool square = tc.isWatercolor();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, !square);

    // 카드: 1 px --line, 선택 2 px --accent(04 §2.1.1)
    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(isChecked() ? tc[T::Accent] : tc[T::Line], 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(card, square ? 0 : 8, square ? 0 : 8);
    if (isChecked())
        p.drawRoundedRect(card.adjusted(1, 1, -1, -1), square ? 0 : 7, square ? 0 : 7);

    // 그림 — 내장 값으로 "라이트는 이렇게, 다크는 이렇게"(지금 디자인)
    const QRectF picture(6, 6, width() - 12, 78);
    auto half = [&](const QRectF &r, fs::Variant v, int lines) {
        const fs::Design d = tc.design();
        auto c = [&](T t) { return QColor::fromRgba(fs::builtinColor(t, v, d)); };
        p.setPen(Qt::NoPen);
        p.setBrush(c(T::Win));
        p.drawRect(r);
        const QRectF inner = r.adjusted(6, 6, -6, -6);
        p.setBrush(c(T::Line));
        p.drawRoundedRect(QRectF(inner.left(), inner.top(), inner.width(), 6), 2, 2);
        const QRectF panel(inner.left(), inner.top() + 10, inner.width(), inner.height() - 10);
        p.setBrush(c(T::Surface));
        p.drawRoundedRect(panel, 3, 3);
        qreal y = panel.top() + 4;
        for (int i = 0; i < lines; ++i) {
            p.setBrush(i == 0 ? m_accent : c(T::BtnLine));
            p.drawRoundedRect(QRectF(panel.left() + 4, y, panel.width() - 8 - (i % 2) * panel.width() * 0.25, 3), 1.5, 1.5);
            y += 6;
        }
    };
    p.save();
    QPainterPath clip;
    clip.addRoundedRect(picture, square ? 0 : 5, square ? 0 : 5);
    p.setClipPath(clip);
    switch (m_mode) {
    case System:
        half(QRectF(picture.left(), picture.top(), picture.width() / 2, picture.height()), fs::Variant::Light, 3);
        half(QRectF(picture.center().x(), picture.top(), picture.width() / 2, picture.height()), fs::Variant::Dark, 3);
        break;
    case Light:
        half(picture, fs::Variant::Light, 4);
        break;
    case Dark:
        half(picture, fs::Variant::Dark, 4);
        break;
    }
    p.restore();
    p.setPen(QPen(QColor(0, 0, 0, 31), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(picture.adjusted(0.5, 0.5, -0.5, -0.5), square ? 0 : 5, square ? 0 : 5);

    // 이름 줄: 라디오 16 + 간격 8 + 12.5 px
    QStyleOptionButton radio;
    radio.initFrom(this);
    radio.rect = QRect(8, 6 + 78 + 8 + 2, 16, 16);
    radio.state |= isChecked() ? QStyle::State_On : QStyle::State_Off;
    radio.state &= ~QStyle::State_HasFocus;
    style()->drawPrimitive(QStyle::PE_IndicatorRadioButton, &radio, &p, this);
    p.setFont(fs::pixelFont(font(), 12.5));
    p.setPen(isEnabled() ? tc[T::Fg] : tc[T::Fg3]);
    p.drawText(QRect(8 + 16 + 8, 6 + 78 + 8, width() - 40, 20),
               Qt::AlignLeft | Qt::AlignVCenter | (showMnemonic(this) ? Qt::TextShowMnemonic : Qt::TextHideMnemonic), text());
    if (hasFocus()) {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);
        focus.rect = rect();
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &p, this);
    }
}

// ---------------------------------------------------------------------------------------------
// ColorSwatchButton · AccentPicker

ColorSwatchButton::ColorSwatchButton(QWidget *parent)
    : QAbstractButton(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(24, 24);
    repaintOnThemeChange(this);
}

void ColorSwatchButton::setColor(const QColor &color)
{
    m_color = color;
    update();
}

void ColorSwatchButton::setAddButton(bool add)
{
    m_add = add;
    setCheckable(!add);
    update();
}

void ColorSwatchButton::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF full(0.5, 0.5, 23, 23);
    if (m_add) {
        QPen pen(tc[T::Fg3], 1.0, Qt::DashLine);
        pen.setDashPattern({2, 2});
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(full.adjusted(1, 1, -1, -1));
        p.setPen(tc[T::Fg2]);
        p.setFont(fs::pixelFont(font(), 14));
        p.drawText(rect(), Qt::AlignCenter, u"＋"_s);
    } else {
        p.setPen(Qt::NoPen);
        if (isChecked()) {
            // 선택 고리: 2 px --fg + 2 px --surface 틈
            p.setBrush(tc[T::Fg]);
            p.drawEllipse(QRectF(0, 0, 24, 24));
            p.setBrush(tc[T::Surface]);
            p.drawEllipse(QRectF(2, 2, 20, 20));
            p.setBrush(m_color);
            p.drawEllipse(QRectF(4, 4, 16, 16));
        } else {
            p.setBrush(m_color);
            p.drawEllipse(QRectF(2, 2, 20, 20));
        }
    }
    if (hasFocus()) {
        QPen pen(tc[T::Focus], 1.0, Qt::DotLine);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(full);
    }
}

AccentPicker::AccentPicker(QWidget *parent)
    : QWidget(parent)
    , m_group(new QButtonGroup(this))
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(6);
    m_group->setExclusive(true);
    m_custom = new ColorSwatchButton(this);
    m_custom->setCheckable(true);
    m_custom->setVisible(false);
    m_add = new ColorSwatchButton(this);
    m_add->setAddButton(true);
    m_add->setToolTip(tr("직접 선택…"));
    m_add->setVisible(false);
    connect(m_add, &QAbstractButton::clicked, this, &AccentPicker::customRequested);
    connect(m_custom, &QAbstractButton::clicked, this, [this] {
        m_current = m_custom->color();
        Q_EMIT accentChosen(m_current);
    });
    setAccessibleName(tr("강조색"));
    setSwatches(defaultSwatches());
}

QList<AccentPicker::Swatch> AccentPicker::defaultSwatches()
{
    return {{tr("파랑"), QColor(0x1F, 0x5F, 0xD1)}, {tr("청록"), QColor(0x0F, 0x7A, 0x6E)}, {tr("보라"), QColor(0x6D, 0x3F, 0xC0)},
            {tr("자주"), QColor(0xA3, 0x24, 0x6B)}, {tr("호박"), QColor(0x9A, 0x5B, 0x00)}, {tr("회청"), QColor(0x4A, 0x55, 0x68)}};
}

void AccentPicker::setSwatches(const QList<Swatch> &swatches)
{
    m_swatches = swatches;
    rebuild();
}

void AccentPicker::rebuild()
{
    qDeleteAll(m_buttons);
    m_buttons.clear();
    while (m_layout->count())
        m_layout->takeAt(0);
    for (int i = 0; i < m_swatches.size(); ++i) {
        auto *b = new ColorSwatchButton(this);
        b->setCheckable(true);
        b->setColor(m_swatches.at(i).color);
        b->setToolTip(m_swatches.at(i).name);
        b->setAccessibleName(m_swatches.at(i).name);
        m_group->addButton(b, i);
        m_layout->addWidget(b);
        connect(b, &QAbstractButton::clicked, this, [this, i] {
            m_current = i == 0 ? std::nullopt : std::optional<QColor>(m_swatches.at(i).color);
            m_custom->setVisible(false);
            Q_EMIT accentChosen(m_current);
        });
        m_buttons.append(b);
    }
    m_group->addButton(m_custom, int(m_swatches.size()));
    m_layout->addWidget(m_custom);
    m_layout->addWidget(m_add);
    m_layout->addStretch(1);
    setCurrent(m_current);
}

void AccentPicker::setAddButtonVisible(bool visible)
{
    m_add->setVisible(visible);
}

void AccentPicker::setCurrent(const std::optional<QColor> &accent)
{
    m_current = accent;
    if (!accent || !accent->isValid()) {
        m_custom->setVisible(false);
        if (!m_buttons.isEmpty())
            m_buttons.first()->setChecked(true);
        return;
    }
    for (int i = 1; i < m_swatches.size(); ++i) {
        if (m_swatches.at(i).color.rgb() == accent->rgb()) {
            m_custom->setVisible(false);
            m_buttons.at(i)->setChecked(true);
            return;
        }
    }
    m_custom->setColor(*accent);
    m_custom->setToolTip(tr("사용자 지정 %1").arg(fs::colorHex(*accent)));
    m_custom->setVisible(true);
    m_custom->setChecked(true);
}

QString AccentPicker::currentName() const
{
    if (!m_current || !m_current->isValid())
        return m_swatches.isEmpty() ? QString() : m_swatches.first().name;
    for (const Swatch &s : m_swatches) {
        if (s.color.rgb() == m_current->rgb())
            return s.name;
    }
    return tr("사용자 지정 %1").arg(fs::colorHex(*m_current));
}

// ---------------------------------------------------------------------------------------------
// HexColorEdit

HexColorEdit::HexColorEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setFont(fs::monoFont(12));
    setFixedWidth(96);
    setValidator(new QRegularExpressionValidator(QRegularExpression(u"#?[0-9A-Fa-f]{0,8}"_s), this));
    setAccessibleName(tr("16진수 색"));
}

void HexColorEdit::setColor(const QColor &color)
{
    m_color = color;
    setText(color.isValid() ? fs::colorHex(color) : QString());
}

void HexColorEdit::commit()
{
    const QColor c = fs::parseColorHex(text());
    if (!c.isValid()) {
        setText(m_color.isValid() ? fs::colorHex(m_color) : QString());
        return;
    }
    setText(fs::colorHex(c));
    if (c != m_color) {
        m_color = c;
        Q_EMIT colorEdited(c);
    }
}

void HexColorEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        commit();
        event->accept();  // Enter를 먹는다(04 §5-2)
        return;
    }
    QLineEdit::keyPressEvent(event);
}

void HexColorEdit::focusOutEvent(QFocusEvent *event)
{
    commit();
    QLineEdit::focusOutEvent(event);
}

// ---------------------------------------------------------------------------------------------
// ToggleChip

ToggleChip::ToggleChip(QWidget *parent)
    : ToggleChip(QString(), parent)
{
}

ToggleChip::ToggleChip(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
{
    setCheckable(true);
    setAutoDefault(false);
    setAttribute(Qt::WA_Hover);
    repaintOnThemeChange(this);
}

QSize ToggleChip::sizeHint() const
{
    QFont f = fs::pixelFont(font(), 12.5);
    const int w = QFontMetrics(f).horizontalAdvance(QString(text()).remove(u'&'));
    return QSize(std::max(32, w + 20), 28);
}

void ToggleChip::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    const qreal r = tc.isWatercolor() ? 0 : 4;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, r > 0);
    const bool on = isChecked();
    QColor fill = on ? tc[T::AccentSoft] : tc[T::Btn];
    if (isEnabled() && underMouse())
        fill = mix(fill, tc[T::Fg], isDown() ? 0.09 : 0.04);
    p.setPen(QPen(on ? tc[T::Accent] : tc[T::BtnLine], 1.0));
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    QFont f = font();
    f = fs::pixelFont(f, 12.5, f.weight() >= QFont::DemiBold ? QFont::Bold : QFont::Normal);
    f.setItalic(font().italic());
    f.setUnderline(font().underline());
    f.setStrikeOut(font().strikeOut());
    p.setFont(f);
    p.setPen(!isEnabled() ? tc[T::Fg3] : on ? tc[T::AccentFg] : tc[T::Fg]);
    p.drawText(rect(), Qt::AlignCenter | (showMnemonic(this) && !font().underline() ? Qt::TextShowMnemonic : Qt::TextHideMnemonic),
               text());
    if (hasFocus()) {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);
        focus.rect = rect();
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &p, this);
    }
}

// ---------------------------------------------------------------------------------------------
// ColorPickButton

ColorPickButton::ColorPickButton(QWidget *parent)
    : QPushButton(parent)
{
    setAutoDefault(false);
    setFocusPolicy(Qt::StrongFocus);
    m_suggestions = {QColor(0x1D, 0x5B, 0xC7), QColor(0x0F, 0x7A, 0x6E), QColor(0x6D, 0x3F, 0xC0), QColor(0xA3, 0x24, 0x6B),
                     QColor(0x9A, 0x5B, 0x00), QColor(0xA8, 0x32, 0x1F), QColor(0x2E, 0x85, 0x40), QColor(0x6B, 0x71, 0x7C),
                     QColor(0x82, 0xAE, 0xF6), QColor(0x4F, 0xD1, 0xBF), QColor(0xB3, 0x94, 0xF0), QColor(0xF0, 0x7D, 0xB8),
                     QColor(0xF0, 0xB2, 0x4D), QColor(0xF0, 0x8A, 0x7A), QColor(0xFF, 0xF1, 0xC9), QColor(0x3A, 0x2F, 0x10)};
    connect(this, &QPushButton::clicked, this, &ColorPickButton::showPopup);
    repaintOnThemeChange(this);
}

void ColorPickButton::setColor(const std::optional<QColor> &color)
{
    m_color = color;
    setAccessibleDescription(color ? fs::colorHex(*color) : tr("지정 안 함"));
    update();
}

QSize ColorPickButton::sizeHint() const
{
    return QSize(148, 30);
}

QSize ColorPickButton::minimumSizeHint() const
{
    return QSize(90, 30);
}

void ColorPickButton::choose(const std::optional<QColor> &color)
{
    if (color == m_color)
        return;
    setColor(color);
    Q_EMIT colorChanged(color);
}

void ColorPickButton::showPopup()
{
    QMenu menu(this);
    auto *grid = new QWidget(&menu);
    auto *layout = new QGridLayout(grid);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);
    for (int i = 0; i < m_suggestions.size(); ++i) {
        auto *b = new QToolButton(grid);
        b->setAutoRaise(true);
        b->setFixedSize(22, 22);
        const QColor c = m_suggestions.at(i);
        QPixmap pm(18, 18);
        pm.fill(c);
        b->setIcon(QIcon(pm));
        b->setIconSize(QSize(16, 16));
        b->setToolTip(fs::colorHex(c));
        connect(b, &QToolButton::clicked, &menu, [this, c, &menu] {
            menu.close();
            choose(c);
        });
        layout->addWidget(b, i / 8, i % 8);
    }
    auto *action = new QWidgetAction(&menu);
    action->setDefaultWidget(grid);
    menu.addAction(action);
    menu.addSeparator();
    if (m_allowNone)
        connect(menu.addAction(tr("지정 안 함")), &QAction::triggered, this, [this] { choose(std::nullopt); });
    connect(menu.addAction(tr("사용자 지정…")), &QAction::triggered, this, [this] {
        const QColor c = QColorDialog::getColor(m_color.value_or(QColor(Qt::black)), this, tr("색 선택"),
                                                QColorDialog::DontUseNativeDialog);
        if (c.isValid())
            choose(c);
    });
    menu.exec(mapToGlobal(QPoint(0, height())));
}

void ColorPickButton::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QPainter p(this);
    // 입력 칸과 같은 틀(두 디자인 — 스타일이 그린다)
    QStyleOptionFrame frame;
    frame.initFrom(this);
    frame.lineWidth = 1;
    frame.state |= QStyle::State_Sunken;
    style()->drawPrimitive(QStyle::PE_PanelLineEdit, &frame, &p, this);

    const QRectF swatch(4, (height() - 22) / 2.0, 22, 22);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_color) {
        paintSwatch(&p, swatch, *m_color, tc, 4);
        p.setFont(fs::monoFont(12.5));
        p.setPen(isEnabled() ? tc[T::Fg] : tc[T::Fg3]);
        p.drawText(QRectF(swatch.right() + 8, 0, width() - swatch.right() - 30, height()), Qt::AlignLeft | Qt::AlignVCenter,
                   fs::colorHex(*m_color));
    } else {
        QPen pen(tc[T::Fg3], 1.0, Qt::DashLine);
        pen.setDashPattern({2, 2});
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(swatch.adjusted(0.5, 0.5, -0.5, -0.5), tc.isWatercolor() ? 0 : 4, tc.isWatercolor() ? 0 : 4);
        p.setPen(QPen(tc[T::Fg3], 1.0));
        p.drawLine(swatch.bottomLeft() + QPointF(3, -3), swatch.topRight() + QPointF(-3, 3));
        p.setFont(fs::pixelFont(font(), 12.5));
        p.drawText(QRectF(swatch.right() + 8, 0, width() - swatch.right() - 30, height()), Qt::AlignLeft | Qt::AlignVCenter,
                   tr("지정 안 함"));
    }
    fs::paintGlyph(&p, fs::Glyph::ChevronDown, QRectF(width() - 18, (height() - 10) / 2.0, 10, 10), tc[T::Fg3]);
    if (hasFocus()) {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);
        focus.rect = rect();
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &p, this);
    }
}

// ---------------------------------------------------------------------------------------------
// CheckListCombo

CheckListCombo::CheckListCombo(QWidget *parent)
    : QComboBox(parent)
{
    setModel(new QStandardItemModel(this));
    view()->viewport()->installEventFilter(this);
}

void CheckListCombo::setCheckItems(const QStringList &items)
{
    auto *m = static_cast<QStandardItemModel *>(model());
    m->clear();
    for (const QString &text : items) {
        auto *item = new QStandardItem(text);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setData(Qt::Unchecked, Qt::CheckStateRole);
        m->appendRow(item);
    }
    update();
}

int CheckListCombo::checkedMask() const
{
    const auto *m = static_cast<const QStandardItemModel *>(model());
    int mask = 0;
    for (int i = 0; i < m->rowCount(); ++i) {
        if (m->item(i)->checkState() == Qt::Checked)
            mask |= 1 << i;
    }
    return mask;
}

void CheckListCombo::setCheckedMask(int mask)
{
    auto *m = static_cast<QStandardItemModel *>(model());
    for (int i = 0; i < m->rowCount(); ++i)
        m->item(i)->setCheckState((mask & (1 << i)) ? Qt::Checked : Qt::Unchecked);
    update();
}

QString CheckListCombo::summaryText() const
{
    const auto *m = static_cast<const QStandardItemModel *>(model());
    QStringList parts;
    for (int i = 0; i < m->rowCount(); ++i) {
        if (m->item(i)->checkState() == Qt::Checked)
            parts.append(m->item(i)->text());
    }
    return parts.isEmpty() ? tr("없음") : parts.join(u" · "_s);
}

void CheckListCombo::hidePopup()
{
    if (m_keepOpen) {
        m_keepOpen = false;
        return;
    }
    QComboBox::hidePopup();
}

bool CheckListCombo::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == view()->viewport() && event->type() == QEvent::MouseButtonRelease) {
        const QModelIndex index = view()->indexAt(static_cast<QMouseEvent *>(event)->position().toPoint());
        if (index.isValid()) {
            auto *item = static_cast<QStandardItemModel *>(model())->itemFromIndex(index);
            item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
            m_keepOpen = true;  // 여러 개를 고를 수 있게 팝업을 열어 둔다
            update();
            Q_EMIT checkedMaskChanged(checkedMask());
            return true;
        }
    }
    return QComboBox::eventFilter(watched, event);
}

void CheckListCombo::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);
    QStyleOptionComboBox option;
    initStyleOption(&option);
    option.currentText = summaryText();
    option.currentIcon = QIcon();
    painter.drawComplexControl(QStyle::CC_ComboBox, option);
    painter.drawControl(QStyle::CE_ComboBoxLabel, option);
}

// ---------------------------------------------------------------------------------------------
// KeyCaptureEdit

KeyCaptureEdit::KeyCaptureEdit(QWidget *parent)
    : QWidget(parent)
    , m_caption(tr("누른 키 · Esc로 취소"))
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled, false);  // 한국어 IME가 켜져 있어도 가상 키로 받는다
    setAccessibleName(tr("단축키 입력"));
}

void KeyCaptureEdit::startCapture()
{
    m_capturing = true;
    m_pending = QKeySequence();
    m_mods = {};
    setFocus(Qt::OtherFocusReason);
    grabKeyboard();
    update();
}

void KeyCaptureEdit::stopCapture()
{
    if (m_capturing)
        releaseKeyboard();
    m_capturing = false;
    m_mods = {};
    update();
}

void KeyCaptureEdit::setPending(const QKeySequence &key)
{
    m_pending = key;
    update();
}

void KeyCaptureEdit::setCaption(const QString &caption)
{
    m_caption = caption;
    update();
}

QSize KeyCaptureEdit::sizeHint() const
{
    return QSize(200, 24);
}

bool KeyCaptureEdit::event(QEvent *event)
{
    // 입력 중에는 대화상자 기본 단추 · 니모닉 · 창 단축키가 동작하지 않는다
    if (m_capturing && event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    return QWidget::event(event);
}

bool KeyCaptureEdit::focusNextPrevChild(bool next)
{
    if (m_capturing)
        return false;  // Tab · Shift+Tab도 키로 받는다("패널 전환 = Tab")
    return QWidget::focusNextPrevChild(next);
}

void KeyCaptureEdit::keyPressEvent(QKeyEvent *event)
{
    if (!m_capturing) {
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta || key == Qt::Key_unknown) {
        m_mods = mods;  // 수식키만 누른 동안은 "Ctrl+…"
        update();
        return;
    }
    if (mods == Qt::NoModifier && key == Qt::Key_Escape) {
        stopCapture();
        Q_EMIT cancelled();
        return;
    }
    if (mods == Qt::NoModifier && (key == Qt::Key_Backspace || key == Qt::Key_Delete)) {
        stopCapture();
        Q_EMIT cleared();
        return;
    }
    const int effective = (key == Qt::Key_Backtab) ? Qt::Key_Tab : key;
    m_pending = QKeySequence(QKeyCombination(mods, Qt::Key(effective)));
    update();
    Q_EMIT captured(m_pending);
}

void KeyCaptureEdit::keyReleaseEvent(QKeyEvent *event)
{
    if (m_capturing) {
        m_mods = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
        update();
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void KeyCaptureEdit::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    update();
}

void KeyCaptureEdit::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QPainter p(this);
    QString text;
    if (!m_pending.isEmpty()) {
        text = m_pending.toString(QKeySequence::NativeText);
    } else {
        QStringList parts;
        if (m_mods & Qt::ControlModifier)
            parts.append(u"Ctrl"_s);
        if (m_mods & Qt::AltModifier)
            parts.append(u"Alt"_s);
        if (m_mods & Qt::ShiftModifier)
            parts.append(u"Shift"_s);
        text = parts.isEmpty() ? u" "_s : parts.join(u'+') + u"+…"_s;
    }
    const QSize chip = keyCapSize(text);
    const QRectF chipRect(0, (height() - chip.height()) / 2.0, std::max(chip.width(), 28), chip.height());
    paintKeyCap(&p, chipRect, text, tc, false, true);
    p.setFont(fs::pixelFont(font(), 12));
    p.setPen(tc[T::Fg2]);
    p.drawText(QRectF(chipRect.right() + 8, 0, width() - chipRect.right() - 8, height()), Qt::AlignLeft | Qt::AlignVCenter, m_caption);
}

} // namespace fm::ui
