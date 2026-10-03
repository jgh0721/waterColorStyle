#pragma once

// 다중 이름 변경 미리보기(02 §9.3) — 모델은 Qt 표 모델, 뷰는 파일 목록과 같은 Qtitan 밴드 테이블 뷰.
// 1줄: # · 원래 이름 · → · 새 이름 · 크기 · 촬영 날짜 · 상태 / 2줄: 원래 이름 아래 줄에 새 이름이 크기 · 날짜 폭까지 걸친다.
// QtitanDataGrid 없이 빌드하면(FM_WITH_QTITAN 0) 뷰는 안내 문구만 보인다(모델 · 2줄 판정 · 높이는 같다).

#include "fmdialogs/RenameEngine.h"

#include <fmfilelist/ListAppearance.h>

#include <QAbstractTableModel>
#include <QWidget>

#include <memory>

namespace fm::dialogs {

class RenamePreviewModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { IndexColumn, OriginalColumn, ArrowColumn, NewNameColumn, SizeColumn, DateColumn, StatusColumn, ColumnCount };
    enum Role {
        StateRole = Qt::UserRole + 1,  // RenamePreviewRow::State
        IsLongRole,
        NewNameRole,
    };

    explicit RenamePreviewModel(QObject *parent = nullptr);

    /// 행 수가 같으면 dataChanged(스크롤 위치 유지), 다르면 초기화.
    void setRows(const QList<RenamePreviewRow> &rows);
    const QList<RenamePreviewRow> &rows() const noexcept { return m_rows; }

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QList<RenamePreviewRow> m_rows;
};

class RenamePreviewViewPrivate;

class RenamePreviewView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(fm::filelist::ViewMode viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(bool twoLine READ isTwoLine NOTIFY twoLineChanged)

public:
    explicit RenamePreviewView(QWidget *parent = nullptr);
    ~RenamePreviewView() override;

    void setModel(RenamePreviewModel *model);
    RenamePreviewModel *model() const;

    /// OneLine · TwoLine · Auto(긴 이름이 하나라도 있으면 2줄 — 02 §9.3.4).
    fm::filelist::ViewMode viewMode() const noexcept;
    void setViewMode(fm::filelist::ViewMode mode);
    bool isTwoLine() const noexcept;

    /// 머리글 + rows × 레코드 높이.
    int preferredHeight(int rows) const;

    QSize sizeHint() const override;

Q_SIGNALS:
    void viewModeChanged(fm::filelist::ViewMode mode);
    void twoLineChanged(bool twoLine);

protected:
    void changeEvent(QEvent *event) override;

private:
    friend class RenamePreviewViewPrivate;
    std::unique_ptr<RenamePreviewViewPrivate> d;
};

} // namespace fm::dialogs
