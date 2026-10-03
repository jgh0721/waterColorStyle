#pragma once

#include "fmfilelist/ListAppearance.h"

#include <QListView>
#include <QModelIndex>
#include <QWidget>

#include <memory>

class QAbstractItemModel;
class QAbstractScrollArea;
class QStackedWidget;

#if FM_WITH_QTITAN
namespace Qtitan {
class CardGrid;
class GridCardView;
}
#endif

namespace fm::filelist {

/// 섬네일 보기 구현 공통 인터페이스 — 선택은 모델의 MarkedRole, 커서는 구현의 현재 항목.
class ThumbnailBackend
{
public:
    virtual ~ThumbnailBackend() = default;
    virtual QWidget *widget() = 0;
    virtual void setModel(QAbstractItemModel *model) = 0;
    virtual void setAppearance(const ThumbnailAppearance &appearance) = 0;
    virtual int cursorRow() const = 0;
    virtual void setCursorRow(int row) = 0;
    virtual void setPreviewMode(bool preview) = 0;
    /// 스크롤 영역(목록은 자기 자신, 카드는 Qtitan 그리드) — 스크롤 측정(비교 창)에 쓴다.
    virtual QAbstractScrollArea *scrollArea() = 0;
};

/// Qt 목록 구현 — QListView IconMode + 타일 델리게이트. 남는 폭은 열 사이에 고르게 나눈다(space-between).
class ThumbnailListView : public QListView, public ThumbnailBackend
{
    Q_OBJECT
public:
    explicit ThumbnailListView(QWidget *parent = nullptr);

    QWidget *widget() override { return this; }
    void setModel(QAbstractItemModel *model) override;
    void setAppearance(const ThumbnailAppearance &appearance) override;
    const ThumbnailAppearance &appearance() const noexcept { return m_appearance; }
    int cursorRow() const override;
    void setCursorRow(int row) override;
    void setPreviewMode(bool preview) override;
    QAbstractScrollArea *scrollArea() override { return this; }

    /// QListView가 줄바꿈에 쓰는 폭 — 스크롤 막대가 필요할 때 나타나는 방식이면 그 폭을 미리 뺀다.
    int layoutWidth() const;

Q_SIGNALS:
    void cursorRowChanged(int row);
    void rowActivated(const QModelIndex &index);
    void paneActivated();
    void sizeStepRequested(int delta);
    void upRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private:
    void updateGrid();

    ThumbnailAppearance m_appearance;
};

#if FM_WITH_QTITAN
/// Qtitan 카드 구현 — CardGrid::CardView(가로 우선) + 패치 Q7(캡션 · 제목 숨김) + 레코드 그리기(Q3).
/// 카드 폭은 타일 폭, 카드 사이 간격은 고정 4 px(Qtitan 배치 — 남는 폭을 나누지 않는다).
class ThumbnailCardView : public QWidget, public ThumbnailBackend
{
    Q_OBJECT
public:
    explicit ThumbnailCardView(QWidget *parent = nullptr);
    ~ThumbnailCardView() override;

    QWidget *widget() override { return this; }
    void setModel(QAbstractItemModel *model) override;
    void setAppearance(const ThumbnailAppearance &appearance) override;
    const ThumbnailAppearance &appearance() const noexcept { return m_appearance; }
    int cursorRow() const override;
    void setCursorRow(int row) override;
    void setPreviewMode(bool preview) override;
    QAbstractScrollArea *scrollArea() override;

Q_SIGNALS:
    void cursorRowChanged(int row);
    void rowActivated(const QModelIndex &index);
    void paneActivated();
    void sizeStepRequested(int delta);
    void upRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    class TileDelegate;
    class TileRecordPainter;
    void configureColumns();
    void applyAppearance();
    int columnsPerRow() const;

    Qtitan::CardGrid *m_grid = nullptr;
    Qtitan::GridCardView *m_view = nullptr;
    QAbstractItemModel *m_model = nullptr;
    std::unique_ptr<TileRecordPainter> m_recordPainter;
    TileDelegate *m_delegate = nullptr;
    ThumbnailAppearance m_appearance;
};
#endif // FM_WITH_QTITAN

/// 섬네일 정보 줄(27 px): "정렬  이름 ↑ · 폴더 먼저 …  보통 · 96 px · Ctrl+휠".
class ThumbnailInfoBar : public QWidget
{
    Q_OBJECT
public:
    explicit ThumbnailInfoBar(QWidget *parent = nullptr);

    void setSortText(const QString &text);
    void setSizeText(const QString &text);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_sort;
    QString m_size;
};

/// 섬네일 보기 — 두 구현(Qt 목록 · Qtitan 카드)을 감싸고 backend 속성으로 바꾼다. 바꿔도 선택 · 커서가 유지된다.
/// QtitanDataGrid 없이 빌드하면(FM_WITH_QTITAN 0) Qt 목록만 있고 QtitanCards로 바꾸는 요청은 무시한다.
class ThumbnailView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(Backend backend READ backend WRITE setBackend NOTIFY backendChanged)
    Q_PROPERTY(bool paneActive READ isPaneActive WRITE setPaneActive)
    Q_PROPERTY(bool infoBarVisible READ isInfoBarVisible WRITE setInfoBarVisible)
    Q_PROPERTY(int cursorRow READ cursorRow WRITE setCursorRow NOTIFY cursorRowChanged)

public:
    enum Backend { QtList, QtitanCards };
    Q_ENUM(Backend)

    explicit ThumbnailView(QWidget *parent = nullptr);
    ~ThumbnailView() override;

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const noexcept { return m_model; }

    Backend backend() const noexcept { return m_backend; }
    void setBackend(Backend backend);

    const ThumbnailAppearance &appearance() const noexcept { return m_appearance; }
    void setAppearance(const ThumbnailAppearance &appearance);

    bool isPaneActive() const;
    void setPaneActive(bool active);

    bool isInfoBarVisible() const;
    void setInfoBarVisible(bool visible);
    /// 정보 줄의 정렬 문구(예: "이름 ↑ · 폴더 먼저").
    void setSortText(const QString &text);

    void setPreviewMode(bool preview);

    int cursorRow() const;
    void setCursorRow(int row);

    ThumbnailListView *listView() const noexcept { return m_list; }
#if FM_WITH_QTITAN
    ThumbnailCardView *cardView() const noexcept { return m_cards; }
#endif
    /// 지금 구현의 스크롤 영역.
    QAbstractScrollArea *scrollArea() const;

Q_SIGNALS:
    void backendChanged(Backend backend);
    void cursorRowChanged(int row);
    void activated(const QModelIndex &index);
    void upRequested();
    void paneActivated();
    /// Ctrl+휠 — 크기 단계를 바꾸고 싶다(보통 이 보기가 appearance를 바꿔 반영한다).
    void sizeChanged(int size);

private:
    ThumbnailBackend *current() const;
    void stepSize(int delta);
    void updateInfo();

    QAbstractItemModel *m_model = nullptr;
    ThumbnailInfoBar *m_info = nullptr;
    QStackedWidget *m_stack = nullptr;
    ThumbnailListView *m_list = nullptr;
#if FM_WITH_QTITAN
    ThumbnailCardView *m_cards = nullptr;
#endif
    Backend m_backend = QtList;
    ThumbnailAppearance m_appearance;
};

} // namespace fm::filelist
