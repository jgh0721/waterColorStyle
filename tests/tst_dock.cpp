// 도크(07) 테스트 — QDockWidget 스타일 치수(세 디자인)와 fmdock 2: 영역 · 탭 · 분할(탭 묶음 나누기 · 중앙 영역),
// 닫기 · 다시 열기 · 지우기, 잠금, 탭 넘침 · 순서, 떠 있는 창, 자동 숨김, 상태(JSON) 왕복 · 거절, 이름 붙인 배치,
// 포커스 표시, 끌어 놓기 판정 · 끌기 세션, 보기 메뉴, 단축키 공유, 테마 전환, 소멸 순서.

#include <fmdock/DockManager.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>

#include "DockInternal_p.h"
#include "DropResolver_p.h"
#include "StateCodec_p.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QSplitter>
#include <QStyleOption>
#include <QTest>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

using namespace Qt::StringLiterals;
namespace fs = fm::style;
namespace fd = fm::dock;
namespace dd = fm::dock::detail;
using fd::DockArea;
using fd::DockManager;
using fd::DockWidget;
using fd::DropArea;
using fd::SideBarLocation;

namespace {

void settle()
{
    for (int i = 0; i < 4; ++i)
        QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

QRect globalRect(const QWidget *w)
{
    return QRect(w->mapToGlobal(QPoint(0, 0)), w->size());
}

DockWidget *makeDock(const QString &id, const QString &title)
{
    auto *dock = new DockWidget(id, title);
    auto *edit = new QPlainTextEdit;
    edit->setPlainText(title);
    dock->setWidget(edit);
    return dock;
}

/// 중앙(문서) 영역 + 왼쪽 탐색기 · 오른쪽 개요/기호 탭 · 오른쪽 아래 속성 · 아래 문제 · 문서 둘.
struct Fixture
{
    QMainWindow window;
    DockManager *manager = nullptr;
    DockWidget *explorer = nullptr;
    DockWidget *outline = nullptr;
    DockWidget *symbols = nullptr;
    DockWidget *props = nullptr;
    DockWidget *problems = nullptr;
    DockWidget *doc1 = nullptr;
    DockWidget *doc2 = nullptr;

    Fixture()
    {
        window.resize(1200, 760);
        manager = new DockManager(&window);
        manager->setCentralPlaceholderText(u"문서를 여세요"_s);
        doc1 = makeDock(u"doc:index.rst"_s, u"index.rst"_s);
        doc2 = makeDock(u"doc:README.md"_s, u"README.md"_s);
        doc1->setDocument(true);
        doc2->setDocument(true);
        manager->addDockWidgetToCentral(doc1);
        manager->addDockWidgetToCentral(doc2);
        explorer = makeDock(u"explorer"_s, u"탐색기"_s);
        manager->addDockWidget(DropArea::Left, explorer);
        outline = makeDock(u"outline"_s, u"개요"_s);
        DockArea *right = manager->addDockWidget(DropArea::Right, outline);
        symbols = makeDock(u"symbols"_s, u"기호"_s);
        manager->addDockWidgetTab(symbols, right);
        props = makeDock(u"props"_s, u"속성"_s);
        manager->addDockWidget(DropArea::Bottom, props, right);
        problems = makeDock(u"problems"_s, u"문제"_s);
        manager->addDockWidget(DropArea::Bottom, problems);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();
    }

    ~Fixture()
    {
        for (fd::FloatingWindow *w : manager->floatingWindows())
            w->hide();
    }
};

} // namespace

class TestDock : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        fs::ThemeManager::instance().install(*static_cast<QApplication *>(QCoreApplication::instance()));
    }

    void cleanup()
    {
        fs::ThemeManager::instance().setDesign(fs::Design::Standard);
        settle();
    }

    // ------------------------------------------------------------------ 스타일(QDockWidget · 2판 치수)

    void titleMetrics_data()
    {
        QTest::addColumn<int>("design");
        QTest::addColumn<int>("button");
        QTest::newRow("standard") << int(fs::Design::Standard) << 22;
        QTest::newRow("watercolor") << int(fs::Design::Watercolor) << 16;
        QTest::newRow("paperfluent") << int(fs::Design::PaperFluent) << 22;
    }

    void titleMetrics()
    {
        QFETCH(int, design);
        QFETCH(int, button);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        QDockWidget dock(u"폴더"_s);
        QStyle *style = dock.style();
        QStyleOptionDockWidget opt;
        opt.initFrom(&dock);
        opt.rect = QRect(0, 0, 300, 28);
        opt.closable = true;
        opt.floatable = true;
        const QRect close = style->subElementRect(QStyle::SE_DockWidgetCloseButton, &opt, &dock);
        const QRect floatButton = style->subElementRect(QStyle::SE_DockWidgetFloatButton, &opt, &dock);
        QCOMPARE(close.size(), QSize(button, button));
        QCOMPARE(floatButton.size(), QSize(button, button));
        QVERIFY(floatButton.right() < close.left());
    }

    void stripMetrics_data()
    {
        QTest::addColumn<int>("design");
        QTest::addColumn<int>("strip");
        QTest::addColumn<int>("splitter");
        QTest::addColumn<int>("sideBar");
        QTest::newRow("standard") << int(fs::Design::Standard) << 32 << 5 << 28;
        QTest::newRow("watercolor") << int(fs::Design::Watercolor) << 28 << 4 << 24;
        QTest::newRow("paperfluent") << int(fs::Design::PaperFluent) << 32 << 6 << 28;
    }

    void stripMetrics()
    {
        // 07 §6.4 — 탭 줄 · 분할 손잡이 · 사이드바 두께가 디자인을 따른다(실행 중 전환 포함)
        QFETCH(int, design);
        QFETCH(int, strip);
        QFETCH(int, splitter);
        QFETCH(int, sideBar);
        Fixture f;
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        settle();
        DockArea *area = f.outline->dockArea();
        QCOMPARE(area->titleBarGeometry().height(), strip);
        auto *split = qobject_cast<QSplitter *>(area->parentWidget());
        QVERIFY(split);
        QCOMPARE(split->handleWidth(), splitter);
        QVERIFY(f.manager->setAutoHidden(f.problems, true, SideBarLocation::Bottom));
        settle();
        dd::SideBar *bar = f.manager->findChild<dd::SideBar *>(u"fmDockSideBarBottom"_s);
        QVERIFY(bar && bar->isVisible());
        QCOMPARE(bar->height(), sideBar);
    }

    // ------------------------------------------------------------------ 배치

    void layoutBasics()
    {
        Fixture f;
        fd::DockContainer *c = f.manager->dockContainer();
        QCOMPARE(c->centralAreas().size(), 1);
        QVERIFY(f.doc1->isInCentralArea() && f.doc2->isInCentralArea());
        QCOMPARE(f.doc1->dockArea(), f.doc2->dockArea());
        QCOMPARE(f.outline->dockArea(), f.symbols->dockArea());
        QVERIFY(f.outline->dockArea() != f.props->dockArea());
        // 왼쪽 탐색기 · 오른쪽 개요 · 아래 문제(창 전체 폭)
        const QRect explorer = globalRect(f.explorer->dockArea());
        const QRect central = globalRect(f.doc1->dockArea());
        const QRect outline = globalRect(f.outline->dockArea());
        const QRect props = globalRect(f.props->dockArea());
        const QRect problems = globalRect(f.problems->dockArea());
        QVERIFY(explorer.right() < central.left());
        QVERIFY(central.right() < outline.left());
        QVERIFY(outline.bottom() < props.top());
        QCOMPARE(props.left(), outline.left());
        QVERIFY(problems.top() > central.bottom());
        QVERIFY(problems.width() > central.width());
        QCOMPARE(f.outline->dockArea()->currentDockWidget(), f.symbols);  // 새로 넣은 탭이 앞으로
        QCOMPARE(c->openedDockAreas().size(), 5);
    }

    void splitTabGroup()
    {
        // 1판(QMainWindow)이 못 하던 것 — 탭 묶음 옆으로 나누기
        Fixture f;
        DockArea *group = f.outline->dockArea();
        QVERIFY(f.manager->moveDockWidget(f.symbols, DropArea::Right, group));
        settle();
        QVERIFY(f.symbols->dockArea() != group);
        QCOMPARE(group->dockWidgetsCount(), 1);
        QVERIFY(globalRect(f.symbols->dockArea()).left() > globalRect(group).right());
        // 탭으로 되돌리면 빈 영역 · 분할이 접힌다
        QVERIFY(f.manager->moveDockWidget(f.symbols, DropArea::Center, group));
        settle();
        QCOMPARE(f.symbols->dockArea(), group);
        QCOMPARE(f.manager->dockContainer()->openedDockAreas().size(), 5);
    }

    void centralRegion()
    {
        // 07 §4.6 — 문서 영역을 나누어도 둘 다 중앙이고, 창 크기 변화는 중앙만 받는다
        Fixture f;
        DockArea *docs = f.doc1->dockArea();
        QVERIFY(f.manager->moveDockWidget(f.doc2, DropArea::Right, docs));
        settle();
        QVERIFY(f.doc2->dockArea() != docs);
        QVERIFY(f.doc2->isInCentralArea());
        QCOMPARE(f.manager->dockContainer()->centralAreas().size(), 2);
        // 줄였다 늘린다 — 배율 1.5의 1920 화면(논리 1280)에서도 늘린 폭이 화면 안에 든다
        f.window.resize(900, 600);
        settle();
        const int explorerWidth = f.explorer->dockArea()->width();
        const int docsWidth = docs->width() + f.doc2->dockArea()->width();
        f.window.resize(f.window.width() + 300, f.window.height());
        settle();
        QCOMPARE(f.explorer->dockArea()->width(), explorerWidth);
        QVERIFY(docs->width() + f.doc2->dockArea()->width() >= docsWidth + 250);
        // 자유 혼합(기본) — 도구 도크도 중앙에 탭으로, 문서도 바깥으로
        QVERIFY(f.manager->moveDockWidget(f.props, DropArea::Center, docs));
        QVERIFY(f.props->isInCentralArea());
        QVERIFY(f.manager->moveDockWidget(f.doc1, DropArea::Bottom, f.outline->dockArea()));
        QVERIFY(!f.doc1->isInCentralArea());
        // 제한 플래그
        f.manager->setConfigFlag(DockManager::CentralRejectsTools);
        QVERIFY(!f.manager->moveDockWidget(f.symbols, DropArea::Center, f.doc2->dockArea()));
        f.manager->setConfigFlag(DockManager::CentralRejectsTools, false);
        // 문서가 모두 빠지면 빈 자리가 보인다
        f.manager->moveDockWidget(f.doc2, DropArea::Left);
        f.manager->moveDockWidget(f.props, DropArea::Left);
        settle();
        QVERIFY(f.manager->dockContainer()->centralPlaceholder()->isVisible());
        QVERIFY(f.manager->moveDockWidget(f.doc2, DropArea::Center));
        settle();
        QVERIFY(f.doc2->isInCentralArea());
        QVERIFY(!f.manager->dockContainer()->centralPlaceholder()->isVisible());
    }

    void closeAndReopen()
    {
        Fixture f;
        DockArea *group = f.outline->dockArea();
        f.outline->requestClose();
        settle();
        QVERIFY(f.outline->isClosed());
        QVERIFY(!f.outline->toggleViewAction()->isChecked());
        QCOMPARE(group->currentDockWidget(), f.symbols);
        QCOMPARE(group->openDockWidgetsCount(), 1);
        f.symbols->toggleView(false);
        settle();
        QVERIFY(group->isHidden());  // 열린 도크가 없으면 영역을 숨긴다(지우지 않음)
        f.outline->toggleViewAction()->trigger();
        settle();
        QVERIFY(!f.outline->isClosed());
        QCOMPARE(f.outline->dockArea(), group);
        QVERIFY(group->isVisible());
        QVERIFY(f.outline->isVisible());
        QCOMPARE(group->indexOf(f.outline), 0);
        // 현재 탭인 채로 닫았다 열어도 보인다
        f.outline->toggleView(false);
        f.outline->toggleView(true);
        settle();
        QVERIFY(f.outline->isVisible());
    }

    void closeVariants()
    {
        Fixture f;
        // CustomCloseHandling — 닫지 않고 신호만
        f.props->setFeature(DockWidget::CustomCloseHandling, true);
        QSignalSpy requested(f.props, &DockWidget::closeRequested);
        f.props->requestClose();
        QCOMPARE(requested.count(), 1);
        QVERIFY(!f.props->isClosed());
        // DeleteOnClose — 관리자에서 빠지고 지워진다, 빈 영역은 접힌다
        f.problems->setFeature(DockWidget::DeleteOnClose, true);
        QPointer<DockWidget> gone(f.problems);
        f.problems->requestClose();
        settle();
        QVERIFY(!gone);
        QVERIFY(!f.manager->findDockWidget(u"problems"_s));
        QCOMPARE(f.manager->dockContainer()->dockAreas().size(), 4);
        // Closable이 없으면 닫기는 무시
        f.explorer->setFeature(DockWidget::Closable, false);
        f.explorer->requestClose();
        QVERIFY(!f.explorer->isClosed());
        // 앱이 도크를 직접 지워도 영역이 정리된다
        delete f.explorer;
        settle();
        QCOMPARE(f.manager->dockContainer()->dockAreas().size(), 3);
    }

    void lockFeatures()
    {
        Fixture f;
        DockArea *area = f.outline->dockArea();
        auto buttons = [area](const QString &kind) {
            for (auto *b : area->titleBar()->findChildren<dd::DockButton *>()) {
                if (b->kind() == kind)
                    return b;
            }
            return static_cast<dd::DockButton *>(nullptr);
        };
        QVERIFY(buttons(u"float"_s)->isVisible());
        QVERIFY(buttons(u"pin"_s)->isVisible());
        f.manager->lockFeatures(DockWidget::Movable | DockWidget::Floatable | DockWidget::Pinnable);
        settle();
        QVERIFY(!f.outline->hasFeature(DockWidget::Movable));
        QVERIFY(f.outline->features().testFlag(DockWidget::Movable));  // 도크 자신의 값은 그대로
        QVERIFY(!buttons(u"float"_s)->isVisible());
        QVERIFY(!buttons(u"pin"_s)->isVisible());
        QVERIFY(!f.manager->setAutoHidden(f.outline, true));
        f.manager->lockFeatures(DockWidget::NoFeatures);
        settle();
        QVERIFY(buttons(u"pin"_s)->isVisible());
    }

    void tabOverflowAndOrder()
    {
        Fixture f;
        DockArea *area = f.explorer->dockArea();
        QList<DockWidget *> extra;
        for (int i = 0; i < 8; ++i) {
            auto *d = makeDock(u"x%1"_s.arg(i), u"아주 긴 도구 창 이름 %1"_s.arg(i));
            f.manager->addDockWidgetTab(d, area);
            extra << d;
        }
        settle();
        QVERIFY(area->hiddenTabCount() > 0);
        area->setCurrentDockWidget(extra.last());
        settle();
        dd::DockTab *tab = area->titleBar()->tabBar()->tabFor(extra.last());
        QVERIFY(tab && tab->isVisible());  // 선택 탭은 언제나 보인다
        // 순서 바꾸기 — 도크 순서 · 선택 유지
        const int from = area->indexOf(extra.last());
        area->moveTab(from, 0);
        QCOMPARE(area->dockWidget(0), extra.last());
        QCOMPARE(area->currentDockWidget(), extra.last());
    }

    void floatingWindow()
    {
        Fixture f;
        DockArea *origin = f.outline->dockArea();
        f.outline->setFloating();
        settle();
        QVERIFY(f.outline->isFloating());
        fd::FloatingWindow *w = f.outline->floatingWindow();
        QVERIFY(w && w->isVisible());
        QCOMPARE(w->windowTitle(), u"개요"_s);
        QVERIFY(!f.outline->dockArea()->titleBarGeometry().isValid());  // 도크 하나 — 제목 줄은 창 제목으로
        // 떠 있는 창 안에서 탭 · 분할
        QVERIFY(f.manager->moveDockWidget(f.symbols, DropArea::Center, f.outline->dockArea()));
        settle();
        QVERIFY(f.outline->dockArea()->titleBarGeometry().isValid());
        QVERIFY(f.manager->moveDockWidget(f.props, DropArea::Bottom, f.outline->dockArea()));
        settle();
        QCOMPARE(w->dockContainer()->openedDockAreas().size(), 2);
        // 두 번 눌러 다시 붙이기 — 마지막으로 붙어 있던 자리로
        dd::ManagerAccess::dockBack(f.manager, f.outline);
        settle();
        QVERIFY(!f.outline->isFloating());
        QPointer<fd::FloatingWindow> guard(w);
        f.manager->moveDockWidget(f.symbols, DropArea::Center, f.outline->dockArea());
        f.manager->moveDockWidget(f.props, DropArea::Bottom, f.outline->dockArea());
        settle();
        QVERIFY(!guard);  // 빈 떠 있는 창은 지운다
        Q_UNUSED(origin)
    }

    void autoHide()
    {
        Fixture f;
        DockArea *group = f.outline->dockArea();
        QSignalSpy changed(f.manager, &DockManager::autoHideChanged);
        QVERIFY(f.manager->setAutoHidden(f.outline, true, SideBarLocation::Right));
        settle();
        QVERIFY(f.outline->isAutoHidden());
        QCOMPARE(f.outline->autoHideSide(), SideBarLocation::Right);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(f.manager->autoHideDockWidgets(SideBarLocation::Right), QList<DockWidget *>{f.outline});
        auto *bar = f.manager->findChild<dd::SideBar *>(u"fmDockSideBarRight"_s);
        QVERIFY(bar && bar->isVisible());
        // 펼치기 — 컨테이너 오른쪽에 겹친다
        f.manager->expandAutoHide(f.outline);
        settle();
        QCOMPARE(f.manager->expandedAutoHideDockWidget(), f.outline);
        auto *flyout = qobject_cast<dd::AutoHideFlyout *>(f.outline->parentWidget());
        QVERIFY(flyout && flyout->isVisible());
        const QRect container = globalRect(f.manager->dockContainer());
        QCOMPARE(globalRect(flyout).right(), container.right());
        // 바깥을 누르면 접힌다
        QTest::mouseClick(f.doc1->widget(), Qt::LeftButton);
        settle();
        QVERIFY(!f.manager->expandedAutoHideDockWidget());
        // Esc로 접기
        f.manager->expandAutoHide(f.outline);
        settle();
        f.outline->widget()->setFocus();
        QTest::keyClick(f.outline->widget(), Qt::Key_Escape);
        settle();
        QVERIFY(!f.manager->expandedAutoHideDockWidget());
        // 다른 쪽으로 옮기기 · 되돌리기
        QVERIFY(f.manager->setAutoHidden(f.outline, true, SideBarLocation::Left));
        QCOMPARE(f.outline->autoHideSide(), SideBarLocation::Left);
        QVERIFY(f.manager->setAutoHidden(f.outline, false));
        settle();
        QVERIFY(!f.outline->isAutoHidden());
        QCOMPARE(f.outline->dockArea(), group);
        QVERIFY(!bar->isVisible());
        // 영역째 고정 — 빈 영역은 접힌다
        dd::ManagerAccess::pinArea(f.manager, group, SideBarLocation::Bottom);
        settle();
        QVERIFY(f.outline->isAutoHidden() && f.symbols->isAutoHidden());
        QCOMPARE(f.manager->autoHideDockWidgets(SideBarLocation::Bottom).size(), 2);
    }

    // ------------------------------------------------------------------ 상태

    void stateRoundTrip()
    {
        Fixture f;
        f.manager->moveDockWidget(f.doc2, DropArea::Right, f.doc1->dockArea());
        f.outline->setFloating();
        f.manager->setAutoHidden(f.problems, true, SideBarLocation::Bottom);
        f.symbols->toggleView(false);
        f.manager->lockFeatures(DockWidget::Floatable);
        settle();
        const QByteArray state = f.manager->saveState(3);
        const QJsonObject json = QJsonDocument::fromJson(state).object();
        QCOMPARE(json.value(u"format"_s).toString(), u"fmdock"_s);
        QCOMPARE(json.value(u"version"_s).toInt(), 2);
        QCOMPARE(json.value(u"user"_s).toInt(), 3);
        QCOMPARE(json.value(u"floating"_s).toArray().size(), 1);
        QVERIFY(json.value(u"closed"_s).toArray().contains(u"symbols"_s));

        // 다르게 흐트러뜨린 뒤 되살린다
        f.manager->lockFeatures(DockWidget::NoFeatures);
        f.manager->setAutoHidden(f.problems, false);
        f.symbols->toggleView(true);
        f.manager->moveDockWidget(f.doc2, DropArea::Center, f.doc1->dockArea());
        f.manager->moveDockWidget(f.explorer, DropArea::Bottom);
        dd::ManagerAccess::dockBack(f.manager, f.outline);
        settle();
        QSignalSpy restored(f.manager, &DockManager::stateRestored);
        QVERIFY(!f.manager->restoreState(state, 0));  // 사용자 판이 다르면 거절
        QVERIFY(f.manager->restoreState(state, 3));
        settle();
        QCOMPARE(restored.count(), 1);
        QVERIFY(f.doc2->dockArea() != f.doc1->dockArea());
        QVERIFY(f.doc1->isInCentralArea() && f.doc2->isInCentralArea());
        QVERIFY(f.outline->isFloating());
        QVERIFY(f.problems->isAutoHidden());
        QCOMPARE(f.problems->autoHideSide(), SideBarLocation::Bottom);
        QVERIFY(f.symbols->isClosed());
        QCOMPARE(f.manager->lockedFeatures(), DockWidget::Features(DockWidget::Floatable));
        QVERIFY(globalRect(f.explorer->dockArea()).right() < globalRect(f.doc1->dockArea()).left());
        // 다시 저장하면 같은 배치(크기는 반올림 오차를 허용해 비교에서 뺀다)
        auto shape = [](QByteArray bytes) {
            QJsonObject o = QJsonDocument::fromJson(bytes).object();
            QString s = QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
            static const QRegularExpression sizes(uR"("sizes":\[[^\]]*\])"_s);
            static const QRegularExpression rect(uR"("rect":\[[^\]]*\])"_s);
            return s.remove(sizes).remove(rect);
        };
        QCOMPARE(shape(f.manager->saveState(3)), shape(state));
    }

    void stateRejected_data()
    {
        QTest::addColumn<QByteArray>("state");
        QTest::newRow("garbage") << QByteArray("garbage");
        QTest::newRow("v1-binary") << QByteArray("FMDK\x00\x01", 6);
        QTest::newRow("format") << QByteArray(R"({"format":"other","version":2,"user":0})");
        QTest::newRow("version") << QByteArray(R"({"format":"fmdock","version":1,"user":0})");
        QTest::newRow("duplicate") << QByteArray(
            R"({"format":"fmdock","version":2,"user":0,"main":{"tree":{"split":"h","children":[{"area":["outline"]},{"area":["outline"]}]}}})");
        QByteArray deep = R"({"format":"fmdock","version":2,"user":0,"main":{"tree":)";
        for (int i = 0; i < 40; ++i)
            deep += R"({"split":"h","children":[)";
        deep += R"({"area":["outline"]})";
        for (int i = 0; i < 40; ++i)
            deep += "]}";
        deep += "}}";
        QTest::newRow("too-deep") << deep;
    }

    void stateRejected()
    {
        QFETCH(QByteArray, state);
        Fixture f;
        const QByteArray before = f.manager->saveState();
        QVERIFY(!f.manager->restoreState(state));
        settle();
        QCOMPARE(f.manager->saveState(), before);  // 거절하면 아무것도 바꾸지 않는다
    }

    void stateUnknownAndMissing()
    {
        Fixture f;
        f.manager->setDefaultLayout(f.manager->saveState());
        // 모르는 id는 건너뛰고, 상태에 없는 도크는 기본 배치에서 같은 영역이던 도크 곁으로
        const QByteArray state = R"({"format":"fmdock","version":2,"user":0,"main":{"tree":{"split":"h","sizes":[200,700],
            "children":[{"area":["explorer","ghost"]},{"central":true,"area":["doc:index.rst","doc:README.md"]}]}}})";
        QVERIFY(f.manager->restoreState(state));
        settle();
        QVERIFY(f.explorer->dockArea());
        QVERIFY(f.doc1->isInCentralArea());
        QCOMPARE(f.outline->dockArea(), f.symbols->dockArea());  // 기본 배치의 같은 영역 — 둘 다 상태에 없음
        QVERIFY(f.problems->dockArea());
        QVERIFY(f.manager->resetToDefaultLayout());
        settle();
        QVERIFY(f.problems->dockArea() != f.explorer->dockArea());
    }

    void perspectives()
    {
        Fixture f;
        QSignalSpy changed(f.manager, &DockManager::perspectivesChanged);
        f.manager->addPerspective(u"편집"_s);
        f.manager->moveDockWidget(f.explorer, DropArea::Right);
        f.manager->addPerspective(u"검토"_s);
        QCOMPARE(f.manager->perspectiveNames(), (QStringList{u"검토"_s, u"편집"_s}));
        QVERIFY(f.manager->openPerspective(u"편집"_s));
        settle();
        QVERIFY(globalRect(f.explorer->dockArea()).right() < globalRect(f.doc1->dockArea()).left());
        f.manager->removePerspective(u"검토"_s);
        QCOMPARE(f.manager->perspectiveNames(), QStringList{u"편집"_s});
        QCOMPARE(changed.count(), 3);
        QVERIFY(!f.manager->openPerspective(u"없음"_s));
    }

    // ------------------------------------------------------------------ 포커스 · 메뉴 · 단축키

    void focusHighlight()
    {
        Fixture f;
        f.window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&f.window));
        QSignalSpy spy(f.manager, &DockManager::focusedDockWidgetChanged);
        f.outline->widget()->setFocus();
        settle();
        QCOMPARE(f.manager->focusedDockWidget(), f.outline);
        QCOMPARE(f.outline->dockArea()->property(fs::props::kPaneActive), QVariant(true));
        QCOMPARE(f.explorer->dockArea()->property(fs::props::kPaneActive), QVariant(false));
        f.doc2->widget()->setFocus();
        settle();
        QCOMPARE(f.manager->focusedDockWidget(), f.doc2);
        QCOMPARE(f.outline->dockArea()->property(fs::props::kPaneActive), QVariant(false));
        QCOMPARE(f.manager->lastActiveCentralArea(), f.doc2->dockArea());
        QVERIFY(spy.count() >= 2);
        // 중앙에 새 문서 — 마지막 활성 중앙 영역으로
        auto *doc3 = makeDock(u"doc:new"_s, u"새 문서"_s);
        f.manager->addDockWidgetToCentral(doc3);
        QCOMPARE(doc3->dockArea(), f.doc2->dockArea());
    }

    void keyboard()
    {
        Fixture f;
        f.window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&f.window));
        DockArea *docs = f.doc1->dockArea();
        docs->setCurrentDockWidget(f.doc1);
        f.doc1->widget()->setFocus();
        settle();
        QTest::keyClick(f.doc1->widget(), Qt::Key_PageDown, Qt::ControlModifier);
        settle();
        QCOMPARE(docs->currentDockWidget(), f.doc2);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_PageUp, Qt::ControlModifier);
        settle();
        QCOMPARE(docs->currentDockWidget(), f.doc1);
        // Ctrl+F4 — 문서 탭만 닫는다
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_F4, Qt::ControlModifier);
        settle();
        QVERIFY(f.doc1->isClosed());
        f.outline->widget()->setFocus();
        QTest::keyClick(f.outline->widget(), Qt::Key_F4, Qt::ControlModifier);
        settle();
        QVERIFY(!f.outline->isClosed());
        // 영역 사이 이동
        f.manager->focusNextArea(true);
        settle();
        QVERIFY(f.manager->focusedDockWidget() != f.outline);
        // QTest가 남긴 Ctrl 상태를 비운다(끌기는 Ctrl을 '떠 있는 창으로'로 읽는다)
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Shift);
    }

    void nativeFloatingFrame()
    {
        // 실제 Windows 창에서만(VM 대화형 세션) — 프레임 없는 떠 있는 창의 적중 판정과 OS 이동 중 붙이기(07 §4.5)
#ifndef Q_OS_WIN
        QSKIP("Windows only");
#else
        if (dd::isHeadlessPlatform())
            QSKIP("needs a real window (QT_QPA_PLATFORM=windows)");
        Fixture f;
        f.outline->setFloating();
        settle();
        fd::FloatingWindow *w = f.outline->floatingWindow();
        QVERIFY(w && QTest::qWaitForWindowExposed(w));
        settle();
        const HWND hwnd = reinterpret_cast<HWND>(w->winId());
        QVERIFY(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_THICKFRAME);
        RECT wr;
        GetWindowRect(hwnd, &wr);
        const qreal dpr = w->devicePixelRatioF();
        const auto hit = [&](const QPoint &local) {
            const LPARAM lp = MAKELPARAM(wr.left + qRound(local.x() * dpr), wr.top + qRound(local.y() * dpr));
            return SendMessageW(hwnd, WM_NCHITTEST, 0, lp);
        };
        auto *title = w->findChild<dd::FloatingTitleBar *>();
        QVERIFY(title);
        QCOMPARE(hit(title->geometry().center() - QPoint(title->width() / 4, 0)), LRESULT(HTCAPTION));
        QCOMPARE(hit(QPoint(1, w->height() / 2)), LRESULT(HTLEFT));
        QCOMPARE(hit(QPoint(w->width() - 2, w->height() - 2)), LRESULT(HTBOTTOMRIGHT));
        auto *close = title->findChildren<dd::CaptionButtonWidget *>().last();
        QCOMPARE(hit(close->mapTo(w, close->rect().center())), LRESULT(HTCLIENT));
        QCOMPARE(hit(QPoint(w->width() / 2, w->height() / 2)), LRESULT(HTCLIENT));

        // OS 이동 흉내 — 커서를 메인 컨테이너 오른쪽 가장자리 표시에 두고 창을 옮겼다 놓는다
        const QRect container = globalRect(f.manager->dockContainer());
        const QPoint target = dd::edgeIndicatorRect(container, DropArea::Right, dd::DropOptions{}).center();
        const QPoint nativeTarget(qRound(target.x() * dpr), qRound(target.y() * dpr));
        SetCursorPos(nativeTarget.x(), nativeTarget.y());
        SendMessageW(hwnd, WM_ENTERSIZEMOVE, 0, 0);
        RECT moving = wr;
        OffsetRect(&moving, 40, 40);
        SetWindowPos(hwnd, nullptr, moving.left, moving.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        SendMessageW(hwnd, WM_MOVING, 0, reinterpret_cast<LPARAM>(&moving));
        settle();
        QVERIFY(dd::ManagerAccess::drag(f.manager));
        SendMessageW(hwnd, WM_EXITSIZEMOVE, 0, 0);
        settle();
        QVERIFY(!f.outline->isFloating());
        QVERIFY(globalRect(f.outline->dockArea()).right() >= container.right() - 2);
#endif
    }

    void viewMenu()
    {
        Fixture f;
        QMenu menu;
        f.manager->populateViewMenu(&menu);
        QVERIFY(menu.actions().contains(f.outline->toggleViewAction()));
        QAction *lock = nullptr;
        for (QAction *a : menu.actions()) {
            if (a->text() == u"도크 잠금(&K)")
                lock = a;
        }
        QVERIFY(lock);
        lock->toggle();
        QVERIFY(f.manager->lockedFeatures().testFlag(DockWidget::Movable));
        // 열 때마다 다시 채운다 — 새 도크가 들어온다
        auto *later = makeDock(u"later"_s, u"나중"_s);
        f.manager->addDockWidget(DropArea::Right, later);
        Q_EMIT menu.aboutToShow();
        QVERIFY(menu.actions().contains(later->toggleViewAction()));
    }

    void shortcutsInFloatingWindow()
    {
        Fixture f;
        auto *action = new QAction(u"찾기"_s, &f.window);
        action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
        f.window.addAction(action);
        f.manager->setShortcutSource(&f.window);
        f.outline->setFloating();
        settle();
        QVERIFY(f.outline->floatingWindow()->actions().contains(action));
    }

    void noTopLevelWindows()
    {
        Fixture f;
        f.manager->setConfigFlag(DockManager::NoTopLevelWindows);
        f.outline->setFloating();
        settle();
        QVERIFY(!f.outline->isFloating());
        QVERIFY(f.manager->floatingWindows().isEmpty());
    }

    // ------------------------------------------------------------------ 끌어 놓기

    void resolver()
    {
        dd::DropContainerGeom g;
        g.rect = QRect(100, 100, 1000, 700);
        g.outer = QRect(80, 100, 1040, 700);
        g.allowSideBars = true;
        dd::DropAreaGeom a;
        a.rect = QRect(400, 100, 500, 400);
        a.strip = QRect(400, 100, 500, 32);
        a.content = QRect(400, 132, 500, 368);
        a.tabs = {QRect(404, 104, 80, 28), QRect(486, 104, 80, 28)};
        a.tabIndexes = {0, 1};
        g.areas << a;
        const dd::DropOptions o;
        using K = dd::DropTarget::Kind;

        dd::DropVisual v;
        dd::DropTarget t = dd::resolveDrop(g, dd::edgeIndicatorRect(g.rect, DropArea::Left, o).center(), o, &v);
        QCOMPARE(t.kind, K::ContainerEdge);
        QCOMPARE(t.side, DropArea::Left);
        QVERIFY(v.preview.width() < g.rect.width() / 2);

        t = dd::resolveDrop(g, dd::crossIndicatorRect(a.content, DropArea::Right, o).center(), o, &v);
        QCOMPARE(t.kind, K::AreaSide);
        QCOMPARE(t.side, DropArea::Right);
        QCOMPARE(t.area, 0);
        QCOMPARE(v.preview.left(), a.rect.left() + a.rect.width() / 2);
        QCOMPARE(v.indicators.size(), 4 + 5);

        t = dd::resolveDrop(g, dd::crossIndicatorRect(a.content, DropArea::Center, o).center(), o, &v);
        QCOMPARE(t.kind, K::AreaCenter);

        t = dd::resolveDrop(g, QPoint(470, 116), o, &v);  // 첫 탭 오른쪽 절반 — 두 번째 자리(가장자리 24 px 안이어도 탭 줄이 먼저)
        QCOMPARE(t.kind, K::TabInsert);
        QCOMPARE(t.index, 1);
        QVERIFY(v.caret.isValid());

        t = dd::resolveDrop(g, QPoint(470, 103), o, &v);  // 가장자리 8 px 안 — 사이드바가 먼저
        QCOMPARE(t.kind, K::SideBar);
        QCOMPARE(t.sideBar, SideBarLocation::Top);

        t = dd::resolveDrop(g, QPoint(90, 400), o, &v);  // 바깥 가장자리 24 px 안 — 사이드바
        QCOMPARE(t.kind, K::SideBar);
        QCOMPARE(t.sideBar, SideBarLocation::Left);
        QVERIFY(v.sideZone.isValid());

        t = dd::resolveDrop(g, QPoint(700, 700), o, &v);  // 영역 밖 · 표시 밖 — 떠 있는 창
        QCOMPARE(t.kind, K::None);

        g.areas[0].allowCenter = false;  // 자기 영역 — 탭 자리 없음
        t = dd::resolveDrop(g, dd::crossIndicatorRect(a.content, DropArea::Center, o).center(), o, &v);
        QCOMPARE(t.kind, K::None);
        QCOMPARE(v.indicators.size(), 4 + 4);

        dd::DropContainerGeom empty;
        empty.rect = QRect(0, 0, 400, 300);
        dd::DropAreaGeom placeholder;
        placeholder.rect = placeholder.content = QRect(50, 50, 300, 200);
        placeholder.placeholder = true;
        empty.areas << placeholder;
        t = dd::resolveDrop(empty, QPoint(200, 150), o, &v);
        QCOMPARE(t.kind, K::CentralEmpty);
    }

    void dragSession()
    {
        Fixture f;
        f.window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&f.window));
        DockArea *group = f.outline->dockArea();
        // 문제 → 개요 묶음 오른쪽
        dd::ManagerAccess::startDrag(f.manager, {f.problems}, f.problems->dockArea(), f.problems,
                                     globalRect(f.problems).center(), QPoint(20, 10));
        dd::DragSession *drag = dd::ManagerAccess::drag(f.manager);
        QVERIFY(drag);
        const QRect content(group->mapToGlobal(group->contentGeometry().topLeft()), group->contentGeometry().size());
        const QPoint right = dd::crossIndicatorRect(content, DropArea::Right, dd::DropOptions{}).center();
        drag->move(right);
        QCOMPARE(drag->target().kind, dd::DropTarget::Kind::AreaSide);
        QVERIFY(drag->drop(right));
        settle();
        QVERIFY(!dd::ManagerAccess::drag(f.manager));
        QVERIFY(globalRect(f.problems->dockArea()).left() > globalRect(group).right() - 2);
        // Esc 취소 — 아무것도 옮기지 않는다
        DockArea *before = f.explorer->dockArea();
        dd::ManagerAccess::startDrag(f.manager, {f.explorer}, before, f.explorer, globalRect(f.explorer).center(),
                                     QPoint(20, 10));
        QTest::keyClick(f.explorer->widget(), Qt::Key_Escape);
        settle();
        QVERIFY(!dd::ManagerAccess::drag(f.manager));
        QCOMPARE(f.explorer->dockArea(), before);
        // 표시 밖에 놓으면 떠 있는 창
        dd::ManagerAccess::startDrag(f.manager, {f.props}, f.props->dockArea(), f.props, globalRect(f.props).center(),
                                     QPoint(20, 10));
        const QPoint outside = globalRect(&f.window).bottomRight() + QPoint(200, 200);
        QVERIFY(dd::ManagerAccess::drag(f.manager)->drop(outside));
        settle();
        QVERIFY(f.props->isFloating());
    }

    // ------------------------------------------------------------------ 테마 · 소멸

    void themesRender()
    {
        // 세 디자인 × 변형에서 그려 본다(그리기 경로 전체 — 탭 · 단추 · 사이드바 · 펼친 창 · 떠 있는 창 · 표시)
        Fixture f;
        f.manager->setAutoHidden(f.problems, true, SideBarLocation::Bottom);
        f.manager->expandAutoHide(f.problems);
        f.outline->setFloating();
        using TM = fs::ThemeManager;
        for (fs::Design design : {fs::Design::Standard, fs::Design::Watercolor, fs::Design::PaperFluent}) {
            for (int variant = 0; variant < 3; ++variant) {
                TM::instance().setDesign(design);
                TM::instance().setScheme(variant == 0 ? TM::Scheme::Light : TM::Scheme::Dark);
                TM::instance().setDarkTone(variant == 2 ? TM::DarkTone::Navy : TM::DarkTone::Gray);
                settle();
                const QImage image = f.window.grab().toImage();
                QVERIFY(!image.isNull());
                QVERIFY(!f.outline->floatingWindow()->grab().isNull());
                QVERIFY(!f.problems->parentWidget()->grab().isNull());
                // 탭 줄 바탕은 그 디자인 · 변형의 토큰(시안3은 Alt)
                DockArea *area = f.explorer->dockArea();
                // 마지막 탭과 첫 영역 단추 사이
                int tabsRight = 0;
                int buttonsLeft = area->width();
                for (auto *tab : area->titleBar()->findChildren<dd::DockTab *>()) {
                    if (tab->isVisible())
                        tabsRight = std::max(tabsRight, tab->mapTo(area, QPoint(tab->width(), 0)).x());
                }
                for (auto *b : area->titleBar()->findChildren<dd::DockButton *>()) {
                    if (b->isVisible())
                        buttonsLeft = std::min(buttonsLeft, b->mapTo(area, QPoint(0, 0)).x());
                }
                QVERIFY(buttonsLeft - tabsRight > 4);
                const QPoint probe((tabsRight + buttonsLeft) / 2, area->titleBarGeometry().top() + 3);
                const QImage areaImage = area->grab().toImage();
                // grab은 장치 픽셀 — 배율이 1이 아니면 논리 좌표를 옮긴다
                const QColor strip = areaImage.pixelColor((QPointF(probe) * areaImage.devicePixelRatio()).toPoint());
                if (const QString dir = qEnvironmentVariable("FMDOCK_SHOT_DIR"); !dir.isEmpty()) {
                    const QString tag = u"%1-%2"_s.arg(int(design)).arg(variant);
                    image.save(dir + u"/window-"_s + tag + u".png"_s);
                    areaImage.save(dir + u"/area-"_s + tag + u".png"_s);
                }
                const fs::ThemeColors &tc = fs::themeColorsFor(area);
                const QColor want = tc.isPaperFluent() ? tc[fs::Token::Alt] : tc[fs::Token::Win];
                QVERIFY2(qAbs(strip.red() - want.red()) + qAbs(strip.green() - want.green()) + qAbs(strip.blue() - want.blue()) < 12,
                         qPrintable(strip.name() + u" ≠ "_s + want.name()));
            }
        }
        TM::instance().setScheme(TM::Scheme::Light);
        TM::instance().setDarkTone(TM::DarkTone::Gray);
    }

    void destruction()
    {
        auto *window = new QMainWindow;
        auto *manager = new DockManager(window);
        QPointer<DockWidget> a = makeDock(u"a"_s, u"A"_s);
        QPointer<DockWidget> b = makeDock(u"b"_s, u"B"_s);
        QPointer<DockWidget> c = makeDock(u"c"_s, u"C"_s);
        manager->addDockWidget(DropArea::Left, a);
        manager->addAutoHideDockWidget(SideBarLocation::Right, b);
        manager->addFloatingDockWidget(c);
        QPointer<fd::FloatingWindow> floating = c->floatingWindow();
        window->show();
        settle();
        delete window;
        settle();
        QVERIFY(!a && !b && !c && !floating);
    }
};

QTEST_MAIN(TestDock)
#include "tst_dock.moc"
