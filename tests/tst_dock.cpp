// 도크(07) 테스트 — QDockWidget 스타일 치수(두 디자인), 도킹 관리자(fmdock): 사용자 제목 줄, 끌어 놓기(옆 · 탭 · 가장자리),
// 자동 숨김(사이드바 · 펼친 창 · 되돌림), 상태 · 이름 붙인 배치 저장, 활성 도크, 도크 탭 닫기 단추.

#include <fmdock/DockManager.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>

#include <QAbstractButton>
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QStyleOption>
#include <QTabBar>
#include <QTest>
#include <QToolBar>

using namespace Qt::StringLiterals;
namespace fs = fm::style;
using fm::dock::DockManager;

namespace {

/// 도크 넷(왼쪽 · 오른쪽 · 아래)이 있는 메인 창.
struct Fixture
{
    QMainWindow window;
    DockManager *manager = nullptr;
    QDockWidget *left = nullptr;
    QDockWidget *right = nullptr;
    QDockWidget *bottom = nullptr;
    QDockWidget *extra = nullptr;

    Fixture()
    {
        window.resize(900, 600);
        window.setCentralWidget(new QLabel(u"중앙"_s));
        manager = new DockManager(&window);
        left = manager->addDock(u"left"_s, u"왼쪽"_s, new QLineEdit, Qt::LeftDockWidgetArea);
        right = manager->addDock(u"right"_s, u"오른쪽"_s, new QLineEdit, Qt::RightDockWidgetArea);
        bottom = manager->addDock(u"bottom"_s, u"아래"_s, new QLineEdit, Qt::BottomDockWidgetArea);
        extra = manager->addDock(u"extra"_s, u"더"_s, new QLineEdit, Qt::RightDockWidgetArea);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();
    }

    static void settle()
    {
        for (int i = 0; i < 3; ++i)
            QCoreApplication::processEvents();
    }

    QToolBar *sideBar(const QString &name) const { return window.findChild<QToolBar *>(name); }
    QTabBar *dockTabBar() const
    {
        const auto bars = window.findChildren<QTabBar *>(Qt::FindDirectChildrenOnly);
        for (QTabBar *bar : bars) {
            if (bar->inherits("QMainWindowTabBar") && bar->isVisible())
                return bar;
        }
        return nullptr;
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
    }

    void titleMetrics_data()
    {
        QTest::addColumn<int>("design");
        QTest::addColumn<int>("button");
        QTest::newRow("standard") << int(fs::Design::Standard) << 22;
        QTest::newRow("watercolor") << int(fs::Design::Watercolor) << 16;
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
        const QRect text = style->subElementRect(QStyle::SE_DockWidgetTitleBarText, &opt, &dock);
        QCOMPARE(close.size(), QSize(button, button));
        QCOMPARE(floatButton.size(), QSize(button, button));
        QVERIFY(close.right() < 300 && close.right() >= 296);
        QVERIFY(floatButton.right() < close.left());
        QVERIFY(text.right() < floatButton.left());
        // 세로 제목 줄 — 단추는 위쪽, 글자는 아래쪽
        opt.verticalTitleBar = true;
        opt.rect = QRect(0, 0, 28, 300);
        const QRect vClose = style->subElementRect(QStyle::SE_DockWidgetCloseButton, &opt, &dock);
        const QRect vText = style->subElementRect(QStyle::SE_DockWidgetTitleBarText, &opt, &dock);
        QCOMPARE(vClose.size(), QSize(button, button));
        QVERIFY(vClose.top() < 6);
        QVERIFY(vText.top() > vClose.bottom());
        // 기본 제목 줄 단추에는 스타일이 종류를 붙인다
        dock.show();
        auto *closeButton = dock.findChild<QAbstractButton *>(u"qt_dockwidget_closebutton"_s);
        QVERIFY(closeButton);
        QCOMPARE(closeButton->property(fs::props::kDockButton).toString(), u"close"_s);
        QCOMPARE(closeButton->sizeHint(), QSize(button, button));
    }

    void managedTitleBar()
    {
        Fixture f;
        QWidget *title = f.left->titleBarWidget();
        QVERIFY(title);
        QCOMPARE(title->metaObject()->className(), "fm::dock::DockTitleBar");
        QStringList kinds;
        for (auto *button : title->findChildren<QAbstractButton *>()) {
            if (!button->isHidden())
                kinds << button->property(fs::props::kDockButton).toString();
        }
        kinds.sort();
        QCOMPARE(kinds, (QStringList{u"close"_s, u"float"_s, u"pin"_s}));
        // 관리 도크는 활성 여부를 관리자가 정한다(포커스로 짐작하지 않게 속성이 늘 있다)
        QVERIFY(f.bottom->property(fs::props::kPaneActive).isValid());
        QCOMPARE(f.bottom->property(fs::props::kPaneActive), QVariant(false));
    }

    void dropSides()
    {
        Fixture f;
        QVERIFY(f.manager->dropOnto(f.extra, f.left, DockManager::DropSide::Right));
        Fixture::settle();
        QCOMPARE(f.window.dockWidgetArea(f.extra), Qt::LeftDockWidgetArea);
        QVERIFY(f.window.tabifiedDockWidgets(f.left).isEmpty());
        QVERIFY(f.left->geometry().left() < f.extra->geometry().left());

        QVERIFY(f.manager->dropOnto(f.extra, f.left, DockManager::DropSide::Left));
        Fixture::settle();
        QVERIFY(f.extra->geometry().left() < f.left->geometry().left());

        QVERIFY(f.manager->dropOnto(f.extra, f.left, DockManager::DropSide::Bottom));
        Fixture::settle();
        QVERIFY(f.left->geometry().top() < f.extra->geometry().top());

        QVERIFY(f.manager->dropOnto(f.extra, f.left, DockManager::DropSide::Top));
        Fixture::settle();
        QVERIFY(f.extra->geometry().top() < f.left->geometry().top());

        QVERIFY(f.manager->dropOnto(f.extra, f.left, DockManager::DropSide::Center));
        Fixture::settle();
        QVERIFY(f.window.tabifiedDockWidgets(f.left).contains(f.extra));

        QVERIFY(f.manager->dropToEdge(f.extra, Qt::BottomDockWidgetArea));
        Fixture::settle();
        QCOMPARE(f.window.dockWidgetArea(f.extra), Qt::BottomDockWidgetArea);
        // 자기 자신 위에는 놓지 않는다
        QVERIFY(!f.manager->dropOnto(f.extra, f.extra, DockManager::DropSide::Left));
    }

    void tabCloseButton()
    {
        Fixture f;
        QVERIFY(f.manager->dropOnto(f.extra, f.right, DockManager::DropSide::Center));
        Fixture::settle();
        QTabBar *bar = f.dockTabBar();
        QVERIFY(bar);
        QVERIFY(bar->tabsClosable());
        int index = -1;
        for (int i = 0; i < bar->count(); ++i) {
            if (bar->tabText(i) == f.extra->windowTitle())
                index = i;
        }
        QVERIFY(index >= 0);
        Q_EMIT bar->tabCloseRequested(index);
        Fixture::settle();
        QVERIFY(f.extra->isHidden());
    }

    void autoHide()
    {
        Fixture f;
        const int width = f.left->width();
        f.manager->setAutoHidden(f.left, true);
        Fixture::settle();
        QVERIFY(f.manager->isAutoHidden(f.left));
        QCOMPARE(f.window.dockWidgetArea(f.left), Qt::NoDockWidgetArea);
        QToolBar *bar = f.sideBar(u"fmDockSideBarLeft"_s);
        QVERIFY(bar);
        QVERIFY(bar->isVisible());
        // 압정이 '도크에 고정'으로 바뀌고 떼어 내기는 숨는다
        QWidget *title = f.left->titleBarWidget();
        QStringList kinds;
        for (auto *button : title->findChildren<QAbstractButton *>()) {
            if (!button->isHidden())
                kinds << button->property(fs::props::kDockButton).toString();
        }
        kinds.sort();
        QCOMPARE(kinds, (QStringList{u"close"_s, u"unpin"_s}));

        f.manager->showAutoHidden(f.left);
        Fixture::settle();
        QCOMPARE(f.manager->expandedDock(), f.left);
        QWidget *overlay = f.left->parentWidget();
        QVERIFY(overlay);
        QCOMPARE(overlay->objectName(), u"fmDockAutoHideOverlay"_s);
        QVERIFY2(overlay->isVisible(), "펼친 창이 보여야 한다");
        QVERIFY(f.left->isVisible());
        QVERIFY(!overlay->geometry().isEmpty());
        QVERIFY(overlay->geometry().left() >= bar->geometry().right());

        f.manager->showAutoHidden(nullptr);
        Fixture::settle();
        QVERIFY(!overlay->isVisible());
        QVERIFY(!f.manager->expandedDock());

        f.manager->setAutoHidden(f.left, false);
        Fixture::settle();
        QVERIFY(!f.manager->isAutoHidden(f.left));
        QCOMPARE(f.window.dockWidgetArea(f.left), Qt::LeftDockWidgetArea);
        QVERIFY(f.left->isVisible());
        QVERIFY(qAbs(f.left->width() - width) < 40);
        QVERIFY(!bar->isVisible());
    }

    void autoHideOverlayBounds()
    {
        // 탭 묶음의 숨은 탭은 창 밖 좌표에 보이는 채로 남는다 — 펼친 창 자리 계산이 끌려가면 안 된다.
        // 넓은 내용은 펼친 창을 최소 폭까지 넓힌다(좁히면 제목 줄 단추가 잘린다).
        Fixture f;
        QVERIFY(f.manager->dropOnto(f.extra, f.right, DockManager::DropSide::Center));
        auto *wide = new QLabel(QString(u'가').repeated(40));
        QDockWidget *log = f.manager->addDock(u"log"_s, u"로그"_s, wide, Qt::LeftDockWidgetArea);
        f.manager->setAutoHidden(log, true);
        Fixture::settle();
        f.manager->showAutoHidden(log);
        Fixture::settle();
        QWidget *overlay = log->parentWidget();
        QVERIFY(overlay && overlay->isVisible());
        QVERIFY2(f.window.rect().contains(overlay->geometry()), "펼친 창은 메인 창 안에 있어야 한다");
        QVERIFY(overlay->width() >= overlay->minimumSizeHint().width());
        QVERIFY(log->geometry().right() < overlay->width());
    }

    void stateAndLayouts()
    {
        Fixture f;
        f.manager->setAutoHidden(f.bottom, true);
        QVERIFY(f.manager->dropOnto(f.extra, f.right, DockManager::DropSide::Center));
        Fixture::settle();
        const QByteArray state = f.manager->saveState();
        f.manager->saveLayout(u"작업"_s);
        QCOMPARE(f.manager->layoutNames(), QStringList{u"작업"_s});

        // 배치를 바꾼 뒤 되돌린다
        f.manager->setAutoHidden(f.bottom, false);
        QVERIFY(f.manager->dropToEdge(f.extra, Qt::LeftDockWidgetArea));
        Fixture::settle();
        QVERIFY(!f.manager->isAutoHidden(f.bottom));
        QVERIFY(f.manager->restoreState(state));
        Fixture::settle();
        QVERIFY(f.manager->isAutoHidden(f.bottom));
        QVERIFY(f.window.tabifiedDockWidgets(f.right).contains(f.extra));

        QVERIFY(f.manager->applyLayout(u"작업"_s));
        f.manager->removeLayout(u"작업"_s);
        QVERIFY(f.manager->layoutNames().isEmpty());
        QVERIFY(!f.manager->restoreState(QByteArray("garbage")));
    }

    void activeDock()
    {
        Fixture f;
        f.window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&f.window));
        f.right->widget()->setFocus();
        Fixture::settle();
        QCOMPARE(f.manager->activeDock(), f.right);
        QCOMPARE(f.right->property(fs::props::kPaneActive), QVariant(true));
        QCOMPARE(f.left->property(fs::props::kPaneActive), QVariant(false));
        f.left->widget()->setFocus();
        Fixture::settle();
        QCOMPARE(f.manager->activeDock(), f.left);
        QCOMPARE(f.right->property(fs::props::kPaneActive), QVariant(false));
    }
};

QTEST_MAIN(TestDock)
#include "tst_dock.moc"
