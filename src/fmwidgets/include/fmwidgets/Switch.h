#pragma once

#include <QCheckBox>

namespace fm::ui {

/// 켬 / 끔 스위치 (시안1 40 × 20, 시안2 52 × 22). QCheckBox라서 toggled · checkState 등을 그대로 쓴다.
/// onText · offText를 둘 다 주면 상태에 따라 글자가 바뀐다 (목업의 '켬' · '끔').
class Switch : public QCheckBox
{
    Q_OBJECT
    Q_PROPERTY(QString onText READ onText WRITE setOnText)
    Q_PROPERTY(QString offText READ offText WRITE setOffText)

public:
    explicit Switch(QWidget *parent = nullptr);
    explicit Switch(const QString &text, QWidget *parent = nullptr);

    QString onText() const { return m_onText; }
    void setOnText(const QString &text);
    QString offText() const { return m_offText; }
    void setOffText(const QString &text);

protected:
    /// setChecked()가 신호를 막은 채 불려도(설정 창 동기화) 글자가 따라가게.
    void checkStateSet() override;

private:
    void syncText();

    QString m_onText;
    QString m_offText;
};

} // namespace fm::ui
