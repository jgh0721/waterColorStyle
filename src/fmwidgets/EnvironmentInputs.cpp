#include <fmwidgets/EnvironmentInputs.h>
#include <fmwidgets/Button.h>
#include <QApplication>
#include <QCompleter>
#include <QStringListModel>
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QSignalBlocker>

namespace fm::ui {
ExpandableValueEdit::ExpandableValueEdit(QWidget* Parent) : TextField(Parent), completion_(new QCompleter(this)), names_(new QStringListModel(this))
{
    setMonospace(true); completion_->setModel(names_); completion_->setCaseSensitivity(Qt::CaseInsensitive);
    completion_->setCompletionMode(QCompleter::PopupCompletion); completion_->setWidget(this);
    connect(this, &QLineEdit::textEdited, this, [this](const QString& Text) {
        emit expansionRequested(Text);
        const auto Left = Text.left(cursorPosition()); const auto Open = Left.lastIndexOf('%');
        if (Open < 0 || Left.count('%') % 2 == 0) { completion_->popup()->hide(); return; }
        completion_->setCompletionPrefix(Left.mid(Open + 1)); completion_->complete();
    });
    connect(completion_, qOverload<const QString&>(&QCompleter::activated), this, [this](const QString& Name) {
        const auto Before = text(); const int Cursor = cursorPosition(), Open = Before.left(Cursor).lastIndexOf('%');
        if (Open < 0) return;
        int End = Cursor; if (Before.mid(Cursor, 1) == "%") ++End;
        setText(Before.left(Open + 1) + Name + '%' + Before.mid(End)); setCursorPosition(Open + Name.size() + 2);
        emit expansionRequested(text());
    });
}
void ExpandableValueEdit::setVariableNames(const QStringList& Names) { auto Sorted = Names; Sorted.sort(Qt::CaseInsensitive); names_->setStringList(Sorted); }
MaskedValueEdit::MaskedValueEdit(QWidget* Parent) : QWidget(Parent), editor_(new TextField(this)), reveal_(new Button(tr("누르는 동안 보기"), this))
{
    auto* Layout = new QHBoxLayout(this); Layout->setContentsMargins(0, 0, 0, 0); Layout->addWidget(editor_, 1); Layout->addWidget(reveal_);
    editor_->setEchoMode(QLineEdit::Password); editor_->setContextMenuPolicy(Qt::NoContextMenu); setFocusProxy(editor_);
    connect(editor_, &QLineEdit::textChanged, this, &MaskedValueEdit::textChanged);
    connect(reveal_, &QPushButton::pressed, this, [this] { editor_->setEchoMode(QLineEdit::Normal); });
    connect(reveal_, &QPushButton::released, this, &MaskedValueEdit::hideValue);
    qApp->installEventFilter(this);
}
void MaskedValueEdit::setText(const QString& Text) { hideValue(); editor_->setText(Text); }
QString MaskedValueEdit::text() const { return editor_->text(); }
void MaskedValueEdit::setReadOnly(bool ReadOnly) { editor_->setReadOnly(ReadOnly); }
bool MaskedValueEdit::revealed() const { return editor_->echoMode() == QLineEdit::Normal; }
void MaskedValueEdit::hideValue() { editor_->setEchoMode(QLineEdit::Password); }
bool MaskedValueEdit::eventFilter(QObject* Watched, QEvent* Event)
{
    if (Event->type() == QEvent::ApplicationDeactivate || Event->type() == QEvent::WindowDeactivate || Event->type() == QEvent::Hide
        || (Event->type() == QEvent::FocusOut && (Watched == editor_ || Watched == reveal_))) hideValue();
    // Holding reveal never grants clipboard access; release/focus loss remasks.
    if (Watched == editor_ && Event->type() == QEvent::KeyPress) {
        const auto* Key = static_cast<QKeyEvent*>(Event);
        if (Key->matches(QKeySequence::Copy) || Key->matches(QKeySequence::Cut)) return true;
    }
    return QWidget::eventFilter(Watched, Event);
}
OrderedTokenEdit::OrderedTokenEdit(QWidget* Parent) : QWidget(Parent), chips_(new QHBoxLayout), input_(new TextField(this))
{
    setFocusPolicy(Qt::StrongFocus); auto* Layout = new QVBoxLayout(this); Layout->setContentsMargins(0, 0, 0, 0); Layout->addLayout(chips_);
    auto* Row = new QHBoxLayout; Row->addWidget(input_, 1); auto* Add = new Button(tr("추가"), this); Row->addWidget(Add); Layout->addLayout(Row);
    input_->setPlaceholderText(tr("확장명 (예: .EXE)"));
    connect(Add, &QPushButton::clicked, this, [this] {
        const auto Token = input_->text().trimmed(); if (!Token.startsWith('.') || Token.contains(';') || Token.contains('/') || Token.contains('\\')) return;
        tokens_.append(Token); current_ = tokens_.size() - 1; input_->clear(); rebuild(); emit valueChanged(value());
    });
}
void OrderedTokenEdit::setValue(const QString& Value) { tokens_ = Value.isEmpty() ? QStringList() : Value.split(';', Qt::KeepEmptyParts); current_ = tokens_.isEmpty() ? -1 : 0; rebuild(); }
QString OrderedTokenEdit::value() const { return tokens_.join(';'); }
bool OrderedTokenEdit::moveCurrent(int Delta)
{
    const int Target = current_ + Delta; if (current_ < 0 || Target < 0 || Target >= tokens_.size()) return false;
    tokens_.move(current_, Target); current_ = Target; rebuild(); emit valueChanged(value()); return true;
}
void OrderedTokenEdit::rebuild()
{
    while (auto* Item = chips_->takeAt(0)) { if (auto* Widget = Item->widget()) Widget->deleteLater(); delete Item; }
    for (int I = 0; I < tokens_.size(); ++I) {
        auto* Chip = new Button(tokens_[I].isEmpty() ? tr("빈 항목") : tokens_[I], this); Chip->setCheckable(true); Chip->setChecked(I == current_); Chip->setFocusPolicy(Qt::NoFocus); chips_->addWidget(Chip);
        connect(Chip, &QPushButton::clicked, this, [this, I] { current_ = I; rebuild(); setFocus(); });
    }
    chips_->addStretch();
}
void OrderedTokenEdit::keyPressEvent(QKeyEvent* Event)
{
    if (Event->key() == Qt::Key_Left || Event->key() == Qt::Key_Right) {
        const int Delta = Event->key() == Qt::Key_Left ? -1 : 1;
        if (Event->modifiers().testFlag(Qt::ControlModifier)) moveCurrent(Delta);
        else if (!tokens_.isEmpty()) { current_ = qBound(0, current_ + Delta, int(tokens_.size()) - 1); rebuild(); }
        Event->accept(); return;
    }
    if (Event->key() == Qt::Key_Delete && current_ >= 0) { tokens_.removeAt(current_); current_ = qMin(current_, int(tokens_.size()) - 1); rebuild(); emit valueChanged(value()); Event->accept(); return; }
    QWidget::keyPressEvent(Event);
}
}
