#pragma once

#include <fmfilelist/ListAppearance.h>
#include <fmfilelist/ThumbnailView.h>

#include <QStringList>
#include <QWidget>

class QLabel;
class QStackedWidget;

namespace fm::filelist {
class FileListModel;
class FileListView;
class FileSortProxy;
class LocalFileSource;
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
    int cursor = 0;
    int sortColumn = -1;               // -1 = 원본 순서(샘플 보드 순서)
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    QStringList back;
    QStringList forward;
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
    void setQuickFilter(const QString &text);

    fm::filelist::FileListView *listView() const noexcept { return m_list; }
    fm::filelist::ThumbnailView *thumbnailView() const noexcept { return m_thumbs; }
    fm::filelist::FileSortProxy *model() const noexcept { return m_proxy; }

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
};

} // namespace fm::app
