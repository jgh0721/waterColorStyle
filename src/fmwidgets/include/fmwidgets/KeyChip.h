#pragma once

#include <QWidget>

namespace fm::ui {

/// 키 칩(목업 .kbd) — mono 11 px, 높이 18, 좌우 5. 단추 · 라디오 안의 칩은 동적 속성 fmKeyHint로
/// 스타일이 그리고(fm::style::setKeyHint), 이 위젯은 글 사이에 따로 놓는 칩에 쓴다.
class KeyChip : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString keys READ keys WRITE setKeys)

public:
    explicit KeyChip(QWidget *parent = nullptr);
    explicit KeyChip(const QString &keys, QWidget *parent = nullptr);

    QString keys() const { return m_keys; }
    void setKeys(const QString &keys);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_keys;
};

} // namespace fm::ui
