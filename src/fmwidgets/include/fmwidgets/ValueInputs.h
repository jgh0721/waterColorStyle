#pragma once
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QWidget>
#include <optional>

namespace fm::ui {
class SegmentedControl;
class Label;
// Reusable inputs with the fm style's field chrome and explicit editor semantics.
class ChoiceBox : public QComboBox
{
    Q_OBJECT
public:
    explicit ChoiceBox(QWidget* Parent = nullptr);
};
class TextField : public QLineEdit
{
    Q_OBJECT
public:
    explicit TextField(QWidget* Parent = nullptr);
    void setMonospace(bool Enabled);
};
class MultilineField : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit MultilineField(QWidget* Parent = nullptr);
};
class InlineNameEditor : public TextField
{
    Q_OBJECT
public:
    explicit InlineNameEditor(QWidget* Parent = nullptr);
signals:
    void commitRequested(const QString& Text);
    void cancelRequested();
protected:
    void keyPressEvent(QKeyEvent* Event) override;
};
class UnsignedNumberEdit : public QWidget
{
    Q_OBJECT
public:
    explicit UnsignedNumberEdit(QWidget* Parent = nullptr);
    void setBitWidth(int Bits);
    int bitWidth() const { return bits_; }
    bool setRadix(int Radix);
    int radix() const { return radix_; }
    bool setValue(quint64 Value);
    std::optional<quint64> value() const;
    QString draft() const;
    void setDraft(const QString& Text);
    TextField* editor() const { return editor_; }
signals:
    void draftChanged();
    void validityChanged(bool Valid);
private:
    void updateValidation();
    TextField* editor_;
    SegmentedControl* radixSelector_;
    Label* error_;
    int bits_ = 32, radix_ = 16;
};
}