#pragma once

#include <QLayout>
#include <QList>

namespace fm::ui {

/// 흐름 배치 — 웹 문서의 글줄처럼(인라인 흐름 · flex-wrap: wrap) 항목을 한 줄에 차례로 놓다가 폭이 모자라면 다음 줄로.
///
/// - 항목은 sizeHint 크기(최소 · 최대로 제한). 폭에 따라 높이가 바뀌는 항목(heightForWidth)도 그 폭의 높이로 놓는다.
/// - 줄 정렬: Qt::AlignLeft(앞) · AlignHCenter · AlignRight(뒤) · AlignJustify(남는 폭을 항목 사이에 나눔, 마지막 줄은 앞).
///   줄 안 세로 정렬: AlignTop · AlignVCenter · AlignBottom.
/// - 높이는 폭으로 정해진다(hasHeightForWidth). sizeHint = 한 줄에 다 놓은 크기(max-content),
///   minimumSize = 가장 넓은 항목 하나(min-content).
/// - 간격 -1이면 스타일 값(PM_LayoutHorizontalSpacing · Vertical, 없으면 위젯 종류별 layoutSpacing).
/// - 숨긴 위젯은 자리를 차지하지 않고, 오른쪽에서 왼쪽 배치(layoutDirection)면 줄이 오른쪽부터 찬다.
class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent = nullptr);
    FlowLayout(int horizontalSpacing, int verticalSpacing, QWidget *parent = nullptr);
    ~FlowLayout() override;

    /// 위젯을 index 자리에 넣는다(범위 밖이면 끝).
    void insertWidget(int index, QWidget *widget);
    /// 항목을 index 자리에 넣는다(takeAt으로 뺀 항목을 옮길 때).
    void insertItem(int index, QLayoutItem *item);

    int horizontalSpacing() const;
    void setHorizontalSpacing(int spacing);
    int verticalSpacing() const;
    void setVerticalSpacing(int spacing);
    /// 가로 · 세로 간격을 같이(QLayout::setSpacing과 같다).
    void setSpacing(int spacing) override;
    int spacing() const override;

    Qt::Alignment lineAlignment() const noexcept { return m_lineAlignment; }
    void setLineAlignment(Qt::Alignment alignment);

    void addItem(QLayoutItem *item) override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void setGeometry(const QRect &rect) override;
    void invalidate() override;

private:
    /// rect 안에 놓는다(apply면 항목 위치를 정한다). 쓴 높이(여백 포함)를 돌려준다.
    int layoutItems(const QRect &rect, bool apply) const;
    int spacingFor(const QLayoutItem *item, Qt::Orientation orientation) const;

    QList<QLayoutItem *> m_items;
    int m_hSpacing = -1;
    int m_vSpacing = -1;
    Qt::Alignment m_lineAlignment = Qt::AlignLeft | Qt::AlignTop;
    mutable int m_cachedWidth = -1;
    mutable int m_cachedHeight = -1;
};

} // namespace fm::ui
