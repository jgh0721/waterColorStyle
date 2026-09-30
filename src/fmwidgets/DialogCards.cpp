#include "fmwidgets/DialogCards.h"

#include "fmwidgets/Label.h"

#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeManager.h>

#include <QEvent>
#include <QHelpEvent>
#include <QPainter>
#include <QRadioButton>
#include <QStyleOptionFocusRect>
#include <QToolTip>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace fm::ui {

namespace fs = fm::style;
using T = fs::Token;

namespace {

// 03 §1.5 키-값 카드
constexpr int kKvPadX = 14;
constexpr int kKvPadY = 12;
constexpr int kKvRow = 20;
constexpr int kKvRowGap = 6;
constexpr int kKvColGap = 12;
constexpr int kKvValueGap = 6;
constexpr int kKvIcon = 16;
// 03 §1.6 항목 목록 카드
constexpr int kListPadY = 4;
constexpr int kListPadX = 12;
constexpr int kListRow = 28;
constexpr int kListGap = 8;
constexpr int kListIcon = 14;
// 03 §1.11 대안 동작 카드
constexpr int kAltPadX = 14;
constexpr int kAltPadY = 10;
constexpr int kAltGap = 12;
constexpr int kAltIcon = 20;
constexpr int kAltChevron = 12;
constexpr int kAltLine1 = 18;
constexpr int kAltLine2 = 17;

fs::Tone toStyleTone(Tag::Tone tone)
{
    switch (tone) {
    case Tag::Ok:     return fs::Tone::Ok;
    case Tag::Info:   return fs::Tone::Info;
    case Tag::Mute:   return fs::Tone::Mute;
    case Tag::Bad:    return fs::Tone::Bad;
    case Tag::Warn:   return fs::Tone::Warn;
    case Tag::Danger: return fs::Tone::Danger;
    }
    return fs::Tone::Mute;
}

QColor mix(const QColor &base, const QColor &over, qreal amount)
{
    return QColor::fromRgbF(base.redF() + (over.redF() - base.redF()) * amount,
                            base.greenF() + (over.greenF() - base.greenF()) * amount,
                            base.blueF() + (over.blueF() - base.blueF()) * amount);
}

void paintIcon(QPainter *p, const IconSpec &icon, const QRectF &rect, const fs::ThemeColors &tc)
{
    if (icon.isNull())
        return;
    const fs::Glyph g = glyph::toStyle(icon.glyph);
    if (g == fs::Glyph::Shield)
        fs::paintGlyph(p, g, rect, tc[T::Shield], tc[T::Shield2]);
    else
        fs::paintGlyph(p, g, rect, tc[icon.primary], tc[icon.secondary]);
}

QFont valueFont(const QFont &base, KeyValueCard::ValueStyle style)
{
    switch (style) {
    case KeyValueCard::Mono:   return fs::monoFont(12.5);
    case KeyValueCard::Strong: return fs::pixelFont(base, 13, QFont::DemiBold);
    case KeyValueCard::Normal: break;
    }
    return fs::pixelFont(base, 13);
}

void repaintOnThemeChange(QWidget *w)
{
    QObject::connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, w, qOverload<>(&QWidget::update));
}

} // namespace

// ---------------------------------------------------------------------------------------------
// KeyValueCard

KeyValueCard::KeyValueCard(QWidget *parent)
    : Card(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    repaintOnThemeChange(this);
}

void KeyValueCard::addRow(const Row &row)
{
    m_rows.append(row);
    refreshAccessibility();
    updateGeometry();
    update();
}

void KeyValueCard::addRow(const QString &label, const QString &value, ValueStyle style)
{
    Row row;
    row.label = label;
    row.value = value;
    row.style = style;
    addRow(row);
}

void KeyValueCard::setRows(const QList<Row> &rows)
{
    m_rows = rows;
    refreshAccessibility();
    updateGeometry();
    update();
}

void KeyValueCard::clear()
{
    setRows({});
}

void KeyValueCard::setLabelWidth(int width)
{
    m_labelWidth = std::max(0, width);
    updateGeometry();
    update();
}

int KeyValueCard::effectiveLabelWidth() const
{
    // 다른 언어에서 라벨이 길면 가장 긴 라벨에 맞춰 늘린다(03 §1.5)
    const QFontMetrics fm(fs::pixelFont(font(), 13));
    int widest = m_labelWidth;
    for (const Row &row : m_rows)
        widest = std::max(widest, fm.horizontalAdvance(row.label));
    return widest;
}

QSize KeyValueCard::sizeHint() const
{
    const int n = int(m_rows.size());
    const int h = 2 + 2 * kKvPadY + (n > 0 ? n * kKvRow + (n - 1) * kKvRowGap : 0);
    return QSize(510, h);
}

QSize KeyValueCard::minimumSizeHint() const
{
    return QSize(2 + 2 * kKvPadX + effectiveLabelWidth() + kKvColGap + 80, sizeHint().height());
}

QList<KeyValueCard::Placed> KeyValueCard::place() const
{
    QList<Placed> out;
    const int valueLeft = 1 + kKvPadX + effectiveLabelWidth() + kKvColGap;
    const int right = width() - 1 - kKvPadX;
    const QFont trailingFont = fs::withTabularNumbers(fs::pixelFont(font(), 13));
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &row = m_rows.at(i);
        const qreal top = 1 + kKvPadY + i * (kKvRow + kKvRowGap);
        Placed placed;
        qreal x = valueLeft;
        if (!row.icon.isNull())
            x += kKvIcon + kKvValueGap;
        const int trailing = row.trailing.isEmpty() ? 0 : QFontMetrics(trailingFont).horizontalAdvance(row.trailing) + kKvValueGap;
        const QFontMetrics fm(valueFont(font(), row.style));
        const int available = std::max(0, int(right - x) - trailing);
        placed.shown = fm.elidedText(row.value, row.elide, available);
        placed.name = QRectF(x, top, std::min(available, fm.horizontalAdvance(placed.shown) + 1), kKvRow);
        out.append(placed);
    }
    return out;
}

void KeyValueCard::paintEvent(QPaintEvent *event)
{
    Card::paintEvent(event);  // 카드 바탕 · 테두리(스타일)
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QPainter p(this);
    const QFont labelFont = fs::pixelFont(font(), 13);
    const QFont trailingFont = fs::withTabularNumbers(labelFont);
    const int labelLeft = 1 + kKvPadX;
    const int labelWidth = effectiveLabelWidth();
    const int valueLeft = labelLeft + labelWidth + kKvColGap;
    const QList<Placed> placed = place();
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &row = m_rows.at(i);
        const qreal top = 1 + kKvPadY + i * (kKvRow + kKvRowGap);
        p.setFont(labelFont);
        p.setPen(tc[T::Fg3]);
        p.drawText(QRectF(labelLeft, top, labelWidth, kKvRow), Qt::AlignLeft | Qt::AlignVCenter, row.label);

        if (!row.tagText.isEmpty()) {
            const fs::Tone tone = toStyleTone(row.tagTone);
            const QSize size = fs::tagSize(row.tagText, tone, true, false);
            fs::paintTag(&p, QRectF(valueLeft, top + (kKvRow - size.height()) / 2.0, size.width(), size.height()),
                         row.tagText, tone, tc, true);
            continue;
        }
        if (!row.icon.isNull())
            paintIcon(&p, row.icon, QRectF(valueLeft, top + (kKvRow - kKvIcon) / 2.0, kKvIcon, kKvIcon), tc);
        const Placed &at = placed.at(i);
        p.setFont(valueFont(font(), row.style));
        p.setPen(isEnabled() ? tc[T::Fg] : tc[T::Fg3]);
        p.drawText(QRectF(at.name.left(), top, at.name.width() + 1, kKvRow), Qt::AlignLeft | Qt::AlignVCenter, at.shown);
        if (!row.trailing.isEmpty()) {
            p.setFont(trailingFont);
            p.setPen(tc[T::Fg3]);
            p.drawText(QRectF(at.name.right() + kKvValueGap, top, width(), kKvRow), Qt::AlignLeft | Qt::AlignVCenter,
                       row.trailing);
        }
    }
}

bool KeyValueCard::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const QList<Placed> placed = place();
        for (int i = 0; i < placed.size(); ++i) {
            if (placed.at(i).name.contains(help->pos()) && placed.at(i).shown != m_rows.at(i).value) {
                QToolTip::showText(help->globalPos(), m_rows.at(i).value, this);
                return true;
            }
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    return Card::event(event);
}

void KeyValueCard::refreshAccessibility()
{
    QStringList lines;
    for (const Row &row : std::as_const(m_rows)) {
        QString value = row.tagText.isEmpty() ? row.value : row.tagText;
        if (!row.trailing.isEmpty())
            value += u' ' + row.trailing;
        lines.append(row.label + u": "_s + value);
    }
    setAccessibleDescription(lines.join(u'\n'));
}

// ---------------------------------------------------------------------------------------------
// ItemListCard

ItemListCard::ItemListCard(QWidget *parent)
    : Card(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    repaintOnThemeChange(this);
}

void ItemListCard::addItem(const Item &item)
{
    m_items.append(item);
    refreshAccessibility();
    updateGeometry();
    update();
}

void ItemListCard::setItems(const QList<Item> &items)
{
    m_items = items;
    refreshAccessibility();
    updateGeometry();
    update();
}

void ItemListCard::clear()
{
    setItems({});
}

void ItemListCard::setMaxVisibleRows(int rows)
{
    m_maxRows = std::max(1, rows);
    updateGeometry();
    update();
}

int ItemListCard::visibleRows() const
{
    return std::min(int(m_items.size()), m_maxRows);
}

QSize ItemListCard::sizeHint() const
{
    return QSize(510, 2 + 2 * kListPadY + visibleRows() * kListRow);
}

QSize ItemListCard::minimumSizeHint() const
{
    return QSize(160, sizeHint().height());
}

void ItemListCard::paintEvent(QPaintEvent *event)
{
    Card::paintEvent(event);
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    QPainter p(this);
    const bool overflow = m_items.size() > m_maxRows;
    const int rows = visibleRows();
    const QFont mono = fs::monoFont(12.5);
    const int left = 1 + kListPadX;
    const int right = width() - 1 - kListPadX;
    for (int i = 0; i < rows; ++i) {
        const qreal top = 1 + kListPadY + i * kListRow;
        if (overflow && i == rows - 1) {
            p.setFont(fs::pixelFont(font(), 13));
            p.setPen(tc[T::Fg3]);
            p.drawText(QRectF(left, top, right - left, kListRow), Qt::AlignLeft | Qt::AlignVCenter,
                       tr("… 외 %1개").arg(m_items.size() - (rows - 1)));
            break;
        }
        const Item &item = m_items.at(i);
        fs::paintGlyph(&p, glyph::toStyle(item.glyph), QRectF(left, top + (kListRow - kListIcon) / 2.0, kListIcon, kListIcon),
                       tc[T::Fg3]);
        qreal textRight = right;
        if (!item.tagText.isEmpty()) {
            const fs::Tone tone = toStyleTone(item.tagTone);
            const QSize size = fs::tagSize(item.tagText, tone, true, false);
            fs::paintTag(&p, QRectF(right - size.width(), top + (kListRow - size.height()) / 2.0, size.width(), size.height()),
                         item.tagText, tone, tc, true);
            textRight -= size.width() + kListGap;
        }
        const qreal textLeft = left + kListIcon + kListGap;
        p.setFont(mono);
        p.setPen(tc[T::Fg]);
        const int available = std::max(0, int(textRight - textLeft));
        p.drawText(QRectF(textLeft, top, available, kListRow), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(mono).elidedText(item.text, Qt::ElideMiddle, available));
    }
}

void ItemListCard::refreshAccessibility()
{
    QStringList lines;
    for (const Item &item : std::as_const(m_items))
        lines.append(item.tagText.isEmpty() ? item.text : item.text + u" (" + item.tagText + u')');
    setAccessibleDescription(lines.join(u'\n'));
}

// ---------------------------------------------------------------------------------------------
// ActionCard

ActionCard::ActionCard(QWidget *parent)
    : QAbstractButton(parent)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    repaintOnThemeChange(this);
}

void ActionCard::setDetail(const QString &detail)
{
    m_detail = detail;
    setAccessibleDescription(detail);
    update();
}

void ActionCard::setGlyph(glyph::Glyph glyph)
{
    IconSpec icon = m_icon;
    icon.glyph = glyph;
    if (glyph == glyph::Folder)
        icon = IconSpec::folder();
    setIcon(icon);
}

void ActionCard::setIcon(const IconSpec &icon)
{
    m_icon = icon;
    update();
}

QSize ActionCard::sizeHint() const
{
    return QSize(420, 2 + 2 * kAltPadY + kAltLine1 + kAltLine2);
}

QSize ActionCard::minimumSizeHint() const
{
    return QSize(200, sizeHint().height());
}

void ActionCard::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    update();
}

void ActionCard::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    update();
}

void ActionCard::paintEvent(QPaintEvent *)
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    const bool square = fs::squareCorners(tc);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, !square);

    // 목업은 상태를 정하지 않았다 — 단추 규칙을 빌려 올림 4 % · 누름 9 %(03 §1.11 R8)
    QColor fill = tc[T::Surface];
    if (isEnabled() && isDown())
        fill = mix(fill, tc[T::Fg], 0.09);
    else if (isEnabled() && underMouse())
        fill = mix(fill, tc[T::Fg], 0.04);
    const qreal radius = square ? 0 : 6;
    p.setPen(QPen(tc[T::Line], 1.0));
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    const qreal left = 1 + kAltPadX;
    paintIcon(&p, m_icon, QRectF(left, (height() - kAltIcon) / 2.0, kAltIcon, kAltIcon), tc);
    const qreal chevronLeft = width() - 1 - kAltPadX - kAltChevron;
    fs::paintGlyph(&p, fs::Glyph::ChevronRight, QRectF(chevronLeft, (height() - kAltChevron) / 2.0, kAltChevron, kAltChevron),
                   tc[T::Fg3]);

    const qreal textLeft = left + kAltIcon + kAltGap;
    const int textWidth = std::max(0, int(chevronLeft - kAltGap - textLeft));
    const qreal top = (height() - kAltLine1 - kAltLine2) / 2.0;
    QStyleOption option;
    option.initFrom(this);
    const bool mnemonic = style()->styleHint(QStyle::SH_UnderlineShortcut, &option, this);
    p.setFont(fs::pixelFont(font(), 13, QFont::DemiBold));
    p.setPen(isEnabled() ? tc[T::Fg] : tc[T::Fg3]);
    p.drawText(QRectF(textLeft, top, textWidth, kAltLine1),
               Qt::AlignLeft | Qt::AlignVCenter | (mnemonic ? Qt::TextShowMnemonic : Qt::TextHideMnemonic), text());
    const QFont mono = fs::monoFont(12);
    p.setFont(mono);
    p.setPen(tc[T::Fg3]);
    p.drawText(QRectF(textLeft, top + kAltLine1, textWidth, kAltLine2), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(mono).elidedText(m_detail, Qt::ElideMiddle, textWidth));

    if (hasFocus()) {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);
        focus.rect = rect();
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &p, this);
    }
}

// ---------------------------------------------------------------------------------------------
// OptionRadio

OptionRadio::OptionRadio(QWidget *parent)
    : OptionRadio(QString(), QString(), parent)
{
}

OptionRadio::OptionRadio(const QString &text, const QString &description, QWidget *parent)
    : QWidget(parent)
    , m_radio(new QRadioButton(text, this))
    , m_description(new Label(description, Label::Help, this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_radio);
    // 설명은 표시기 16 + 간격 8만큼 들여 첫 줄 글자와 맞춘다(03 §1.8)
    m_description->setContentsMargins(24, 0, 0, 0);
    m_description->setVisible(!description.isEmpty());
    m_description->installEventFilter(this);
    layout->addWidget(m_description);
    setFocusProxy(m_radio);
    connect(m_radio, &QRadioButton::toggled, this, &OptionRadio::toggled);
}

QString OptionRadio::text() const
{
    return m_radio->text();
}

void OptionRadio::setText(const QString &text)
{
    m_radio->setText(text);
}

QString OptionRadio::description() const
{
    return m_description->text();
}

void OptionRadio::setDescription(const QString &description)
{
    m_description->setText(description);
    m_description->setVisible(!description.isEmpty());
    m_radio->setAccessibleDescription(description);
}

bool OptionRadio::isChecked() const
{
    return m_radio->isChecked();
}

void OptionRadio::setChecked(bool checked)
{
    m_radio->setChecked(checked);
}

bool OptionRadio::eventFilter(QObject *watched, QEvent *event)
{
    // 설명을 눌러도 선택된다
    if (watched == m_description && event->type() == QEvent::MouseButtonRelease && m_radio->isEnabled()) {
        m_radio->setFocus(Qt::MouseFocusReason);
        m_radio->click();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace fm::ui
