// 메인 창(fmdemo) 동작 테스트 — 보드 기본 상태, 패널 전환, 샘플 폴더 이동 · 위로(커서 복원), 방문 기록, 탭, 보기 방식.

#include "FilePanel.h"
#include "MainWindow.h"

#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmfilelist/ThumbnailView.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/BreadcrumbBar.h>

#include <QAction>
#include <QStackedWidget>
#include <QTabBar>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace fm::app;
namespace fl = fm::filelist;

class TestMainWindow : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void boardState();
    void switchPanel();
    void enterAndGoUp();
    void history();
    void tabs();
    void viewModes();
    void breadcrumbs();

private:
    QString nameAt(FilePanel *panel, int row) const
    {
        return panel->model()->index(row, fl::NameColumn).data(fl::FullNameRole).toString();
    }
    int rowOf(FilePanel *panel, const QString &name) const
    {
        for (int r = 0; r < panel->model()->rowCount(); ++r)
            if (nameAt(panel, r) == name)
                return r;
        return -1;
    }
    std::unique_ptr<MainWindow> m_window;
};

void TestMainWindow::initTestCase()
{
    fm::style::ThemeManager::instance().setScheme(fm::style::ThemeManager::Scheme::Light);
    fm::style::ThemeManager::instance().install(*qApp);
    m_window = std::make_unique<MainWindow>();
    m_window->loadBoardState();
    m_window->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
}

void TestMainWindow::boardState()
{
    QCOMPARE(m_window->activePanel(), m_window->rightPanel());
    QCOMPARE(m_window->windowTitle(), u"D:\\Downloads — 파일 관리자"_s);
    FilePanel *left = m_window->leftPanel();
    QCOMPARE(left->currentPath(), u"D:\\Work\\fm-core"_s);
    QCOMPARE(left->viewMode(), fl::ViewMode::OneLine);
    QCOMPARE(left->listView()->cursorRow(), 7);  // src
    QCOMPARE(nameAt(left, 7), u"src"_s);
    FilePanel *right = m_window->rightPanel();
    QCOMPARE(right->listView()->cursorRow(), 3);
    QTRY_VERIFY(right->listView()->isTwoLine());  // 자동 → 긴 이름이 많아 2줄
    QCOMPARE(right->thumbnailBackend(), fl::ThumbnailView::QtitanCards);
    QCOMPARE(left->thumbnailBackend(), fl::ThumbnailView::QtList);
}

void TestMainWindow::switchPanel()
{
    QAction *tab = m_window->findChild<QAction *>(u"switchPanel"_s);
    QVERIFY(tab);
    tab->trigger();
    QCOMPARE(m_window->activePanel(), m_window->leftPanel());
    QCOMPARE(m_window->windowTitle(), u"D:\\Work\\fm-core — 파일 관리자"_s);
    QVERIFY(m_window->leftPanel()->isActive());
    QVERIFY(!m_window->rightPanel()->isActive());
    tab->trigger();
    QCOMPARE(m_window->activePanel(), m_window->rightPanel());
}

void TestMainWindow::enterAndGoUp()
{
    FilePanel *left = m_window->leftPanel();
    left->listView()->setCursorRow(7);
    Q_EMIT left->listView()->activated(left->model()->index(7, fl::NameColumn));  // src 열기
    QCOMPARE(left->currentPath(), u"D:\\Work\\fm-core\\src"_s);
    QCOMPARE(left->model()->rowCount(), 1);  // 샘플에 없는 폴더: ".."만
    left->goUp();
    QCOMPARE(left->currentPath(), u"D:\\Work\\fm-core"_s);
    QCOMPARE(left->listView()->cursorRow(), rowOf(left, u"src"_s));  // 방금 나온 폴더에 커서
    left->goUp();
    QCOMPARE(left->currentPath(), u"D:\\Work"_s);
    QCOMPARE(nameAt(left, left->listView()->cursorRow()), u"fm-core"_s);
    left->goUp();
    QCOMPARE(left->currentPath(), u"D:\\"_s);
    left->goUp();  // 드라이브 루트에서는 그대로
    QCOMPARE(left->currentPath(), u"D:\\"_s);
}

void TestMainWindow::history()
{
    FilePanel *left = m_window->leftPanel();
    left->openLocation(false, u"D:\\Downloads"_s);
    QCOMPARE(left->currentPath(), u"D:\\Downloads"_s);
    left->goBack();
    QCOMPARE(left->currentPath(), u"D:\\"_s);
    left->goForward();
    QCOMPARE(left->currentPath(), u"D:\\Downloads"_s);
    left->openLocation(false, u"D:\\Work\\fm-core"_s);
}

void TestMainWindow::tabs()
{
    FilePanel *left = m_window->leftPanel();
    left->listView()->setCursorRow(12);
    QTabBar *bar = left->findChild<QTabBar *>();
    QVERIFY(bar);
    QCOMPARE(bar->count(), 3);
    QCOMPARE(bar->tabText(0), u"fm-core"_s);
    bar->setCurrentIndex(1);
    QCOMPARE(left->currentPath(), u"D:\\Work\\qtitan-samples"_s);
    bar->setCurrentIndex(0);
    QCOMPARE(left->currentPath(), u"D:\\Work\\fm-core"_s);
    QCOMPARE(left->listView()->cursorRow(), 12);  // 탭마다 커서를 기억

    left->newTab();
    QCOMPARE(bar->count(), 4);
    QCOMPARE(left->currentTab(), 1);
    QCOMPARE(left->currentPath(), u"D:\\Work\\fm-core"_s);
    QCOMPARE(left->viewMode(), fl::ViewMode::Auto);  // 새 탭 기본값 = 자동
    left->closeCurrentTab();
    QCOMPARE(bar->count(), 3);
}

void TestMainWindow::viewModes()
{
    FilePanel *right = m_window->rightPanel();
    auto *stack = right->findChild<QStackedWidget *>();
    QVERIFY(stack);
    right->setViewMode(fl::ViewMode::Thumbnails);
    QCOMPARE(stack->currentWidget(), static_cast<QWidget *>(right->thumbnailView()));
    QCOMPARE(right->thumbnailView()->cursorRow(), 3);  // 보기를 바꿔도 커서 유지
    right->setViewMode(fl::ViewMode::OneLine);
    QCOMPARE(stack->currentWidget(), static_cast<QWidget *>(right->listView()));
    QVERIFY(!right->listView()->isTwoLine());
    QCOMPARE(right->listView()->cursorRow(), 3);
    right->setViewMode(fl::ViewMode::Auto);
}

void TestMainWindow::breadcrumbs()
{
    FilePanel *left = m_window->leftPanel();
    auto *crumbs = left->findChild<fm::ui::BreadcrumbBar *>();
    QVERIFY(crumbs);
    QCOMPARE(crumbs->segments(), (QStringList{u"Work"_s, u"fm-core"_s}));
    Q_EMIT crumbs->segmentClicked(0);
    QCOMPARE(left->currentPath(), u"D:\\Work"_s);
    QCOMPARE(crumbs->segments(), QStringList{u"Work"_s});
}

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
