#pragma once

#include "fmfilelist/ListAppearance.h"
#include "fmfilelist/ListColumns.h"

#include <QModelIndex>
#include <QWidget>

#include <memory>

class QAbstractItemModel;
class QTimer;

namespace fm::filelist {

class FileListViewPrivate;

/// 파일 목록(1줄 · 2줄 · 자동) — Qtitan 밴드 테이블 뷰 하나로 두 배치를 모두 그린다(BandSpec).
///
/// - 모델은 FileRoles 규약을 따른다(보통 FileSortProxy). 정렬은 머리글 클릭을 받아 프록시가 하고,
///   Qtitan 자체 정렬은 모델 순서를 그대로 두게 한다(NoSortRole).
/// - 선택은 모델의 MarkedRole(TC 방식 — 클릭은 커서만 옮긴다), 커서는 Qtitan 포커스 행이다.
///   Insert · Space = 표시 후 아래로, Shift+↑↓ = 표시하며 이동, Ctrl+A = 모두 표시, 숫자 패드 * = 반전.
/// - 레코드 바탕 · 선택 · 커서 틀은 레코드 단위(패치 Q3), 글자는 셀 델리게이트가 그린다.
/// - 색은 themeColorsFor(this)에서 읽으므로 ThemeScope(설정창의 적용 전 색)가 미리보기에 적용된다.
/// - QtitanDataGrid 없이 빌드하면(FM_WITH_QTITAN 0) 안내 문구만 보인다. API와 상태(모델 · 커서 · 정렬 · 표시)는 같다.
class FileListView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(fm::filelist::ViewMode viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(bool twoLine READ isTwoLine NOTIFY twoLineChanged)
    Q_PROPERTY(bool paneActive READ isPaneActive WRITE setPaneActive)
    Q_PROPERTY(bool previewMode READ isPreviewMode WRITE setPreviewMode)
    Q_PROPERTY(int cursorRow READ cursorRow WRITE setCursorRow NOTIFY cursorRowChanged)

public:
    explicit FileListView(QWidget *parent = nullptr);
    ~FileListView() override;

    /// FileRoles 규약 모델. QSortFilterProxyModel이면 머리글 클릭으로 정렬한다.
    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;

    /// 열 배치(기본: 파일 목록 열). 설정의 미리보기는 다른 모델(그룹 · 열 세트)과 배치를 준다.
    const ListColumnLayout &columnLayout() const noexcept;
    void setColumnLayout(const ListColumnLayout &layout);

    /// OneLine · TwoLine · Auto (Thumbnails는 ThumbnailView가 맡는다 — 여기서는 Auto처럼 다룬다).
    ViewMode viewMode() const noexcept;
    void setViewMode(ViewMode mode);
    /// 지금 실제 배치가 2줄인지(자동이면 규칙의 결과).
    bool isTwoLine() const noexcept;

    const ListAppearance &appearance() const noexcept;
    void setAppearance(const ListAppearance &appearance);

    /// 활성 패널(fmPaneActive). 선택 색 · 커서 모양이 달라진다.
    bool isPaneActive() const;
    void setPaneActive(bool active);

    /// 미리보기(설정창): 포커스 · 스크롤 막대 · 머리글 메뉴 없음, 높이는 행 수로 정한다.
    bool isPreviewMode() const noexcept;
    void setPreviewMode(bool preview);

    int cursorRow() const;
    void setCursorRow(int row);
    QModelIndex cursorIndex() const;

    /// 정렬 표시만 바꾸거나(apply = false — 샘플 데이터는 보드 순서 그대로 두고 "이름 ↑"만 보인다) 실제로 정렬한다.
    void setSortIndicator(int column, Qt::SortOrder order, bool apply = true);
    int sortColumn() const;
    Qt::SortOrder sortOrder() const;

    void toggleMark(int row);

    /// 1줄 배치에서 이름이 잘리는 행의 비율(0 ~ 1). 자동 규칙: 폭 < autoWidth 또는 이 비율 ≥ autoPercent면 2줄,
    /// 1줄로 되돌릴 때는 80 px 여유(폭 ≥ autoWidth + 80 그리고 80 px 좁은 폭에서도 비율 미만).
    /// 비율은 보이는 행이 아니라 전체 행(최대 400)으로 계산한다 — 스크롤만으로 배치가 바뀌지 않게.
    double truncatedNameRatio(int width) const;

    /// 미리보기 모드의 높이 — 머리글 + 행 수 × 레코드 높이.
    int preferredHeight(int rows) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void viewModeChanged(fm::filelist::ViewMode mode);
    void twoLineChanged(bool twoLine);
    void cursorRowChanged(int row);
    /// Enter · 두 번 클릭.
    void activated(const QModelIndex &index);
    /// Backspace — 상위 폴더로.
    void upRequested();
    /// 클릭 · 포커스로 이 목록의 패널이 활성이 되려 한다.
    void paneActivated();
    void sortChanged(int column, Qt::SortOrder order);
    /// 표시가 바뀌었다(상태 줄 갱신용).
    void marksChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class FileListViewPrivate;
    std::unique_ptr<FileListViewPrivate> d;
};

} // namespace fm::filelist
