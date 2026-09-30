#include "fmwidgets/DialogWidgets.h"

#include "fmwidgets/Button.h"

#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>

#include <QEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMetaMethod>
#include <QPainter>
#include <QShortcut>
#include <QStyleOptionButton>
#include <QStylePainter>

#include <cmath>

using namespace Qt::StringLiterals;

namespace fm::ui {

using fm::style::Token;

namespace {

Token kindToken(ChoiceCard::FileKind kind)
{
    switch (kind) {
    case ChoiceCard::DocKind:     return Token::KDoc;
    case ChoiceCard::CodeKind:    return Token::KCode;
    case ChoiceCard::ImageKind:   return Token::KImg;
    case ChoiceCard::ExeKind:     return Token::KExe;
    case ChoiceCard::PdfKind:     return Token::KPdf;
    case ChoiceCard::ArchiveKind: return Token::KZip;
    case ChoiceCard::SystemKind:  return Token::KSys;
    }
    return Token::KDoc;
}

QColor mix(const QColor &base, const QColor &over, qreal amount)
{
    return QColor::fromRgbF(base.redF() + (over.redF() - base.redF()) * amount,
                            base.greenF() + (over.greenF() - base.greenF()) * amount,
                            base.blueF() + (over.blueF() - base.blueF()) * amount);
}

void paintFileIcon(QPainter *p, const QRectF &r, ChoiceCard::FileKind kind, bool isDir, const fm::style::ThemeColors &tc)
{
    if (isDir)
        fm::style::paintGlyph(p, fm::style::Glyph::Folder, r, tc[Token::Folder]);
    else
        fm::style::paintGlyph(p, fm::style::Glyph::File, r, tc[Token::Fg3], tc[kindToken(kind)]);
}

} // namespace

// ---------------------------------------------------------------- PathEdit

PathEdit::PathEdit(QWidget *parent)
    : QWidget(parent)
    , m_edit(new QLineEdit(this))
    , m_recent(new Button(this))
    , m_browse(new Button(this))
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(m_edit, 1);
    layout->addWidget(m_recent);
    layout->addWidget(m_browse);
    m_edit->setFont(fm::style::monoFont(13));
    m_edit->installEventFilter(this);
    m_recent->setGlyph(glyph::Clock);
    m_recent->setToolTip(tr("최근 대상 (Alt+↓)"));
    m_recent->setAccessibleName(tr("최근 대상"));
    m_recent->setAutoDefault(false);
    m_browse->setGlyph(glyph::More);
    m_browse->setToolTip(tr("찾아보기"));
    m_browse->setAccessibleName(tr("폴더 찾아보기"));
    m_browse->setAutoDefault(false);
    setFocusProxy(m_edit);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_edit, &QLineEdit::textChanged, this, &PathEdit::pathChanged);
    connect(m_recent, &QPushButton::clicked, this, &PathEdit::showHistory);
    connect(m_browse, &QPushButton::clicked, this, &PathEdit::browse);
}

QString PathEdit::path() const
{
    return m_edit->text();
}

void PathEdit::setPath(const QString &path)
{
    m_edit->setText(path);
}

QString PathEdit::placeholderText() const
{
    return m_edit->placeholderText();
}

void PathEdit::setPlaceholderText(const QString &text)
{
    m_edit->setPlaceholderText(text);
}

void PathEdit::setHistory(const QStringList &history)
{
    m_history = history;
}

bool PathEdit::isHistoryVisible() const
{
    return m_recent->isVisibleTo(this);
}

void PathEdit::setHistoryVisible(bool visible)
{
    m_recent->setVisible(visible);
}

bool PathEdit::isBrowseVisible() const
{
    return m_browse->isVisibleTo(this);
}

void PathEdit::setBrowseVisible(bool visible)
{
    m_browse->setVisible(visible);
}

void PathEdit::setMonospace(bool on)
{
    m_monospace = on;
    m_edit->setFont(on ? fm::style::monoFont(13) : fm::style::pixelFont(font(), 13));
}

void PathEdit::showHistory()
{
    QMenu menu(this);
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    const QIcon folder = fm::style::glyphIcon(fm::style::Glyph::Folder, tc[Token::Folder], 16);
    if (m_history.isEmpty())
        menu.addAction(tr("최근 대상 없음"))->setEnabled(false);
    for (const QString &path : std::as_const(m_history)) {
        QAction *action = menu.addAction(folder, path);
        action->setFont(fm::style::monoFont(12));
        connect(action, &QAction::triggered, this, [this, path] {
            setPath(path);
            m_edit->setFocus();
            m_edit->selectAll();
        });
    }
    menu.setMinimumWidth(m_edit->width());
    menu.exec(m_edit->mapToGlobal(QPoint(0, m_edit->height())));
}

void PathEdit::browse()
{
    if (isSignalConnected(QMetaMethod::fromSignal(&PathEdit::browseRequested))) {
        Q_EMIT browseRequested();
        return;
    }
    const QString dir = QFileDialog::getExistingDirectory(this, tr("폴더 찾아보기"), path());
    if (!dir.isEmpty())
        setPath(QDir::toNativeSeparators(dir) + (dir.endsWith(u'/') ? QString() : u"\\"_s));
}

bool PathEdit::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Down && (key->modifiers() & Qt::AltModifier)) {
            showHistory();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ---------------------------------------------------------------- ChipButton

ChipButton::ChipButton(QWidget *parent)
    : ChipButton(QString(), parent)
{
}

ChipButton::ChipButton(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
{
    setAutoDefault(false);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    fm::style::setSmall(this);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, [this] {
        updateGeometry();
        update();
    });
}

void ChipButton::setKeyHint(const QString &keys)
{
    m_keyHint = keys;
    updateGeometry();
    update();
}

void ChipButton::setGlyph(glyph::Glyph glyph)
{
    m_glyph = glyph;
    update();
}

void ChipButton::setMonospace(bool on)
{
    m_monospace = on;
    updateGeometry();
    update();
}

QFont ChipButton::labelFont() const
{
    return m_monospace ? fm::style::monoFont(12) : fm::style::pixelFont(font(), 12);
}

QSize ChipButton::sizeHint() const
{
    // 좌 5 · [키 칩] · 6 · [아이콘 14] · 6 · [글자] · 우 8
    const bool wc = fm::style::themeColorsFor(this).isWatercolor();
    // 정수로 버린 폭이면 그릴 때 말줄임이 생긴다 — 소수 폭을 올린다
    int width = 5 + 8 + int(std::ceil(QFontMetricsF(labelFont()).horizontalAdvance(text()))) + 1;
    if (!m_keyHint.isEmpty())
        width += fm::style::keyChipSize(m_keyHint).width() + 6;
    if (m_glyph != glyph::None)
        width += 14 + 6;
    return QSize(wc ? std::max(width, 80) : width, wc ? 24 : 28);
}

QSize ChipButton::minimumSizeHint() const
{
    return sizeHint();
}

void ChipButton::paintEvent(QPaintEvent *)
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    QStylePainter p(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    option.text.clear();
    option.icon = QIcon();
    p.drawControl(QStyle::CE_PushButtonBevel, option);
    const int shift = isDown() && tc.isWatercolor() ? 1 : 0;
    const QRect r = rect().translated(shift, shift);
    qreal x = r.left() + 5;
    if (!m_keyHint.isEmpty()) {
        const QSize chip = fm::style::keyChipSize(m_keyHint);
        fm::style::paintKeyChip(&p, QRectF(x, r.center().y() + 1 - chip.height() / 2.0, chip.width(), chip.height()), m_keyHint, tc);
        x += chip.width() + 6;
    }
    if (m_glyph != glyph::None) {
        const fm::style::Glyph g = glyph::toStyle(m_glyph);
        fm::style::paintGlyph(&p, g, QRectF(x, r.center().y() + 0.5 - 7, 14, 14),
                              g == fm::style::Glyph::Folder ? tc[Token::Folder] : tc[Token::Fg]);
        x += 14 + 6;
    }
    p.setFont(labelFont());
    p.setPen(isEnabled() ? tc[Token::Fg] : tc[Token::Fg3]);
    const int available = int(r.x() + r.width() - 8 - x);
    p.drawText(QRectF(x, r.top(), available + 1, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(labelFont()).elidedText(text(), Qt::ElideMiddle, available));
}

// ---------------------------------------------------------------- RecentTargetsBar

RecentTargetsBar::RecentTargetsBar(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(6);
    m_layout->addStretch(1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void RecentTargetsBar::setTargets(const QStringList &targets)
{
    m_targets = targets;
    qDeleteAll(m_chips);
    m_chips.clear();
    qDeleteAll(m_shortcuts);
    m_shortcuts.clear();
    for (int i = 0; i < targets.size(); ++i) {
        auto *chip = new ChipButton(targets.at(i), this);
        chip->setKeyHint(QString::number(i + 1));
        chip->setToolTip(tr("Alt+%1").arg(i + 1));
        chip->setAccessibleName(targets.at(i));
        m_layout->insertWidget(i, chip);
        m_chips.append(chip);
        const QString path = targets.at(i);
        connect(chip, &QPushButton::clicked, this, [this, i, path] { Q_EMIT targetActivated(i, path); });
        if (i < 9) {
            auto *shortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key(Qt::Key_1 + i)), this);
            shortcut->setContext(Qt::WindowShortcut);
            connect(shortcut, &QShortcut::activated, this, [this, i, path] { Q_EMIT targetActivated(i, path); });
            m_shortcuts.append(shortcut);
        }
    }
}

// ---------------------------------------------------------------- ChoiceCard

ChoiceCard::ChoiceCard(QWidget *parent)
    : ChoiceCard(QString(), parent)
{
}

ChoiceCard::ChoiceCard(const QString &text, QWidget *parent)
    : QRadioButton(text, parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_Hover);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, qOverload<>(&QWidget::update));
}

void ChoiceCard::setDetail(const QString &detail)
{
    m_detail = detail;
    update();
}

void ChoiceCard::setKeyHint(const QString &keys)
{
    m_keyHint = keys;
    update();
}

void ChoiceCard::setFileKind(FileKind kind)
{
    m_kind = kind;
    update();
}

QSize ChoiceCard::sizeHint() const
{
    return QSize(220, 40);
}

QSize ChoiceCard::minimumSizeHint() const
{
    return QSize(140, 40);
}

bool ChoiceCard::hitButton(const QPoint &pos) const
{
    return rect().contains(pos);
}

void ChoiceCard::paintEvent(QPaintEvent *)
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    const bool wc = tc.isWatercolor();
    const qreal radius = wc ? 0 : 6;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, !wc);
    const bool on = isChecked();
    QColor fill = on ? tc[Token::AccentSoft] : tc[Token::Surface];
    if (!on && underMouse() && isEnabled())
        fill = mix(fill, tc[Token::Fg], 0.04);
    p.setPen(QPen(on ? tc[Token::Accent] : tc[Token::Line], 1.0));
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    // 라디오 표시는 스타일에 맡긴다(두 디자인 자동)
    QStyleOptionButton option;
    initStyleOption(&option);
    const int indicator = style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, &option, this);
    option.rect = QRect(12, (height() - indicator) / 2, indicator, indicator);
    option.state &= ~QStyle::State_HasFocus;
    style()->drawPrimitive(QStyle::PE_IndicatorRadioButton, &option, &p, this);

    qreal x = 12 + indicator + 10;
    paintFileIcon(&p, QRectF(x, height() / 2.0 - 8, 16, 16), m_kind, false, tc);
    x += 16 + 10;
    qreal right = width() - 12;
    if (!m_keyHint.isEmpty()) {
        const QSize chip = fm::style::keyChipSize(m_keyHint);
        right -= chip.width();
        fm::style::paintKeyChip(&p, QRectF(right, height() / 2.0 - chip.height() / 2.0 + 0.5, chip.width(), chip.height()), m_keyHint, tc);
        right -= 10;
    }
    const QFont mono = fm::style::monoFont(12);
    if (!m_detail.isEmpty()) {
        const int w = QFontMetrics(mono).horizontalAdvance(m_detail);
        p.setFont(mono);
        p.setPen(tc[Token::Fg3]);
        p.drawText(QRectF(right - w, 0, w, height()), Qt::AlignRight | Qt::AlignVCenter, m_detail);
        right -= w + 10;
    }
    const QFont name = fm::style::pixelFont(font(), 13);
    p.setFont(name);
    p.setPen(isEnabled() ? tc[Token::Fg] : tc[Token::Fg3]);
    const int available = int(right - x);
    p.drawText(QRectF(x, 0, available, height()), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(name).elidedText(text().remove(u'&'), Qt::ElideRight, available));

    if (hasFocus()) {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);
        focus.rect = rect();
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &p, this);
    }
}

// ---------------------------------------------------------------- FileSummaryList

FileSummaryList::FileSummaryList(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, qOverload<>(&QWidget::update));
}

void FileSummaryList::setItems(const QList<Item> &items)
{
    m_items = items;
    updateGeometry();
    update();
}

void FileSummaryList::setMaxVisibleRows(int rows)
{
    m_maxRows = std::max(1, rows);
    updateGeometry();
    update();
}

int FileSummaryList::visibleRows() const
{
    return std::min<int>(int(m_items.size()), m_maxRows);
}

QSize FileSummaryList::sizeHint() const
{
    return QSize(320, std::max(1, visibleRows()) * 28);
}

QSize FileSummaryList::minimumSizeHint() const
{
    return sizeHint();
}

void FileSummaryList::paintEvent(QPaintEvent *)
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    QPainter p(this);
    const QFont name = fm::style::pixelFont(font(), 13);
    const QFont size = fm::style::withTabularNumbers(fm::style::pixelFont(font(), 12));
    const int rows = visibleRows();
    const bool overflow = m_items.size() > m_maxRows;
    for (int i = 0; i < rows; ++i) {
        const QRect row(12, i * 28, width() - 24, 28);
        if (overflow && i == rows - 1) {
            p.setFont(name);
            p.setPen(tc[Token::Fg3]);
            p.drawText(row, Qt::AlignLeft | Qt::AlignVCenter, tr("… 외 %1개").arg(m_items.size() - (rows - 1)));
            break;
        }
        const Item &item = m_items.at(i);
        paintFileIcon(&p, QRectF(row.left(), row.center().y() + 0.5 - 8, 16, 16), item.kind, item.isDir, tc);
        p.setFont(size);
        const int sizeWidth = QFontMetrics(size).horizontalAdvance(item.size);
        p.setPen(tc[Token::Fg2]);
        p.drawText(QRect(row.right() - sizeWidth, row.top(), sizeWidth + 1, row.height()), Qt::AlignRight | Qt::AlignVCenter, item.size);
        int nameRight = row.right() - sizeWidth - 8;
        if (item.readOnly) {
            const QSize tag = fm::style::tagSize(tr("읽기 전용"), fm::style::Tone::Mute, true, false);
            fm::style::paintTag(&p, QRectF(nameRight - tag.width(), row.center().y() - tag.height() / 2.0, tag.width(), tag.height()),
                                tr("읽기 전용"), fm::style::Tone::Mute, tc, true);
            nameRight -= tag.width() + 8;
        }
        p.setFont(name);
        p.setPen(tc[Token::Fg]);
        const int left = row.left() + 16 + 8;
        p.drawText(QRect(left, row.top(), nameRight - left, row.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(name).elidedText(item.name, Qt::ElideMiddle, nameRight - left));
    }
}

// ---------------------------------------------------------------- FolderPlanView

FolderPlanView::FolderPlanView(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, qOverload<>(&QWidget::update));
}

void FolderPlanView::setPlan(const QList<Node> &nodes)
{
    m_nodes = nodes;
    updateGeometry();
    update();
}

void FolderPlanView::setMaxVisibleRows(int rows)
{
    m_maxRows = std::max(1, rows);
    updateGeometry();
}

QSize FolderPlanView::sizeHint() const
{
    return QSize(320, std::max(1, std::min<int>(int(m_nodes.size()), m_maxRows)) * 24);
}

QSize FolderPlanView::minimumSizeHint() const
{
    return sizeHint();
}

void FolderPlanView::paintEvent(QPaintEvent *)
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    QPainter p(this);
    const QFont regular = fm::style::pixelFont(font(), 13);
    QFont bold = regular;
    bold.setWeight(QFont::DemiBold);
    const QFont status = fm::style::pixelFont(font(), 11.5);
    QFont statusBold = status;
    statusBold.setWeight(QFont::DemiBold);
    const QFont mono = fm::style::monoFont(12);
    // 들여쓰기: 첫 줄 0, 둘째 줄 18, 그다음 +22(가지 글자 칸 14 + 간격 8)
    const int rows = std::min<int>(int(m_nodes.size()), m_maxRows);
    for (int i = 0; i < rows; ++i) {
        const Node &node = m_nodes.at(i);
        const QRect row(0, i * 24, width(), 24);
        qreal x = 0;
        if (i > 0) {
            const qreal branch = 18 + (i - 1) * 22;
            p.setFont(mono);
            p.setPen(tc[Token::Fg3]);
            p.drawText(QRectF(branch, row.top(), 14, row.height()), Qt::AlignCenter, u"└"_s);
            x = branch + 22;
        }
        const QRectF icon(x, row.center().y() + 0.5 - 8, 16, 16);
        switch (node.state) {
        case Node::Current:
        case Node::Existing:
            fm::style::paintGlyph(&p, fm::style::Glyph::Folder, icon, tc[Token::Folder]);
            break;
        case Node::New:
            fm::style::paintGlyph(&p, fm::style::Glyph::FolderOutline, icon, tc[Token::AccentFg]);
            break;
        case Node::Invalid:
            fm::style::paintGlyph(&p, fm::style::Glyph::FolderOutline, icon, tc[Token::Danger]);
            break;
        }
        x += 16 + 8;
        // 상태 글자(오른쪽 정렬)
        QString text;
        QColor color = tc[Token::Fg3];
        QFont font = status;
        switch (node.state) {
        case Node::Current:  text = tr("현재 위치"); break;
        case Node::Existing: text = tr("이미 있음"); break;
        case Node::New:      text = tr("새로 만듦"); color = tc[Token::AccentFg]; font = statusBold; break;
        case Node::Invalid:  text = tr("쓸 수 없는 이름"); color = tc[Token::Danger]; font = statusBold; break;
        }
        const int textWidth = QFontMetrics(font).horizontalAdvance(text);
        qreal statusLeft = row.right() - textWidth;
        p.setFont(font);
        p.setPen(color);
        p.drawText(QRectF(statusLeft, row.top(), textWidth + 1, row.height()), Qt::AlignRight | Qt::AlignVCenter, text);
        if (node.state == Node::New) {
            statusLeft -= 4 + 10;
            fm::style::paintGlyph(&p, fm::style::Glyph::PlusSmall, QRectF(statusLeft, row.center().y() + 0.5 - 5, 10, 10), color);
        }
        p.setFont(node.state == Node::New ? bold : regular);
        p.setPen(node.state == Node::Invalid ? tc[Token::Danger] : tc[Token::Fg]);
        const int available = int(statusLeft - 8 - x);
        p.drawText(QRectF(x, row.top(), available, row.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(p.font()).elidedText(node.name, Qt::ElideMiddle, available));
    }
}

// ---------------------------------------------------------------- TokenButton

TokenButton::TokenButton(QWidget *parent)
    : QPushButton(parent)
{
    setAutoDefault(false);
    setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

TokenButton::TokenButton(const QString &token, const QString &caption, QWidget *parent)
    : TokenButton(parent)
{
    setToken(token);
    setCaption(caption);
}

void TokenButton::setToken(const QString &token)
{
    m_token = token;
    setAccessibleName(m_token + u' ' + m_caption);
    updateGeometry();
    update();
}

void TokenButton::setCaption(const QString &caption)
{
    m_caption = caption;
    setAccessibleName(m_token + u' ' + m_caption);
    updateGeometry();
    update();
}

QSize TokenButton::sizeHint() const
{
    const QFont code = fm::style::monoFont(11.5, QFont::Medium);
    const QFont cap = fm::style::pixelFont(font(), 11.5);
    return QSize(4 + QFontMetrics(code).horizontalAdvance(m_token) + 5 + QFontMetrics(cap).horizontalAdvance(m_caption) + 4, 24);
}

QSize TokenButton::minimumSizeHint() const
{
    return QSize(40, 24);
}

void TokenButton::paintEvent(QPaintEvent *)
{
    // 평면(두 디자인 — 워터컬러 CSS가 .tok을 다시 칠하지 않는다): --btn · 테두리 --btn-line · 모서리 4(시안2 0)
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    const bool wc = tc.isWatercolor();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, !wc);
    QColor fill = tc[Token::Btn];
    if (isDown())
        fill = mix(fill, tc[Token::Fg], 0.08);
    else if (underMouse() && isEnabled())
        fill = mix(fill, tc[Token::Fg], 0.04);
    p.setPen(QPen(tc[Token::BtnLine], 1.0));
    p.setBrush(fill);
    const qreal radius = wc ? 0 : 4;
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    const QFont code = fm::style::monoFont(11.5, QFont::Medium);
    const QFont cap = fm::style::pixelFont(font(), 11.5);
    const int codeWidth = QFontMetrics(code).horizontalAdvance(m_token);
    const int capWidth = QFontMetrics(cap).horizontalAdvance(m_caption);
    const int total = codeWidth + 5 + capWidth;
    const int left = std::max(4, (width() - total) / 2);
    p.setFont(code);
    p.setPen(tc[Token::AccentFg]);
    p.drawText(QRect(left, 0, codeWidth + 1, height()), Qt::AlignLeft | Qt::AlignVCenter, m_token);
    p.setFont(cap);
    p.setPen(tc[Token::Fg2]);
    p.drawText(QRect(left + codeWidth + 5, 0, width() - left - codeWidth - 5, height()), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(cap).elidedText(m_caption, Qt::ElideRight, width() - left - codeWidth - 9));
    if (hasFocus()) {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);
        focus.rect = rect();
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &p, this);
    }
}

} // namespace fm::ui
