// 웹 배치 테스트 — 흐름 배치(FlowLayout: 줄 바꿈 · 줄 정렬 · 양쪽 맞춤 · 오른쪽에서 왼쪽 · 높이)와
// 유연 상자(FlexLayout: grow · shrink · 최소 · 최대 고정, justify-content 여섯, align-items · align-self, 줄 바꿈 ·
// 거꾸로 줄 바꿈, row-reverse · column, order, align-content, 오른쪽에서 왼쪽, sizeHint · minimumSize · heightForWidth).

#include <fmwidgets/FlexLayout.h>
#include <fmwidgets/FlowLayout.h>
#include <fmwidgets/LayoutBoxes.h>

#include <QApplication>
#include <QLabel>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;
using fm::ui::FlexLayout;
using fm::ui::FlowLayout;

namespace {

/// 기준 크기(sizeHint)와 최소 크기가 정해진 상자.
class Box : public QWidget
{
public:
    explicit Box(QSize hint, QSize min = {}, QSize max = {}, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_hint(hint)
        , m_min(min.isValid() ? min : QSize(0, 0))
    {
        if (max.isValid())
            setMaximumSize(max);
    }
    QSize sizeHint() const override { return m_hint; }
    QSize minimumSizeHint() const override { return m_min; }

private:
    QSize m_hint;
    QSize m_min;
};

/// 상자 n개를 담은 컨테이너 — 배치는 setGeometry로 직접 부른다(창을 띄우지 않는다).
template <typename Layout>
struct Fixture
{
    QWidget host;
    Layout *layout = new Layout(&host);
    QList<Box *> boxes;

    Fixture() { layout->setContentsMargins(0, 0, 0, 0); }

    Box *add(QSize hint, QSize min = {}, QSize max = {})
    {
        auto *box = new Box(hint, min, max);
        boxes.append(box);
        layout->addWidget(box);
        return box;
    }
    void place(int width, int height) { layout->setGeometry(QRect(0, 0, width, height)); }
    QRect at(int i) const { return boxes.at(i)->geometry(); }
};

} // namespace

class TestLayouts : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ------------------------------------------------------------------ 흐름 배치

    void flowWraps()
    {
        Fixture<FlowLayout> f;
        f.layout->setSpacing(10);
        for (int i = 0; i < 5; ++i)
            f.add(QSize(40, 20), QSize(40, 20));
        f.place(100, 200);
        QCOMPARE(f.at(0), QRect(0, 0, 40, 20));
        QCOMPARE(f.at(1), QRect(50, 0, 40, 20));
        QCOMPARE(f.at(2), QRect(0, 30, 40, 20));
        QCOMPARE(f.at(4), QRect(0, 60, 40, 20));
        QVERIFY(f.layout->hasHeightForWidth());
        QCOMPARE(f.layout->heightForWidth(100), 80);
        QCOMPARE(f.layout->heightForWidth(250), 20);  // 한 줄
        // max-content · min-content
        QCOMPARE(f.layout->sizeHint(), QSize(5 * 40 + 4 * 10, 20));
        QCOMPARE(f.layout->minimumSize(), QSize(40, 20));
    }

    void flowAlignment_data()
    {
        QTest::addColumn<int>("alignment");
        QTest::addColumn<int>("x0");
        QTest::addColumn<int>("x1");
        QTest::newRow("left") << int(Qt::AlignLeft) << 0 << 50;
        QTest::newRow("center") << int(Qt::AlignHCenter) << 5 << 55;
        QTest::newRow("right") << int(Qt::AlignRight) << 10 << 60;
        QTest::newRow("justify") << int(Qt::AlignJustify) << 0 << 60;
    }
    void flowAlignment()
    {
        QFETCH(int, alignment);
        QFETCH(int, x0);
        QFETCH(int, x1);
        Fixture<FlowLayout> f;
        f.layout->setSpacing(10);
        for (int i = 0; i < 3; ++i)
            f.add(QSize(40, 20), QSize(40, 20));
        f.layout->setLineAlignment(Qt::Alignment(alignment) | Qt::AlignTop);
        f.place(100, 100);
        QCOMPARE(f.at(0).x(), x0);
        QCOMPARE(f.at(1).x(), x1);
        // 마지막 줄은 양쪽 맞춤에서도 앞쪽
        if (Qt::Alignment(alignment) == Qt::AlignJustify)
            QCOMPARE(f.at(2).x(), 0);
    }

    void flowVerticalAlignAndHidden()
    {
        Fixture<FlowLayout> f;
        f.layout->setSpacing(0);
        f.add(QSize(30, 40), QSize(30, 40));
        Box *hidden = f.add(QSize(30, 10), QSize(30, 10));
        f.add(QSize(30, 10), QSize(30, 10));
        hidden->setVisible(false);
        f.layout->setLineAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        f.place(200, 100);
        QCOMPARE(f.at(2), QRect(30, 15, 30, 10));  // 숨긴 상자 자리는 없다 · 줄 높이 40의 가운데
    }

    void flowRightToLeft()
    {
        Fixture<FlowLayout> f;
        f.layout->setSpacing(10);
        f.add(QSize(40, 20), QSize(40, 20));
        f.add(QSize(40, 20), QSize(40, 20));
        f.host.setLayoutDirection(Qt::RightToLeft);
        f.place(100, 100);
        QCOMPARE(f.at(0).x(), 60);
        QCOMPARE(f.at(1).x(), 10);
    }

    // ------------------------------------------------------------------ 유연 상자

    void flexGrow()
    {
        Fixture<FlexLayout> f;
        f.layout->setGap(0);
        f.add(QSize(50, 20));
        f.add(QSize(50, 20));
        f.add(QSize(50, 20));
        f.layout->setGrow(f.boxes[0], 1);
        f.layout->setGrow(f.boxes[1], 2);
        f.place(300, 40);
        QCOMPARE(f.at(0), QRect(0, 0, 100, 40));    // + 150 × 1/3, 교차 축은 늘림(stretch)
        QCOMPARE(f.at(1), QRect(100, 0, 150, 40));  // + 150 × 2/3
        QCOMPARE(f.at(2), QRect(250, 0, 50, 40));   // grow 0
        QCOMPARE(f.layout->expandingDirections(), Qt::Orientations(Qt::Horizontal));
    }

    void flexGrowFraction()
    {
        // grow 합이 1보다 작으면 남는 길이의 그 비율만 쓴다(flex-grow: 0.5)
        Fixture<FlexLayout> f;
        f.layout->setGap(0);
        f.add(QSize(100, 20));
        f.layout->setGrow(f.boxes[0], 0.5);
        f.place(300, 20);
        QCOMPARE(f.at(0).width(), 200);
    }

    void flexMaxClamp()
    {
        Fixture<FlexLayout> f;
        f.layout->setGap(0);
        f.add(QSize(50, 20), {}, QSize(70, 100));
        f.add(QSize(50, 20));
        f.layout->setGrow(f.boxes[0], 1);
        f.layout->setGrow(f.boxes[1], 1);
        f.place(300, 20);
        QCOMPARE(f.at(0).width(), 70);   // 최대에 걸려 고정
        QCOMPARE(f.at(1).width(), 230);  // 나머지를 다 받는다
    }

    void flexShrink()
    {
        // 모자란 60을 shrink × 기준(60 · 60 · 120)으로 나눈다
        Fixture<FlexLayout> f;
        f.layout->setGap(0);
        for (int i = 0; i < 3; ++i)
            f.add(QSize(60, 20));
        f.layout->setShrink(f.boxes[2], 2);
        f.place(120, 20);
        QCOMPARE(f.at(0).width(), 45);
        QCOMPARE(f.at(1).width(), 45);
        QCOMPARE(f.at(2).width(), 30);
    }

    void flexShrinkMinClamp()
    {
        // 최소 55에 걸린 항목을 고정하고 나머지가 더 줄어든다. shrink 0은 줄지 않는다.
        Fixture<FlexLayout> f;
        f.layout->setGap(0);
        f.add(QSize(60, 20), QSize(55, 0));
        f.add(QSize(60, 20));
        f.add(QSize(60, 20));
        f.layout->setShrink(f.boxes[2], 0);
        f.place(140, 20);  // 모자란 40 — 첫 판 40 · 40에서 첫째가 최소 55에 걸린다
        QCOMPARE(f.at(2).width(), 60);
        QCOMPARE(f.at(0).width(), 55);
        QCOMPARE(f.at(1).width(), 140 - 55 - 60);
    }

    void flexJustify_data()
    {
        QTest::addColumn<int>("justify");
        QTest::addColumn<QList<int>>("xs");
        using J = FlexLayout::Justify;
        QTest::newRow("start") << int(J::Start) << QList<int>{0, 50, 100};
        QTest::newRow("end") << int(J::End) << QList<int>{150, 200, 250};
        QTest::newRow("center") << int(J::Center) << QList<int>{75, 125, 175};
        QTest::newRow("space-between") << int(J::SpaceBetween) << QList<int>{0, 125, 250};
        QTest::newRow("space-around") << int(J::SpaceAround) << QList<int>{25, 125, 225};
        QTest::newRow("space-evenly") << int(J::SpaceEvenly) << QList<int>{38, 125, 213};
    }
    void flexJustify()
    {
        QFETCH(int, justify);
        QFETCH(QList<int>, xs);
        Fixture<FlexLayout> f;
        f.layout->setGap(0);
        for (int i = 0; i < 3; ++i)
            f.add(QSize(50, 20));
        f.layout->setJustifyContent(FlexLayout::Justify(justify));
        f.place(300, 20);
        for (int i = 0; i < 3; ++i)
            QVERIFY2(qAbs(f.at(i).x() - xs[i]) <= 1, qPrintable(u"%1: %2 ≠ %3"_s.arg(i).arg(f.at(i).x()).arg(xs[i])));
    }

    void flexAlignItems_data()
    {
        QTest::addColumn<int>("align");
        QTest::addColumn<QRect>("rect");
        using A = FlexLayout::Align;
        QTest::newRow("stretch") << int(A::Stretch) << QRect(0, 0, 50, 100);
        QTest::newRow("start") << int(A::Start) << QRect(0, 0, 50, 20);
        QTest::newRow("center") << int(A::Center) << QRect(0, 40, 50, 20);
        QTest::newRow("end") << int(A::End) << QRect(0, 80, 50, 20);
    }
    void flexAlignItems()
    {
        QFETCH(int, align);
        QFETCH(QRect, rect);
        Fixture<FlexLayout> f;
        f.add(QSize(50, 20));
        f.add(QSize(50, 20));
        f.layout->setAlignItems(FlexLayout::Align(align));
        f.layout->setAlignSelf(f.boxes[1], FlexLayout::Align::End);  // 항목별 덮어쓰기
        f.place(300, 100);
        QCOMPARE(f.at(0), rect);
        QCOMPARE(f.at(1).bottom(), 99);
        QCOMPARE(f.at(1).height(), 20);
    }

    void flexWrap()
    {
        Fixture<FlexLayout> f;
        f.layout->setWrap(FlexLayout::Wrap::Wrap);
        f.layout->setGap(10);
        f.layout->setAlignItems(FlexLayout::Align::Start);
        f.layout->setAlignContent(FlexLayout::AlignContent::Start);
        for (int i = 0; i < 5; ++i)
            f.add(QSize(40, 20));
        QVERIFY(f.layout->hasHeightForWidth());
        QCOMPARE(f.layout->heightForWidth(100), 3 * 20 + 2 * 10);
        f.place(100, 200);
        QCOMPARE(f.at(1), QRect(50, 0, 40, 20));
        QCOMPARE(f.at(2), QRect(0, 30, 40, 20));
        QCOMPARE(f.at(4), QRect(0, 60, 40, 20));
        // 거꾸로 줄 바꿈: 첫 줄이 아래
        f.layout->setWrap(FlexLayout::Wrap::WrapReverse);
        f.place(100, 80);
        QCOMPARE(f.at(0).y(), 60);
        QCOMPARE(f.at(4).y(), 0);
    }

    void flexAlignContent_data()
    {
        QTest::addColumn<int>("align");
        QTest::addColumn<QList<int>>("ys");
        QTest::addColumn<int>("height");
        using C = FlexLayout::AlignContent;
        QTest::newRow("stretch") << int(C::Stretch) << QList<int>{0, 70, 140} << 60;
        QTest::newRow("center") << int(C::Center) << QList<int>{60, 90, 120} << 20;
        QTest::newRow("space-between") << int(C::SpaceBetween) << QList<int>{0, 90, 180} << 20;
        QTest::newRow("end") << int(C::End) << QList<int>{120, 150, 180} << 20;
    }
    void flexAlignContent()
    {
        QFETCH(int, align);
        QFETCH(QList<int>, ys);
        QFETCH(int, height);
        Fixture<FlexLayout> f;
        f.layout->setWrap(FlexLayout::Wrap::Wrap);
        f.layout->setGap(10);
        f.layout->setAlignContent(FlexLayout::AlignContent(align));
        for (int i = 0; i < 6; ++i)
            f.add(QSize(40, 20));
        f.place(100, 200);  // 줄 셋(자연 높이 80) · 남는 120
        for (int line = 0; line < 3; ++line) {
            QCOMPARE(f.at(line * 2).y(), ys[line]);
            QCOMPARE(f.at(line * 2).height(), height);  // 줄 높이로 늘림(align-items 기본 stretch)
        }
    }

    void flexDirections()
    {
        Fixture<FlexLayout> f;
        f.layout->setGap(0);
        for (int i = 0; i < 3; ++i)
            f.add(QSize(50, 20));
        f.layout->setDirection(FlexLayout::Direction::RowReverse);
        f.place(300, 20);
        QCOMPARE(f.at(0).x(), 250);
        QCOMPARE(f.at(2).x(), 150);

        f.layout->setDirection(FlexLayout::Direction::Column);
        f.layout->setAlignItems(FlexLayout::Align::Center);
        f.layout->setGap(5);
        f.place(100, 300);
        QCOMPARE(f.at(0), QRect(25, 0, 50, 20));
        QCOMPARE(f.at(1), QRect(25, 25, 50, 20));
        QCOMPARE(f.layout->sizeHint(), QSize(50, 3 * 20 + 2 * 5));

        f.layout->setDirection(FlexLayout::Direction::ColumnReverse);
        f.place(100, 300);
        QCOMPARE(f.at(0).bottom(), 299);
    }

    void flexOrderAndHidden()
    {
        Fixture<FlexLayout> f;
        f.layout->setGap(0);
        for (int i = 0; i < 3; ++i)
            f.add(QSize(50, 20));
        f.layout->setOrder(f.boxes[2], -1);
        f.boxes[1]->setVisible(false);
        f.place(300, 20);
        QCOMPARE(f.at(2).x(), 0);   // order -1이 맨 앞
        QCOMPARE(f.at(0).x(), 50);  // 숨긴 상자는 자리가 없다
    }

    void flexRightToLeft()
    {
        Fixture<FlexLayout> f;
        f.layout->setGap(0);
        f.add(QSize(50, 20));
        f.add(QSize(50, 20));
        f.host.setLayoutDirection(Qt::RightToLeft);
        f.place(300, 20);
        QCOMPARE(f.at(0).x(), 250);
        QCOMPARE(f.at(1).x(), 200);
    }

    void flexSizes()
    {
        Fixture<FlexLayout> f;
        f.layout->setGap(10);
        f.layout->setContentsMargins(0, 0, 0, 0);
        f.add(QSize(50, 20), QSize(30, 10));
        f.add(QSize(60, 30), QSize(40, 10));
        QCOMPARE(f.layout->sizeHint(), QSize(50 + 10 + 60, 30));
        QCOMPARE(f.layout->minimumSize(), QSize(30 + 10 + 40, 10));
        f.layout->setWrap(FlexLayout::Wrap::Wrap);
        QCOMPARE(f.layout->minimumSize(), QSize(40, 10));  // 줄 바꿈이면 가장 큰 항목 하나
    }

    // ------------------------------------------------------------------ 배치 상자(Designer용)

    void boxAdoptsChildren()
    {
        // 자식을 부모만 주고 만들어도(uic · Designer) 상자가 스스로 배치에 넣는다 — 크기를 물으면 바로
        fm::ui::FlowBox box;
        box.flowLayout()->setSpacing(10);
        for (int i = 0; i < 3; ++i)
            new Box(QSize(40, 20), QSize(40, 20), {}, &box);
        QCOMPARE(box.sizeHint(), QSize(3 * 40 + 2 * 10, 20));
        QCOMPARE(box.flowLayout()->count(), 3);
        box.setFixedWidth(100);
        box.show();
        QVERIFY(QTest::qWaitForWindowExposed(&box));
        QCOMPARE(box.layout()->itemAt(2)->widget()->geometry().topLeft(), QPoint(0, 30));
    }

    void boxItemOrderBeforeChildren()
    {
        // .ui · uic는 상자 속성(itemOrder)을 자식보다 먼저 준다 — 자식이 들어올 때 그 차례를 따른다
        fm::ui::FlowBox box;
        box.setItemOrder({u"c"_s, u"a"_s, u"b"_s});
        for (const QString &name : {u"a"_s, u"b"_s, u"c"_s}) {
            auto *w = new Box(QSize(30, 20), {}, {}, &box);
            w->setObjectName(name);
        }
        box.syncChildren();
        QCOMPARE(box.itemOrder(), (QStringList{u"c"_s, u"a"_s, u"b"_s}));
        // 나중에 바꿔도 따른다
        box.setItemOrder({u"b"_s, u"c"_s, u"a"_s});
        QCOMPARE(box.itemOrder(), (QStringList{u"b"_s, u"c"_s, u"a"_s}));
    }

    void flexBoxDynamicProperties()
    {
        fm::ui::FlexBox box;
        box.setColumnGap(0);
        box.setMargin(0);
        auto *a = new Box(QSize(50, 20), {}, {}, &box);
        auto *b = new Box(QSize(50, 20), {}, {}, &box);
        b->setProperty(fm::ui::FlexBox::kGrow, 1.0);  // Designer 속성 창의 동적 속성
        box.syncChildren();
        box.resize(300, 20);
        box.layout()->activate();
        QCOMPARE(b->width(), 250);
        a->setProperty(fm::ui::FlexBox::kOrder, 1);  // 뒤로
        box.layout()->activate();
        QCOMPARE(b->x(), 0);
        QCOMPARE(a->x(), 250);
        QCOMPARE(box.direction(), fm::ui::FlexBox::Row);
        box.setDirection(fm::ui::FlexBox::Column);
        QCOMPARE(box.flexLayout()->direction(), FlexLayout::Direction::Column);
    }

    void boxDesignModeReorder()
    {
        // Designer: 자식을 다른 자식 위에 놓으면 그 자리로, 차례 신호가 나간다. 디자인 모드가 아니면 제자리로 돌아간다.
        fm::ui::FlowBox box;
        box.flowLayout()->setSpacing(10);
        QList<Box *> items;
        for (const QString &name : {u"a"_s, u"b"_s, u"c"_s}) {
            auto *w = new Box(QSize(40, 20), QSize(40, 20), {}, &box);
            w->setObjectName(name);
            items.append(w);
        }
        box.resize(300, 40);
        box.show();
        QVERIFY(QTest::qWaitForWindowExposed(&box));
        QSignalSpy spy(&box, &fm::ui::LayoutBox::itemOrderChanged);

        items[2]->move(items[0]->pos());  // 디자인 모드가 아니면 차례는 그대로(다음 배치에서 제자리)
        QTest::qWait(20);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(box.itemOrder(), (QStringList{u"a"_s, u"b"_s, u"c"_s}));
        box.layout()->invalidate();
        box.layout()->activate();
        QCOMPARE(items[2]->pos(), QPoint(100, 0));

        box.setDesignMode(true);
        items[2]->move(items[0]->pos() + QPoint(5, 0));  // c를 a 위로
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(box.itemOrder(), (QStringList{u"c"_s, u"a"_s, u"b"_s}));
        QTRY_COMPARE(items[2]->pos(), QPoint(0, 0));
        QCOMPARE(items[0]->pos(), QPoint(50, 0));
    }

    void flexHeightForWidthItems()
    {
        // 세로 방향 · 줄 바꿈 글자: 폭이 좁으면 높이가 커진다
        QWidget host;
        auto *layout = new FlexLayout(FlexLayout::Direction::Column, &host);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *label = new QLabel(QString(u"가나다라마바사 "_s).repeated(20));
        label->setWordWrap(true);
        layout->addWidget(label);
        QVERIFY(layout->hasHeightForWidth());
        QVERIFY(layout->heightForWidth(120) > layout->heightForWidth(600));
    }
};

QTEST_MAIN(TestLayouts)
#include "tst_layouts.moc"
