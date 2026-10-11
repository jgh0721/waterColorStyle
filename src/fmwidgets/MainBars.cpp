// 메인 창 막대 — 명령줄 · 기능 키 · 찾기 상자 (01 §1.3 A3 · A5 · A6, 06 §4.13 · §4.15).

#include "fmwidgets/CommandLine.h"
#include "fmwidgets/FindBox.h"
#include "fmwidgets/FunctionKeyBar.h"

#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QStyleOptionButton>
#include <QStylePainter>

using namespace Qt::StringLiterals;

namespace fm::ui {

using fm::style::Token;

// ---------------------------------------------------------------- CommandLine

CommandLine::CommandLine(QWidget *parent)
    : QWidget(parent)
    , m_prompt(new QLabel(this))
    , m_edit(new QLineEdit(this))
{
    auto *layout = new QHBoxLayout(this);
    layout->setSpacing(8);
    layout->addWidget(m_prompt);
    layout->addWidget(m_edit, 1);
    m_prompt->setTextInteractionFlags(Qt::NoTextInteraction);
    applyFonts();
    m_edit->setPlaceholderText(u"명령 입력 — Ctrl+↓ 기록"_s);
    m_edit->setAccessibleName(u"명령줄"_s);
    m_edit->installEventFilter(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    connect(m_edit, &QLineEdit::returnPressed, this, [this] {
        const QString command = m_edit->text().trimmed();
        if (command.isEmpty())
            return;
        m_history.removeAll(command);
        m_history.prepend(command);
        while (m_history.size() > 50)
            m_history.removeLast();
        m_edit->clear();
        Q_EMIT commandEntered(command);
    });
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, &CommandLine::applyMetrics);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::fontsChanged, this, &CommandLine::applyFonts);
    applyMetrics();
}

QString CommandLine::prompt() const
{
    return m_prompt->text();
}

void CommandLine::setPrompt(const QString &path)
{
    m_prompt->setText(path.endsWith(u'>') ? path : path + u'>');
}

void CommandLine::applyFonts()
{
    // 설정 › 일반 · 모양의 고정폭 글꼴 · 크기(기본 Cascadia Mono 12)
    const QFont mono = fm::style::monoFont(fm::style::ThemeManager::instance().monoFontPx());
    m_prompt->setFont(mono);
    m_edit->setFont(mono);
}

void CommandLine::applyMetrics()
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    const bool wc = tc.isWatercolor();
    // 시안1: 34 · 좌우 12 · 틀 없는 입력(바탕 = 줄 바탕) / 시안2: 33 · 좌우 8 · 들어간 입력 25
    layout()->setContentsMargins(wc ? 8 : 12, wc ? 0 : 1, wc ? 8 : 12, 0);
    m_edit->setFrame(wc);
    QPalette palette = m_edit->palette();
    palette.setColor(QPalette::Base, wc ? tc[Token::Field] : tc[Token::Surface]);
    m_edit->setPalette(palette);
    m_edit->setFixedHeight(wc ? 25 : 28);
    QPalette labelPalette = m_prompt->palette();
    labelPalette.setColor(QPalette::WindowText, tc[Token::Fg2]);
    m_prompt->setPalette(labelPalette);
    setFixedHeight(wc ? 33 : 34);
    update();
}

QSize CommandLine::sizeHint() const
{
    return QSize(480, fm::style::themeColorsFor(this).isWatercolor() ? 33 : 34);
}

void CommandLine::paintEvent(QPaintEvent *)
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    QPainter p(this);
    if (tc.isWatercolor()) {
        p.fillRect(rect(), tc[Token::Win]);
    } else {
        p.fillRect(rect(), tc[Token::Surface]);
        p.fillRect(QRect(0, 0, width(), 1), tc[Token::Line]);
    }
}

void CommandLine::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::StyleChange)
        applyMetrics();
}

bool CommandLine::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Down && (key->modifiers() & Qt::ControlModifier)) {
            showHistory();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CommandLine::showHistory()
{
    QMenu menu(this);
    if (m_history.isEmpty()) {
        menu.addAction(u"기록 없음"_s)->setEnabled(false);
    } else {
        for (const QString &command : std::as_const(m_history)) {
            QAction *action = menu.addAction(command);
            connect(action, &QAction::triggered, this, [this, command] {
                m_edit->setText(command);
                m_edit->setFocus();
            });
        }
    }
    // 명령줄은 창 아래쪽이므로 위로 편다
    const QSize size = menu.sizeHint();
    menu.exec(m_edit->mapToGlobal(QPoint(0, -size.height())));
}

// ---------------------------------------------------------------- FunctionKeyButton

FunctionKeyButton::FunctionKeyButton(QWidget *parent)
    : FunctionKeyButton(QString(), QString(), parent)
{
}

FunctionKeyButton::FunctionKeyButton(const QString &keys, const QString &text, QWidget *parent)
    : QPushButton(text, parent)
    , m_keys(keys)
{
    setFocusPolicy(Qt::NoFocus);
    setAutoDefault(false);
    setAccessibleName(keys + u' ' + text);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
}

void FunctionKeyButton::setKeys(const QString &keys)
{
    m_keys = keys;
    update();
}

QSize FunctionKeyButton::sizeHint() const
{
    const bool wc = fm::style::themeColorsFor(this).isWatercolor();
    const QFont font = fm::style::pixelFont(this->font(), 12);
    const int textWidth = QFontMetrics(font).horizontalAdvance(text());
    const int keyWidth = wc ? QFontMetrics(fm::style::pixelFont(this->font(), 12, QFont::Bold)).horizontalAdvance(m_keys + u' ')
                            : fm::style::keyChipSize(m_keys).width() + 8;
    return QSize(12 + keyWidth + textWidth, wc ? 28 : 31);
}

QSize FunctionKeyButton::minimumSizeHint() const
{
    return QSize(40, sizeHint().height());
}

void FunctionKeyButton::paintEvent(QPaintEvent *)
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    const bool wc = tc.isWatercolor();
    const QFont font = fm::style::pixelFont(this->font(), 12);
    if (wc) {
        // 입체 누름 단추 + 굵은 키 + 글자
        QStylePainter sp(this);
        QStyleOptionButton option;
        initStyleOption(&option);
        option.text.clear();
        sp.drawControl(QStyle::CE_PushButtonBevel, option);
        const fm::style::WatercolorChrome &x = fm::style::watercolorChrome(tc.variant());
        const bool hot = isEnabled() && (underMouse() || isDown());
        const QColor color = !isEnabled() ? x.disFg : hot ? x.hoverFg : tc[Token::Fg];
        const QFont boldFont = fm::style::pixelFont(this->font(), 12, QFont::Bold);
        const QString keyText = m_keys + u' ';
        const int keyWidth = QFontMetrics(boldFont).horizontalAdvance(keyText);
        const int textWidth = QFontMetrics(font).horizontalAdvance(text());
        const int shift = isDown() ? 1 : 0;
        const QRect r = rect().translated(shift, shift);
        const int left = r.left() + std::max(6, (r.width() - keyWidth - textWidth) / 2);
        sp.setPen(color);
        sp.setFont(boldFont);
        sp.drawText(QRect(left, r.top(), keyWidth, r.height()), Qt::AlignLeft | Qt::AlignVCenter, keyText);
        sp.setFont(font);
        sp.drawText(QRect(left + keyWidth, r.top(), r.right() - left - keyWidth - 5, r.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(font).elidedText(text(), Qt::ElideRight, r.right() - left - keyWidth - 5));
        return;
    }
    // 시안1: 평면 칸, 마우스 올림 --accent-soft, 키 칩 + 간격 8 + 글자(가운데)
    QPainter p(this);
    if (isEnabled() && (underMouse() || isDown()))
        p.fillRect(rect(), tc[Token::AccentSoft]);
    const QSize chip = fm::style::keyChipSize(m_keys);
    const int textWidth = QFontMetrics(font).horizontalAdvance(text());
    const int total = chip.width() + 8 + textWidth;
    const int left = std::max(6, (width() - total) / 2);
    const QRectF chipRect(left, (height() - chip.height()) / 2.0, chip.width(), chip.height());
    fm::style::paintKeyChip(&p, chipRect, m_keys, tc);
    p.setFont(font);
    p.setPen(isEnabled() ? tc[Token::Fg] : tc[Token::Fg3]);
    const int textLeft = int(chipRect.right()) + 8;
    p.drawText(QRect(textLeft, 0, width() - textLeft - 6, height()), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(font).elidedText(text(), Qt::ElideRight, width() - textLeft - 6));
}

// ---------------------------------------------------------------- FunctionKeyBar

FunctionKeyBar::FunctionKeyBar(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, &FunctionKeyBar::applyMetrics);
    applyMetrics();
}

FunctionKeyButton *FunctionKeyBar::addKey(const QString &keys, const QString &text)
{
    auto *button = new FunctionKeyButton(keys, text, this);
    m_layout->addWidget(button, 1);
    m_buttons.append(button);
    connect(button, &QPushButton::clicked, this, [this, keys] { Q_EMIT triggered(keys); });
    return button;
}

void FunctionKeyBar::applyMetrics()
{
    const bool wc = fm::style::themeColorsFor(this).isWatercolor();
    // 시안1: 32 · 위 1 px 선 · 칸 사이 1 px 선 / 시안2: 36 · 간격 4 · 안쪽 2 4 6
    m_layout->setContentsMargins(wc ? 4 : 0, wc ? 2 : 1, wc ? 4 : 0, wc ? 6 : 0);
    m_layout->setSpacing(wc ? 4 : 1);
    setFixedHeight(wc ? 36 : 32);
    for (FunctionKeyButton *b : std::as_const(m_buttons)) {
        b->setFixedHeight(wc ? 28 : 31);
        b->update();
    }
    update();
}

QSize FunctionKeyBar::sizeHint() const
{
    return QSize(720, fm::style::themeColorsFor(this).isWatercolor() ? 36 : 32);
}

void FunctionKeyBar::paintEvent(QPaintEvent *)
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    QPainter p(this);
    p.fillRect(rect(), tc[Token::Win]);
    if (tc.isWatercolor())
        return;
    p.fillRect(QRect(0, 0, width(), 1), tc[Token::Line]);
    // 칸 사이 1 px — 간격 1 px 자리에 선을 칠한다
    for (int i = 1; i < m_buttons.size(); ++i) {
        const QRect r = m_buttons.at(i)->geometry();
        p.fillRect(QRect(r.left() - 1, 1, 1, height() - 1), tc[Token::Line]);
    }
}

void FunctionKeyBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::StyleChange)
        applyMetrics();
}

// ---------------------------------------------------------------- FindBox

FindBox::FindBox(QWidget *parent)
    : QLineEdit(parent)
{
    setPlaceholderText(u"현재 폴더에서 찾기   Ctrl+F"_s);
    setAccessibleName(u"현재 폴더에서 찾기"_s);
    setClearButtonEnabled(true);
    setFont(fm::style::pixelFont(font(), 12.5));
    m_icon = addAction(QIcon(), QLineEdit::LeadingPosition);
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, &FindBox::refreshIcon);
    refreshIcon();
}

QSize FindBox::sizeHint() const
{
    return QSize(300, QLineEdit::sizeHint().height());
}

void FindBox::refreshIcon()
{
    m_icon->setIcon(fm::style::glyphIcon(fm::style::Glyph::Search, fm::style::themeColorsFor(this)[Token::Fg3], 14));
}

void FindBox::changeEvent(QEvent *event)
{
    QLineEdit::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
        refreshIcon();
}

} // namespace fm::ui
