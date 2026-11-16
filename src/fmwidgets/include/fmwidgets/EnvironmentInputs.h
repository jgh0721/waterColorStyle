#pragma once
#include <fmwidgets/ValueInputs.h>
#include <QStringList>

class QCompleter;
class QStringListModel;
class QHBoxLayout;
namespace fm::ui {
class Button;
class ExpandableValueEdit final : public TextField
{
    Q_OBJECT
public:
    explicit ExpandableValueEdit(QWidget* Parent = nullptr);
    void setVariableNames(const QStringList& Names);
signals:
    void expansionRequested(const QString& Raw);
private:
    QCompleter* completion_;
    QStringListModel* names_;
};
class MaskedValueEdit final : public QWidget
{
    Q_OBJECT
public:
    explicit MaskedValueEdit(QWidget* Parent = nullptr);
    void setText(const QString& Text);
    QString text() const;
    void setReadOnly(bool ReadOnly);
    bool revealed() const;
    TextField* editor() const { return editor_; }
signals:
    void textChanged(const QString& Text);
protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;
private:
    void hideValue();
    TextField* editor_;
    Button* reveal_;
};
class OrderedTokenEdit final : public QWidget
{
    Q_OBJECT
public:
    explicit OrderedTokenEdit(QWidget* Parent = nullptr);
    void setValue(const QString& Value);
    QString value() const;
    int currentIndex() const { return current_; }
    bool moveCurrent(int Delta);
signals:
    void valueChanged(const QString& Value);
protected:
    void keyPressEvent(QKeyEvent* Event) override;
private:
    void rebuild();
    QStringList tokens_;
    int current_ = -1;
    QHBoxLayout* chips_;
    TextField* input_;
};
}
