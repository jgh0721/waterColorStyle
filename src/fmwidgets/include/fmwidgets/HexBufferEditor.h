#pragma once
#include <QAbstractScrollArea>
#include <QByteArray>
class QUndoStack;
namespace fm::ui {
class HexPatchCommand;
class HexBufferEditor : public QAbstractScrollArea
{
    Q_OBJECT
public:
    static constexpr qsizetype MaximumBytes = 64 * 1024 * 1024;
    explicit HexBufferEditor(QWidget* Parent = nullptr);
    bool setData(const QByteArray& Data);
    const QByteArray& data() const { return bytes_; }
    void setReadOnly(bool ReadOnly) { readOnly_ = ReadOnly; viewport()->update(); }
    bool isReadOnly() const { return readOnly_; }
    qsizetype cursorOffset() const { return cursor_; }
    void setCursorOffset(qsizetype Offset);
    void setSelection(qsizetype Offset, qsizetype Length);
    qsizetype selectionOffset() const { return qMin(cursor_, anchor_); }
    qsizetype selectionLength() const { return qAbs(cursor_ - anchor_); }
    bool replaceSelection(const QByteArray& Bytes);
    void undo();
    void redo();
signals:
    void dataChanged();
    void cursorChanged(qsizetype Offset);
    void editRejected(const QString& Reason);
protected:
    void paintEvent(QPaintEvent* Event) override;
    void resizeEvent(QResizeEvent* Event) override;
    void keyPressEvent(QKeyEvent* Event) override;
    void mousePressEvent(QMouseEvent* Event) override;
    void mouseMoveEvent(QMouseEvent* Event) override;
private:
    friend class HexPatchCommand;
    void applyChange(qsizetype Offset, qsizetype Removed, const QByteArray& Added);
    bool replaceRange(qsizetype Offset, qsizetype Removed, const QByteArray& Added);
    void moveCursor(qsizetype Offset, bool Extend);
    void updateMetrics();
    qsizetype hit(const QPoint& Position, bool* Ascii = nullptr) const;
    void copy();
    void paste();
    QByteArray bytes_;
    QUndoStack* undo_;
    qsizetype cursor_ = 0, anchor_ = 0, undoBytes_ = 0;
    bool readOnly_ = false, ascii_ = false, lowNibble_ = false;
    int cell_ = 9, rowHeight_ = 22;
};
}