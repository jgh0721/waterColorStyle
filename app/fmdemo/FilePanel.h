#pragma once

#include <fmfilelist/ColumnSets.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/ListAppearance.h>
#include <fmfilelist/ThumbnailView.h>

#include <QModelIndex>
#include <QStringList>
#include <QWidget>

#include <cstdint>
#include <memory>

class QLabel;
class QStackedWidget;

namespace fm::filelist {
class FileGroupMatcher;
class FileListModel;
class FileListView;
class FileSortProxy;
class LocalFileSource;
class ThumbnailProvider;
}

namespace fm::ui {
class BreadcrumbBar;
class DriveButton;
class PanelStatusBar;
class PanelTabStrip;
class SegmentedControl;
}

namespace fm::app {

/// 탭 하나의 상태 — 원본 · 경로 · 보기 방식 · 정렬 · 커서 · 방문 기록.
struct TabState
{
    bool local = false;                // false = 샘플 데이터(D:), true = 이 PC(읽기 전용)
    QString path;                      // Windows 표기("D:\\Work\\fm-core")
    fm::filelist::ViewMode mode = fm::filelist::ViewMode::Auto;  // 새 탭 기본값 = 자동(BandSpec)
    bool modeSet = false;              // 이 탭에서 표시 방식을 정함 — 설정의 기본 표시 방식보다 우선(04 §2.3.4)
    bool autoThumbs = false;           // 이미지 · 동영상이 많은 폴더라 섬네일로 보는 중(자동 섬네일 — mode는 그대로)
    QString columnSet;                 // 탭에서 고른 열 세트 id(Ctrl+Shift+C) — 비면 자동 적용(05 §2.2.1)
    int cursor = 0;
    int sortColumn = -1;               // -1 = 원본 순서(샘플 보드 순서)
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    QStringList back;
    QStringList forward;
};

/// 탭 설정(설정 › 일반 · 모양 › 폴더 탭, 파일 패널 › 기본 표시 방식).
struct TabOptions
{
    enum class Position : std::uint8_t { RightOfCurrent, End };
    enum class Title : std::uint8_t { FolderName, DriveAndFolder, FullPath };

    fm::filelist::ViewMode defaultMode = fm::filelist::ViewMode::Auto;  // 새 탭 · 표시 방식을 정하지 않은 탭
    Position position = Position::RightOfCurrent;
    Title title = Title::FolderName;
    bool rememberView = true;  // 끄면 패널의 모든 탭이 같은 표시 방식을 쓴다

    bool operator==(const TabOptions &) const = default;
};

/// 파일 패널(01 §1.3 A4 · §7.1) — 탭 줄 → 주소 줄(드라이브 · 경로 이동 줄 · 여유 공간 · 보기 방식) →
/// 목록(FileListView) / 섬네일(ThumbnailView) → 상태 줄. 두 디자인이 같은 위젯 트리를 쓴다.
class FilePanel : public QWidget
{
    Q_OBJECT
public:
    explicit FilePanel(QWidget *parent = nullptr);
    ~FilePanel() override;

    void setTabs(const QList<TabState> &tabs, int current);
    int currentTab() const noexcept { return m_current; }
    const QList<TabState> &tabs() const noexcept { return m_tabs; }

    /// 지금 탭의 경로(Windows 표기).
    QString currentPath() const;
    bool isLocal() const;

    bool isActive() const noexcept { return m_active; }
    void setActive(bool active);

    fm::filelist::ViewMode viewMode() const;
    void setViewMode(fm::filelist::ViewMode mode);

    void setListAppearance(const fm::filelist::ListAppearance &appearance);
    void setThumbnailAppearance(const fm::filelist::ThumbnailAppearance &appearance);
    fm::filelist::ThumbnailView::Backend thumbnailBackend() const;
    void setThumbnailBackend(fm::filelist::ThumbnailView::Backend backend);
    void setShowHidden(bool on);
    /// 보호된 운영 체제 파일(시스템 속성) 표시 — 숨김 파일 표시가 켜져 있을 때 의미가 있다.
    bool showProtected() const noexcept { return m_showProtected; }
    void setShowProtected(bool on);
    /// 크기 · 날짜 표시 형식(두 원본 모두).
    void setDisplayFormat(const fm::filelist::DisplayFormat &format);
    const TabOptions &tabOptions() const noexcept { return m_tabOptions; }
    /// 적용하면 표시 방식을 정하지 않은 탭(기억 끔이면 모든 탭)이 기본 표시 방식으로 바뀌고 탭 이름이 바로 바뀐다.
    void setTabOptions(const TabOptions &options);
    /// 이 패널의 실제 폴더 섬네일 생성기(만드는 방법 · 대상 · 동시 개수 설정).
    fm::filelist::ThumbnailProvider *thumbnailProvider() const;
    /// 자동 섬네일(설정 › 섬네일 보기): 폴더를 열 때 파일 중 이미지 · 동영상 비율이 percent 이상이면 섬네일로 본다.
    /// 표시 방식을 직접 바꾸면 그 탭의 자동 섬네일은 풀린다.
    void setAutoThumbnails(bool on, int percent);
    /// 열 세트(설정 › 열 · 사용자 정의 열) — 폴더를 열 때 자동 적용 규칙으로 고르고, 탭에서 고른 세트가 있으면 그것.
    /// 부르기 전에는 목록 기본 열(스냅숏 · 보드). groups는 그룹 비율 규칙에 쓴다.
    void setColumnSettings(const fm::filelist::ColumnSettings &settings,
                           std::shared_ptr<const fm::filelist::FileGroupMatcher> groups);
    /// 다음 열 세트(Ctrl+Shift+C) — 탭 값으로 남아 자동 적용보다 우선한다. 마지막 세트 다음은 자동으로 돌아간다.
    void cycleColumnSet();
    /// 지금 목록에 쓰는 열 세트 이름(열 세트를 쓰지 않으면 빈 문자열).
    QString columnSetName() const;
    void setQuickFilter(const QString &text);

    fm::filelist::FileListView *listView() const noexcept { return m_list; }
    fm::filelist::ThumbnailView *thumbnailView() const noexcept { return m_thumbs; }
    fm::filelist::FileSortProxy *model() const noexcept { return m_proxy; }
    /// 커서 행(보이는 보기 — 목록 또는 섬네일).
    int cursorRow() const;
    /// 커서 항목(프록시 인덱스, 이름 열). 빈 폴더면 잘못된 인덱스.
    QModelIndex cursorIndex() const;
    /// 파일 작업 대상 — 표시한 행, 없으면 커서 행(".." 제외). 프록시 인덱스(이름 열).
    QModelIndexList operationRows() const;

    /// 목록 · 섬네일 중 보이는 쪽으로 키보드 초점을 둔다.
    void focusView();

public Q_SLOTS:
    void goUp();
    void goBack();
    void goForward();
    void refresh();
    void newTab();
    void closeCurrentTab();
    void nextTab();
    /// 원본 · 경로로 간다(방문 기록에 남긴다).
    void openLocation(bool local, const QString &path);

Q_SIGNALS:
    /// 클릭 · 초점으로 이 패널이 활성이 되려 한다.
    void activateRequested(fm::app::FilePanel *panel);
    /// 경로(또는 탭)가 바뀌었다 — 창 제목 · 명령줄 프롬프트 갱신용.
    void locationChanged();
    /// 보이는 표시 방식이 바뀌었다(직접 · 자동 섬네일).
    void viewModeChanged();
    /// 커서가 다른 항목으로 갔다(목록 · 섬네일) — 미리보기 · 속성 도크 갱신용. 경로가 바뀔 때도 따로 온다.
    void cursorChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupAddressBar(QWidget *bar);
    void buildDriveMenu();
    void saveCurrent();
    void loadCurrent();
    void navigate(bool local, const QString &path, const QString &child, bool record);
    void activate(const QModelIndex &index);
    void placeCursor(const QString &child, int fallbackRow);
    void updateAddress();
    void updateStatus();
    void updateTabTitles();
    void applyViewMode();
    void evaluateAutoThumbnails();
    void evaluateColumnSet();
    void applyChrome();
    QString tabTitle(const TabState &tab) const;

    fm::ui::PanelTabStrip *m_tabStrip = nullptr;
    QWidget *m_addressBar = nullptr;
    fm::ui::DriveButton *m_drive = nullptr;
    fm::ui::BreadcrumbBar *m_crumbs = nullptr;
    QLabel *m_free = nullptr;
    fm::ui::SegmentedControl *m_modeSelector = nullptr;
    QStackedWidget *m_stack = nullptr;
    fm::filelist::FileListView *m_list = nullptr;
    fm::filelist::ThumbnailView *m_thumbs = nullptr;
    fm::ui::PanelStatusBar *m_status = nullptr;

    fm::filelist::FileSortProxy *m_proxy = nullptr;
    fm::filelist::FileListModel *m_mock = nullptr;
    fm::filelist::LocalFileSource *m_local = nullptr;

    QList<TabState> m_tabs;
    int m_current = -1;
    bool m_active = false;
    bool m_loading = false;
    QString m_pendingChild;
    QString m_mockFree;
    bool m_showProtected = false;
    TabOptions m_tabOptions;
    bool m_autoThumbs = false;
    int m_autoThumbsPercent = 70;
    bool m_columnSetsEnabled = false;
    fm::filelist::ColumnSettings m_columnSettings;
    std::shared_ptr<const fm::filelist::FileGroupMatcher> m_groupMatcher;
    int m_appliedSet = -1;
};

} // namespace fm::app
