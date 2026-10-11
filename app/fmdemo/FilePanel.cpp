#include "FilePanel.h"

#include <fmfilelist/FileListModel.h>
#include <fmfilelist/FileListStats.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmfilelist/ColumnValues.h>
#include <fmfilelist/FileGroups.h>
#include <fmfilelist/LocalFileSource.h>
#include <fmfilelist/ThumbnailProvider.h>
#include <fmfilelist/MockFileSource.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/BreadcrumbBar.h>
#include <fmwidgets/DriveButton.h>
#include <fmwidgets/PanelStatusBar.h>
#include <fmwidgets/PanelTabStrip.h>
#include <fmwidgets/SegmentedControl.h>

#include <QDir>
#include <QEvent>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QStackedWidget>
#include <QStorageInfo>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;
namespace fl = fm::filelist;
namespace fs = fm::style;

namespace fm::app {

namespace {

/// 주소 줄 바탕(01 §1.3 A4b · 06 §4.14): 시안1 36 · --surface, 시안2 32 · --win. 아래 1 px --line은 같다.
class AddressBarFrame : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        p.fillRect(rect(), tc.isWatercolor() ? tc[fs::Token::Win] : tc[fs::Token::Surface]);
        p.fillRect(QRect(0, height() - 1, width(), 1), tc[fs::Token::Line]);
    }
};

QString encode(bool local, const QString &path)
{
    return (local ? u"L|"_s : u"M|"_s) + path;
}

QString nativePath(const QString &path)
{
    return QDir::toNativeSeparators(QDir::cleanPath(QDir::fromNativeSeparators(path)));
}

} // namespace

FilePanel::FilePanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabStrip = new fm::ui::PanelTabStrip;
    m_addressBar = new AddressBarFrame;
    setupAddressBar(m_addressBar);
    m_stack = new QStackedWidget;
    m_list = new fl::FileListView;
    m_thumbs = new fl::ThumbnailView;
    m_stack->addWidget(m_list);
    m_stack->addWidget(m_thumbs);
    m_status = new fm::ui::PanelStatusBar;
    layout->addWidget(m_tabStrip);
    layout->addWidget(m_addressBar);
    layout->addWidget(m_stack, 1);
    layout->addWidget(m_status);

    m_proxy = new fl::FileSortProxy(this);
    m_proxy->setShowSystem(m_showProtected);
    m_mock = new fl::FileListModel(this);
    m_local = new fl::LocalFileSource(this);
    m_local->setShowHidden(true);
    m_proxy->setSourceModel(m_mock);
    m_list->setModel(m_proxy);
    m_thumbs->setModel(m_proxy);

    QTabBar *tabs = m_tabStrip->tabBar();
    connect(tabs, &QTabBar::currentChanged, this, [this](int index) {
        if (m_loading || index < 0 || index == m_current)
            return;
        saveCurrent();
        m_current = index;
        loadCurrent();
        Q_EMIT activateRequested(this);
    });
    connect(tabs, &QTabBar::tabMoved, this, [this](int from, int to) {
        m_tabs.move(from, to);
        m_current = m_tabStrip->tabBar()->currentIndex();
    });
    connect(m_tabStrip, &fm::ui::PanelTabStrip::newTabRequested, this, &FilePanel::newTab);
    connect(m_tabStrip, &fm::ui::PanelTabStrip::closeTabRequested, this, [this](int index) {
        if (m_tabs.size() <= 1)
            return;
        saveCurrent();
        m_loading = true;
        m_tabs.removeAt(index);
        m_tabStrip->tabBar()->removeTab(index);
        m_current = m_tabStrip->tabBar()->currentIndex();
        m_loading = false;
        loadCurrent();
    });

    connect(m_list, &fl::FileListView::activated, this, &FilePanel::activate);
    connect(m_thumbs, &fl::ThumbnailView::activated, this, &FilePanel::activate);
    connect(m_list, &fl::FileListView::upRequested, this, &FilePanel::goUp);
    connect(m_thumbs, &fl::ThumbnailView::upRequested, this, &FilePanel::goUp);
    connect(m_list, &fl::FileListView::cursorRowChanged, this, &FilePanel::cursorChanged);
    connect(m_thumbs, &fl::ThumbnailView::cursorRowChanged, this, &FilePanel::cursorChanged);
    connect(m_list, &fl::FileListView::paneActivated, this, [this] { Q_EMIT activateRequested(this); });
    connect(m_thumbs, &fl::ThumbnailView::paneActivated, this, [this] { Q_EMIT activateRequested(this); });
    connect(m_list, &fl::FileListView::sortChanged, this, [this](int column, Qt::SortOrder order) {
        if (m_loading || m_current < 0)
            return;
        m_tabs[m_current].sortColumn = column;
        m_tabs[m_current].sortOrder = order;
    });
    connect(m_modeSelector, &fm::ui::SegmentedControl::currentIndexChanged, this, [this](int index) {
        if (m_loading)
            return;
        setViewMode(fl::ViewMode(index));
        Q_EMIT activateRequested(this);
    });
    connect(m_crumbs, &fm::ui::BreadcrumbBar::segmentClicked, this, [this](int index) {
        openLocation(isLocal(), fl::MockFileSource::pathPrefix(currentPath(), index + 1));
        Q_EMIT activateRequested(this);
    });
    connect(m_crumbs, &fm::ui::BreadcrumbBar::pathEntered, this, [this](const QString &text) {
        const QString path = nativePath(text.trimmed());
        // 샘플 드라이브(D:)는 샘플 경로로, 그 밖에는 실제 폴더(있을 때만)로
        if (!isLocal() && path.startsWith(u"D:"_s, Qt::CaseInsensitive))
            openLocation(false, path);
        else if (QDir(path).exists())
            openLocation(true, path);
    });

    connect(m_proxy, &QAbstractItemModel::modelReset, this, &FilePanel::updateStatus);
    connect(m_proxy, &QAbstractItemModel::layoutChanged, this, &FilePanel::updateStatus);
    connect(m_proxy, &QAbstractItemModel::rowsInserted, this, &FilePanel::updateStatus);
    connect(m_proxy, &QAbstractItemModel::rowsRemoved, this, &FilePanel::updateStatus);
    connect(m_proxy, &QAbstractItemModel::dataChanged, this, &FilePanel::updateStatus);
    connect(m_local->model(), &fl::FileSystemListProxy::loaded, this, [this] {
        if (isLocal()) {
            evaluateColumnSet();  // 그룹 비율 규칙은 목록을 다 읽어야 판정된다 — 세트를 먼저 바꾸고 커서를 둔다
            placeCursor(m_pendingChild, m_current >= 0 ? m_tabs[m_current].cursor : 0);
            evaluateAutoThumbnails();
        }
        updateStatus();
    });

    for (QWidget *w : {static_cast<QWidget *>(m_tabStrip->tabBar()), m_addressBar, static_cast<QWidget *>(m_crumbs),
                       static_cast<QWidget *>(m_drive), static_cast<QWidget *>(m_status)})
        w->installEventFilter(this);

    connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, &FilePanel::applyChrome);
    applyChrome();
}

FilePanel::~FilePanel()
{
    // 뷰가 모델보다 먼저 사라지게 한다(Qtitan 편집기가 파괴 중에 인덱스를 읽는다).
    delete m_stack;
    m_stack = nullptr;
}

void FilePanel::setupAddressBar(QWidget *bar)
{
    auto *h = new QHBoxLayout(bar);
    m_drive = new fm::ui::DriveButton;
    auto *menu = new QMenu(m_drive);
    m_drive->setMenu(menu);
    connect(menu, &QMenu::aboutToShow, this, &FilePanel::buildDriveMenu);
    m_crumbs = new fm::ui::BreadcrumbBar;
    m_free = new QLabel;
    m_free->setTextInteractionFlags(Qt::NoTextInteraction);
    m_modeSelector = new fm::ui::SegmentedControl({u"1줄"_s, u"2줄"_s, u"자동"_s, u"섬네일"_s});
    m_modeSelector->setSegmentSize(fm::ui::SegmentedControl::Compact);
    m_modeSelector->setItemGlyphs({fm::ui::glyph::ViewOneLine, fm::ui::glyph::ViewTwoLine, fm::ui::glyph::None,
                                   fm::ui::glyph::ViewThumbnails});
    m_modeSelector->setItemToolTips({u"한 줄에 모든 열"_s, u"이름과 메타데이터를 두 줄로"_s, u"잘리는 이름이 많으면 2줄로"_s,
                                     u"섬네일 (Ctrl+Shift+4)"_s});
    m_modeSelector->setAccessibleName(u"레코드 표시 방식"_s);
    h->addWidget(m_drive);
    h->addWidget(m_crumbs, 1);
    h->addWidget(m_free);
    h->addWidget(m_modeSelector);
}

void FilePanel::applyChrome()
{
    const fs::ThemeColors &tc = fs::themeColorsFor(this);
    const bool wc = tc.isWatercolor();
    // 주소 줄: 시안1 36 · 좌우 8 · 간격 8 / 시안2 32 · 간격 6 (아래 1 px 선 포함)
    auto *h = static_cast<QHBoxLayout *>(m_addressBar->layout());
    h->setContentsMargins(8, 0, 8, 1);
    h->setSpacing(wc ? 6 : 8);
    m_addressBar->setFixedHeight(wc ? 32 : 36);
    m_free->setFont(fs::withTabularNumbers(fs::pixelFont(font(), 12)));
    QPalette palette = m_free->palette();
    palette.setColor(QPalette::WindowText, wc ? tc[fs::Token::Fg] : tc[fs::Token::Fg3]);
    m_free->setPalette(palette);
    m_addressBar->update();
}

bool FilePanel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
        Q_EMIT activateRequested(this);
    return QWidget::eventFilter(watched, event);
}

void FilePanel::buildDriveMenu()
{
    QMenu *menu = m_drive->menu();
    menu->clear();
    const fl::MockFolder sample = fl::MockFileSource::folder(u"D:\\"_s);
    QAction *mock = menu->addAction(u"D:  샘플 데이터 — %1 · %2"_s.arg(sample.volumeLabel, sample.freeText));
    mock->setCheckable(true);
    mock->setChecked(!isLocal());
    connect(mock, &QAction::triggered, this, [this] { openLocation(false, u"D:\\"_s); });
    menu->addSection(u"이 PC (읽기 전용)"_s);
    const auto volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &volume : volumes) {
        if (!volume.isValid() || !volume.isReady())
            continue;
        const QString root = QDir::toNativeSeparators(volume.rootPath());
        const QString name = volume.name().isEmpty() ? u"로컬 디스크"_s : volume.name();
        QAction *action = menu->addAction(u"%1  %2 — 여유 %3 / %4"_s.arg(root.left(2), name, fl::formatSize(volume.bytesAvailable()),
                                                                     fl::formatSize(volume.bytesTotal())));
        action->setCheckable(true);
        action->setChecked(isLocal() && currentPath().startsWith(root.left(2), Qt::CaseInsensitive));
        connect(action, &QAction::triggered, this, [this, root] { openLocation(true, root); });
    }
}

void FilePanel::setTabs(const QList<TabState> &tabs, int current)
{
    m_loading = true;
    QTabBar *bar = m_tabStrip->tabBar();
    while (bar->count() > 0)
        bar->removeTab(0);
    m_tabs = tabs;
    for (const TabState &tab : std::as_const(m_tabs))
        bar->addTab(tabTitle(tab));
    m_current = std::clamp(current, 0, int(m_tabs.size()) - 1);
    bar->setCurrentIndex(m_current);
    m_loading = false;
    loadCurrent();
}

QString FilePanel::currentPath() const
{
    return m_current >= 0 ? m_tabs.at(m_current).path : QString();
}

bool FilePanel::isLocal() const
{
    return m_current >= 0 && m_tabs.at(m_current).local;
}

QString FilePanel::tabTitle(const TabState &tab) const
{
    const QStringList parts = fl::MockFileSource::pathSegments(tab.path);
    const QString root = tab.path.left(2) + u'\\';
    if (parts.isEmpty())
        return root;
    switch (m_tabOptions.title) {
    case TabOptions::Title::FolderName:
        break;
    case TabOptions::Title::DriveAndFolder:  // "D:\fm-core", 더 깊으면 "D:\…\fm-core"
        return parts.size() == 1 ? root + parts.last() : root + u"…\\"_s + parts.last();
    case TabOptions::Title::FullPath:
        return tab.path;
    }
    return parts.last();
}

void FilePanel::setActive(bool active)
{
    m_active = active;
    m_tabStrip->setPaneActive(active);
    m_list->setPaneActive(active);
    m_thumbs->setPaneActive(active);
}

fl::ViewMode FilePanel::viewMode() const
{
    if (m_current < 0)
        return fl::ViewMode::Auto;
    const TabState &tab = m_tabs.at(m_current);
    return tab.autoThumbs ? fl::ViewMode::Thumbnails : tab.mode;
}

void FilePanel::setViewMode(fl::ViewMode mode)
{
    if (m_current < 0)
        return;
    if (m_tabOptions.rememberView) {
        m_tabs[m_current].mode = mode;
        m_tabs[m_current].modeSet = true;
        m_tabs[m_current].autoThumbs = false;  // 직접 고르면 자동 섬네일보다 우선
    } else {
        for (TabState &tab : m_tabs) {  // 탭마다 기억하지 않으면 패널 전체에
            tab.mode = mode;
            tab.autoThumbs = false;
        }
    }
    applyViewMode();
}

fl::ThumbnailProvider *FilePanel::thumbnailProvider() const
{
    return m_local->thumbnails();
}

void FilePanel::setAutoThumbnails(bool on, int percent)
{
    m_autoThumbs = on;
    m_autoThumbsPercent = std::clamp(percent, 1, 100);
    evaluateAutoThumbnails();
}

void FilePanel::evaluateAutoThumbnails()
{
    if (m_current < 0)
        return;
    TabState &tab = m_tabs[m_current];
    bool on = false;
    if (m_autoThumbs && tab.mode != fl::ViewMode::Thumbnails) {
        // 파일(폴더 제외) 중 이미지 · 동영상(종류 Img)의 비율
        int files = 0;
        int media = 0;
        for (int r = 0; r < m_proxy->rowCount(); ++r) {
            const QModelIndex i = m_proxy->index(r, fl::NameColumn);
            if (i.data(fl::IsDirRole).toBool())
                continue;
            ++files;
            if (fl::Kind(i.data(fl::KindRole).toInt()) == fl::Kind::Img)
                ++media;
        }
        on = files > 0 && media * 100 >= m_autoThumbsPercent * files;
    }
    if (tab.autoThumbs == on)
        return;
    tab.autoThumbs = on;
    applyViewMode();
}

void FilePanel::setColumnSettings(const fl::ColumnSettings &settings, std::shared_ptr<const fl::FileGroupMatcher> groups)
{
    m_columnSettings = settings;
    m_groupMatcher = std::move(groups);
    m_columnSetsEnabled = !settings.sets.isEmpty();
    m_appliedSet = -1;  // 세트 내용이 바뀌었을 수 있다 — 다시 적용
    evaluateColumnSet();
}

void FilePanel::cycleColumnSet()
{
    if (!m_columnSetsEnabled || m_current < 0)
        return;
    // 자동이면 지금 보이는 세트의 다음, 고른 세트면 그 다음 — 마지막 세트 다음은 자동(탭 값 비움)
    const QList<fl::ColumnSet> &sets = m_columnSettings.sets;
    const QString current = m_tabs[m_current].columnSet;
    QString id;
    if (current.isEmpty()) {
        id = sets.at((m_appliedSet + 1) % int(sets.size())).id;
    } else {
        int i = 0;
        while (i < sets.size() && sets.at(i).id != current)
            ++i;
        id = i + 1 < sets.size() ? sets.at(i + 1).id : QString();
    }
    if (m_tabOptions.rememberView) {
        m_tabs[m_current].columnSet = id;
    } else {
        for (TabState &tab : m_tabs)  // 탭마다 기억하지 않으면 패널 전체에
            tab.columnSet = id;
    }
    evaluateColumnSet();
}

QString FilePanel::columnSetName() const
{
    return m_appliedSet >= 0 && m_appliedSet < m_columnSettings.sets.size() ? m_columnSettings.sets.at(m_appliedSet).name : QString();
}

void FilePanel::evaluateColumnSet()
{
    if (!m_columnSetsEnabled || m_current < 0)
        return;
    int index = -1;
    const QString chosen = m_tabs[m_current].columnSet;
    for (int i = 0; i < m_columnSettings.sets.size() && !chosen.isEmpty(); ++i) {
        if (m_columnSettings.sets.at(i).id == chosen)
            index = i;
    }
    if (index < 0)
        index = fl::resolveColumnSet(m_columnSettings, currentPath(), isLocal(), m_groupMatcher.get(), m_proxy);
    if (index == m_appliedSet)
        return;
    m_appliedSet = index;
    const fl::ColumnSet &set = m_columnSettings.sets.at(index);
    QList<fl::ColumnDef> extras;
    const fl::ListColumnLayout layout = fl::mainLayoutForSet(set, &extras);
    // 읽을 Windows 속성 — 속성 열과 식 안의 [System.…]
    QStringList properties;
    static const QRegularExpression token(u"\\[([^\\]]+)\\]"_s);
    for (const fl::ColumnDef &d : std::as_const(extras)) {
        if (d.kind == fl::ColumnDef::Kind::WindowsProperty)
            properties.append(d.source);
        else if (d.kind == fl::ColumnDef::Kind::Expression)
            for (QRegularExpressionMatchIterator it = token.globalMatch(d.source); it.hasNext();) {
                const QString name = it.next().captured(1).trimmed();
                if (name.contains(u'.'))
                    properties.append(name);
            }
    }
    properties.removeDuplicates();
    m_local->properties()->setProperties(properties);
    const int cursor = cursorRow();
    m_mock->setExtraColumns({extras, set.columns, nullptr});  // 샘플 파일에는 Windows 속성이 없다(빈 값 규칙)
    m_local->model()->setExtraColumns({extras, set.columns, m_local->properties()});
    m_list->setColumnLayout(layout);
    if (cursor >= 0) {
        m_list->setCursorRow(cursor);
        m_thumbs->setCursorRow(cursor);
    }
}

void FilePanel::setShowProtected(bool on)
{
    m_showProtected = on;
    m_proxy->setShowSystem(on);
}

void FilePanel::setDisplayFormat(const fl::DisplayFormat &format)
{
    m_mock->setDisplayFormat(format);
    m_local->model()->setDisplayFormat(format);
}

void FilePanel::setTabOptions(const TabOptions &options)
{
    m_tabOptions = options;
    for (TabState &tab : m_tabs) {
        if (!options.rememberView || !tab.modeSet) {
            tab.mode = options.defaultMode;
            tab.modeSet = false;
        }
    }
    updateTabTitles();
    applyViewMode();
}

int FilePanel::cursorRow() const
{
    return m_stack->currentWidget() == m_thumbs ? m_thumbs->cursorRow() : m_list->cursorRow();
}

QModelIndex FilePanel::cursorIndex() const
{
    const int row = cursorRow();
    return row >= 0 && row < m_proxy->rowCount() ? m_proxy->index(row, fl::NameColumn) : QModelIndex();
}

QModelIndexList FilePanel::operationRows() const
{
    QModelIndexList rows;
    for (int r = 0; r < m_proxy->rowCount(); ++r) {
        const QModelIndex i = m_proxy->index(r, fl::NameColumn);
        if (i.data(fl::MarkedRole).toBool() && !i.data(fl::IsUpRole).toBool())
            rows.append(i);
    }
    if (rows.isEmpty()) {
        const int cursor = cursorRow();
        const QModelIndex i = cursor >= 0 ? m_proxy->index(cursor, fl::NameColumn) : QModelIndex();
        if (i.isValid() && !i.data(fl::IsUpRole).toBool())
            rows.append(i);
    }
    return rows;
}

void FilePanel::applyViewMode()
{
    const fl::ViewMode mode = viewMode();
    const bool thumbs = mode == fl::ViewMode::Thumbnails;
    const int cursor = m_stack->currentWidget() == m_thumbs ? m_thumbs->cursorRow() : m_list->cursorRow();
    const bool hadFocus = m_stack->currentWidget() && m_stack->currentWidget()->hasFocus();
    m_loading = true;
    m_modeSelector->setCurrentIndex(int(mode));
    m_loading = false;
    m_list->setViewMode(mode);
    m_stack->setCurrentWidget(thumbs ? static_cast<QWidget *>(m_thumbs) : m_list);
    if (cursor >= 0) {
        m_list->setCursorRow(cursor);
        m_thumbs->setCursorRow(cursor);
    }
    if (hadFocus)
        focusView();
    Q_EMIT viewModeChanged();
}

void FilePanel::setListAppearance(const fl::ListAppearance &appearance)
{
    m_list->setAppearance(appearance);
    // 섬네일 캡션도 목록 글꼴(글꼴만 — 크기는 타일 치수에 묶여 있다)
    if (!appearance.fontFamily.isEmpty() && m_thumbs->font().families().value(0) != appearance.fontFamily) {
        QFont f = m_thumbs->font();
        f.setFamilies({appearance.fontFamily, u"Segoe UI"_s, u"Malgun Gothic"_s});
        m_thumbs->setFont(f);
    }
}

void FilePanel::setThumbnailAppearance(const fl::ThumbnailAppearance &appearance)
{
    m_thumbs->setAppearance(appearance);
}

fl::ThumbnailView::Backend FilePanel::thumbnailBackend() const
{
    return m_thumbs->backend();
}

void FilePanel::setThumbnailBackend(fl::ThumbnailView::Backend backend)
{
    m_thumbs->setBackend(backend);
}

void FilePanel::setShowHidden(bool on)
{
    m_proxy->setShowHidden(on);
    m_local->setShowHidden(on);
}

void FilePanel::setQuickFilter(const QString &text)
{
    m_proxy->setQuickFilter(text);
}

void FilePanel::focusView()
{
    if (QWidget *w = m_stack->currentWidget())
        w->setFocus(Qt::OtherFocusReason);
}

void FilePanel::saveCurrent()
{
    if (m_current < 0 || m_current >= m_tabs.size())
        return;
    const int cursor = m_stack->currentWidget() == m_thumbs ? m_thumbs->cursorRow() : m_list->cursorRow();
    m_tabs[m_current].cursor = std::max(0, cursor);
}

void FilePanel::loadCurrent()
{
    if (m_current < 0 || m_current >= m_tabs.size())
        return;
    const TabState &tab = m_tabs.at(m_current);
    navigate(tab.local, tab.path, QString(), false);
    applyViewMode();
}

void FilePanel::openLocation(bool local, const QString &path)
{
    navigate(local, local ? nativePath(path) : path, QString(), true);
}

void FilePanel::navigate(bool local, const QString &path, const QString &child, bool record)
{
    if (m_current < 0)
        return;
    TabState &tab = m_tabs[m_current];
    const bool moved = tab.local != local || tab.path.compare(path, Qt::CaseInsensitive) != 0;
    if (record && moved) {
        tab.back.append(encode(tab.local, tab.path));
        tab.forward.clear();
        tab.cursor = 0;
        if (tab.local != local)
            tab.sortColumn = -1;
    }
    tab.local = local;
    tab.path = path;
    m_pendingChild = child;
    m_loading = true;
    if (local) {
        if (m_proxy->sourceModel() != m_local->model())
            m_proxy->setSourceModel(m_local->model());
        if (tab.sortColumn < 0)
            tab.sortColumn = fl::NameColumn;  // 실제 폴더는 이름순
        m_local->setPath(path);
        m_list->setSortIndicator(tab.sortColumn, tab.sortOrder, true);
    } else {
        const fl::MockFolder folder = fl::MockFileSource::folder(path);
        m_mock->setEntries(folder.entries);
        if (m_proxy->sourceModel() != m_mock)
            m_proxy->setSourceModel(m_mock);
        m_mockFree = folder.freeText;
        if (tab.sortColumn < 0) {
            m_proxy->sort(-1);  // 보드의 순서 그대로 — "이름 ↑" 표시만
            m_list->setSortIndicator(fl::NameColumn, Qt::AscendingOrder, false);
        } else {
            m_list->setSortIndicator(tab.sortColumn, tab.sortOrder, true);
        }
    }
    m_loading = false;
    evaluateColumnSet();  // 열 세트(경로 · 알려진 폴더 규칙은 바로, 그룹 비율은 실제 폴더면 loaded에서 다시)
    // 샘플은 바로, 실제 폴더는 읽은 뒤(loaded)에 커서를 둔다. 이미 읽어 둔 폴더는 바로 둔다.
    placeCursor(child, tab.cursor);
    // 자동 섬네일 — 샘플은 바로, 실제 폴더는 이미 읽어 둔 경우에만(아니면 loaded에서)
    if (!local || m_proxy->rowCount() > (m_local->model()->hasUpRow() ? 1 : 0))
        evaluateAutoThumbnails();
    updateAddress();
    updateStatus();
    updateTabTitles();
    Q_EMIT locationChanged();
}

void FilePanel::placeCursor(const QString &child, int fallbackRow)
{
    int row = -1;
    if (!child.isEmpty()) {
        for (int r = 0; r < m_proxy->rowCount(); ++r) {
            if (m_proxy->index(r, fl::NameColumn).data(fl::FullNameRole).toString().compare(child, Qt::CaseInsensitive) == 0) {
                row = r;
                break;
            }
        }
        if (row >= 0)
            m_pendingChild.clear();
    }
    if (row < 0)
        row = std::clamp(fallbackRow, 0, std::max(0, m_proxy->rowCount() - 1));
    if (m_proxy->rowCount() == 0)
        return;
    m_list->setCursorRow(row);
    m_thumbs->setCursorRow(row);
}

void FilePanel::activate(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    if (index.data(fl::IsUpRole).toBool()) {
        goUp();
        return;
    }
    if (!index.data(fl::IsDirRole).toBool())
        return;  // 파일은 열지 않는다(읽기 전용 데모)
    const QString name = index.data(fl::FullNameRole).toString();
    if (isLocal())
        openLocation(true, index.data(fl::FilePathRole).toString());
    else
        openLocation(false, fl::MockFileSource::childPath(currentPath(), name));
}

void FilePanel::goUp()
{
    if (m_current < 0)
        return;
    const QString path = currentPath();
    const QString parent = fl::MockFileSource::parentPath(path);
    if (parent.isEmpty())
        return;
    const QStringList parts = fl::MockFileSource::pathSegments(path);
    TabState &tab = m_tabs[m_current];
    tab.back.append(encode(tab.local, tab.path));
    tab.forward.clear();
    tab.cursor = 0;
    navigate(tab.local, parent, parts.isEmpty() ? QString() : parts.last(), false);
}

void FilePanel::goBack()
{
    if (m_current < 0 || m_tabs[m_current].back.isEmpty())
        return;
    TabState &tab = m_tabs[m_current];
    const QString entry = tab.back.takeLast();
    tab.forward.append(encode(tab.local, tab.path));
    tab.cursor = 0;
    navigate(entry.startsWith(u"L|"), entry.mid(2), QString(), false);
}

void FilePanel::goForward()
{
    if (m_current < 0 || m_tabs[m_current].forward.isEmpty())
        return;
    TabState &tab = m_tabs[m_current];
    const QString entry = tab.forward.takeLast();
    tab.back.append(encode(tab.local, tab.path));
    tab.cursor = 0;
    navigate(entry.startsWith(u"L|"), entry.mid(2), QString(), false);
}

void FilePanel::refresh()
{
    saveCurrent();
    loadCurrent();
}

void FilePanel::newTab()
{
    saveCurrent();
    TabState tab;
    tab.mode = m_tabOptions.defaultMode;
    if (m_current >= 0) {
        tab.local = m_tabs[m_current].local;
        tab.path = m_tabs[m_current].path;
        if (!m_tabOptions.rememberView)
            tab.mode = m_tabs[m_current].mode;  // 패널 전체가 같은 방식
    } else {
        tab.path = u"D:\\"_s;
    }
    m_loading = true;
    const int index = m_tabOptions.position == TabOptions::Position::End ? int(m_tabs.size()) : m_current + 1;
    m_tabs.insert(index, tab);
    m_tabStrip->tabBar()->insertTab(index, tabTitle(tab));
    m_tabStrip->tabBar()->setCurrentIndex(index);
    m_current = index;
    m_loading = false;
    loadCurrent();
}

void FilePanel::closeCurrentTab()
{
    if (m_tabs.size() > 1)
        Q_EMIT m_tabStrip->closeTabRequested(m_current);
}

void FilePanel::nextTab()
{
    if (m_tabs.size() > 1)
        m_tabStrip->tabBar()->setCurrentIndex((m_current + 1) % int(m_tabs.size()));
}

void FilePanel::updateAddress()
{
    const QString path = currentPath();
    m_drive->setText(path.left(2).toUpper());
    m_drive->updateGeometry();
    m_crumbs->setSegments(fl::MockFileSource::pathSegments(path));
    m_crumbs->setFullPath(path);
    m_free->setText(isLocal() ? m_local->freeSpaceText() : m_mockFree);
}

void FilePanel::updateStatus()
{
    const fl::FileListStats stats = fl::FileListStats::compute(m_proxy);
    m_status->setLeftText(stats.selectionText());
    m_status->setRightText(stats.secondaryText());
}

void FilePanel::updateTabTitles()
{
    QTabBar *bar = m_tabStrip->tabBar();
    for (int i = 0; i < m_tabs.size() && i < bar->count(); ++i)
        bar->setTabText(i, tabTitle(m_tabs.at(i)));
}

} // namespace fm::app
