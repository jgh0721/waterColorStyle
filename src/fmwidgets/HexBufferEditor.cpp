#include "fmwidgets/HexBufferEditor.h"
#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeManager.h>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QUndoCommand>
#include <QUndoStack>

namespace fm::ui {
class HexPatchCommand final : public QUndoCommand
{
public:
    HexPatchCommand(HexBufferEditor* Editor, qsizetype Offset, QByteArray Before, QByteArray After)
        : editor_(Editor), offset_(Offset), before_(std::move(Before)), after_(std::move(After)) {}
    void redo() override { editor_->applyChange(offset_, before_.size(), after_); }
    void undo() override { editor_->applyChange(offset_, after_.size(), before_); }
private:
    HexBufferEditor* editor_;
    qsizetype offset_;
    QByteArray before_, after_;
};
HexBufferEditor::HexBufferEditor(QWidget* Parent) : QAbstractScrollArea(Parent), undo_(new QUndoStack(this))
{
    setFocusPolicy(Qt::StrongFocus); setFont(fm::style::monoFont(12)); setAccessibleName(tr("이진 데이터 16진 편집기"));
    setAccessibleDescription(tr("오프셋 · 16진 · 문자. 화살표로 이동, Shift로 선택, Ctrl+C/V로 16진 복사와 붙여넣기."));
    connect(verticalScrollBar(), &QScrollBar::valueChanged, viewport(), qOverload<>(&QWidget::update));
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, viewport(), qOverload<>(&QWidget::update));
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, [this] {
        setFont(fm::style::monoFont(12)); updateMetrics(); viewport()->update();
    });
    updateMetrics();
}
bool HexBufferEditor::setData(const QByteArray& Data)
{
    if (Data.size() > MaximumBytes) { emit editRejected(tr("이진 데이터가 편집 한도를 초과합니다.")); return false; }
    undo_->clear(); undoBytes_ = 0; bytes_ = Data; cursor_ = anchor_ = 0; lowNibble_ = false;
    updateMetrics(); viewport()->update(); emit dataChanged(); emit cursorChanged(0); return true;
}
void HexBufferEditor::updateMetrics()
{
    const QFontMetrics Metrics(font()); cell_ = qMax(1, Metrics.horizontalAdvance('0')); rowHeight_ = Metrics.height() + 6;
    verticalScrollBar()->setPageStep(qMax(1, viewport()->height() / rowHeight_));
    verticalScrollBar()->setRange(0, qMax(0, int(bytes_.size() / 16 + 1) - verticalScrollBar()->pageStep()));
    horizontalScrollBar()->setRange(0, qMax(0, 77 * cell_ - viewport()->width()));
}
void HexBufferEditor::resizeEvent(QResizeEvent* Event) { QAbstractScrollArea::resizeEvent(Event); updateMetrics(); }
void HexBufferEditor::paintEvent(QPaintEvent*)
{
    QPainter Painter(viewport()); Painter.setFont(font()); const auto Palette = palette();
    Painter.fillRect(viewport()->rect(), Palette.base());
    const int Left = -horizontalScrollBar()->value(), First = verticalScrollBar()->value();
    const int Rows = viewport()->height() / rowHeight_ + 1;
    for (int Line = 0; Line < Rows; ++Line) {
        const qsizetype Start = qsizetype(First + Line) * 16; if (Start > bytes_.size()) break;
        const int Y = Line * rowHeight_; Painter.setPen(Palette.color(QPalette::Disabled, QPalette::Text));
        Painter.drawText(QRect(Left, Y, 9 * cell_, rowHeight_), Qt::AlignVCenter | Qt::AlignRight, QString::number(Start, 16).rightJustified(8, '0').toUpper());
        for (int Column = 0; Column < 16 && Start + Column <= bytes_.size(); ++Column) {
            const auto Offset = Start + Column; const bool Exists = Offset < bytes_.size();
            const bool Selected = Offset >= selectionOffset() && Offset < selectionOffset() + selectionLength();
            const QRect Hex(Left + (11 + Column * 3) * cell_, Y, cell_ * 2, rowHeight_);
            const QRect Character(Left + (61 + Column) * cell_, Y, cell_, rowHeight_);
            if (Selected) { Painter.fillRect(Hex, Palette.highlight()); Painter.fillRect(Character, Palette.highlight()); }
            Painter.setPen(Selected ? Palette.highlightedText().color() : Palette.text().color());
            const auto Byte = Exists ? quint8(bytes_[Offset]) : 0;
            Painter.drawText(Hex, Qt::AlignCenter, Exists ? QString::number(Byte, 16).rightJustified(2, '0').toUpper() : "--");
            Painter.drawText(Character, Qt::AlignCenter, Exists && Byte >= 32 && Byte < 127 ? QString(QChar(ushort(Byte))) : Exists ? QStringLiteral("·") : QString());
            if (hasFocus() && Offset == cursor_) {
                Painter.setPen(Palette.highlight().color()); Painter.drawRect((ascii_ ? Character : Hex).adjusted(0, 1, -1, -2));
            }
        }
    }
}
void HexBufferEditor::moveCursor(qsizetype Offset, bool Extend)
{
    cursor_ = qBound<qsizetype>(0, Offset, bytes_.size()); if (!Extend) anchor_ = cursor_; lowNibble_ = false;
    const int Row = int(cursor_ / 16), First = verticalScrollBar()->value();
    if (Row < First) verticalScrollBar()->setValue(Row);
    else if (Row >= First + verticalScrollBar()->pageStep()) verticalScrollBar()->setValue(Row - verticalScrollBar()->pageStep() + 1);
    viewport()->update(); emit cursorChanged(cursor_);
}
void HexBufferEditor::setCursorOffset(qsizetype Offset) { moveCursor(Offset, false); }
void HexBufferEditor::setSelection(qsizetype Offset, qsizetype Length)
{
    anchor_ = qBound<qsizetype>(0, Offset, bytes_.size());
    moveCursor(anchor_ + qBound<qsizetype>(0, Length, bytes_.size() - anchor_), true);
}
void HexBufferEditor::applyChange(qsizetype Offset, qsizetype Removed, const QByteArray& Added)
{
    bytes_.replace(Offset, Removed, Added); updateMetrics(); moveCursor(Offset + Added.size(), false); emit dataChanged();
}
bool HexBufferEditor::replaceRange(qsizetype Offset, qsizetype Removed, const QByteArray& Added)
{
    if (readOnly_ || Offset < 0 || Removed < 0 || Offset > bytes_.size() || Removed > bytes_.size() - Offset) return false;
    if (Added.size() > MaximumBytes - (bytes_.size() - Removed)) { emit editRejected(tr("이진 데이터가 편집 한도를 초과합니다.")); return false; }
    const auto Before = bytes_.mid(Offset, Removed);
    if (Before == Added) { moveCursor(Offset + Added.size(), false); return true; }
    // Bound draft undo storage independently of the persistent registry journal.
    const auto Cost = Removed + Added.size();
    if (undoBytes_ + Cost > MaximumBytes * 2) { undo_->clear(); undoBytes_ = 0; }
    undoBytes_ += Cost; undo_->push(new HexPatchCommand(this, Offset, Before, Added)); return true;
}
bool HexBufferEditor::replaceSelection(const QByteArray& Bytes) { return replaceRange(selectionOffset(), selectionLength(), Bytes); }
void HexBufferEditor::undo() { if (!readOnly_) undo_->undo(); }
void HexBufferEditor::redo() { if (!readOnly_) undo_->redo(); }
void HexBufferEditor::copy()
{
    const auto Count = selectionLength() ? selectionLength() : qMin<qsizetype>(1, bytes_.size() - cursor_);
    QApplication::clipboard()->setText(QString::fromLatin1(bytes_.mid(selectionOffset(), Count).toHex(' ').toUpper()));
}
void HexBufferEditor::paste()
{
    const auto Text = QApplication::clipboard()->text();
    if (Text.size() > MaximumBytes * 3) { emit editRejected(tr("붙여넣을 데이터가 너무 큽니다.")); return; }
    QByteArray Parsed; int High = -1;
    for (const auto Character : Text) {
        if (Character.isSpace()) continue;
        const auto C = Character.toLower().unicode();
        const int Digit = C >= '0' && C <= '9' ? C - '0' : C >= 'a' && C <= 'f' ? C - 'a' + 10 : -1;
        if (Digit < 0) { emit editRejected(tr("공백으로 구분한 16진 바이트를 입력하세요.")); return; }
        if (High < 0) High = Digit; else { Parsed.append(char((High << 4) | Digit)); High = -1; }
    }
    if (High >= 0) { emit editRejected(tr("16진 숫자는 바이트마다 두 자리여야 합니다.")); return; }
    if (!Parsed.isEmpty()) replaceSelection(Parsed);
}
qsizetype HexBufferEditor::hit(const QPoint& Position, bool* Ascii) const
{
    const int X = Position.x() + horizontalScrollBar()->value(); const bool Text = X >= 61 * cell_;
    if (Ascii) *Ascii = Text;
    const int Column = qBound(0, Text ? X / cell_ - 61 : (X / cell_ - 11) / 3, 15);
    const qsizetype Row = qMax(0, Position.y() / rowHeight_ + verticalScrollBar()->value());
    return qMin(bytes_.size(), Row * 16 + Column);
}
void HexBufferEditor::mousePressEvent(QMouseEvent* Event)
{
    if (Event->button() != Qt::LeftButton) { QAbstractScrollArea::mousePressEvent(Event); return; }
    setFocus(Qt::MouseFocusReason); moveCursor(hit(Event->position().toPoint(), &ascii_), Event->modifiers().testFlag(Qt::ShiftModifier)); Event->accept();
}
void HexBufferEditor::mouseMoveEvent(QMouseEvent* Event)
{
    if (Event->buttons().testFlag(Qt::LeftButton)) { moveCursor(hit(Event->position().toPoint()), true); Event->accept(); return; }
    QAbstractScrollArea::mouseMoveEvent(Event);
}
void HexBufferEditor::keyPressEvent(QKeyEvent* Event)
{
    const bool Extend = Event->modifiers().testFlag(Qt::ShiftModifier);
    if (Event->matches(QKeySequence::Copy)) { copy(); Event->accept(); return; }
    if (Event->matches(QKeySequence::SelectAll)) { setSelection(0, bytes_.size()); Event->accept(); return; }
    if (Event->matches(QKeySequence::Undo)) { undo(); Event->accept(); return; }
    if (Event->matches(QKeySequence::Redo)) { redo(); Event->accept(); return; }
    if (Event->matches(QKeySequence::Paste)) { if (!readOnly_) paste(); Event->accept(); return; }
    if (Event->matches(QKeySequence::Cut)) { if (!readOnly_) { copy(); if (selectionLength()) replaceSelection({}); else if (cursor_ < bytes_.size()) replaceRange(cursor_, 1, {}); } Event->accept(); return; }
    switch (Event->key()) {
    case Qt::Key_Left: moveCursor(cursor_ - 1, Extend); return;
    case Qt::Key_Right: moveCursor(cursor_ + 1, Extend); return;
    case Qt::Key_Up: moveCursor(cursor_ - 16, Extend); return;
    case Qt::Key_Down: moveCursor(cursor_ + 16, Extend); return;
    case Qt::Key_PageUp: moveCursor(cursor_ - verticalScrollBar()->pageStep() * 16, Extend); return;
    case Qt::Key_PageDown: moveCursor(cursor_ + verticalScrollBar()->pageStep() * 16, Extend); return;
    case Qt::Key_Home: moveCursor(Event->modifiers().testFlag(Qt::ControlModifier) ? 0 : cursor_ / 16 * 16, Extend); return;
    case Qt::Key_End: moveCursor(Event->modifiers().testFlag(Qt::ControlModifier) ? bytes_.size() : cursor_ / 16 * 16 + 15, Extend); return;
    default: break;
    }
    if (readOnly_) { QAbstractScrollArea::keyPressEvent(Event); return; }
    if (Event->key() == Qt::Key_Delete || Event->key() == Qt::Key_Backspace) {
        if (selectionLength()) replaceSelection({});
        else if (Event->key() == Qt::Key_Backspace && cursor_) replaceRange(cursor_ - 1, 1, {});
        else if (Event->key() == Qt::Key_Delete && cursor_ < bytes_.size()) replaceRange(cursor_, 1, {});
        Event->accept(); return;
    }
    if (Event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) { QAbstractScrollArea::keyPressEvent(Event); return; }
    if (Event->text().size() == 1) {
        const auto Character = Event->text()[0];
        const auto Start = selectionOffset(); const auto Removed = selectionLength() ? selectionLength() : qMin<qsizetype>(1, bytes_.size() - Start);
        if (ascii_ && Character.unicode() >= 32 && Character.unicode() <= 255) {
            replaceRange(Start, Removed, QByteArray(1, char(Character.unicode()))); Event->accept(); return;
        }
        const auto C = Character.toLower().unicode();
        const int Digit = C >= '0' && C <= '9' ? C - '0' : C >= 'a' && C <= 'f' ? C - 'a' + 10 : -1;
        if (!ascii_ && Digit >= 0) {
            const bool Low = lowNibble_ && !selectionLength();
            const int Before = Start < bytes_.size() ? quint8(bytes_[Start]) : 0;
            const char Byte = char(Low ? (Before & 0xf0) | Digit : (Digit << 4) | (Before & 0x0f));
            if (replaceRange(Start, Removed, QByteArray(1, Byte)) && !Low) { moveCursor(Start, false); lowNibble_ = true; }
            Event->accept(); return;
        }
    }
    QAbstractScrollArea::keyPressEvent(Event);
}
}