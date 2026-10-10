// 메인 창(fmdemo) 동작 테스트 — 보드 기본 상태, 패널 전환, 샘플 폴더 이동 · 위로(커서 복원), 방문 기록, 탭, 보기 방식,
// 설정 연결 · 디자인 전환, 도구 창(대화상자 카탈로그 · 섬네일 비교) · 일괄 스냅숏.

#include "CatalogWindow.h"
#include "DockPanes.h"
#include "FilePanel.h"
#include "MainWindow.h"
#include "SingleInstance.h"
#include "Snapshots.h"
#include "ThumbnailCompare.h"

#include <fmdialogs/DialogCatalog.h>
#include <fmdialogs/ElevationDialog.h>
#include <fmdialogs/ElevationFlow.h>
#include <fmdialogs/ProgressDialog.h>
#include <fmdialogs/ProgressSimulator.h>
#include <fmdock/DockManager.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmdialogs/SettingsDialog.h>
#include <fmfilelist/ThumbnailProvider.h>
#include <fmfilelist/ThumbnailView.h>
#include <fmsettings/SettingsStore.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/BreadcrumbBar.h>
#include <fmstyle/StylePaint.h>
#include <fmwidgets/CommandLine.h>
#include <fmwidgets/FunctionKeyBar.h>
#include <fmwidgets/SettingsWidgets.h>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QImage>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QScrollBar>
#include <QSet>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeView>
#include <QTreeWidget>

#include <atomic>
#include <thread>

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
    void appPaletteKeptOnStyleWrap();
    void panelAndTabSettings();
    void listFontAndDensity();
    void thumbnailSettings();
    void jobAndKeySettings();
    void generalSettings();
    void columnSetsInPanels();
    void docks();
    void catalogWindow();
    void thumbnailCompare();
    void snapshots();

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
#if FM_WITH_QTITAN
    QTRY_VERIFY(right->listView()->isTwoLine());  // 자동 → 긴 이름이 많아 2줄
    QCOMPARE(right->thumbnailBackend(), fl::ThumbnailView::QtitanCards);
#else
    QCOMPARE(right->thumbnailBackend(), fl::ThumbnailView::QtList);  // 카드 구현이 없다
#endif
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
    QVERIFY(!progress.first()->isVisible());  // 설정 기본값: 진행 창은 1초 뒤에 띄운다
    QTRY_VERIFY_WITH_TIMEOUT(progress.first()->isVisible(), 3000);
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

void TestMainWindow::appPaletteKeptOnStyleWrap()
{
    // Qtitan 그리드가 앱 스타일을 CommonStyle로 감쌀 때 앱 팔레트를 빈 팔레트로 지웠다(Q11) — 다크 구성표에서
    // Qt 목록(섬네일 · 카탈로그 트리)의 바탕이 시스템 색(#2d2d2d)이 되었다. 디자인 전환 뒤의 감싸기(Q9, 한 차례 늦음)까지 본다.
#if !FM_WITH_QTITAN
    QSKIP("QtitanDataGrid 없는 빌드 — 앱 스타일을 감싸는 Qtitan 그리드가 없다");
#else
    namespace fs = fm::style;
    auto &tm = fs::ThemeManager::instance();
    const fs::Design design = tm.design();
    const fs::ThemeManager::Scheme scheme = tm.scheme();
    tm.setScheme(fs::ThemeManager::Scheme::Dark);
    for (const fs::Design d : {fs::Design::Watercolor, fs::Design::Standard}) {
        tm.setDesign(d);
        QTest::qWait(20);
        { fl::FileListView grid; }  // 새 그리드도 감싸기를 확인한다
        QTest::qWait(20);
        const QColor surface = tm.colors()[fs::Token::Surface];
        QCOMPARE(QApplication::palette().color(QPalette::Base), surface);
        QCOMPARE(QApplication::palette("QAbstractItemView").color(QPalette::Base), surface);
    }
    // 감싸면서 넣은 위젯 종류별 팔레트(그 순간의 머리글 팔레트)는 다음 색 구성표 전환에서 지워진다
    tm.setScheme(fs::ThemeManager::Scheme::Light);
    QTest::qWait(20);
    {
        fl::FileListView view;
        const QList<QWidget *> children = view.findChildren<QWidget *>();
        const auto grid = std::find_if(children.begin(), children.end(), [](const QWidget *w) { return w->inherits("Qtitan::GridBase"); });
        QVERIFY(grid != children.end());
        QCOMPARE(QApplication::palette(*grid).color(QPalette::Base), tm.colors()[fs::Token::Surface]);
    }
    tm.setDesign(design);
    tm.setScheme(scheme);
    QTest::qWait(20);
#endif
}

void TestMainWindow::panelAndTabSettings()
{
    // 설정 › 파일 패널(보호된 OS 파일 · 크기 단위 · 날짜 형식 · 기본 표시 방식) · 일반 › 폴더 탭을 메인 창에 적용
    namespace st = fm::settings;
    m_window->loadBoardState();
    m_window->captureSettings();
    const st::AppSettings original = st::SettingsStore::instance().settings();
    QVERIFY(original.panel.showProtectedOs);  // 보드는 desktop.ini(숨김 · 시스템)를 보인다
    FilePanel *right = m_window->rightPanel();
    QTabBar *tabs = right->findChild<QTabBar *>();
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 2);
    QVERIFY(rowOf(right, u"desktop.ini"_s) >= 0);

    st::AppSettings s = original;
    s.panel.showProtectedOs = false;
    s.panel.sizeUnit = st::PanelSettings::SizeUnit::KB;
    s.panel.dateFormat = u"yy.MM.dd"_s;
    s.panel.defaultViewMode = fl::ViewMode::OneLine;
    s.tabs.title = st::TabSettings::TabTitle::DriveAndFolder;
    s.tabs.newTabPosition = st::TabSettings::NewTabPosition::End;
    st::SettingsStore::instance().setSettings(s);

    QCOMPARE(rowOf(right, u"desktop.ini"_s), -1);  // 숨김은 보이되 보호된 OS 파일은 숨김
    const int vc = rowOf(right, u"vc_redist.x64.exe"_s);
    QVERIFY(vc >= 0);
    const qint64 bytes = right->model()->index(vc, fl::NameColumn).data(fl::SizeBytesRole).toLongLong();
    QCOMPARE(right->model()->index(vc, fl::SizeColumn).data().toString(), fl::formatSize(bytes, fl::SizeUnit::KB));
    QVERIFY(right->model()->index(vc, fl::SizeColumn).data().toString().endsWith(u" KB"_s));
    QCOMPARE(right->model()->index(vc, fl::ModifiedColumn).data().toString(), u"26.09.22"_s);
    QCOMPARE(tabs->tabText(0), u"D:\\Downloads"_s);
    QCOMPARE(right->viewMode(), fl::ViewMode::Auto);  // 탭이 정한 방식은 그대로
    tabs->setCurrentIndex(1);
    QCOMPARE(right->viewMode(), fl::ViewMode::OneLine);  // 정하지 않은 탭은 새 기본값
    tabs->setCurrentIndex(0);
    right->newTab();
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->currentIndex(), 2);  // 맨 끝
    QCOMPARE(right->viewMode(), fl::ViewMode::OneLine);
    right->closeCurrentTab();
    QCOMPARE(tabs->count(), 2);

    // 탭마다 기억하지 않으면 모든 탭이 기본값을 쓰고, 바꾸면 패널 전체가 바뀐다
    s.tabs.rememberViewPerTab = false;
    st::SettingsStore::instance().setSettings(s);
    tabs->setCurrentIndex(0);
    QCOMPARE(right->viewMode(), fl::ViewMode::OneLine);
    right->setViewMode(fl::ViewMode::TwoLine);
    tabs->setCurrentIndex(1);
    QCOMPARE(right->viewMode(), fl::ViewMode::TwoLine);

    st::SettingsStore::instance().setSettings(original);
    m_window->loadBoardState();
    QVERIFY(rowOf(right, u"desktop.ini"_s) >= 0);
    QCOMPARE(tabs->tabText(0), u"Downloads"_s);
}

void TestMainWindow::listFontAndDensity()
{
    // 설정 › 일반 · 모양 — 목록 글꼴 · 행 밀도는 파일 목록에, 고정폭 글꼴은 명령줄 · monoFont()에 바로 반영
    namespace st = fm::settings;
    namespace fs = fm::style;
    m_window->captureSettings();
    const st::AppSettings original = st::SettingsStore::instance().settings();
    FilePanel *right = m_window->rightPanel();
    auto *commandLine = m_window->findChild<fm::ui::CommandLine *>();
    QVERIFY(commandLine);
    const int normalHeight = right->listView()->preferredHeight(10);

    st::AppSettings s = original;
    s.appearance.listFontFamily = u"Malgun Gothic"_s;
    s.appearance.listFontPx = 15;
    s.appearance.density = st::AppearanceSettings::Density::Relaxed;
    s.appearance.monoFontFamily = u"Consolas"_s;
    s.appearance.monoFontPx = 14;
    st::SettingsStore::instance().setSettings(s);
    QTest::qWait(20);

    const QFont listFont = right->listView()->font();
    QCOMPARE(listFont.families().value(0), u"Malgun Gothic"_s);
    QCOMPARE(listFont.pixelSize(), 15);
    QCOMPARE(m_window->listAppearance().density, fl::RowDensity::Relaxed);
    QVERIFY(right->listView()->preferredHeight(10) > normalHeight);  // 행이 높아졌다
    QCOMPARE(right->thumbnailView()->font().families().value(0), u"Malgun Gothic"_s);
    QCOMPARE(fs::ThemeManager::instance().monoFontFamily(), u"Consolas"_s);
    QCOMPARE(fs::monoFont(12).families().value(0), u"Consolas"_s);
    QCOMPARE(commandLine->lineEdit()->font().families().value(0), u"Consolas"_s);
    QCOMPARE(commandLine->lineEdit()->font().pointSizeF(), 14 * 0.75);

    st::SettingsStore::instance().setSettings(original);
    QTest::qWait(20);
    QCOMPARE(right->listView()->preferredHeight(10), normalHeight);
    QCOMPARE(fs::monoFont(12).families().value(0), u"Cascadia Mono"_s);
}

void TestMainWindow::thumbnailSettings()
{
    // 설정 › 섬네일 보기 — 만들기 설정은 두 패널의 생성기로, 자동 섬네일은 폴더를 열 때 판정
    namespace st = fm::settings;
    m_window->loadBoardState();
    m_window->captureSettings();
    const st::AppSettings original = st::SettingsStore::instance().settings();
    FilePanel *right = m_window->rightPanel();

    st::AppSettings s = original;
    s.thumbs.provider = st::ThumbSettings::Provider::Builtin;
    s.thumbs.targets = st::ThumbSettings::Images;
    s.thumbs.concurrency = 2;
    s.thumbs.iconsOnlyOnNetworkRemovable = false;
    s.thumbs.autoThumbnailFolders = true;
    s.thumbs.autoThumbnailPct = 50;
    st::SettingsStore::instance().setSettings(s);
    for (FilePanel *panel : {m_window->leftPanel(), right}) {
        fl::ThumbnailProvider *provider = panel->thumbnailProvider();
        QCOMPARE(provider->method(), fl::ThumbnailProvider::Method::Builtin);
        QCOMPARE(provider->targets(), int(fl::ThumbnailProvider::Images));
        QCOMPARE(provider->maxThreads(), 2);
        QVERIFY(!provider->skipsSlowVolumes());
    }

    // D:\Pictures — 파일 8개 중 이미지 · 동영상 4개(50 %)
    QCOMPARE(right->viewMode(), fl::ViewMode::Auto);
    right->openLocation(false, u"D:\\Pictures"_s);
    QCOMPARE(right->viewMode(), fl::ViewMode::Thumbnails);
    right->openLocation(false, u"D:\\Backup"_s);
    QCOMPARE(right->viewMode(), fl::ViewMode::Auto);  // 탭의 방식으로 돌아간다
    s.thumbs.autoThumbnailPct = 90;
    st::SettingsStore::instance().setSettings(s);
    right->openLocation(false, u"D:\\Pictures"_s);
    QCOMPARE(right->viewMode(), fl::ViewMode::Auto);
    s.thumbs.autoThumbnailPct = 50;
    st::SettingsStore::instance().setSettings(s);  // 적용하면 지금 폴더도 다시 판정
    QCOMPARE(right->viewMode(), fl::ViewMode::Thumbnails);
    right->setViewMode(fl::ViewMode::OneLine);  // 직접 고르면 자동이 풀린다
    QCOMPARE(right->viewMode(), fl::ViewMode::OneLine);

    st::SettingsStore::instance().setSettings(original);
    m_window->loadBoardState();
}

void TestMainWindow::jobAndKeySettings()
{
    // 설정 › 파일 작업(동시 작업 수 · 진행 창 표시 · 기본 삭제 방식 · 삭제 전 확인) · 키보드(기능 키 막대 글자)
    namespace st = fm::settings;
    namespace fd = fm::dialogs;
    m_window->loadBoardState();
    m_window->captureSettings();
    const st::AppSettings original = st::SettingsStore::instance().settings();
    st::AppSettings s = original;
    s.fileOps.maxConcurrentJobs = 1;
    s.fileOps.progressDelayMs = 0;
    st::SettingsStore::instance().setSettings(s);

    const qint64 big = qint64(8) << 30;
    QPointer<fd::ProgressDialog> first = m_window->startJob(fd::ProgressDialog::Copy, u"D:\\a"_s, u"E:\\b"_s, {u"a.bin"_s}, {big});
    QPointer<fd::ProgressDialog> second = m_window->startJob(fd::ProgressDialog::Copy, u"D:\\a"_s, u"E:\\b"_s, {u"b.bin"_s}, {big});
    QCOMPARE(m_window->runningJobs(), 1);
    QVERIFY(!first->isWaiting());
    QVERIFY(second->isWaiting());
    QVERIFY(second->isVisible());  // 대기 중인 작업도 보인다
    first->close();                // 취소하면 다음 작업이 시작된다
    QTRY_VERIFY(!second->isWaiting());
    second->close();
    QTRY_COMPARE(m_window->jobs().size(), 0);

    // 진행 창 표시 안 함 — 띄우지 않고, 끝나면(완료되면 닫기를 꺼도) 창을 남기지 않는다
    s.fileOps.progressDelayMs = -1;
    s.fileOps.closeWhenDone = false;
    st::SettingsStore::instance().setSettings(s);
    QPointer<fd::ProgressDialog> hidden = m_window->startJob(fd::ProgressDialog::Copy, u"D:\\a"_s, u"E:\\b"_s, {u"c.bin"_s}, {2 * 1024 * 1024});
    QVERIFY(!hidden->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(hidden.isNull(), 5000);

    // 기본 삭제 방식 = 영구 삭제, 삭제 전 확인 끔 — F8은 바로 영구 삭제, Shift+Del은 휴지통
    s.fileOps.progressDelayMs = 0;
    s.fileOps.confirmDelete = false;
    s.fileOps.deleteMode = st::FileOpsSettings::DeleteMode::Permanent;
    st::SettingsStore::instance().setSettings(s);
    m_window->setActivePanel(m_window->rightPanel());  // 보드: 오른쪽에 표시 3개
    m_window->findChild<QAction *>(u"delete"_s)->trigger();
    QCOMPARE(m_window->jobs().size(), 1);
    QVERIFY(m_window->jobs().first()->windowTitle().contains(u"영구 삭제 중"_s));
    m_window->jobs().first()->close();
    QTRY_COMPARE(m_window->jobs().size(), 0);
    m_window->findChild<QAction *>(u"deletePermanent"_s)->trigger();
    QCOMPARE(m_window->jobs().size(), 1);
    const QString title = m_window->jobs().first()->windowTitle();
    QVERIFY(title.contains(u"삭제 중"_s) && !title.contains(u"영구"_s));
    m_window->jobs().first()->close();
    QTRY_COMPARE(m_window->jobs().size(), 0);

    // 기능 키 막대 — 복사를 F9로 바꾸면 F5 칸의 글자도 F9
    auto *keys = m_window->findChild<fm::ui::FunctionKeyBar *>();
    QVERIFY(keys);
    auto copyButton = [keys] {
        for (fm::ui::FunctionKeyButton *b : keys->buttons())
            if (b->property("fmCommand").toString() == u"copy")
                return b;
        return static_cast<fm::ui::FunctionKeyButton *>(nullptr);
    };
    QVERIFY(copyButton());
    QCOMPARE(copyButton()->keys(), u"F5"_s);
    s.keys.overrides.insert(u"copy"_s, {QKeySequence(Qt::Key_F9)});
    st::SettingsStore::instance().setSettings(s);
    QCOMPARE(copyButton()->keys(), u"F9"_s);

    st::SettingsStore::instance().setSettings(original);
    QCOMPARE(copyButton()->keys(), u"F5"_s);
    m_window->loadBoardState();
}

void TestMainWindow::generalSettings()
{
    // 설정 › 일반 — 창 하나만 실행(로컬 소켓) · 시작할 때(마지막 탭 복원 · 지정한 폴더) · 다른 인스턴스의 요청은 새 탭
    namespace st = fm::settings;
    {
        const QString key = u"fmdemo-test-"_s + QString::number(QCoreApplication::applicationPid());
        SingleInstance first(key);
        QVERIFY(first.listen());
        QSignalSpy received(&first, &SingleInstance::messageReceived);
        // 두 번째 인스턴스는 다른 프로세스 — 여기서는 다른 스레드에서 보낸다(받는 쪽 이벤트 루프가 돌아야 쓰기가 끝난다)
        auto forwardFromThread = [&key](const QString &message) {
            std::atomic<bool> ok = false;
            std::thread sender([&] { ok = SingleInstance(key).forward(message, 3000); });
            [[maybe_unused]] const bool done = QTest::qWaitFor([&] { return ok.load(); }, 4000);
            sender.join();
            return ok.load();
        };
        QVERIFY(forwardFromThread(u"C:\\Work"_s));
        QTRY_COMPARE(received.size(), 1);
        QCOMPARE(received.first().first().toString(), u"C:\\Work"_s);
        QVERIFY(forwardFromThread(QString()));  // 폴더 없이 다시 실행 → 새 탭만
        QTRY_COMPARE(received.size(), 2);
        QCOMPARE(received.last().first().toString(), QString());
        QVERIFY(!SingleInstance(key + u"-none"_s).forward(u"x"_s, 200));  // 듣는 인스턴스가 없으면 직접 뜬다
    }

    m_window->loadBoardState();
    m_window->captureSettings();
    const st::AppSettings original = st::SettingsStore::instance().settings();

    // 세션 — 두 패널의 탭 · 보기 방식 · 활성 패널, JSON으로 저장 · 복원
    const st::SessionState board = m_window->sessionState();
    QCOMPARE(board.left.size(), 3);
    QCOMPARE(board.right.size(), 2);
    QCOMPARE(board.left.first().path, u"D:\\Work\\fm-core"_s);
    QCOMPARE(board.left.first().mode, fl::ViewMode::OneLine);
    QVERIFY(board.left.first().modeSet);
    QVERIFY(board.left.at(2).local);
    QVERIFY(board.rightActive);
    st::AppSettings saved = original;
    saved.session = board;
    QCOMPARE(st::AppSettings::fromJson(saved.toJson()).session, board);
    QVERIFY(st::differingSections(original, saved) & st::Section::Session);

    st::SessionState changed = board;
    changed.left = {{false, u"D:\\Backup"_s, fl::ViewMode::TwoLine, true}};
    changed.leftCurrent = 0;
    changed.rightCurrent = 1;
    changed.rightActive = false;
    m_window->restoreSession(changed);
    QCOMPARE(m_window->leftPanel()->tabs().size(), 1);
    QCOMPARE(m_window->leftPanel()->currentPath(), u"D:\\Backup"_s);
    QCOMPARE(m_window->leftPanel()->viewMode(), fl::ViewMode::TwoLine);
    QCOMPARE(m_window->rightPanel()->currentPath(), u"D:\\Backup"_s);  // 오른쪽 두 번째 탭
    QCOMPARE(m_window->activePanel(), m_window->leftPanel());

    // 시작할 때 = 마지막 탭과 폴더 복원
    st::AppSettings s = original;
    s.session = board;
    s.general.startup = st::GeneralSettings::Startup::RestoreLastTabs;
    st::SettingsStore::instance().setSettings(s);
    m_window->applyStartup();
    QCOMPARE(m_window->sessionState(), board);

    // 시작할 때 = 지정한 폴더(실제 폴더, 두 패널)
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString native = QDir::toNativeSeparators(dir.path());
    s.general.startup = st::GeneralSettings::Startup::SpecificFolders;
    s.general.startupFolders = {native};
    st::SettingsStore::instance().setSettings(s);
    m_window->applyStartup();
    for (FilePanel *panel : {m_window->leftPanel(), m_window->rightPanel()}) {
        QCOMPARE(panel->tabs().size(), 1);
        QVERIFY(panel->isLocal());
        QCOMPARE(panel->currentPath().compare(native, Qt::CaseInsensitive), 0);
    }

    // 다른 인스턴스의 요청 — 활성 패널에 새 탭으로 그 폴더
    m_window->setActivePanel(m_window->rightPanel());
    m_window->openInNewTab(u"D:\\Downloads"_s);
    QCOMPARE(m_window->rightPanel()->tabs().size(), 2);
    QCOMPARE(m_window->rightPanel()->currentTab(), 1);
    QVERIFY(m_window->rightPanel()->isLocal());

    // 알림 영역 아이콘 — 시스템 트레이가 없는 환경(offscreen)에서는 만들지 않는다
    s.general.trayIcon = st::GeneralSettings::TrayIcon::Always;
    st::SettingsStore::instance().setSettings(s);
    QCOMPARE(m_window->trayIcon() != nullptr, QSystemTrayIcon::isSystemTrayAvailable());

    st::SettingsStore::instance().setSettings(original);
    m_window->loadBoardState();
}

void TestMainWindow::columnSetsInPanels()
{
    // 설정 › 열 · 사용자 정의 열 — 폴더마다 자동 적용, Ctrl+Shift+C로 탭의 세트 바꾸기(마지막 다음은 자동)
    namespace st = fm::settings;
    m_window->loadBoardState();
    FilePanel *left = m_window->leftPanel();
    FilePanel *right = m_window->rightPanel();
    m_window->applySettings(st::Section::Columns);
    QCOMPARE(left->columnSetName(), u"소스 코드"_s);  // D:\Work\* 규칙
    QCOMPARE(right->columnSetName(), u"기본"_s);      // 샘플 D:\Downloads는 알려진 폴더가 아니다
    QCOMPARE(right->model()->columnCount(), int(fl::ColumnCount));
    QCOMPARE(left->listView()->columnLayout().find(fl::AttrColumn), nullptr);  // 소스 코드 세트: 속성 열 없음

    m_window->setActivePanel(right);
    QAction *cycle = m_window->findChild<QAction *>(u"columnSet"_s);
    QVERIFY(cycle);
    cycle->trigger();
    QCOMPARE(right->columnSetName(), u"사진 · 영상"_s);
    QCOMPARE(right->model()->columnCount(), int(fl::ColumnCount) + 4);  // 촬영 날짜 · 해상도 · 재생 시간 · 카메라
    QCOMPARE(right->model()->index(1, fl::ColumnCount + 2).data().toString(), u"—"_s);  // 샘플 파일 — 재생 시간 없음
    const QStringList expected = {u"소스 코드"_s, u"다운로드"_s, u"설치 패키지 검토"_s, u"기본"_s};
    for (const QString &name : expected) {
        cycle->trigger();
        QCOMPARE(right->columnSetName(), name);
    }
    QVERIFY(right->tabs().first().columnSet.isEmpty());  // 마지막 다음은 자동

    // 탭마다 기억 — 다운로드 세트를 고른 탭과 다른 탭(자동)
    cycle->trigger();
    cycle->trigger();
    cycle->trigger();
    QCOMPARE(right->columnSetName(), u"다운로드"_s);
    QTabBar *tabs = right->findChild<QTabBar *>();
    tabs->setCurrentIndex(1);
    QCOMPARE(right->columnSetName(), u"기본"_s);
    tabs->setCurrentIndex(0);
    QCOMPARE(right->columnSetName(), u"다운로드"_s);
    m_window->loadBoardState();
}

void TestMainWindow::docks()
{
    // 도크(07 §6) — 처음에는 모두 닫힘(보드 그대로) · 보기 › 도크 · 아래 도크는 명령줄 위 · 미리보기 · 속성이 커서를 따름 ·
    // 폴더 트리로 이동 · 작업 대기열 · 세션 저장 · 복원
    namespace fd = fm::dialogs;
    namespace st = fm::settings;
    m_window->loadBoardState();
    fm::dock::DockManager *docks = m_window->dockManager();
    QVERIFY(docks);
    auto *host = m_window->findChild<QMainWindow *>(u"dockHost"_s);
    QVERIFY(host);
    QCOMPARE(docks->window(), host);
    const QStringList ids = {u"folderTree"_s, u"preview"_s, u"properties"_s, u"jobs"_s};
    for (const QString &id : ids) {
        QDockWidget *dock = docks->dock(id);
        QVERIFY2(dock, qPrintable(id));
        QVERIFY2(!dock->isVisible() && !dock->toggleViewAction()->isChecked(), qPrintable(id));
    }
    QCOMPARE(host->centralWidget()->width(), host->width());  // 닫힌 도크는 자리를 차지하지 않는다

    // 보기 › 도크 — 켜기 · 끄기 넷과 배치 하위 메뉴
    QMenu *dockMenu = nullptr;
    for (QAction *a : m_window->menuBar()->actions()) {
        if (a->text() != u"보기")
            continue;
        for (QAction *item : a->menu()->actions()) {
            if (item->menu() && item->text() == u"도크(&D)")
                dockMenu = item->menu();
        }
    }
    QVERIFY(dockMenu);
    QVERIFY(dockMenu->actions().contains(docks->dock(u"preview"_s)->toggleViewAction()));

    QDockWidget *preview = docks->dock(u"preview"_s);
    QDockWidget *properties = docks->dock(u"properties"_s);
    QDockWidget *jobs = docks->dock(u"jobs"_s);
    preview->toggleViewAction()->trigger();
    jobs->toggleViewAction()->trigger();
    QTRY_VERIFY(preview->isVisible() && jobs->isVisible());
    QCOMPARE(host->dockWidgetArea(preview), Qt::RightDockWidgetArea);
    // 아래 도크는 명령줄 · 기능 키 막대 위(패널 영역 안)
    auto *commandLine = m_window->findChild<fm::ui::CommandLine *>();
    QVERIFY(jobs->mapTo(m_window.get(), QPoint(0, jobs->height())).y() <= commandLine->mapTo(m_window.get(), QPoint()).y());

    // 미리보기가 활성 패널의 커서를 따른다(샘플 — 가짜 섬네일 또는 종류 아이콘)
    FilePanel *right = m_window->rightPanel();
    m_window->setActivePanel(right);
    m_window->updateDockPanes();
    QVERIFY(right->cursorIndex().isValid());
    QVERIFY(m_window->previewPane()->content() != u"empty");

    // 속성 탭을 앞으로 — 보일 때 채운다
    properties->toggleViewAction()->trigger();
    properties->raise();
    QTRY_VERIFY(m_window->propertiesPane()->isVisible());
    QTRY_COMPARE(m_window->propertiesPane()->value(u"이름"_s), right->cursorIndex().data(fl::FullNameRole).toString());
    QCOMPARE(m_window->propertiesPane()->value(u"원본"_s), u"샘플 데이터"_s);
    const int next = right->cursorRow() + 1;
    right->listView()->setCursorRow(next);
    QTRY_COMPARE(m_window->propertiesPane()->value(u"이름"_s), nameAt(right, next));

    // 작업 대기열 — 진행 창마다 한 줄, 두 번 누르면 진행 창을 앞으로
    const int before = m_window->jobs().size();
    const fd::ProgressDialog::Operation board = fd::ProgressDialog::boardCopy();  // 3.95 GB — 미리 돌리면 약 38 %
    QPointer<fd::ProgressDialog> job =
        m_window->startJob(fd::ProgressDialog::Copy, board.source, board.target, board.fileNames, board.fileSizes);
    m_window->jobsPane()->refresh();
    QTreeWidget *list = m_window->jobsPane()->list();
    QCOMPARE(list->topLevelItemCount(), before + 1);
    QCOMPARE(list->topLevelItem(before)->text(0), job->summaryText());
    QVERIFY(!list->topLevelItem(before)->text(2).isEmpty());
    // 일시 중지 — 상태 글자와 막대 색(대리자가 옵션의 styleObject로 fmProgress를 넘긴다)
    job->applyVariant(u"progress.copy.detail.paused"_s);
    m_window->jobsPane()->refresh();
    QVERIFY(list->topLevelItem(before)->text(2).startsWith(u"일시 중지"_s));
    QTRY_VERIFY(list->isVisible());
    QVERIFY(job->simulator()->percent() > 20);
    const QRect cell = list->visualRect(list->model()->index(before, 1));
    const QColor bar = list->viewport()->grab().toImage().pixelColor(cell.left() + 6 + (cell.width() - 12) / 10,
                                                                     cell.center().y() + 1);
    const QColor paused = fm::style::themeColorsFor(list)[fm::style::Token::Paused];
    QVERIFY2(qAbs(bar.red() - paused.red()) + qAbs(bar.green() - paused.green()) + qAbs(bar.blue() - paused.blue()) < 24,
             qPrintable(bar.name() + u" ≠ "_s + paused.name()));
    job->close();
    QTRY_VERIFY(!job);
    m_window->jobsPane()->refresh();
    QCOMPARE(list->topLevelItemCount(), before);

    // 폴더 트리 — 고른 실제 폴더로 활성 패널이 간다. 실제 파일은 미리보기가 열어 읽는다(글 · 그림), 속성에 그림 크기.
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    {
        QFile text(folder.filePath(u"notes.txt"_s));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.write("첫 줄\n둘째 줄\n");
        QImage image(4, 3, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(folder.filePath(u"red.png"_s)));
    }
    const QString temp = QDir::toNativeSeparators(folder.path());
    Q_EMIT m_window->folderTreePane()->folderActivated(temp);
    QVERIFY(right->isLocal());
    QCOMPARE(right->currentPath().compare(temp, Qt::CaseInsensitive), 0);
    QTRY_VERIFY(rowOf(right, u"notes.txt"_s) >= 0 && rowOf(right, u"red.png"_s) >= 0);
    preview->raise();
    QTRY_VERIFY(m_window->previewPane()->isVisible());
    right->listView()->setCursorRow(rowOf(right, u"notes.txt"_s));
    m_window->updateDockPanes();
    QCOMPARE(m_window->previewPane()->content(), u"text"_s);
    right->listView()->setCursorRow(rowOf(right, u"red.png"_s));
    m_window->updateDockPanes();
    QCOMPARE(m_window->previewPane()->content(), u"image"_s);
    properties->raise();
    QTRY_COMPARE(m_window->propertiesPane()->value(u"그림 크기"_s), u"4 × 3 픽셀"_s);
    QCOMPARE(m_window->propertiesPane()->value(u"원본"_s), u"이 PC (읽기 전용)"_s);
    right->goBack();

    // 세션 — 배치와 이름 붙인 배치가 설정 JSON을 거쳐 되살아난다
    docks->saveLayout(u"검토"_s);
    st::AppSettings settings;
    settings.session = m_window->sessionState();
    QVERIFY(!settings.session.docks.isEmpty());
    const st::AppSettings loaded = st::AppSettings::fromJson(settings.toJson());
    QCOMPARE(loaded.session.docks, settings.session.docks);
    QCOMPARE(loaded.session.dockLayouts.keys(), QStringList{u"검토"_s});
    for (QDockWidget *dock : docks->docks())
        dock->hide();
    docks->setLayouts({});
    m_window->restoreDocks(loaded.session);
    QTRY_VERIFY(preview->isVisible() || properties->isVisible());
    QVERIFY(jobs->isVisible());
    QCOMPARE(docks->layoutNames(), QStringList{u"검토"_s});

    docks->setLayouts({});
    for (QDockWidget *dock : docks->docks())
        dock->hide();
    m_window->loadBoardState();
}

void TestMainWindow::catalogWindow()
{
    // 도구 › 대화상자 카탈로그 — 변형 전부가 묶음 아래에 있고, 찾기 · 열기 · 모두 닫기가 된다
    namespace fd = fm::dialogs;
    m_window->openCatalog();
    QPointer<CatalogWindow> catalog = m_window->findChild<CatalogWindow *>();
    QVERIFY(catalog);
    QVERIFY(catalog->isWindow());
    auto *tree = catalog->findChild<QTreeView *>(u"catalogTree"_s);
    QVERIFY(tree);
    const QAbstractItemModel *model = tree->model();
    const QList<fd::DialogVariant> variants = fd::dialogVariants();
    QSet<QString> ids, groups;
    for (const fd::DialogVariant &v : variants)
        groups.insert(v.dialog);
    QCOMPARE(model->rowCount(), groups.size());
    for (int g = 0; g < model->rowCount(); ++g) {
        const QModelIndex group = model->index(g, 0);
        for (int r = 0; r < model->rowCount(group); ++r)
            ids.insert(model->index(r, 1, group).data().toString());
    }
    QCOMPARE(ids.size(), variants.size());
    for (const fd::DialogVariant &v : variants)
        QVERIFY2(ids.contains(v.id), qPrintable(v.id));

    // 찾기 — ID로 거르면 그 변형과 묶음만 남는다
    auto *search = catalog->findChild<fm::ui::SearchField *>();
    QVERIFY(search);
    search->setText(u"elev.uac"_s);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->rowCount(model->index(0, 0)), 1);
    search->clear();
    QCOMPARE(model->rowCount(), groups.size());

    // 열기 · 모두 닫기(닫으면 지워진다)
    QVERIFY(catalog->openVariant(u"copy.default"_s));
    QVERIFY(catalog->openVariant(u"elev.uac"_s));
    QVERIFY(!catalog->openVariant(u"no.such"_s));
    QCOMPARE(catalog->openCount(), 2);
    catalog->closeAll();
    QTRY_COMPARE(catalog->openCount(), 0);

    // 대화상자가 열린 채로 창을 지워도 안전하고, 메뉴로 다시 열면 새로 만든다
    QVERIFY(catalog->openVariant(u"copy.default"_s));
    delete catalog.data();
    QVERIFY(catalog.isNull());
    m_window->openCatalog();
    catalog = m_window->findChild<CatalogWindow *>();
    QVERIFY(catalog);
    catalog->close();
}

void TestMainWindow::thumbnailCompare()
{
    // 도구 › 섬네일 비교 — 같은 모델을 두 구현에 물리고 연결 시간 · 스크롤을 잰다
    ThumbnailCompare compare;
    compare.resize(1200, 800);
    compare.show();
    QVERIFY(QTest::qWaitForWindowExposed(&compare));
    QSignalSpy loaded(&compare, &ThumbnailCompare::loaded);

    compare.setArrivalSimulation(false);
    compare.load(ThumbnailCompare::Source::Mock10k);
    QCOMPARE(loaded.count(), 1);
    auto *list = compare.findChild<fl::ThumbnailListView *>();
    QVERIFY(list);
#if FM_WITH_QTITAN
    auto *cards = compare.findChild<fl::ThumbnailCardView *>();
    QVERIFY(cards);
    constexpr int backends = 2;
#else
    constexpr int backends = 1;  // 카드 구현이 없으면 Qt 목록만 잰다
#endif
    QCOMPARE(list->model()->rowCount(), 10000);
    for (int b = 0; b < backends; ++b)
        QVERIFY(compare.result(b).attachMs >= 0);
    compare.measureScroll();
    for (int b = 0; b < backends; ++b) {
        const ThumbnailCompare::Result &r = compare.result(b);
        QCOMPARE(r.scrollSteps, 40);
        QVERIFY(r.scrollAvgMs > 0);
        QVERIFY(r.scrollMaxMs >= r.scrollAvgMs);
    }
    QCOMPARE(list->verticalScrollBar()->value(), 0);  // 측정 뒤 맨 위로
#if FM_WITH_QTITAN
    QCOMPARE(cards->scrollArea()->verticalScrollBar()->value(), 0);
#endif
    QVERIFY(compare.lastReport().contains(u"40단계"_s));

    // 도착 흉내 — 처음엔 "만드는 중", 배치가 다 오면 실제 그림 종류
    compare.setArrivalSimulation(true);
    compare.load(ThumbnailCompare::Source::Mock10k);
    QCOMPARE(loaded.count(), 2);
    list = compare.findChild<fl::ThumbnailListView *>();
    QVERIFY(list);
    const QModelIndex last = list->model()->index(9996, fl::NameColumn);  // DSC009996.arw
    QCOMPARE(last.data(fl::ArtRole).toInt(), int(fl::Art::Loading));
    QTRY_COMPARE_WITH_TIMEOUT(list->model()->index(9996, fl::NameColumn).data(fl::ArtRole).toInt(), int(fl::Art::Photo), 10000);

    // 실제 폴더(읽기 전용) — 목록을 다 읽은 뒤 두 구현을 만든다
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    for (const QString &name : {u"a.txt"_s, u"b.png"_s, u"c.pdf"_s}) {
        QFile f(dir.filePath(name));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
    }
    compare.load(ThumbnailCompare::Source::Folder, dir.path());
    QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 3, 10000);
    list = compare.findChild<fl::ThumbnailListView *>();
    QVERIFY(list);
    QSet<QString> names;
    for (int r = 0; r < list->model()->rowCount(); ++r)
        names.insert(list->model()->index(r, fl::NameColumn).data(fl::FullNameRole).toString());
    QVERIFY(names.contains(u"a.txt"_s) && names.contains(u"b.png"_s) && names.contains(u"c.pdf"_s));
}

void TestMainWindow::snapshots()
{
    // fmdemo --shot <폴더> — 화면 × 테마마다 1배율 PNG, 틀을 입힌 PNG, index.html
    namespace fd = fm::dialogs;
    namespace fs = fm::style;
    auto &tm = fs::ThemeManager::instance();
    const fs::Design design = tm.design();
    const fs::ThemeManager::Scheme scheme = tm.scheme();
    const fs::ThemeManager::DarkTone tone = tm.darkTone();

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SnapshotOptions options;
    options.dir = dir.path();
    options.only = {u"copy.default"_s, u"elev.uac"_s};
    options.themes = {u"std-light"_s, u"wc-navy"_s};
    options.settleMs = 50;
    const int code = runSnapshots(options);
    tm.setDesign(design);
    tm.setDarkTone(tone);
    tm.setScheme(scheme);
    QTest::qWait(50);
    QCOMPARE(code, 0);

    int checked = 0;
    for (const fd::DialogVariant &v : fd::dialogVariants()) {
        if (!options.only.contains(v.id))
            continue;
        for (const QString &theme : std::as_const(options.themes)) {
            const QImage plain(dir.filePath(theme + u'/' + v.id + u".png"_s));
            const QImage framed(dir.filePath(theme + u"/framed/"_s + v.id + u".png"_s));
            QVERIFY2(!plain.isNull() && !framed.isNull(), qPrintable(theme + u' ' + v.id));
            QCOMPARE(plain.width(), v.client.width());  // 1배율 = 목업 CSS 픽셀
            if (v.client.height() > 0)
                QCOMPARE(plain.height(), v.client.height());
            // 시안1: 1 px 테두리 + 36 px 제목 · 시안2: 3 px 틀 + 27 px 제목
            const QSize frame = theme.startsWith(u"wc"_s) ? QSize(6, 27 + 3) : QSize(2, 36 + 2);
            QCOMPARE(framed.size(), plain.size() + frame);
            ++checked;
        }
    }
    QCOMPARE(checked, 4);
    QFile index(dir.filePath(u"index.html"_s));
    QVERIFY(index.open(QIODevice::ReadOnly));
    const QString html = QString::fromUtf8(index.readAll());
    QVERIFY(html.contains(u"std-light/framed/copy.default.png"_s));
    QVERIFY(html.contains(u"wc-navy/elev.uac.png"_s));
    QVERIFY(!html.contains(u"/main.png"_s));

    // 고를 화면이 없으면 실패 코드
    options.only = {u"no.such"_s};
    QCOMPARE(runSnapshots(options), 2);
}

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
