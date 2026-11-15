#include "fmwidgets/ValueInputs.h"
#include <fmwidgets/Label.h>
#include <fmwidgets/SegmentedControl.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/StylePaint.h>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <limits>

namespace fm::ui {
ChoiceBox::ChoiceBox(QWidget* Parent) : QComboBox(Parent)
{
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setMinimumContentsLength(8);
}
TextField::TextField(QWidget* Parent) : QLineEdit(Parent) { setClearButtonEnabled(false); }
void TextField::setMonospace(bool Enabled) { setFont(Enabled ? fm::style::monoFont(12) : QFont()); }
MultilineField::MultilineField(QWidget* Parent) : QPlainTextEdit(Parent)
{
    setFont(fm::style::monoFont(12)); setTabChangesFocus(true); setLineWrapMode(QPlainTextEdit::NoWrap);
}
InlineNameEditor::InlineNameEditor(QWidget* Parent) : TextField(Parent)
{
    setMonospace(true);
}
void InlineNameEditor::keyPressEvent(QKeyEvent* Event)
{
    if (Event->key() == Qt::Key_Escape && Event->modifiers() == Qt::NoModifier) {
        Event->accept(); emit cancelRequested(); return;
    }
    if ((Event->key() == Qt::Key_Return || Event->key() == Qt::Key_Enter) && Event->modifiers() == Qt::NoModifier) {
        Event->accept(); emit commitRequested(text()); return;
    }
    TextField::keyPressEvent(Event);
}
UnsignedNumberEdit::UnsignedNumberEdit(QWidget* Parent) : QWidget(Parent)
{
    auto* Layout = new QVBoxLayout(this); Layout->setContentsMargins(0, 0, 0, 0);
    auto* Row = new QHBoxLayout; Layout->addLayout(Row);
    editor_ = new TextField(this); editor_->setMonospace(true); editor_->setAccessibleName(tr("정수 값"));
    radixSelector_ = new SegmentedControl({tr("16진"), tr("10진")}, this);
    radixSelector_->setSegmentSize(SegmentedControl::Compact); radixSelector_->setCurrentIndex(0);
    Row->addWidget(editor_, 1); Row->addWidget(radixSelector_);
    error_ = new Label({}, Label::Help, this); error_->setTone(Label::Danger); Layout->addWidget(error_);
    connect(editor_, &QLineEdit::textChanged, this, [this] { updateValidation(); emit draftChanged(); });
    connect(radixSelector_, &SegmentedControl::currentIndexChanged, this, [this](int Index) {
        if (!setRadix(Index == 0 ? 16 : 10)) {
            const QSignalBlocker Block(radixSelector_); radixSelector_->setCurrentIndex(radix_ == 16 ? 0 : 1);
        }
    });
    setFocusProxy(editor_); setValue(0);
}
void UnsignedNumberEdit::setBitWidth(int Bits)
{
    if (Bits != 32 && Bits != 64) return;
    bits_ = Bits; updateValidation();
}
std::optional<quint64> UnsignedNumberEdit::value() const
{
    auto Text = editor_->text().trimmed();
    if (radix_ == 16 && Text.startsWith("0x", Qt::CaseInsensitive)) Text.remove(0, 2);
    const QRegularExpression Pattern(radix_ == 16 ? "^[0-9a-fA-F]+$" : "^[0-9]+$");
    if (!Pattern.match(Text).hasMatch()) return {};
    bool Ok = false; const auto Value = Text.toULongLong(&Ok, radix_);
    if (!Ok || (bits_ == 32 && Value > std::numeric_limits<quint32>::max())) return {};
    return Value;
}
bool UnsignedNumberEdit::setRadix(int Radix)
{
    if (Radix != 10 && Radix != 16) return false;
    if (Radix == radix_) return true;
    const auto Value = value(); if (!Value) { updateValidation(); return false; }
    radix_ = Radix;
    { const QSignalBlocker Block(radixSelector_); radixSelector_->setCurrentIndex(radix_ == 16 ? 0 : 1); }
    editor_->setText(QString::number(*Value, radix_).toUpper()); return true;
}
bool UnsignedNumberEdit::setValue(quint64 Value)
{
    if (bits_ == 32 && Value > std::numeric_limits<quint32>::max()) return false;
    editor_->setText(QString::number(Value, radix_).toUpper()); return true;
}
QString UnsignedNumberEdit::draft() const { return editor_->text(); }
void UnsignedNumberEdit::setDraft(const QString& Text) { editor_->setText(Text); }
void UnsignedNumberEdit::updateValidation()
{
    const bool Valid = value().has_value();
    error_->setText(Valid ? QString() : tr("%1비트 부호 없는 정수를 입력하세요. 현재 초안은 보존됩니다.").arg(bits_));
    error_->setVisible(!Valid); editor_->setAccessibleDescription(error_->text()); emit validityChanged(Valid);
}
}