#pragma once
#include <QTableWidget>
#include <QHeaderView>
#include <QResizeEvent>
#include <fmstyle/ThemeManager.h>

namespace fm::ui {
// Presentation only: the product owns planning, validation, execution and undo.
class RenameComparisonView : public QTableWidget {
public:
    struct Row { QString original_, proposed_, status_; bool error_ = false; QString size_, date_; };
    explicit RenameComparisonView(QWidget* Parent = nullptr) : QTableWidget(Parent) {
        setColumnCount(5); setHorizontalHeaderLabels({tr("원래 이름"), tr("새 이름"), tr("크기"), tr("수정 날짜"), tr("상태")});
        setEditTriggers(NoEditTriggers); setSelectionBehavior(SelectRows); setWordWrap(false);
        horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        connect(&style::ThemeManager::instance(), &style::ThemeManager::changed, this, [this] { rebuild(); });
    }
    void SetRows(const QList<Row>& Rows) { rows_ = Rows; rebuild(); }
    void SetMode(int Mode) { mode_ = Mode; rebuild(); }
protected:
    void resizeEvent(QResizeEvent* Event) override { QTableWidget::resizeEvent(Event); if (mode_ == 2) rebuild(); }
private:
    void rebuild() {
        const bool Paired = mode_ == 1 || (mode_ == 2 && width() < 1000);
        setColumnHidden(1, Paired); setHorizontalHeaderItem(0, new QTableWidgetItem(Paired ? tr("원래 이름 → 새 이름") : tr("원래 이름")));
        setRowCount(rows_.size());
        const auto& Colors = style::ThemeManager::instance().colors();
        for (int I = 0; I < rows_.size(); ++I) {
            const auto& R = rows_[I];
            const QStringList Texts{Paired ? R.original_ + "\n  ↳ " + R.proposed_ : R.original_, R.proposed_, R.size_, R.date_, R.status_};
            for (int C = 0; C < 5; ++C) {
                auto* Item = new QTableWidgetItem(Texts[C]); Item->setToolTip(Texts[C]);
                if (R.error_) { Item->setBackground(Colors[style::Token::DangerBg]); Item->setForeground(Colors[style::Token::Danger]); }
                setItem(I, C, Item);
            }
            setRowHeight(I, fontMetrics().height() * (Paired ? 2 : 1) + 14);
        }
    }
    QList<Row> rows_;
    int mode_ = 2;
};
}
