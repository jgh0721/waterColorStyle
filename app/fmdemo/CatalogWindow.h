#pragma once

#include <QPointer>
#include <QWidget>

class QDialog;
class QModelIndex;
class QStandardItemModel;
class QSortFilterProxyModel;
class QTreeView;

namespace fm::ui {
class Button;
class Label;
class SearchField;
class SegmentedControl;
class Switch;
}

namespace fm::app {

/// 대화상자 카탈로그(PLAN §9) — 묶음 → 대화상자 변형 트리에서 골라 목업 보드의 상태로 연다.
/// 오른쪽의 디자인 · 색 구성표 · 다크 색조 스위치는 앱 전체(열린 대화상자 포함)에 바로 적용된다.
class CatalogWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CatalogWindow(QWidget *parent = nullptr);
    ~CatalogWindow() override;

    /// 변형을 연다(모덜리스, 닫으면 지워진다). 모르는 ID면 nullptr.
    QDialog *openVariant(const QString &id);
    void closeAll();
    int openCount() const;

private:
    void buildModel();
    void showDetails(const QModelIndex &index);
    QString currentId() const;
    void syncThemeControls();

    QStandardItemModel *m_model = nullptr;
    QSortFilterProxyModel *m_filter = nullptr;
    QTreeView *m_tree = nullptr;
    fm::ui::SearchField *m_search = nullptr;
    fm::ui::Label *m_title = nullptr;
    fm::ui::Label *m_id = nullptr;
    fm::ui::Label *m_meta = nullptr;
    fm::ui::Button *m_open = nullptr;
    fm::ui::Button *m_closeAll = nullptr;
    fm::ui::Label *m_openCount = nullptr;
    fm::ui::Switch *m_watercolor = nullptr;
    fm::ui::SegmentedControl *m_scheme = nullptr;
    fm::ui::SegmentedControl *m_tone = nullptr;
    QList<QPointer<QDialog>> m_dialogs;
    bool m_syncing = false;
};

} // namespace fm::app
