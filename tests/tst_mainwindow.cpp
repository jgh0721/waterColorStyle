// 메인 창(fmdemo) 동작 테스트 — 보드 기본 상태, 패널 전환, 샘플 폴더 이동 · 위로(커서 복원), 방문 기록, 탭, 보기 방식.

#include "FilePanel.h"
#include "MainWindow.h"

#include <fmdialogs/ElevationDialog.h>
#include <fmdialogs/ElevationFlow.h>
#include <fmdialogs/ProgressDialog.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmdialogs/SettingsDialog.h>
#include <fmfilelist/ThumbnailView.h>
#include <fmsettings/SettingsStore.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/BreadcrumbBar.h>
#include <fmwidgets/CommandLine.h>
#include <fmwidgets/FunctionKeyBar.h>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QPointer>
#include <QStackedWidget>
#include <QTabBar>
#include <QTest>
#include <QTimer>

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
    void fileOperations();
    void elevationFlowMenu();
    void settingsWiring();
    void designSwitchFromSettings();

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

// 기능 키 → 대화상자(P5). 모달 대화상자는 열린 뒤 타이머로 닫고, 어떤 대화상자가 어떤 입력으로 열렸는지 본다.
void TestMainWindow::fileOperations()
{
    m_window->loadBoardState();  // 오른쪽 D:\Downloads 활성 · 커서 PDF
    struct Seen
    {
        QString className;
        QString title;
    };
    auto run = [this](const QString &id, bool accept) {
        Seen seen;
        QTimer::singleShot(50, this, [&seen, accept] {
            auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!modal)
                return;
            seen.className = QString::fromLatin1(modal->metaObject()->className());
            seen.title = modal->windowTitle();
            accept ? modal->accept() : modal->reject();
        });
        m_window->findChild<QAction *>(id)->trigger();
        return seen;
    };

    QCOMPARE(run(u"copy"_s, false).className, u"fm::dialogs::CopyDialog"_s);
    QCOMPARE(run(u"move"_s, false).className, u"fm::dialogs::MoveRenameDialog"_s);
    QCOMPARE(run(u"rename"_s, false).className, u"fm::dialogs::MoveRenameDialog"_s);
    QCOMPARE(run(u"newFolder"_s, false).className, u"fm::dialogs::NewFolderDialog"_s);
    QCOMPARE(run(u"newFile"_s, false).className, u"fm::dialogs::NewFileDialog"_s);
    // 대상은 표시한 행(보드의 Downloads는 3개 표시) — 없으면 커서 행
    QCOMPARE(m_window->rightPanel()->operationRows().size(), 3);
    const Seen rename = run(u"multiRename"_s, false);
    QCOMPARE(rename.className, u"fm::dialogs::MultiRenameDialog"_s);
    QCOMPARE(rename.title, u"다중 이름 변경 — 3개 파일 · D:\\Downloads"_s);

    // 삭제 확인 → 진행 창(모덜리스)이 뜬다
    QCOMPARE(run(u"delete"_s, true).className, u"fm::dialogs::DeleteDialog"_s);
    const auto progress = m_window->findChildren<fm::dialogs::ProgressDialog *>();
    QCOMPARE(progress.size(), 1);
    QVERIFY(progress.first()->isVisible());
    QCOMPARE(progress.first()->mode(), fm::dialogs::ProgressDialog::Compact);
    progress.first()->close();
    QTRY_VERIFY(m_window->findChildren<fm::dialogs::ProgressDialog *>().isEmpty());  // WA_DeleteOnClose

    // 표시가 없고 커서가 상위 폴더("..")면 대상이 없다 — 대화상자를 열지 않는다
    m_window->rightPanel()->model()->markAll(false);
    QCOMPARE(m_window->rightPanel()->operationRows().size(), 1);  // 커서 행
    m_window->rightPanel()->listView()->setCursorRow(0);
    QVERIFY(run(u"copy"_s, false).className.isEmpty());
    QTest::qWait(100);  // 열리지 않은 대화상자용 타이머가 지나가게
}

// 도구 › 권한 흐름 시뮬레이션 — 첫 창이 뜨고, 취소하면 흐름이 끝나 스스로 지워진다.
void TestMainWindow::elevationFlowMenu()
{
    QPointer<fm::dialogs::ElevationFlow> flow = m_window->startElevationFlow(0);
    QVERIFY(flow);
    QTRY_VERIFY(qobject_cast<fm::dialogs::ElevationDialog *>(flow->currentDialog()));
    auto *dialog = qobject_cast<fm::dialogs::ElevationDialog *>(flow->currentDialog());
    QCOMPARE(dialog->parentWidget(), static_cast<QWidget *>(m_window.get()));
    dialog->choose(fm::dialogs::elev::Choice::Cancel);
    QTRY_VERIFY(flow.isNull());  // finished → deleteLater
}

void TestMainWindow::settingsWiring()
{
    namespace st = fm::settings;
    auto &store = st::SettingsStore::instance();
    QVERIFY(store.filePath().isEmpty());  // 테스트는 메모리 보관소
    m_window->captureSettings();
    const st::AppSettings original = store.settings();
    QVERIFY(original.panel.showHidden);  // 보드 상태를 옮겼다

    // 설정 명령 → 설정 창 하나(다시 눌러도 같은 창)
    QAction *settings = m_window->findChild<QAction *>(u"settings"_s);
    QVERIFY(settings);
    settings->trigger();
    QPointer<fm::dialogs::SettingsDialog> dialog = m_window->findChild<fm::dialogs::SettingsDialog *>();
    QVERIFY(dialog && dialog->isVisible());
    settings->trigger();
    QCOMPARE(m_window->findChildren<fm::dialogs::SettingsDialog *>().size(), 1);

    // 적용 → 창에 반영: 명령줄 · 기능 키 막대, 숨김 파일, 단축키
    dialog->session()->edit(st::Section::General, [](st::AppSettings &p) {
        p.general.showCommandLine = false;
        p.general.showFunctionKeyBar = false;
    });
    dialog->session()->edit(st::Section::Panel, [](st::AppSettings &p) { p.panel.showHidden = false; });
    dialog->session()->edit(st::Section::Keys, [](st::AppSettings &p) {
        p.keys.overrides.insert(u"newFolder"_s, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N)});
    });
    dialog->applyPending();
    QVERIFY(!m_window->findChild<fm::ui::CommandLine *>()->isVisible());
    QVERIFY(!m_window->findChild<fm::ui::FunctionKeyBar *>()->isVisible());
    QVERIFY(!m_window->findChild<QAction *>(u"showHidden"_s)->isChecked());
    QAction *newFolder = m_window->findChild<QAction *>(u"newFolder"_s);
    QCOMPARE(newFolder->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    QVERIFY(newFolder->toolTip().contains(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N).toString(QKeySequence::NativeText)));

    // 확인으로 닫고 원래 설정으로 되돌린다
    dialog->accept();
    QTRY_VERIFY(dialog.isNull());
    store.setSettings(original);
    QVERIFY(m_window->findChild<fm::ui::CommandLine *>()->isVisible());
    QCOMPARE(newFolder->shortcut(), QKeySequence(Qt::Key_F7));
}

void TestMainWindow::designSwitchFromSettings()
{
    // 설정 창에서 디자인을 워터컬러로 적용하면 앱이 종료되던 문제 — 앱 스타일 교체 중 Qtitan 그리드가
    // QApplication::setStyle을 다시 불렀다(재진입). 설정 창(미리보기 그리드 여럿)을 연 채 두 디자인을 오간다.
    namespace st = fm::settings;
    namespace fs = fm::style;
    m_window->captureSettings();
    const st::AppSettings original = st::SettingsStore::instance().settings();
    m_window->openSettings();
    QPointer<fm::dialogs::SettingsDialog> dialog = m_window->findChild<fm::dialogs::SettingsDialog *>();
    QVERIFY(dialog);
    for (const QString &page : {u"panel"_s, u"groups"_s, u"columns"_s, u"theme"_s, u"thumbs"_s})
        dialog->setCurrentPage(page);  // 미리보기 그리드를 모두 만든다
    for (const fs::Design design : {fs::Design::Watercolor, fs::Design::Standard, fs::Design::Watercolor}) {
        dialog->session()->edit(st::Section::Appearance, [design](st::AppSettings &p) { p.appearance.design = design; });
        dialog->applyPending();
        QCOMPARE(fs::ThemeManager::instance().design(), design);
        QTest::qWait(50);
    }
    dialog->reject();
    QTRY_VERIFY(dialog.isNull());

    // 시작 때처럼 보이기 전의 창에서 저장된 디자인을 적용
    {
        MainWindow hidden;
        hidden.loadBoardState();
        st::AppSettings s = original;
        s.appearance.design = fs::Design::Standard;
        st::SettingsStore::instance().setSettings(s);
        s.appearance.design = fs::Design::Watercolor;
        st::SettingsStore::instance().setSettings(s);
        QCOMPARE(fs::ThemeManager::instance().design(), fs::Design::Watercolor);
    }
    st::SettingsStore::instance().setSettings(original);
    QCOMPARE(fs::ThemeManager::instance().design(), original.appearance.design);
}

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
