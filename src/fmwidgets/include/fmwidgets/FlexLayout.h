#pragma once

#include <QLayout>
#include <QList>

#include <cstdint>

namespace fm::ui {

/// 유연 상자 배치 — 웹의 CSS flexbox(display: flex)를 Qt 배치로. 상자 모형(QBoxLayout)에 줄 바꿈 · 주 축 분배 ·
/// 교차 축 정렬 · 항목별 늘이기 · 줄이기 · 기준 크기 · 순서를 더했다.
///
/// | CSS | FlexLayout |
/// |---|---|
/// | flex-direction | setDirection(Row · RowReverse · Column · ColumnReverse) |
/// | flex-wrap | setWrap(NoWrap · Wrap · WrapReverse) |
/// | justify-content | setJustifyContent(Start · End · Center · SpaceBetween · SpaceAround · SpaceEvenly) |
/// | align-items | setAlignItems(Start · End · Center · Stretch) |
/// | align-content | setAlignContent(Start · End · Center · SpaceBetween · SpaceAround · SpaceEvenly · Stretch) |
/// | row-gap · column-gap · gap | setRowGap · setColumnGap · setGap (-1 = 스타일 간격) |
/// | flex-grow · flex-shrink · flex-basis · align-self · order | Item(grow · shrink · basis · alignSelf · order) |
///
/// - 항목의 기준 크기는 basis(-1이면 sizeHint의 주 축 크기), 최소 · 최대 크기로 제한한다(min-width: auto처럼
///   minimumSize 아래로는 줄지 않는다). 유연 길이는 CSS 9.7절처럼 제한을 어긴 항목을 고정하며 되풀이해 나눈다.
/// - 가로 방향은 폭으로 높이를 정한다(hasHeightForWidth — 줄 바꿈, 폭에 따라 높이가 바뀌는 항목). 세로 방향의 줄 바꿈은
///   높이가 정해졌을 때(setGeometry)만 일어난다.
/// - sizeHint = 줄 바꿈 없이 한 줄(max-content), minimumSize = 줄 바꿈이면 가장 큰 항목, 아니면 줄일 수 있는 만큼 합.
/// - 숨긴 위젯은 자리를 차지하지 않는다. 오른쪽에서 왼쪽 배치면 가로를 뒤집는다(Row는 오른쪽부터 — CSS와 같다).
class FlexLayout : public QLayout
{
public:
    enum class Direction : std::uint8_t { Row, RowReverse, Column, ColumnReverse };
    enum class Wrap : std::uint8_t { NoWrap, Wrap, WrapReverse };
    enum class Justify : std::uint8_t { Start, End, Center, SpaceBetween, SpaceAround, SpaceEvenly };
    /// Auto는 alignSelf에서만 — 상자의 alignItems를 따른다.
    enum class Align : std::uint8_t { Auto, Start, End, Center, Stretch };
    enum class AlignContent : std::uint8_t { Start, End, Center, SpaceBetween, SpaceAround, SpaceEvenly, Stretch };

    /// 항목 하나의 flex 값(CSS flex-grow · flex-shrink · flex-basis · align-self · order).
    struct Item
    {
        qreal grow = 0;
        qreal shrink = 1;
        int basis = -1;  // -1 = auto(sizeHint의 주 축 크기)
        Align alignSelf = Align::Auto;
        int order = 0;

        bool operator==(const Item &) const = default;
    };

    explicit FlexLayout(QWidget *parent = nullptr);
    explicit FlexLayout(Direction direction, QWidget *parent = nullptr);
    ~FlexLayout() override;

    Direction direction() const noexcept { return m_direction; }
    void setDirection(Direction direction);
    Wrap wrap() const noexcept { return m_wrap; }
    void setWrap(Wrap wrap);
    Justify justifyContent() const noexcept { return m_justify; }
    void setJustifyContent(Justify justify);
    Align alignItems() const noexcept { return m_alignItems; }
    /// Auto는 Stretch로 받는다(CSS normal).
    void setAlignItems(Align value);
    AlignContent alignContent() const noexcept { return m_alignContent; }
    void setAlignContent(AlignContent value);
    int rowGap() const noexcept { return m_rowGap; }
    void setRowGap(int gap);
    int columnGap() const noexcept { return m_columnGap; }
    void setColumnGap(int gap);
    void setGap(int gap);
    void setSpacing(int spacing) override { setGap(spacing); }
    int spacing() const override;

    using QLayout::addWidget;
    void addWidget(QWidget *widget, const Item &item);
    /// flex: grow shrink basis 줄임꼴.
    void addWidget(QWidget *widget, qreal grow, qreal shrink = 1, int basis = -1);
    void insertWidget(int index, QWidget *widget, const Item &item = {});
    /// 항목을 index 자리에 넣는다(takeAt으로 뺀 항목을 옮길 때 — flex 값은 다시 준다).
    void insertItem(int index, QLayoutItem *item, const Item &flex = {});
    /// 넣은 위젯의 flex 값(없으면 기본값).
    Item flexItem(const QWidget *widget) const;
    bool setFlexItem(const QWidget *widget, const Item &item);
    bool setGrow(const QWidget *widget, qreal grow);
    bool setShrink(const QWidget *widget, qreal shrink);
    bool setBasis(const QWidget *widget, int basis);
    bool setAlignSelf(const QWidget *widget, Align value);
    bool setOrder(const QWidget *widget, int order);

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
    struct Entry
    {
        QLayoutItem *item;
        Item flex;
    };
    struct Placement
    {
        QLayoutItem *item;
        QRect rect;
    };

    bool horizontal() const noexcept { return m_direction == Direction::Row || m_direction == Direction::RowReverse; }
    int mainGap() const;
    int crossGap() const;
    int resolvedGap(int gap, Qt::Orientation orientation) const;
    Entry *entryFor(const QWidget *widget);
    /// area 안에 놓는다. availableCross < 0이면 교차 축 크기가 정해지지 않은 것(높이 구하기). 쓴 교차 축 크기를 돌려준다.
    int arrange(const QRect &area, int availableCross, QList<Placement> *out) const;

    QList<Entry> m_entries;
    Direction m_direction = Direction::Row;
    Wrap m_wrap = Wrap::NoWrap;
    Justify m_justify = Justify::Start;
    Align m_alignItems = Align::Stretch;
    AlignContent m_alignContent = AlignContent::Stretch;
    int m_rowGap = -1;
    int m_columnGap = -1;
    mutable int m_cachedWidth = -1;
    mutable int m_cachedHeight = -1;
};

} // namespace fm::ui
