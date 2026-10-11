#pragma once

#include <QPushButton>
#include <QWidget>

class QHBoxLayout;

namespace fm::ui {

/// 기능 키 칸 하나 — 시안1: 평면 칸 + 키 칩(높이 18 · 11 px 고정폭) + 간격 8 + 글자, 마우스 올림 --accent-soft.
/// 시안2: 입체 누름 단추 28 + 굵은 키 + 글자(칩 없음).
class FunctionKeyButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(QString keys READ keys WRITE setKeys)

public:
    /// uic · Designer용 — 키 · 글은 keys · text 속성으로.
    explicit FunctionKeyButton(QWidget *parent = nullptr);
    explicit FunctionKeyButton(const QString &keys, const QString &text, QWidget *parent = nullptr);

    QString keys() const { return m_keys; }
    void setKeys(const QString &keys);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_keys;
};

/// 기능 키 막대(01 §1.3 A6 · 06 §4.15) — 같은 폭 칸 9개.
/// 시안1: 32 · 위 1 px --line · 칸 사이 1 px --line. 시안2: 36 · 간격 4 · 안쪽 2 4 6.
class FunctionKeyBar : public QWidget
{
    Q_OBJECT
public:
    explicit FunctionKeyBar(QWidget *parent = nullptr);

    /// 칸을 더한다. 누르면 triggered(keys)와 칸의 clicked가 나온다.
    FunctionKeyButton *addKey(const QString &keys, const QString &text);
    QList<FunctionKeyButton *> buttons() const { return m_buttons; }

    QSize sizeHint() const override;

Q_SIGNALS:
    void triggered(const QString &keys);

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyMetrics();

    QHBoxLayout *m_layout = nullptr;
    QList<FunctionKeyButton *> m_buttons;
};

} // namespace fm::ui
