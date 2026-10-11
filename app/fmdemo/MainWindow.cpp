#include "MainWindow.h"

#include "CatalogWindow.h"
#include "DockPanes.h"
#include "FilePanel.h"
#include "ThumbnailCompare.h"

#include <fmdialogs/ElevationFlow.h>
#include <fmdialogs/FileOpContext.h>
#include <fmdialogs/FileOpDialogs.h>
#include <fmdialogs/MultiRenameDialog.h>
#include <fmdialogs/ProgressDialog.h>
#include <fmdialogs/SettingsDialog.h>
#include <fmdock/DockManager.h>
#include <fmfilelist/FileGroups.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmfilelist/ThumbnailProvider.h>
#include <fmsettings/Commands.h>
#include <fmsettings/SettingsStore.h>
#include <fmstyle/Glyphs.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>
#include <fmwidgets/CommandLine.h>
#include <fmwidgets/FindBox.h>
#include <fmwidgets/FunctionKeyBar.h>
#include <fmwidgets/SegmentedControl.h>
#include <fmwidgets/Switch.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QShortcut>
#include <QSplitter>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <limits>

using namespace Qt::StringLiterals;
namespace fl = fm::filelist;
namespace fs = fm::style;
namespace st = fm::settings;

namespace fm::app {

namespace fd = fm::dialogs;

namespace {

/// 진행 창의 "같은 이름" 정책 글자 — 복사 대화상자의 덮어쓰기 선택(설정 › 파일 작업 › 같은 이름이 있을 때와 같은 순서).
QString conflictPolicyText(int policy)
{
    switch (policy) {
    case 1:  return u"같은 이름이 있으면 덮어쓰기"_s;
    case 2:  return u"같은 이름이 있으면 더 새로울 때만 덮어쓰기"_s;
    case 3:  return u"같은 이름이 있으면 건너뛰기"_s;
    case 4:  return u"같은 이름이 있으면 번호를 붙여 복사"_s;
    default: return u"같은 이름이 있으면 매번 묻기"_s;
    }
}

/// 진행 창 설정 — 파일 작업(자세히 보기 · 완료되면 닫기) · 관리자 권한(제목 경고) · 키보드(Esc).
fd::ProgressDialog::Operation progressOptions(const st::AppSettings &s)
{
    fd::ProgressDialog::Operation op;
    op.detailed = s.fileOps.progressDetailed;
    op.closeWhenDone = s.fileOps.closeWhenDone;
    op.adminTitle = s.elevation.adminTitleWarning;
    op.esc = fd::ProgressDialog::Operation::EscAction(s.keys.progressEsc);
    return op;
}

/// 두 패널 사이 1 px --line(01 §1.3 A4 · 06 §5.8 #13 — 두 디자인 같음).
class PaneSplitterHandle : public QSplitterHandle
{
public:
    using QSplitterHandle::QSplitterHandle;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), fs::themeColorsFor(this)[fs::Token::Line]);
    }
};

class PaneSplitter : public QSplitter
{
public:
    explicit PaneSplitter(QWidget *parent = nullptr) : QSplitter(Qt::Horizontal, parent)
    {
        setHandleWidth(1);
        setChildrenCollapsible(false);
    }

protected:
    QSplitterHandle *createHandle() override { return new PaneSplitterHandle(orientation(), this); }
};

struct ActionDef
{
    const char *id;
    const char *text;
    const char *toolTip;    // 도구 모음 설명(보드 문구)
    QKeySequence shortcut;
    fs::Glyph glyph;
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_localProbe(std::make_unique<fm::dialogs::LocalProbe>())
    , m_mockProbe(std::make_unique<fm::dialogs::MockProbe>())
{
    setWindowIcon(fs::glyphIcon(fs::Glyph::App, fs::ThemeManager::instance().colors()[fs::Token::Accent], 16));
    createActions();
    createMenus();
    createToolBar();
    createCentral();
    createDocks();
    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, [this] {
        syncThemeControls();
        refreshIcons();
    });
    connect(&st::SettingsStore::instance(), &st::SettingsStore::changed, this, &MainWindow::applySettings);
    syncThemeControls();
    refreshIcons();
    resize(1440, 900);
}

MainWindow::~MainWindow() = default;

void MainWindow::createActions()
{
    const QList<ActionDef> defs = {
        {"back", "뒤로", "뒤로 (Alt+←)", QKeySequence(Qt::ALT | Qt::Key_Left), fs::Glyph::Back},
        {"forward", "앞으로", "앞으로 (Alt+→)", QKeySequence(Qt::ALT | Qt::Key_Right), fs::Glyph::Forward},
        {"up", "상위 폴더", "상위 폴더 (Backspace)", QKeySequence(), fs::Glyph::ArrowUp},
        {"refresh", "새로 고침", "새로 고침 (Ctrl+R)", QKeySequence(Qt::CTRL | Qt::Key_R), fs::Glyph::Refresh},
        {"newFolder", "새 폴더", "새 폴더 (F7)", QKeySequence(Qt::Key_F7), fs::Glyph::NewFolder},
        {"newFile", "새 파일", "새 파일 (Shift+F4)", QKeySequence(Qt::SHIFT | Qt::Key_F4), fs::Glyph::NewFile},
        {"copy", "복사", "복사 (F5)", QKeySequence(Qt::Key_F5), fs::Glyph::Copy},
        {"move", "이동", "이동 (F6)", QKeySequence(Qt::Key_F6), fs::Glyph::Move},
        {"rename", "이름 변경", "이름 변경 (F2)", QKeySequence(Qt::Key_F2), fs::Glyph::Rename},
        {"delete", "삭제", "삭제 (F8)", QKeySequence(Qt::Key_F8), fs::Glyph::Trash},
        {"deletePermanent", "영구 삭제", "영구 삭제 (Shift+Del)", QKeySequence(Qt::SHIFT | Qt::Key_Delete), fs::Glyph::None},
        {"multiRename", "다중 이름 변경", "다중 이름 변경 (Ctrl+M)", QKeySequence(Qt::CTRL | Qt::Key_M), fs::Glyph::MultiRename},
        {"view", "보기", "보기 (F3)", QKeySequence(Qt::Key_F3), fs::Glyph::None},
        {"edit", "편집", "편집 (F4)", QKeySequence(Qt::Key_F4), fs::Glyph::None},
        {"newTab", "새 탭", "새 탭 (Ctrl+T)", QKeySequence(Qt::CTRL | Qt::Key_T), fs::Glyph::None},
        {"closeTab", "탭 닫기", "탭 닫기 (Ctrl+W)", QKeySequence(Qt::CTRL | Qt::Key_W), fs::Glyph::None},
        {"nextTab", "다음 탭", "다음 탭 (Ctrl+Tab)", QKeySequence(Qt::CTRL | Qt::Key_Tab), fs::Glyph::None},
        {"find", "찾기", "현재 폴더에서 찾기 (Ctrl+F)", QKeySequence(Qt::CTRL | Qt::Key_F), fs::Glyph::None},
        {"switchPanel", "다른 패널로", "다른 패널로 (Tab)", QKeySequence(Qt::Key_Tab), fs::Glyph::None},
        {"commandLine", "명령줄로 이동", "명령줄로 이동", QKeySequence(Qt::CTRL | Qt::Key_E), fs::Glyph::None},
        {"selectAll", "모두 선택", "모두 선택 (Ctrl+A)", QKeySequence(), fs::Glyph::None},
        {"invertSelection", "선택 반전", "선택 반전 (숫자 패드 *)", QKeySequence(), fs::Glyph::None},
        {"clearSelection", "선택 해제", "선택 해제", QKeySequence(), fs::Glyph::None},
        {"showHidden", "숨김 파일 표시", "숨김 파일 표시 (Ctrl+H)", QKeySequence(Qt::CTRL | Qt::Key_H), fs::Glyph::None},
        {"columnSet", "열 세트 바꾸기", "열 세트 바꾸기 (Ctrl+Shift+C)", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), fs::Glyph::None},
        {"nameBelow", "2줄에서 이름을 아래 줄에", "", QKeySequence(), fs::Glyph::None},
        {"invertCursor", "역상 커서", "", QKeySequence(), fs::Glyph::None},
        {"invertSelectionLook", "역상 선택", "", QKeySequence(), fs::Glyph::None},
        {"sampleData", "샘플 데이터 (D:)", "", QKeySequence(), fs::Glyph::None},
        {"localHome", "이 PC — 홈 폴더 (읽기 전용)", "", QKeySequence(), fs::Glyph::None},
        {"settings", "설정", "설정 (Ctrl+,)", QKeySequence(Qt::CTRL | Qt::Key_Comma), fs::Glyph::None},
        {"about", "정보", "", QKeySequence(), fs::Glyph::None},
        {"exit", "끝내기", "", QKeySequence(Qt::ALT | Qt::Key_F4), fs::Glyph::None},
    };
    for (const ActionDef &d : defs) {
        auto *a = new QAction(QString::fromUtf8(d.text), this);
        a->setObjectName(QString::fromLatin1(d.id));
        if (!d.shortcut.isEmpty()) {
            a->setShortcut(d.shortcut);
            a->setShortcutContext(Qt::WindowShortcut);
        }
        if (*d.toolTip)
            a->setToolTip(QString::fromUtf8(d.toolTip));
        a->setProperty("fmGlyph", int(d.glyph));
        addAction(a);  // 메뉴에 없어도 단축키가 동작하게
        m_actions.insert(QString::fromLatin1(d.id), a);
    }
    action(u"delete"_s)->setShortcuts({QKeySequence(Qt::Key_F8), QKeySequence(Qt::Key_Delete)});

    // 파일 작업(P5) · 설정 창(P7)
    for (const char *id : {"newFolder", "newFile", "copy", "move", "rename", "delete", "deletePermanent", "multiRename"}) {
        const QString key = QString::fromLatin1(id);
        connect(action(key), &QAction::triggered, this, [this, key] { openFileOperation(key); });
    }
    connect(action(u"settings"_s), &QAction::triggered, this, &MainWindow::openSettings);
    connect(action(u"columnSet"_s), &QAction::triggered, this, [this] { m_active->cycleColumnSet(); });
    for (const char *id : {"view", "edit"}) {
        QAction *a = action(QString::fromLatin1(id));
        connect(a, &QAction::triggered, this, [this, a] {
            QMessageBox::information(this, a->text(), u"파일 보기 · 편집은 이 데모의 범위가 아닙니다."_s);
        });
    }
    connect(action(u"back"_s), &QAction::triggered, this, [this] { m_active->goBack(); });
    connect(action(u"forward"_s), &QAction::triggered, this, [this] { m_active->goForward(); });
    connect(action(u"up"_s), &QAction::triggered, this, [this] { m_active->goUp(); });
    connect(action(u"refresh"_s), &QAction::triggered, this, [this] { m_active->refresh(); });
    connect(action(u"newTab"_s), &QAction::triggered, this, [this] { m_active->newTab(); });
    connect(action(u"closeTab"_s), &QAction::triggered, this, [this] { m_active->closeCurrentTab(); });
    connect(action(u"nextTab"_s), &QAction::triggered, this, [this] { m_active->nextTab(); });
    connect(action(u"find"_s), &QAction::triggered, this, [this] {
        m_find->setFocus(Qt::ShortcutFocusReason);
        m_find->selectAll();
    });
    connect(action(u"switchPanel"_s), &QAction::triggered, this, [this] {
        setActivePanel(m_active == m_left ? m_right : m_left);
        m_active->focusView();
    });
    connect(action(u"commandLine"_s), &QAction::triggered, this, [this] { m_commandLine->lineEdit()->setFocus(); });
    connect(action(u"selectAll"_s), &QAction::triggered, this, [this] { m_active->model()->markAll(true); });
    connect(action(u"invertSelection"_s), &QAction::triggered, this, [this] { m_active->model()->invertMarks(); });
    connect(action(u"clearSelection"_s), &QAction::triggered, this, [this] { m_active->model()->markAll(false); });
    QAction *hidden = action(u"showHidden"_s);
    hidden->setCheckable(true);
    hidden->setChecked(true);  // 보드 상태(숨김 파일 보임)
    connect(hidden, &QAction::toggled, this, [this](bool on) {
        m_left->setShowHidden(on);
        m_right->setShowHidden(on);
    });
    connect(action(u"sampleData"_s), &QAction::triggered, this, [this] { m_active->openLocation(false, u"D:\\"_s); });
    connect(action(u"localHome"_s), &QAction::triggered, this, [this] { m_active->openLocation(true, QDir::homePath()); });
    connect(action(u"about"_s), &QAction::triggered, this, [this] {
#if FM_WITH_QTITAN
        const QString grid = u"QtitanDataGrid 9.2.0(패치)"_s;
#else
        const QString grid = u"QtitanDataGrid 없음(목록은 자리 표시)"_s;
#endif
        QMessageBox::about(this, u"정보"_s,
                           u"파일 관리자 UI 데모 — 시안1(기본) · 시안2(워터컬러)\nQt %1 · %2"_s.arg(QString::fromLatin1(qVersion()), grid));
    });
    connect(action(u"exit"_s), &QAction::triggered, this, &QWidget::close);

    // 보기 방식(활성 패널)
    m_viewModes = new QActionGroup(this);
    const QList<std::pair<QString, QKeySequence>> modes = {
        {u"1줄"_s, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1)}, {u"2줄"_s, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_2)},
        {u"자동"_s, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_3)}, {u"섬네일"_s, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_4)}};
    static const char *const modeIds[] = {"viewOneLine", "viewTwoLine", "viewAuto", "viewThumbnails"};  // 명령 id(설정 › 키보드)
    for (int i = 0; i < modes.size(); ++i) {
        QAction *a = m_viewModes->addAction(modes[i].first);
        a->setObjectName(QString::fromLatin1(modeIds[i]));
        m_actions.insert(a->objectName(), a);
        a->setCheckable(true);
        a->setShortcut(modes[i].second);
        a->setData(i);
        addAction(a);
        connect(a, &QAction::triggered, this, [this, i] { m_active->setViewMode(fl::ViewMode(i)); });
    }
    // 섬네일 구현(활성 패널)
    m_backends = new QActionGroup(this);
    for (const auto &[text, backend] : {std::pair{u"Qt 목록 (QListView)"_s, fl::ThumbnailView::QtList},
#if FM_WITH_QTITAN
                                        std::pair{u"Qtitan 카드 (CardView)"_s, fl::ThumbnailView::QtitanCards},
#endif
                                       }) {
        QAction *a = m_backends->addAction(text);
        a->setCheckable(true);
        a->setData(int(backend));
        connect(a, &QAction::triggered, this, [this, backend] { m_active->setThumbnailBackend(backend); });
    }
    // 레코드 구분 · 이름 위치 · 역상(두 패널 — 설정 › 파일 패널의 값)
    const QStringList separators = {u"없음"_s, u"교차 배경"_s, u"구분선"_s, u"여백"_s, u"틴트"_s};
    m_sep1 = new QActionGroup(this);
    m_sep2 = new QActionGroup(this);
    for (int i = 0; i < separators.size(); ++i) {
        QAction *a1 = m_sep1->addAction(separators[i]);
        a1->setCheckable(true);
        a1->setData(i);
        connect(a1, &QAction::triggered, this, [this, i] {
            fl::ListAppearance a = m_listAppearance;
            a.oneLineSeparator = fl::RecordSeparator(i);
            setListAppearance(a);
        });
        QAction *a2 = m_sep2->addAction(separators[i]);
        a2->setCheckable(true);
        a2->setData(i);
        connect(a2, &QAction::triggered, this, [this, i] {
            fl::ListAppearance a = m_listAppearance;
            a.twoLineSeparator = fl::RecordSeparator(i);
            setListAppearance(a);
        });
    }
    for (const char *id : {"nameBelow", "invertCursor", "invertSelectionLook"}) {
        QAction *a = action(QString::fromLatin1(id));
        a->setCheckable(true);
        connect(a, &QAction::toggled, this, [this] {
            fl::ListAppearance a = m_listAppearance;
            a.nameBelow = action(u"nameBelow"_s)->isChecked();
            a.invertCursor = action(u"invertCursor"_s)->isChecked();
            a.invertSelection = action(u"invertSelectionLook"_s)->isChecked();
            setListAppearance(a);
        });
    }
}

void MainWindow::createMenus()
{
    // 메뉴 제목에는 액세스 키 표기가 없다(두 보드 모두 "파일"). Alt+글자는 따로 건다.
    struct MenuDef
    {
        QString title;
        Qt::Key key;
        QStringList actions;
    };
    const QList<MenuDef> menus = {
        {u"파일"_s, Qt::Key_F, {u"newTab"_s, u"-"_s, u"newFolder"_s, u"newFile"_s, u"-"_s, u"copy"_s, u"move"_s, u"rename"_s,
                               u"delete"_s, u"deletePermanent"_s, u"-"_s, u"exit"_s}},
        {u"편집"_s, Qt::Key_E, {u"multiRename"_s, u"-"_s, u"view"_s, u"edit"_s}},
        {u"선택"_s, Qt::Key_S, {u"selectAll"_s, u"invertSelection"_s, u"clearSelection"_s}},
        {u"보기"_s, Qt::Key_V, {}},
        {u"명령"_s, Qt::Key_C, {u"commandLine"_s, u"find"_s}},
        {u"탭"_s, Qt::Key_T, {u"newTab"_s, u"closeTab"_s, u"nextTab"_s, u"-"_s, u"switchPanel"_s}},
        {u"도구"_s, Qt::Key_O, {u"sampleData"_s, u"localHome"_s}},
    };
    for (const MenuDef &def : menus) {
        QMenu *menu = menuBar()->addMenu(def.title);
        for (const QString &id : def.actions) {
            if (id == u"-")
                menu->addSeparator();
            else
                menu->addAction(action(id));
        }
        if (def.title == u"도구") {
            menu->addSeparator();
            QAction *catalog = menu->addAction(u"대화상자 카탈로그(&G)…"_s);
            catalog->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
            connect(catalog, &QAction::triggered, this, &MainWindow::openCatalog);
            connect(menu->addAction(u"섬네일 비교 — Qt 목록 · Qtitan 카드(&T)…"_s), &QAction::triggered, this, &MainWindow::openThumbnailCompare);
            // 권한 흐름 시뮬레이션(PLAN §7.3 · 03 §0)
            menu->addSeparator();
            QMenu *flows = menu->addMenu(u"권한 흐름 시뮬레이션"_s);
            connect(flows->addAction(u"보호된 폴더로 복사 — 거부 · 승인 · UAC 거부"_s), &QAction::triggered, this,
                    [this] { startElevationFlow(0); });
            connect(flows->addAction(u"삭제 — 사전 확인 · 소유권"_s), &QAction::triggered, this,
                    [this] { startElevationFlow(1); });
        }
        if (def.title == u"보기") {
            m_viewMenu = menu;  // 도크 하위 메뉴는 createDocks에서
            menu->addActions(m_viewModes->actions());
            menu->addSeparator();
            menu->addMenu(u"섬네일 구현 (활성 패널)"_s)->addActions(m_backends->actions());
            menu->addMenu(u"1줄 레코드 구분"_s)->addActions(m_sep1->actions());
            menu->addMenu(u"2줄 레코드 구분"_s)->addActions(m_sep2->actions());
            menu->addAction(action(u"nameBelow"_s));
            menu->addAction(action(u"invertCursor"_s));
            menu->addAction(action(u"invertSelectionLook"_s));
            menu->addSeparator();
            menu->addAction(action(u"columnSet"_s));
            menu->addAction(action(u"showHidden"_s));
            menu->addAction(action(u"refresh"_s));
        }
        auto *shortcut = new QShortcut(QKeySequence(Qt::ALT | def.key), this);
        connect(shortcut, &QShortcut::activated, this, [this, menu] { menuBar()->setActiveAction(menu->menuAction()); });
    }
    // 설정은 메뉴가 아니라 바로 여는 항목(01 §1.3 A2)
    menuBar()->addAction(action(u"settings"_s));
    auto *settingsKey = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_N), this);
    connect(settingsKey, &QShortcut::activated, action(u"settings"_s), &QAction::trigger);
    QMenu *help = menuBar()->addMenu(u"도움말"_s);
    help->addAction(action(u"about"_s));
    auto *helpKey = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_H), this);
    connect(helpKey, &QShortcut::activated, this, [this, help] { menuBar()->setActiveAction(help->menuAction()); });
}

void MainWindow::createToolBar()
{
    m_toolBar = addToolBar(u"도구 모음"_s);
    m_toolBar->setObjectName(u"MainToolBar"_s);
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setIconSize(QSize(16, 16));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    const QList<QStringList> groups = {{u"back"_s, u"forward"_s, u"up"_s, u"refresh"_s},
                                       {u"newFolder"_s, u"newFile"_s},
                                       {u"copy"_s, u"move"_s, u"rename"_s, u"delete"_s},
                                       {u"multiRename"_s}};
    for (int g = 0; g < groups.size(); ++g) {
        if (g > 0)
            m_toolBar->addSeparator();
        for (const QString &id : groups[g])
            m_toolBar->addAction(action(id));
    }
    auto *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(spacer);
    m_find = new fm::ui::FindBox;
    m_toolBar->addWidget(m_find);
    connect(m_find, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (m_active)
            m_active->setQuickFilter(text);
    });
    connect(m_find, &QLineEdit::returnPressed, this, [this] { m_active->focusView(); });

    // 디자인 비교 스위치(PLAN §4) — 보드에는 없는 데모용 조절기
    m_toolBar->addSeparator();
    auto *theme = new QWidget;
    auto *h = new QHBoxLayout(theme);
    h->setContentsMargins(4, 0, 4, 0);
    h->setSpacing(8);
    m_watercolor = new fm::ui::Switch(u"워터컬러"_s);
    m_watercolor->setToolTip(u"시안1(기본) ↔ 시안2(워터컬러)"_s);
    m_scheme = new fm::ui::SegmentedControl({u"시스템"_s, u"라이트"_s, u"다크"_s});
    m_scheme->setSegmentSize(fm::ui::SegmentedControl::Small);
    m_scheme->setAccessibleName(u"색 구성표"_s);
    m_tone = new fm::ui::SegmentedControl({u"회색"_s, u"남색"_s});
    m_tone->setSegmentSize(fm::ui::SegmentedControl::Small);
    m_tone->setAccessibleName(u"다크 색조"_s);
    m_tone->setToolTip(u"다크 색조 — 다크(남색)는 시안2 전용"_s);
    h->addWidget(m_watercolor);
    h->addWidget(m_scheme);
    h->addWidget(m_tone);
    m_toolBar->addWidget(theme);

    auto &tm = fs::ThemeManager::instance();
    connect(m_watercolor, &QCheckBox::toggled, this, [this, &tm](bool on) {
        if (m_syncing)
            return;
        QMetaObject::invokeMethod(this, [&tm, on] { tm.setDesign(on ? fs::Design::Watercolor : fs::Design::Standard); },
                                  Qt::QueuedConnection);
    });
    connect(m_scheme, &fm::ui::SegmentedControl::currentIndexChanged, this, [this, &tm](int i) {
        if (m_syncing)
            return;
        const auto scheme = i == 1 ? fs::ThemeManager::Scheme::Light : i == 2 ? fs::ThemeManager::Scheme::Dark
                                                                             : fs::ThemeManager::Scheme::System;
        QMetaObject::invokeMethod(this, [&tm, scheme] { tm.setScheme(scheme); }, Qt::QueuedConnection);
    });
    connect(m_tone, &fm::ui::SegmentedControl::currentIndexChanged, this, [this, &tm](int i) {
        if (m_syncing)
            return;
        const auto tone = i == 1 ? fs::ThemeManager::DarkTone::Navy : fs::ThemeManager::DarkTone::Gray;
        QMetaObject::invokeMethod(this, [&tm, tone] { tm.setDarkTone(tone); }, Qt::QueuedConnection);
    });
}

void MainWindow::createCentral()
{
    auto *central = new QWidget;
    auto *v = new QVBoxLayout(central);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    m_splitter = new PaneSplitter;
    m_left = new FilePanel;
    m_right = new FilePanel;
    m_left->setObjectName(u"leftPanel"_s);
    m_right->setObjectName(u"rightPanel"_s);
    m_splitter->addWidget(m_left);
    m_splitter->addWidget(m_right);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_commandLine = new fm::ui::CommandLine;
    m_functionKeys = new fm::ui::FunctionKeyBar;
    const QList<std::pair<QString, QString>> keys = {
        {u"F2"_s, u"rename"_s}, {u"F3"_s, u"view"_s}, {u"F4"_s, u"edit"_s}, {u"F5"_s, u"copy"_s}, {u"F6"_s, u"move"_s},
        {u"F7"_s, u"newFolder"_s}, {u"F8"_s, u"delete"_s}, {u"⇧F4"_s, u"newFile"_s}, {u"Ctrl+M"_s, u"multiRename"_s}};
    for (const auto &[key, id] : keys) {
        QAction *a = action(id);
        fm::ui::FunctionKeyButton *button = m_functionKeys->addKey(key, a->text());
        button->setProperty("fmCommand", id);  // 단축키를 바꾸면 글자도(applyKeyBindings)
        connect(button, &QPushButton::clicked, a, &QAction::trigger);
    }
    // 도크 자리(07 §6) — 두 패널 영역만 안쪽 QMainWindow에 둔다. 도크가 닫혀 있으면 패널이 그 자리를 다 쓴다.
    m_dockHost = new QMainWindow;
    m_dockHost->setObjectName(u"dockHost"_s);
    m_dockHost->setWindowFlags(Qt::Widget);
    m_dockHost->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
    m_dockHost->setCentralWidget(m_splitter);
    v->addWidget(m_dockHost, 1);
    v->addWidget(m_commandLine);
    v->addWidget(m_functionKeys);
    setCentralWidget(central);

    for (FilePanel *panel : {m_left, m_right}) {
        connect(panel, &FilePanel::activateRequested, this, &MainWindow::setActivePanel);
        connect(panel, &FilePanel::locationChanged, this, [this, panel] {
            if (panel == m_active) {
                updateWindowTitle();
                updateActionStates();
            }
        });
        connect(panel, &FilePanel::viewModeChanged, this, [this, panel] {
            if (panel == m_active)
                updateActionStates();
        });
    }
    connect(m_commandLine, &fm::ui::CommandLine::commandEntered, this, [this](const QString &command) {
        // cd <경로> · cd .. — 그 밖의 명령은 실행하지 않는다(읽기 전용 데모)
        if (command.startsWith(u"cd "_s, Qt::CaseInsensitive) || command.compare(u"cd.."_s, Qt::CaseInsensitive) == 0) {
            const QString target = command.mid(command.indexOf(u"cd"_s, 0, Qt::CaseInsensitive) + 2).trimmed();
            if (target == u"..")
                m_active->goUp();
            else if (!target.isEmpty())
                m_active->openLocation(m_active->isLocal() || !target.startsWith(u"D:"_s, Qt::CaseInsensitive), target);
        }
    });
}

void MainWindow::createDocks()
{
    // 도크(07 §6) — 왼쪽 폴더 트리, 오른쪽 미리보기 · 속성(탭 묶음), 아래 작업 대기열. 처음에는 모두 닫혀 있고
    // (보드 스냅숏 그대로) 보기 › 도크로 열면 이 자리에 붙는다. 끌어 옮긴 배치는 끝낼 때 세션에 저장한다.
    m_docks = new fm::dock::DockManager(m_dockHost);
    m_folderTree = new FolderTreePane;
    m_previewPane = new PreviewPane;
    m_propertiesPane = new PropertiesPane;
    m_jobsPane = new JobsPane([this] { return jobs(); });
    QDockWidget *tree = m_docks->addDock(u"folderTree"_s, u"폴더 트리"_s, m_folderTree, Qt::LeftDockWidgetArea);
    QDockWidget *preview = m_docks->addDock(u"preview"_s, u"미리보기"_s, m_previewPane, Qt::RightDockWidgetArea);
    QDockWidget *properties = m_docks->addDock(u"properties"_s, u"속성"_s, m_propertiesPane, Qt::RightDockWidgetArea);
    QDockWidget *jobsDock = m_docks->addDock(u"jobs"_s, u"작업 대기열"_s, m_jobsPane, Qt::BottomDockWidgetArea);
    m_docks->dropOnto(properties, preview, fm::dock::DockManager::DropSide::Center);
    preview->raise();
    m_dockHost->resizeDocks({tree, preview}, {240, 300}, Qt::Horizontal);
    m_dockHost->resizeDocks({jobsDock}, {150}, Qt::Vertical);
    for (QDockWidget *dock : m_docks->docks()) {
        dock->hide();
        dock->toggleViewAction()->setChecked(false);  // 창을 보이기 전이라 Hide 이벤트가 없다
    }
    m_viewMenu->addSeparator();
    m_docks->populateMenu(m_viewMenu->addMenu(u"도크(&D)"_s));

    // 커서를 빠르게 옮기면 미리보기는 멈춘 뒤 한 번만 읽는다
    m_paneTimer = new QTimer(this);
    m_paneTimer->setSingleShot(true);
    m_paneTimer->setInterval(80);
    connect(m_paneTimer, &QTimer::timeout, this, &MainWindow::updateDockPanes);
    for (FilePanel *panel : {m_left, m_right}) {
        connect(panel, &FilePanel::cursorChanged, m_paneTimer, qOverload<>(&QTimer::start));
        connect(panel, &FilePanel::locationChanged, m_paneTimer, qOverload<>(&QTimer::start));
    }
    for (QDockWidget *dock : m_docks->docks()) {
        // 열거나 탭으로 앞에 올 때 — 숨은 동안에는 읽지 않았다
        connect(dock, &QDockWidget::visibilityChanged, m_paneTimer, [this](bool visible) {
            if (visible)
                m_paneTimer->start();
        });
    }
    connect(m_folderTree, &FolderTreePane::folderActivated, this, [this](const QString &path) {
        if (m_active)
            m_active->openLocation(true, path);
    });
}

void MainWindow::updateDockPanes()
{
    if (!m_docks || !m_active)
        return;
    const bool local = m_active->isLocal();
    m_folderTree->follow(local ? m_active->currentPath() : QString());  // 모델이 없으면 자리만 기억한다
    const QModelIndex index = m_active->cursorIndex();
    if (m_previewPane->isVisible())
        m_previewPane->showItem(index, local);
    if (m_propertiesPane->isVisible())
        m_propertiesPane->showItem(index, local);
}

void MainWindow::openAllDocks()
{
    for (QDockWidget *dock : m_docks->docks()) {
        if (m_docks->isAutoHidden(dock))
            m_docks->setAutoHidden(dock, false);
        dock->show();
    }
    if (QDockWidget *preview = m_docks->dock(u"preview"_s))
        preview->raise();
    m_paneTimer->start();
}

void MainWindow::restoreDocks(const st::SessionState &session)
{
    m_docks->setLayouts(session.dockLayouts);
    if (!session.docks.isEmpty())
        m_docks->restoreState(session.docks);
}

void MainWindow::loadBoardState()
{
    TabState fmCore;
    fmCore.path = u"D:\\Work\\fm-core"_s;
    fmCore.mode = fl::ViewMode::OneLine;
    fmCore.modeSet = true;
    fmCore.cursor = 7;
    TabState samples;
    samples.path = u"D:\\Work\\qtitan-samples"_s;
    TabState cDrive;
    cDrive.local = true;
    cDrive.path = u"C:\\"_s;
    m_left->setTabs({fmCore, samples, cDrive}, 0);

    TabState downloads;
    downloads.path = u"D:\\Downloads"_s;
    downloads.mode = fl::ViewMode::Auto;
    downloads.modeSet = true;
    downloads.cursor = 3;
    TabState backup;
    backup.path = u"D:\\Backup"_s;
    m_right->setTabs({downloads, backup}, 0);

    // 섬네일 두 구현을 나란히 비교할 수 있게 왼쪽 Qt 목록 · 오른쪽 Qtitan 카드(PLAN §7.1)
    m_left->setThumbnailBackend(fl::ThumbnailView::QtList);
    m_right->setThumbnailBackend(fl::ThumbnailView::QtitanCards);
    // 보드는 숨김 · 시스템 파일을 보인다(오른쪽 desktop.ini — "숨김 1개 표시 중")
    for (FilePanel *panel : {m_left, m_right})
        panel->setShowProtected(true);
    setListAppearance(fl::ListAppearance{});
    setActivePanel(m_right);
}

void MainWindow::setListAppearance(const fl::ListAppearance &appearance)
{
    m_listAppearance = appearance;
    fl::ThumbnailAppearance thumbs;
    thumbs.invertCursor = appearance.invertCursor;
    thumbs.invertSelection = appearance.invertSelection;
    for (FilePanel *panel : {m_left, m_right}) {
        panel->setListAppearance(appearance);
        fl::ThumbnailAppearance t = panel->thumbnailView()->appearance();
        t.invertCursor = appearance.invertCursor;
        t.invertSelection = appearance.invertSelection;
        panel->setThumbnailAppearance(t);
    }
    m_syncing = true;
    for (QAction *a : m_sep1->actions())
        a->setChecked(a->data().toInt() == int(appearance.oneLineSeparator));
    for (QAction *a : m_sep2->actions())
        a->setChecked(a->data().toInt() == int(appearance.twoLineSeparator));
    const QSignalBlocker b1(action(u"nameBelow"_s));
    const QSignalBlocker b2(action(u"invertCursor"_s));
    const QSignalBlocker b3(action(u"invertSelectionLook"_s));
    action(u"nameBelow"_s)->setChecked(appearance.nameBelow);
    action(u"invertCursor"_s)->setChecked(appearance.invertCursor);
    action(u"invertSelectionLook"_s)->setChecked(appearance.invertSelection);
    m_syncing = false;
}

void MainWindow::setActivePanel(FilePanel *panel)
{
    if (!panel)
        return;
    const bool changed = m_active != panel;
    m_active = panel;
    m_left->setActive(panel == m_left);
    m_right->setActive(panel == m_right);
    if (changed) {
        const QSignalBlocker blocker(m_find);
        m_find->setText(panel->model()->quickFilter());
        if (m_paneTimer)
            m_paneTimer->start();
    }
    updateWindowTitle();
    updateActionStates();
}

void MainWindow::updateWindowTitle()
{
    if (!m_active)
        return;
    const QString path = m_active->currentPath();
    setWindowTitle(path + u" — 파일 관리자"_s);
    m_commandLine->setPrompt(path);
}

void MainWindow::updateActionStates()
{
    if (!m_active)
        return;
    for (QAction *a : m_viewModes->actions())
        a->setChecked(a->data().toInt() == int(m_active->viewMode()));
    for (QAction *a : m_backends->actions())
        a->setChecked(a->data().toInt() == int(m_active->thumbnailBackend()));
}

void MainWindow::syncThemeControls()
{
    auto &tm = fs::ThemeManager::instance();
    m_syncing = true;
    m_watercolor->setChecked(tm.design() == fs::Design::Watercolor);
    m_scheme->setCurrentIndex(tm.scheme() == fs::ThemeManager::Scheme::Light ? 1
                              : tm.scheme() == fs::ThemeManager::Scheme::Dark ? 2
                                                                              : 0);
    m_tone->setCurrentIndex(tm.darkTone() == fs::ThemeManager::DarkTone::Navy ? 1 : 0);
    m_tone->setEnabled(tm.design() == fs::Design::Watercolor);
    m_syncing = false;
    setWindowIcon(fs::glyphIcon(fs::Glyph::App, tm.colors()[fs::Token::Accent], 16));
}

void MainWindow::refreshIcons()
{
    // 도구 모음 아이콘: 시안1 --fg2, 시안2 --fg(06 §4.13). 사용 안 함은 --fg3 / --x-dfg.
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    fs::GlyphStateColors colors;
    if (tc.isWatercolor()) {
        colors.normal = tc[fs::Token::Fg];
        colors.disabled = fs::watercolorChrome(tc.variant()).disFg;
    } else {
        colors.normal = tc[fs::Token::Fg2];
        colors.disabled = tc[fs::Token::Fg3];
    }
    for (QAction *a : std::as_const(m_actions)) {
        const auto glyph = fs::Glyph(a->property("fmGlyph").toInt());
        if (glyph != fs::Glyph::None)
            a->setIcon(fs::glyphIcon(glyph, colors, 16));
    }
}

void MainWindow::openCatalog()
{
    if (!m_catalog)
        m_catalog = new CatalogWindow(this);
    m_catalog->show();
    m_catalog->raise();
    m_catalog->activateWindow();
}

void MainWindow::openThumbnailCompare()
{
    if (!m_compare) {
        auto *compare = new ThumbnailCompare(this);
        compare->load(ThumbnailCompare::Source::Sample);
        m_compare = compare;
    }
    m_compare->show();
    m_compare->raise();
    m_compare->activateWindow();
}

void MainWindow::openSettings()
{
    if (m_settingsDialog) {
        m_settingsDialog->raise();
        m_settingsDialog->activateWindow();
        return;
    }
    auto *dialog = new fm::dialogs::SettingsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_settingsDialog = dialog;
    connect(dialog, &fm::dialogs::SettingsDialog::thumbnailCacheClearRequested, this, [this] {
        for (FilePanel *panel : {m_left, m_right})
            panel->thumbnailProvider()->clearCache();
    });
    dialog->open();
}

void MainWindow::applySettings(st::Sections sections)
{
    using S = st::Section;
    const st::AppSettings &s = st::SettingsStore::instance().settings();
    if (sections & (S::Appearance | S::Theme))
        st::applyTheme(s);  // 디자인 · 색 구성표 · 다크 색조 · 기준 색 · 제목 표시줄 · 액세스 키 밑줄
    if (sections & S::Keys)
        fs::ThemeManager::instance().setAlwaysShowMnemonics(s.keys.alwaysShowMnemonics);
    if (sections & (S::Panel | S::Appearance))
        setListAppearance(s.panel.toListAppearance(s.appearance));  // 구분 · 역상 등 + 목록 글꼴 · 행 밀도
    if (sections & S::Panel) {
        {
            const QSignalBlocker block(action(u"showHidden"_s));
            action(u"showHidden"_s)->setChecked(s.panel.showHidden);
        }
        for (FilePanel *panel : {m_left, m_right}) {
            panel->setShowHidden(s.panel.showHidden);
            panel->setShowProtected(s.panel.showProtectedOs);
            panel->model()->setFoldersFirst(s.panel.foldersFirst);
            panel->setDisplayFormat(s.panel.toDisplayFormat());
        }
        m_sep1->actions().value(int(s.panel.separator1))->setChecked(true);
        m_sep2->actions().value(int(s.panel.separator2))->setChecked(true);
    }
    if (sections & S::Thumbs) {
        // 섬네일 만들기 — 방법 · 대상 · 느린 드라이브 · 동시 개수(작업 큐만 바뀌고 캐시는 유지), 자동 섬네일
        for (FilePanel *panel : {m_left, m_right}) {
            fl::ThumbnailProvider *provider = panel->thumbnailProvider();
            provider->setMethod(fl::ThumbnailProvider::Method(s.thumbs.provider));
            provider->setTargets(s.thumbs.targets);
            provider->setSkipSlowVolumes(s.thumbs.iconsOnlyOnNetworkRemovable);
            provider->setMaxThreads(s.thumbs.concurrency);
            panel->setAutoThumbnails(s.thumbs.autoThumbnailFolders, s.thumbs.autoThumbnailPct);
        }
    }
    if (sections & (S::Panel | S::Thumbs)) {
        for (FilePanel *panel : {m_left, m_right}) {
            fl::ThumbnailAppearance t = s.thumbs.toThumbnailAppearance(s.panel);
            t.size = panel->thumbnailView()->appearance().size;  // 크기는 패널에서 Ctrl+휠로 바꾼 값 유지
            panel->setThumbnailAppearance(t);
        }
    }
    if (sections & (S::Panel | S::Tabs)) {
        // 새 탭 기본값 · 탭 이름 — 표시 방식을 정하지 않은 탭은 바로 기본 표시 방식으로(04 §2.3.4)
        TabOptions tabs;
        tabs.defaultMode = s.panel.defaultViewMode;
        tabs.position = TabOptions::Position(s.tabs.newTabPosition);
        tabs.title = TabOptions::Title(s.tabs.title);
        tabs.rememberView = s.tabs.rememberViewPerTab;
        for (FilePanel *panel : {m_left, m_right})
            panel->setTabOptions(tabs);
        updateActionStates();
    }
    if (sections & (S::Groups | S::Columns)) {
        // 파일 그룹(행 스타일) · 열 세트(폴더마다 열 배치 — 그룹 비율 규칙이 그룹을 쓴다)
        const auto matcher = std::make_shared<const fl::FileGroupMatcher>(s.groups);
        for (FilePanel *panel : {m_left, m_right}) {
            if (sections & S::Groups)
                panel->model()->setGroupMatcher(matcher);
            panel->setColumnSettings(s.columns, matcher);
        }
    }
    if (sections & S::General) {
        m_commandLine->setVisible(s.general.showCommandLine);
        m_functionKeys->setVisible(s.general.showFunctionKeyBar);
        updateTray();
    }
    if (sections & S::Keys)
        applyKeyBindings(s.keys);
}

void MainWindow::applyKeyBindings(const st::KeyBindingSettings &keys)
{
    // 기능 키 막대 글자 — 명령의 첫 단축키(목업 표기: Shift → ⇧). 키가 없으면 비운다.
    for (fm::ui::FunctionKeyButton *button : m_functionKeys->buttons()) {
        const QString command = button->property("fmCommand").toString();
        if (!st::findCommand(command))
            continue;  // 키보드 설정에 없는 명령(보기 · 편집 — 범위 밖)은 처음 글자 그대로
        const QList<QKeySequence> sequences = st::effectiveKeys(keys, command);
        QString text = sequences.isEmpty() ? QString() : sequences.first().toString(QKeySequence::NativeText);
        text.replace(u"Shift+"_s, u"⇧"_s);
        button->setKeys(text);
    }
    // 명령 id = QAction objectName(05 §2.5.4). 도구 설명의 "(F5)"도 새 키로.
    for (const st::CommandDef &c : st::commands()) {
        QAction *a = action(c.id);
        if (!a)
            continue;
        const QList<QKeySequence> sequences = st::effectiveKeys(keys, c.id);
        a->setShortcuts(sequences);
        if (!a->toolTip().isEmpty() && a->toolTip() != a->text()) {
            QString tip = a->text();
            if (!sequences.isEmpty())
                tip += u" (%1)"_s.arg(sequences.first().toString(QKeySequence::NativeText));
            a->setToolTip(tip);
        }
    }
}

void MainWindow::captureSettings()
{
    st::AppSettings s = st::SettingsStore::instance().settings();
    st::captureTheme(s);
    const fl::ListAppearance &a = m_listAppearance;
    s.panel.separator1 = a.oneLineSeparator;
    s.panel.separator2 = a.twoLineSeparator;
    s.panel.nameBelow = a.nameBelow;
    s.panel.inverseCursor = a.invertCursor;
    s.panel.inverseSelection = a.invertSelection;
    s.panel.showHidden = action(u"showHidden"_s)->isChecked();
    s.panel.showProtectedOs = m_left->showProtected();
    st::SettingsStore::instance().setSettings(s);
}

fm::dialogs::ElevationFlow *MainWindow::startElevationFlow(int scenario)
{
    auto *flow = new fm::dialogs::ElevationFlow(
        scenario == 1 ? fm::dialogs::ElevationFlow::DeleteWithOwnership : fm::dialogs::ElevationFlow::CopyToProtected, this);
    // 설정 › 관리자 권한(사전 확인 · 남은 항목 적용 · UAC 대기 · 도우미 수명 · 소유권) + 진행 창 설정
    const st::AppSettings &s = st::SettingsStore::instance().settings();
    using Ownership = st::ElevationSettings::OwnershipButton;
    fd::ElevationFlow::Options options;
    options.preflight = s.elevation.preflight;
    options.applyToRemaining = s.elevation.applyToRemainingDefault;
    options.uacTimeout = s.elevation.uacTimeoutSec;
    options.helperRunning = helperAlive();
    options.askOwnership = s.elevation.takeOwnership == st::ElevationSettings::Ownership::Ask;
    options.ownershipDefault = s.elevation.ownershipDefault == Ownership::TakeOwnership ? fd::elev::Choice::TakeOwnership
                               : s.elevation.ownershipDefault == Ownership::Cancel      ? fd::elev::Choice::Cancel
                                                                                        : fd::elev::Choice::Skip;
    options.backupAcl = s.elevation.backupAclBeforeChange;
    options.progress = progressOptions(s);
    flow->setOptions(options);
    connect(flow, &fm::dialogs::ElevationFlow::helperApproved, this, [this] { m_helperApprovedAt = QDateTime::currentDateTime(); });
    connect(flow, &fm::dialogs::ElevationFlow::finished, flow, &QObject::deleteLater);
    flow->start();
    return flow;
}

fm::dialogs::FileOpContext MainWindow::operationContext() const
{
    namespace fd = fm::dialogs;
    fd::FileOpContext c;
    c.fileOps = st::SettingsStore::instance().settings().fileOps;  // 대화상자 기본값(설정 › 파일 작업)
    FilePanel *other = m_active == m_left ? m_right : m_left;
    c.sourceDir = m_active->currentPath();
    c.targetDir = other->currentPath();
    for (const QModelIndex &i : m_active->operationRows()) {
        fd::FileItem item;
        item.name = i.data(fl::FullNameRole).toString();
        item.isDir = i.data(fl::IsDirRole).toBool();
        item.size = std::max<qint64>(0, i.data(fl::SizeBytesRole).toLongLong());
        item.readOnly = i.data(fl::AttributesRole).toInt() & fl::ReadOnly;
        item.kind = fd::fileKindFor(item.name, item.isDir);
        item.modified = i.data(fl::ModifiedRole).toDateTime();
        c.items.append(item);
    }
    // 최근 대상: 반대 패널 + 목업 목록
    c.recentTargets = fd::BoardContext::copy().recentTargets;
    if (m_active->isLocal()) {
        c.probe = m_localProbe.get();
    } else {
        m_mockProbe->addPath(c.sourceDir);
        m_mockProbe->addPath(c.targetDir);
        c.probe = m_mockProbe.get();
    }
    return c;
}

void MainWindow::openFileOperation(const QString &id)
{
    namespace fd = fm::dialogs;
    fd::FileOpContext context = operationContext();
    const bool needsItems = id != u"newFolder" && id != u"newFile";
    if (needsItems && context.items.isEmpty()) {
        QApplication::beep();
        return;
    }

    // 진행 창(모덜리스) — 파일 크기로 시뮬레이터를 돌린다. 폴더는 크기를 모르므로 64 MB로 본다.
    const fm::settings::FileOpsSettings &ops = context.fileOps;
    auto showProgress = [this, &context](fd::ProgressDialog::Kind kind, const QString &target, int policy, bool queued) {
        QStringList names;
        QList<qint64> sizes;
        for (const fd::FileItem &item : std::as_const(context.items)) {
            names.append(item.name);
            sizes.append(item.isDir ? qint64(64) << 20 : std::max<qint64>(1, item.size));
        }
        startJob(kind, context.sourceDir, target, names, sizes, conflictPolicyText(policy), queued);
    };

    if (id == u"copy") {
        fd::CopyDialog dialog(context, this);
        const int result = dialog.exec();
        if (result == QDialog::Accepted || result == fd::CopyDialog::Queued) {
            const fd::CopyRequest request = dialog.request();
            showProgress(fd::ProgressDialog::Copy, request.destination, request.overwritePolicy, request.queued);
        }
    } else if (id == u"move" || id == u"rename") {
        const bool move = id == u"move";
        fd::MoveRenameDialog dialog(context, move, this);
        if (dialog.exec() == QDialog::Accepted && move && dialog.plan().operation != fd::MoveRenamePlan::Rename)
            showProgress(fd::ProgressDialog::Move, dialog.request().target, int(ops.onConflict), false);
    } else if (id == u"delete" || id == u"deletePermanent") {
        // 설정 › 파일 작업 › 기본 삭제 방식 — F8 · Del이 그 방식이고 Shift를 누르면 반대로. 휴지통이 없는 드라이브는 영구 삭제.
        bool permanent = (ops.deleteMode == fm::settings::FileOpsSettings::DeleteMode::Permanent) != (id == u"deletePermanent");
        if (context.probe) {
            const fd::VolumeInfo volume = context.probe->volume(context.sourceDir);
            if (volume.valid && !volume.hasRecycleBin)
                permanent = true;
        }
        if (!ops.confirmDelete) {  // 삭제 전 확인 끔 — 바로 진행
            showProgress(permanent ? fd::ProgressDialog::DeletePermanent : fd::ProgressDialog::Delete, QString(),
                         int(ops.onConflict), false);
            return;
        }
        fd::DeleteDialog dialog(context, permanent, this);
        if (dialog.exec() == QDialog::Accepted)
            showProgress(dialog.request().permanent ? fd::ProgressDialog::DeletePermanent : fd::ProgressDialog::Delete,
                         QString(), int(ops.onConflict), false);
    } else if (id == u"newFolder") {
        fd::NewFolderDialog dialog(context, this);
        dialog.exec();
    } else if (id == u"newFile") {
        fd::NewFileDialog dialog(context, this);
        dialog.exec();
    } else if (id == u"multiRename") {
        fd::MultiRenameDialog::Context mc = fd::MultiRenameDialog::boardContext();
        mc.files.clear();
        mc.history.clear();
        mc.existing.clear();
        mc.folder = context.sourceDir;
        QSet<QString> selected;
        for (const fd::FileItem &item : std::as_const(context.items)) {
            fd::RenameFile f;
            f.name = item.name;
            f.size = item.size;
            f.modified = item.modified;
            f.captured = item.modified;  // EXIF는 읽지 않는다(데모)
            mc.files.append(f);
            selected.insert(item.name);
        }
        const fl::FileSortProxy *model = m_active->model();
        for (int r = 0; r < model->rowCount(); ++r) {
            const QModelIndex i = model->index(r, fl::NameColumn);
            const QString name = i.data(fl::FullNameRole).toString();
            if (!i.data(fl::IsUpRole).toBool() && !selected.contains(name))
                mc.existing.insert(name);
        }
        // 설정 › 파일 작업 › 다중 이름 변경 — 미리보기 레코드 · 기본 프리셋 · 되돌리기 기록 수 · 문제가 있을 때
        mc.recordMode = int(ops.renamePreview);
        mc.undoDepth = ops.renameUndoDepth;
        mc.blockOnProblems = ops.renameOnProblem == fm::settings::FileOpsSettings::RenameProblem::Block;
        for (int i = 0; i < mc.presets.size(); ++i) {
            if (mc.presets.at(i).name == ops.renameDefaultPreset && i != mc.preset) {
                mc.preset = i;
                mc.rules = mc.presets.at(i).rules;
            }
        }
        fd::MultiRenameDialog dialog(mc, this);
        dialog.exec();
    }
}

fd::ProgressDialog *MainWindow::startJob(int kind, const QString &source, const QString &target, const QStringList &names,
                                         const QList<qint64> &sizes, const QString &policy, bool queued)
{
    const st::AppSettings &s = st::SettingsStore::instance().settings();
    fd::ProgressDialog::Operation op = progressOptions(s);
    op.kind = fd::ProgressDialog::Kind(kind);
    op.source = source;
    op.target = target;
    op.fileNames = names;
    op.fileSizes = sizes;
    op.policyText = policy.isEmpty() ? conflictPolicyText(int(s.fileOps.onConflict)) : policy;
    auto *job = new fd::ProgressDialog(op, this);
    job->setAttribute(Qt::WA_DeleteOnClose);
    job->setProperty("fmQueued", queued);
    m_jobs.removeAll(nullptr);
    // 동시에 실행할 작업 수(0 = 제한 없음) — 대기열에 넣은 작업은 앞선 작업이 모두 끝나야 시작한다
    const int max = s.fileOps.maxConcurrentJobs;
    const int limit = queued ? 1 : (max > 0 ? max : std::numeric_limits<int>::max());
    if (runningJobs() >= limit)
        job->setWaiting(true);
    m_jobs.append(job);
    updateTray();
    connect(job, &fd::ProgressDialog::completed, this, [this, job] {
        job->setProperty("fmCompleted", true);
        if (!job->isVisible())
            job->close();  // 표시 지연 중에 끝났거나 표시 안 함 — 창을 남기지 않는다
        notifyJobDone(job);
        QTimer::singleShot(0, this, &MainWindow::startWaitingJobs);
        QTimer::singleShot(0, this, &MainWindow::updateTray);
    });
    connect(job, &QObject::destroyed, this, [this] {
        QTimer::singleShot(0, this, &MainWindow::startWaitingJobs);
        QTimer::singleShot(0, this, &MainWindow::updateTray);
    });
    // 진행 창 표시 — 0 = 바로, n ms 뒤(그 전에 끝나면 띄우지 않음), -1 = 표시 안 함. 대기 중인 작업은 바로 보인다.
    const int delay = s.fileOps.progressDelayMs;
    if (delay == 0 || (delay > 0 && job->isWaiting()))
        job->show();
    else if (delay > 0)
        QTimer::singleShot(delay, job, [job] {
            if (!job->property("fmCompleted").toBool())
                job->show();
        });
    return job;
}

int MainWindow::runningJobs() const
{
    int n = 0;
    for (const QPointer<fd::ProgressDialog> &job : m_jobs)
        n += job && !job->isWaiting() && !job->property("fmCompleted").toBool() ? 1 : 0;
    return n;
}

QList<fd::ProgressDialog *> MainWindow::jobs() const
{
    QList<fd::ProgressDialog *> list;
    for (const QPointer<fd::ProgressDialog> &job : m_jobs) {
        if (job)
            list.append(job);
    }
    return list;
}

void MainWindow::startWaitingJobs()
{
    m_jobs.removeAll(nullptr);
    const int max = st::SettingsStore::instance().settings().fileOps.maxConcurrentJobs;
    for (const QPointer<fd::ProgressDialog> &job : std::as_const(m_jobs)) {
        if (!job->isWaiting())
            continue;
        const int limit = job->property("fmQueued").toBool() ? 1 : (max > 0 ? max : std::numeric_limits<int>::max());
        if (runningJobs() < limit)
            job->setWaiting(false);
    }
    updateTray();
}

void MainWindow::notifyJobDone(fd::ProgressDialog *job)
{
    // 설정 › 파일 작업 › 완료되면 알림 — 알림 영역 아이콘이 있으면 풍선, 없으면(창이 활성이 아닐 때) 작업 표시줄 깜박임
    if (!st::SettingsStore::instance().settings().fileOps.notifyWhenDone)
        return;
    if (m_tray && m_tray->isVisible() && QSystemTrayIcon::supportsMessages()) {
        m_tray->showMessage(u"작업 완료"_s, job->summaryText(), QSystemTrayIcon::Information, 4000);
        m_trayHold = true;  // 작업 중에만 모드라도 풍선이 보이는 동안 아이콘을 남긴다
        QTimer::singleShot(5000, this, [this] {
            m_trayHold = false;
            updateTray();
        });
    } else if (!isActiveWindow()) {
        QApplication::alert(this);
    }
}

void MainWindow::updateTray()
{
    using Tray = st::GeneralSettings::TrayIcon;
    const Tray mode = st::SettingsStore::instance().settings().general.trayIcon;
    const int running = runningJobs();
    const bool visible = mode == Tray::Always || (mode == Tray::WhileBusy && (running > 0 || m_trayHold));
    if (!visible) {
        if (m_tray)
            m_tray->hide();
        return;
    }
    if (!m_tray) {
        if (!QSystemTrayIcon::isSystemTrayAvailable())
            return;
        m_tray = new QSystemTrayIcon(windowIcon(), this);
        auto *menu = new QMenu(this);
        menu->addAction(u"창 보이기(&S)"_s, this, [this] {
            showNormal();
            raise();
            activateWindow();
        });
        menu->addSeparator();
        menu->addAction(u"끝내기(&X)"_s, this, &QWidget::close);
        m_tray->setContextMenu(menu);
        connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                showNormal();
                raise();
                activateWindow();
            }
        });
    }
    m_tray->setToolTip(running > 0 ? u"파일 관리자 — 작업 %1개 진행 중"_s.arg(running) : u"파일 관리자"_s);
    m_tray->show();
}

void MainWindow::applyStartup()
{
    using Startup = st::GeneralSettings::Startup;
    const st::AppSettings &s = st::SettingsStore::instance().settings();
    QStringList folders;
    switch (s.general.startup) {
    case Startup::RestoreLastTabs:
        if (!s.session.isEmpty())
            restoreSession(s.session);
        return;
    case Startup::HomeFolder:
        folders = {QDir::toNativeSeparators(QDir::homePath())};
        break;
    case Startup::SpecificFolders:
        folders = s.general.startupFolders;
        if (folders.isEmpty())
            folders = {QDir::toNativeSeparators(QDir::homePath())};  // 지정한 폴더가 없으면 홈 폴더
        break;
    }
    // 왼쪽 = 첫 폴더, 오른쪽 = 둘째 폴더(없으면 첫 폴더) — 실제 폴더(읽기 전용)
    for (int i = 0; i < 2; ++i) {
        FilePanel *panel = i == 0 ? m_left : m_right;
        TabState tab;
        tab.local = true;
        tab.path = QDir::toNativeSeparators(folders.value(i, folders.first()));
        tab.mode = panel->tabOptions().defaultMode;
        panel->setTabs({tab}, 0);
    }
}

st::SessionState MainWindow::sessionState() const
{
    st::SessionState session;
    for (int i = 0; i < 2; ++i) {
        const FilePanel *panel = i == 0 ? m_left : m_right;
        QList<st::SessionState::Tab> &tabs = i == 0 ? session.left : session.right;
        for (const TabState &t : panel->tabs())
            tabs.append({t.local, t.path, t.mode, t.modeSet});
        (i == 0 ? session.leftCurrent : session.rightCurrent) = panel->currentTab();
    }
    session.rightActive = m_active == m_right;
    session.docks = m_docks->saveState();
    session.dockLayouts = m_docks->layouts();
    return session;
}

void MainWindow::restoreSession(const st::SessionState &session)
{
    for (int i = 0; i < 2; ++i) {
        FilePanel *panel = i == 0 ? m_left : m_right;
        const QList<st::SessionState::Tab> &saved = i == 0 ? session.left : session.right;
        if (saved.isEmpty())
            continue;
        QList<TabState> tabs;
        for (const st::SessionState::Tab &t : saved) {
            TabState tab;
            tab.local = t.local;
            tab.path = t.path;
            tab.mode = t.modeSet || panel->tabOptions().rememberView ? t.mode : panel->tabOptions().defaultMode;
            tab.modeSet = t.modeSet;
            tabs.append(tab);
        }
        panel->setTabs(tabs, i == 0 ? session.leftCurrent : session.rightCurrent);
    }
    setActivePanel(session.rightActive ? m_right : m_left);
}

void MainWindow::openInNewTab(const QString &localPath)
{
    m_active->newTab();
    if (!localPath.isEmpty())
        m_active->openLocation(true, localPath);
    if (isMinimized())
        showNormal();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 마지막 탭과 폴더 — 설정 파일이 있을 때만(스냅숏 · 대화상자 단독 실행은 메모리 보관소)
    auto &store = st::SettingsStore::instance();
    if (!store.filePath().isEmpty()) {
        st::AppSettings s = store.settings();
        s.session = sessionState();
        store.setSettings(s);
    }
    QMainWindow::closeEvent(event);
}

bool MainWindow::helperAlive() const
{
    using Lifetime = st::ElevationSettings::HelperLifetime;
    if (!m_helperApprovedAt.isValid())
        return false;
    switch (st::SettingsStore::instance().settings().elevation.helperLifetime) {
    case Lifetime::PerOperation: return false;
    case Lifetime::FiveMinutes:  return m_helperApprovedAt.secsTo(QDateTime::currentDateTime()) < 5 * 60;
    case Lifetime::UntilExit:    return true;
    }
    return false;
}

} // namespace fm::app
