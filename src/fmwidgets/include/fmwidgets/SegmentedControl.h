#pragma once

#include <QStringList>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QHBoxLayout;

namespace fm::ui {

/// 이어 붙인 버튼 가운데 하나를 고르는 세그먼트 컨트롤 (목업의 1줄 · 2줄 · 자동 · 섬네일).
class SegmentedControl : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList items READ items WRITE setItems)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)

public:
    explicit SegmentedControl(QWidget *parent = nullptr);
    explicit SegmentedControl(const QStringList &items, QWidget *parent = nullptr);

    QStringList items() const { return m_items; }
    void setItems(const QStringList &items);

    int currentIndex() const noexcept { return m_current; }
    void setCurrentIndex(int index);

    int count() const noexcept { return int(m_items.size()); }
    QAbstractButton *button(int index) const;

Q_SIGNALS:
    void currentIndexChanged(int index);

protected:
    void changeEvent(QEvent *event) override;

private:
    void rebuild();

    QStringList m_items;
    int m_current = -1;
    QHBoxLayout *m_layout = nullptr;
    QButtonGroup *m_group = nullptr;
};

} // namespace fm::ui
