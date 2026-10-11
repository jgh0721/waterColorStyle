#pragma once

#include "fmwidgets/FlexLayout.h"
#include "fmwidgets/FlowLayout.h"

#include <QPointer>
#include <QStringList>
#include <QWidget>

namespace fm::ui {

/// 배치 상자의 공통 바탕 — FlowLayout · FlexLayout을 Qt Widgets Designer에서 쓰려고 레이아웃을 품은 위젯.
///
/// Designer는 사용자 QLayout을 만들거나 고를 수 없다(레이아웃 도구 모음은 가로 · 세로 · 격자 · 양식뿐). 그래서 상자
/// 위젯이 자기 배치를 갖고, 직접 자식 위젯을 스스로 그 배치에 넣는다 — Designer에서 끌어 넣은 위젯도, uic가 만든 자식도.
/// - 차례: 넣은 차례. Designer에서 자식을 다른 자식 위로 끌어 놓으면 그 자리로 옮기고, 차례는 itemOrder(자식
///   objectName 목록)로 .ui에 남는다. itemOrder는 자식보다 먼저 정해져도(uic · .ui) 자식이 들어올 때 따른다.
/// - Designer에서 이 상자에 Designer 레이아웃(가로 · 세로 · 격자)을 걸지 않는다 — 상자의 배치가 따로 있다.
class LayoutBox : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList itemOrder READ itemOrder WRITE setItemOrder NOTIFY itemOrderChanged)
    Q_PROPERTY(int margin READ margin WRITE setMargin)
    Q_PROPERTY(bool designMode READ isDesignMode WRITE setDesignMode DESIGNABLE false STORED false)

public:
    /// 지금 배치 차례(이름 있는 자식의 objectName).
    QStringList itemOrder() const;
    void setItemOrder(const QStringList &names);
    /// 안쪽 여백(네 변 같음).
    int margin() const;
    void setMargin(int margin);

    /// Designer 안 — 자식을 끌어 옮기면 차례를 바꾼다(플러그인이 켠다).
    bool isDesignMode() const noexcept { return m_designMode; }
    void setDesignMode(bool on);

    /// 직접 자식 중 아직 배치에 없는 위젯을 넣는다(보통은 스스로 부른다).
    void syncChildren();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    /// 사용자가 Designer에서 차례를 바꿨다.
    void itemOrderChanged(const QStringList &order);

protected:
    explicit LayoutBox(QWidget *parent);

    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    /// Designer 안에서만 — 점선 테두리(비었으면 안내 글)로 상자가 보이게.
    void paintEvent(QPaintEvent *event) override;

    /// 상자마다 — 배치에 넣기 · 옮기기, 자식 동적 속성 반영.
    virtual void insertIntoLayout(int index, QWidget *widget) = 0;
    virtual void moveInLayout(int from, int to) = 0;
    virtual void applyChildProperties(QWidget *) {}

    int indexOfWidget(const QWidget *widget) const;

private:
    void adopt(QWidget *child);
    void scheduleSettle();
    void settle();
    void reorderTo(const QStringList &names);

    QStringList m_order;
    bool m_designMode = false;
    bool m_settlePending = false;
    QPointer<QWidget> m_moved;  // 마지막으로 옮겨진 자식(Designer)
};

/// 흐름 상자 — FlowLayout(인라인 흐름: 줄이 차면 다음 줄)을 품은 위젯. 태그 · 칩 · 필터 단추 묶음.
class FlowBox : public LayoutBox
{
    Q_OBJECT
    Q_PROPERTY(int horizontalSpacing READ horizontalSpacing WRITE setHorizontalSpacing)
    Q_PROPERTY(int verticalSpacing READ verticalSpacing WRITE setVerticalSpacing)
    Q_PROPERTY(Qt::Alignment lineAlignment READ lineAlignment WRITE setLineAlignment)

public:
    explicit FlowBox(QWidget *parent = nullptr);

    FlowLayout *flowLayout() const noexcept { return m_layout; }
    int horizontalSpacing() const { return m_layout->horizontalSpacing(); }
    void setHorizontalSpacing(int spacing) { m_layout->setHorizontalSpacing(spacing); }
    int verticalSpacing() const { return m_layout->verticalSpacing(); }
    void setVerticalSpacing(int spacing) { m_layout->setVerticalSpacing(spacing); }
    Qt::Alignment lineAlignment() const { return m_layout->lineAlignment(); }
    void setLineAlignment(Qt::Alignment alignment) { m_layout->setLineAlignment(alignment); }

protected:
    void insertIntoLayout(int index, QWidget *widget) override;
    void moveInLayout(int from, int to) override;

private:
    FlowLayout *m_layout;
};

/// 유연 상자 — FlexLayout(CSS flexbox)을 품은 위젯. 항목별 값은 자식 위젯의 동적 속성으로 준다(Designer 속성 창의 +):
/// flexGrow · flexShrink(실수), flexBasis · flexOrder(정수), flexAlignSelf("auto" · "start" · "end" · "center" · "stretch").
class FlexBox : public LayoutBox
{
    Q_OBJECT
    Q_PROPERTY(Direction direction READ direction WRITE setDirection)
    Q_PROPERTY(WrapMode wrap READ wrap WRITE setWrap)
    Q_PROPERTY(Justify justifyContent READ justifyContent WRITE setJustifyContent)
    Q_PROPERTY(ItemsAlign alignItems READ alignItems WRITE setAlignItems)
    Q_PROPERTY(ContentAlign alignContent READ alignContent WRITE setAlignContent)
    Q_PROPERTY(int rowGap READ rowGap WRITE setRowGap)
    Q_PROPERTY(int columnGap READ columnGap WRITE setColumnGap)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum — 값 차례는 FlexLayout과 같다.
    enum Direction { Row, RowReverse, Column, ColumnReverse };
    Q_ENUM(Direction)
    enum WrapMode { NoWrap, Wrap, WrapReverse };
    Q_ENUM(WrapMode)
    enum Justify { JustifyStart, JustifyEnd, JustifyCenter, SpaceBetween, SpaceAround, SpaceEvenly };
    Q_ENUM(Justify)
    enum ItemsAlign { ItemsStart = 1, ItemsEnd, ItemsCenter, ItemsStretch };
    Q_ENUM(ItemsAlign)
    enum ContentAlign { ContentStart, ContentEnd, ContentCenter, ContentSpaceBetween, ContentSpaceAround,
                        ContentSpaceEvenly, ContentStretch };
    Q_ENUM(ContentAlign)

    /// 자식 동적 속성 이름.
    static constexpr char kGrow[] = "flexGrow";
    static constexpr char kShrink[] = "flexShrink";
    static constexpr char kBasis[] = "flexBasis";
    static constexpr char kAlignSelf[] = "flexAlignSelf";
    static constexpr char kOrder[] = "flexOrder";

    explicit FlexBox(QWidget *parent = nullptr);

    FlexLayout *flexLayout() const noexcept { return m_layout; }
    Direction direction() const { return Direction(m_layout->direction()); }
    void setDirection(Direction direction) { m_layout->setDirection(FlexLayout::Direction(direction)); }
    WrapMode wrap() const { return WrapMode(m_layout->wrap()); }
    void setWrap(WrapMode wrap) { m_layout->setWrap(FlexLayout::Wrap(wrap)); }
    Justify justifyContent() const { return Justify(m_layout->justifyContent()); }
    void setJustifyContent(Justify justify) { m_layout->setJustifyContent(FlexLayout::Justify(justify)); }
    ItemsAlign alignItems() const { return ItemsAlign(m_layout->alignItems()); }
    void setAlignItems(ItemsAlign align) { m_layout->setAlignItems(FlexLayout::Align(align)); }
    ContentAlign alignContent() const { return ContentAlign(m_layout->alignContent()); }
    void setAlignContent(ContentAlign align) { m_layout->setAlignContent(FlexLayout::AlignContent(align)); }
    int rowGap() const { return m_layout->rowGap(); }
    void setRowGap(int gap) { m_layout->setRowGap(gap); }
    int columnGap() const { return m_layout->columnGap(); }
    void setColumnGap(int gap) { m_layout->setColumnGap(gap); }

    /// 자식의 동적 속성에서 읽은 flex 값.
    static FlexLayout::Item flexItemOf(const QWidget *widget);

protected:
    void insertIntoLayout(int index, QWidget *widget) override;
    void moveInLayout(int from, int to) override;
    void applyChildProperties(QWidget *widget) override;

private:
    FlexLayout *m_layout;
};

} // namespace fm::ui
