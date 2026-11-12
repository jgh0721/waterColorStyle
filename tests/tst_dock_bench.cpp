// 도크 성능(07 §8) — Release 빌드에서 목표와 비교한다(Debug는 수치만 출력).
//   · 도크 50개(영역 15) 등록 + 첫 표시 ≤ 150 ms
//   · restoreState(도크 50 · 영역 15 · 떠 있는 창 2) ≤ 60 ms, saveState ≤ 5 ms
//   · 끄는 중 이동 1회(판정 + 겹침 갱신) ≤ 2 ms
//   · 디자인 전환(도크 50) ≤ 100 ms
//   · 도크 하나당 추가 메모리(내용 제외) ≤ 30 KB

#include <fmdock/DockManager.h>
#include <fmstyle/ThemeManager.h>

#include "DockInternal_p.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QMainWindow>
#include <QTest>

#include <algorithm>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <psapi.h>
#endif

using namespace Qt::StringLiterals;
namespace fs = fm::style;
namespace fd = fm::dock;
namespace dd = fm::dock::detail;

namespace {

#ifdef NDEBUG
constexpr bool kEnforce = true;
#else
constexpr bool kEnforce = false;
#endif

void settle()
{
    for (int i = 0; i < 3; ++i)
        QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

double median(QList<double> values)
{
    std::sort(values.begin(), values.end());
    return values.isEmpty() ? 0.0 : values[values.size() / 2];
}

/// 프로세스 전용 바이트(커밋된 힙) — 작업 집합보다 할당량을 정확히 따른다.
qint64 privateBytes()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
                             sizeof(counters)))
        return qint64(counters.PrivateUsage);
#endif
    return 0;
}

void report(const char *name, double value, double limit, const char *unit = "ms")
{
    qInfo("BENCH %-28s %8.2f %s (목표 %.0f)", name, value, unit, limit);
    if (kEnforce && value > limit)
        QFAIL(qPrintable(u"%1 = %2 > %3"_s.arg(QString::fromLatin1(name)).arg(value).arg(limit)));
}

/// 5 열 × 3 행 영역(15)에 도크 50개 — 영역마다 3 ~ 4 탭.
void buildLayout(fd::DockManager *manager, QList<fd::DockWidget *> *docks)
{
    QList<fd::DockArea *> columns;
    int n = 0;
    auto make = [&n, docks]() {
        auto *d = new fd::DockWidget(u"d%1"_s.arg(n), u"도크 %1"_s.arg(n));
        d->setWidget(new QLabel(u"내용 %1"_s.arg(n)), fd::DockWidget::InsertMode::ForceNoScrollArea);
        ++n;
        docks->append(d);
        return d;
    };
    fd::DockArea *previous = nullptr;
    for (int c = 0; c < 5; ++c) {
        fd::DockArea *col = previous ? manager->addDockWidget(fd::DropArea::Right, make(), previous)
                                     : manager->addDockWidget(fd::DropArea::Left, make());
        columns << col;
        previous = col;
    }
    QList<fd::DockArea *> areas = columns;
    for (fd::DockArea *col : columns) {
        fd::DockArea *a = manager->addDockWidget(fd::DropArea::Bottom, make(), col);
        areas << a;
        areas << manager->addDockWidget(fd::DropArea::Bottom, make(), a);
    }
    int i = 0;
    while (n < 50)
        manager->addDockWidgetTab(make(), areas[i++ % areas.size()]);
}

} // namespace

class TestDockBench : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        fs::ThemeManager::instance().install(*static_cast<QApplication *>(QCoreApplication::instance()));
        // 첫 창 · 글꼴 준비 비용은 재지 않는다
        QMainWindow warm;
        warm.resize(400, 300);
        warm.show();
        QVERIFY(QTest::qWaitForWindowExposed(&warm));
    }

    void createAndShow()
    {
        QList<double> runs;
        for (int run = 0; run < 5; ++run) {
            QMainWindow window;
            window.resize(1600, 1000);
            QElapsedTimer t;
            t.start();
            auto *manager = new fd::DockManager(&window);
            QList<fd::DockWidget *> docks;
            buildLayout(manager, &docks);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            settle();
            runs << t.nsecsElapsed() / 1e6;
            QCOMPARE(manager->dockContainer()->openedDockAreas().size(), 15);
        }
        report("create+show (50 docks)", median(runs), 150);
    }

    void saveAndRestore()
    {
        QMainWindow window;
        window.resize(1600, 1000);
        auto *manager = new fd::DockManager(&window);
        QList<fd::DockWidget *> docks;
        buildLayout(manager, &docks);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();
        docks[3]->setFloating();
        docks[17]->setFloating();
        settle();
        QByteArray state;
        QList<double> saves;
        for (int i = 0; i < 20; ++i) {
            QElapsedTimer t;
            t.start();
            state = manager->saveState();
            saves << t.nsecsElapsed() / 1e6;
        }
        report("saveState", median(saves), 5);
        QList<double> restores;
        for (int i = 0; i < 10; ++i) {
            manager->moveDockWidget(docks[i], fd::DropArea::Right);
            settle();
            QElapsedTimer t;
            t.start();
            QVERIFY(manager->restoreState(state));
            restores << t.nsecsElapsed() / 1e6;
            settle();
        }
        report("restoreState (50 · 15 · 2)", median(restores), 60);
        for (fd::FloatingWindow *w : manager->floatingWindows())
            w->hide();
    }

    void dragMove()
    {
        QMainWindow window;
        window.resize(1600, 1000);
        auto *manager = new fd::DockManager(&window);
        QList<fd::DockWidget *> docks;
        buildLayout(manager, &docks);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();
        dd::ManagerAccess::startDrag(manager, {docks[0]}, docks[0]->dockArea(), docks[0],
                                     docks[0]->mapToGlobal(QPoint(10, 10)), QPoint(10, 10));
        dd::DragSession *drag = dd::ManagerAccess::drag(manager);
        QVERIFY(drag);
        const QRect area(window.mapToGlobal(QPoint(0, 0)), window.size());
        QElapsedTimer t;
        t.start();
        constexpr int kSteps = 400;
        for (int i = 0; i < kSteps; ++i) {
            const QPoint p(area.left() + (i * 37) % area.width(), area.top() + (i * 53) % area.height());
            drag->move(p);
            QCoreApplication::processEvents();
        }
        report("drag move (per event)", t.nsecsElapsed() / 1e6 / kSteps, 2);
        drag->cancel();
        settle();
    }

    void designSwitch()
    {
        QMainWindow window;
        window.resize(1600, 1000);
        auto *manager = new fd::DockManager(&window);
        QList<fd::DockWidget *> docks;
        buildLayout(manager, &docks);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();
        QList<double> runs;
        for (fs::Design design : {fs::Design::Watercolor, fs::Design::PaperFluent, fs::Design::Standard,
                                  fs::Design::Watercolor, fs::Design::Standard}) {
            QElapsedTimer t;
            t.start();
            fs::ThemeManager::instance().setDesign(design);
            settle();
            window.repaint();
            runs << t.nsecsElapsed() / 1e6;
        }
        report("design switch (50 docks)", median(runs), 100);
    }

    void memoryPerDock()
    {
        if (privateBytes() == 0)
            QSKIP("process memory counters unavailable");
        QMainWindow window;
        window.resize(1600, 1000);
        auto *manager = new fd::DockManager(&window);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();
        // 영역 8개(4 열 × 2 행)를 먼저 두고 도크는 탭으로 — 창 크기가 바뀌면 백킹 스토어가 다시 잡혀 도크 비용이 아니다
        QList<fd::DockArea *> areas;
        int serial = 0;
        auto make = [&serial]() {
            const QString id = u"m%1"_s.arg(serial++);
            return new fd::DockWidget(id, id);
        };
        fd::DockArea *previous = nullptr;
        for (int c = 0; c < 4; ++c) {
            fd::DockArea *top = previous ? manager->addDockWidget(fd::DropArea::Right, make(), previous)
                                         : manager->addDockWidget(fd::DropArea::Left, make());
            areas << top << manager->addDockWidget(fd::DropArea::Bottom, make(), top);
            previous = top;
        }
        auto add = [&](int count) {
            for (int i = 0; i < count; ++i)
                manager->addDockWidgetTab(make(), areas[serial % areas.size()]);
            settle();
        };
        add(40);  // 예열 — 글꼴 · 스타일 · 첫 할당 비용은 재지 않는다
        const QSize windowSize = window.size();
        const qint64 before = privateBytes();
        constexpr int kCount = 200;
        add(kCount);
        const qint64 after = privateBytes();
        QCOMPARE(window.size(), windowSize);
        const double perDock = double(after - before) / kCount / 1024.0;

        // 기준선 — 같은 창에 도크 하나만큼의 평범한 위젯(도크 · 탭 · 닫기 단추 + 영역 몫 4개)을 보이게 둔다
        auto *plainHost = new QWidget(&window);
        plainHost->setGeometry(0, 0, 400, 400);
        plainHost->show();
        settle();
        const qint64 plainBefore = privateBytes();
        for (int i = 0; i < kCount; ++i) {
            for (int k = 0; k < 7; ++k) {
                auto *w = new QWidget(plainHost);
                w->setGeometry(k * 3, i % 50, 10, 10);
                w->show();
            }
        }
        settle();
        const double perPlain = double(privateBytes() - plainBefore) / kCount / 1024.0;
        qInfo("BENCH %-28s %8.2f KB (위젯 7개 기준선)", "plain widgets per dock", perPlain);
        report("memory per dock - baseline", perDock - perPlain, 30, "KB");
    }

    /// 영역 하나(제목 줄 · 탭 줄 · 단추 · 분할 손잡이)의 몫 — 목표는 없고 수치만 남긴다.
    void memoryPerArea()
    {
        if (privateBytes() == 0)
            QSKIP("process memory counters unavailable");
        QMainWindow window;
        window.resize(1600, 1000);
        auto *manager = new fd::DockManager(&window);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();
        int serial = 0;
        auto make = [&serial]() {
            const QString id = u"a%1"_s.arg(serial++);
            return new fd::DockWidget(id, id);
        };
        // 열 6개를 먼저 두고(예열) 열마다 아래로 영역을 더한다 — 6 × 6 = 36이면 창이 커지지 않는다
        QList<fd::DockArea *> columns;
        fd::DockArea *previous = nullptr;
        for (int c = 0; c < 6; ++c) {
            previous = previous ? manager->addDockWidget(fd::DropArea::Right, make(), previous)
                                : manager->addDockWidget(fd::DropArea::Left, make());
            columns << previous;
        }
        QList<fd::DockArea *> tails = columns;
        auto addRow = [&]() {
            for (int c = 0; c < columns.size(); ++c)
                tails[c] = manager->addDockWidget(fd::DropArea::Bottom, make(), tails[c]);
            settle();
        };
        addRow();  // 예열
        const QSize windowSize = window.size();
        const qint64 before = privateBytes();
        constexpr int kRows = 4;
        for (int r = 0; r < kRows; ++r)
            addRow();
        const qint64 after = privateBytes();
        QCOMPARE(window.size(), windowSize);
        const int added = kRows * int(columns.size());
        QCOMPARE(manager->dockContainer()->openedDockAreas().size(), int(columns.size()) * (kRows + 2));
        qInfo("BENCH %-28s %8.2f KB (도크 1개 포함, 기준선 빼지 않음)", "memory per area",
              double(after - before) / added / 1024.0);
    }
};

QTEST_MAIN(TestDockBench)
#include "tst_dock_bench.moc"
