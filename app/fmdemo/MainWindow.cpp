#include "MainWindow.h"

#include "CatalogWindow.h"
#include "FilePanel.h"
#include "ThumbnailCompare.h"

#include <fmdialogs/ElevationFlow.h>
#include <fmdialogs/FileOpContext.h>
#include <fmdialogs/FileOpDialogs.h>
#include <fmdialogs/MultiRenameDialog.h>
#include <fmdialogs/ProgressDialog.h>
#include <fmdialogs/SettingsDialog.h>
#include <fmfilelist/FileGroups.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
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
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QShortcut>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;
namespace fl = fm::filelist;
namespace fs = fm::style;
namespace st = fm::settings;

namespace fm::app {

namespace {

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
        QMessageBox::about(this, u"정보"_s,
                           u"파일 관리자 UI 데모 — 시안1(기본) · 시안2(워터컬러)\nQt %1 · QtitanDataGrid 9.2.0(패치)"_s.arg(QString::fromLatin1(qVersion())));
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
                                        std::pair{u"Qtitan 카드 (CardView)"_s, fl::ThumbnailView::QtitanCards}}) {
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
            menu->addActions(m_viewModes->actions());
            menu->addSeparator();
            menu->addMenu(u"섬네일 구현 (활성 패널)"_s)->addActions(m_backends->actions());
            menu->addMenu(u"1줄 레코드 구분"_s)->addActions(m_sep1->actions());
            menu->addMenu(u"2줄 레코드 구분"_s)->addActions(m_sep2->actions());
            menu->addAction(action(u"nameBelow"_s));
            menu->addAction(action(u"invertCursor"_s));
            menu->addAction(action(u"invertSelectionLook"_s));
            menu->addSeparator();
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
        connect(button, &QPushButton::clicked, a, &QAction::trigger);
    }
    v->addWidget(m_splitter, 1);
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

void MainWindow::loadBoardState()
{
    TabState fmCore;
    fmCore.path = u"D:\\Work\\fm-core"_s;
    fmCore.mode = fl::ViewMode::OneLine;
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
    downloads.cursor = 3;
    TabState backup;
    backup.path = u"D:\\Backup"_s;
    m_right->setTabs({downloads, backup}, 0);

    // 섬네일 두 구현을 나란히 비교할 수 있게 왼쪽 Qt 목록 · 오른쪽 Qtitan 카드(PLAN §7.1)
    m_left->setThumbnailBackend(fl::ThumbnailView::QtList);
    m_right->setThumbnailBackend(fl::ThumbnailView::QtitanCards);
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
    if (sections & S::Panel) {
        setListAppearance(s.panel.toListAppearance());
        {
            const QSignalBlocker block(action(u"showHidden"_s));
            action(u"showHidden"_s)->setChecked(s.panel.showHidden);
        }
        for (FilePanel *panel : {m_left, m_right}) {
            panel->setShowHidden(s.panel.showHidden);
            panel->model()->setFoldersFirst(s.panel.foldersFirst);
        }
        m_sep1->actions().value(int(s.panel.separator1))->setChecked(true);
        m_sep2->actions().value(int(s.panel.separator2))->setChecked(true);
    }
    if (sections & (S::Panel | S::Thumbs)) {
        for (FilePanel *panel : {m_left, m_right}) {
            fl::ThumbnailAppearance t = s.thumbs.toThumbnailAppearance(s.panel);
            t.size = panel->thumbnailView()->appearance().size;  // 크기는 패널에서 Ctrl+휠로 바꾼 값 유지
            panel->setThumbnailAppearance(t);
        }
    }
    if (sections & S::Groups) {
        const auto matcher = std::make_shared<const fl::FileGroupMatcher>(s.groups);
        for (FilePanel *panel : {m_left, m_right})
            panel->model()->setGroupMatcher(matcher);
    }
    if (sections & S::General) {
        m_commandLine->setVisible(s.general.showCommandLine);
        m_functionKeys->setVisible(s.general.showFunctionKeyBar);
    }
    if (sections & S::Keys)
        applyKeyBindings(s.keys);
}

void MainWindow::applyKeyBindings(const st::KeyBindingSettings &keys)
{
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
    st::SettingsStore::instance().setSettings(s);
}

fm::dialogs::ElevationFlow *MainWindow::startElevationFlow(int scenario)
{
    auto *flow = new fm::dialogs::ElevationFlow(
        scenario == 1 ? fm::dialogs::ElevationFlow::DeleteWithOwnership : fm::dialogs::ElevationFlow::CopyToProtected, this);
    connect(flow, &fm::dialogs::ElevationFlow::finished, flow, &QObject::deleteLater);
    flow->start();
    return flow;
}

fm::dialogs::FileOpContext MainWindow::operationContext() const
{
    namespace fd = fm::dialogs;
    fd::FileOpContext c;
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
    auto showProgress = [this, &context](fd::ProgressDialog::Kind kind, const QString &target) {
        fd::ProgressDialog::Operation op;
        op.kind = kind;
        op.source = context.sourceDir;
        op.target = target;
        for (const fd::FileItem &item : std::as_const(context.items)) {
            op.fileNames.append(item.name);
            op.fileSizes.append(item.isDir ? qint64(64) << 20 : std::max<qint64>(1, item.size));
        }
        op.policyText = u"같은 이름이 있으면 매번 묻기"_s;
        auto *progress = new fd::ProgressDialog(op, this);
        progress->setAttribute(Qt::WA_DeleteOnClose);
        progress->show();
    };

    if (id == u"copy") {
        fd::CopyDialog dialog(context, this);
        const int result = dialog.exec();
        if (result == QDialog::Accepted || result == fd::CopyDialog::Queued)
            showProgress(fd::ProgressDialog::Copy, dialog.request().destination);
    } else if (id == u"move" || id == u"rename") {
        const bool move = id == u"move";
        fd::MoveRenameDialog dialog(context, move, this);
        if (dialog.exec() == QDialog::Accepted && move && dialog.plan().operation != fd::MoveRenamePlan::Rename)
            showProgress(fd::ProgressDialog::Move, dialog.request().target);
    } else if (id == u"delete" || id == u"deletePermanent") {
        fd::DeleteDialog dialog(context, id == u"deletePermanent", this);
        if (dialog.exec() == QDialog::Accepted)
            showProgress(dialog.request().permanent ? fd::ProgressDialog::DeletePermanent : fd::ProgressDialog::Delete,
                         QString());
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
        fd::MultiRenameDialog dialog(mc, this);
        dialog.exec();
    }
}

} // namespace fm::app
