#pragma once
#include "RecordPainting.h"
#include "ListPresentation.h"
#include <fmstyle/ThemeManager.h>
#include <QPainter>
#include <QWidget>

namespace fm::filelist {
// A data-only preview: the host supplies labels, state and draft colors.
// It shares geometry and state painting with real product file records.
class RecordPreview final : public QWidget {
public:
    struct Row { QString name_, metadata_; bool marked_ = false, cursor_ = false; };
    explicit RecordPreview(QWidget* Parent = nullptr) : QWidget(Parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, [this] { update(); });
    }
    void SetRows(QList<Row> Rows) { rows_ = std::move(Rows); updateGeometry(); update(); }
    void SetAppearance(const ListAppearance& Appearance, bool TwoLine) { appearance_ = Appearance; twoLine_ = TwoLine; updateGeometry(); update(); }
    void SetColors(const fm::style::ThemeColors& Colors) { colors_ = Colors; update(); }
    QSize sizeHint() const override { return {400, geometry().pitch * int(rows_.size()) + fontMetrics().height() + 18}; }
    QSize minimumSizeHint() const override { return {180, sizeHint().height()}; }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter Painter(this); const auto Colors = colors_.value_or(fm::style::ThemeManager::instance().colors());
        Painter.fillRect(rect(), Colors[fm::style::Token::Surface]); Painter.setPen(Colors[fm::style::Token::Fg2]);
        const int Head = fontMetrics().height() + 12;
        Painter.drawText(QRect(10, 0, width() - 20, Head), Qt::AlignVCenter, tr("이름 · 파일 정보 · 행 상태"));
        const auto G = geometry(); int Y = Head;
        for (int I = 0; I < rows_.size(); ++I) {
            const auto& RowValue = rows_[I]; const QRect Record(0, Y, width(), G.pitch);
            const detail::RecordState State{true, RowValue.marked_, RowValue.cursor_, false};
            detail::paintRecordBackground(&Painter, Record, G, appearance_, State, I % 2, Colors);
            const auto Text = detail::textColors(Colors, appearance_, State, twoLine_, G.separator);
            auto NameRect = G.nameLine(Record).toRect().adjusted(10, 0, -10, 0);
            if (!twoLine_) NameRect.setWidth(qMax(0, width() * 3 / 5 - 20));
            Painter.setPen(Text.name); Painter.drawText(NameRect, Qt::AlignVCenter, ElideFileName(RowValue.name_, fontMetrics(), NameRect.width(), appearance_.nameElide));
            auto Meta = twoLine_ ? G.metaLine(Record).toRect().adjusted(10, 0, -10, 0) : QRect(width() * 3 / 5, Y, width() * 2 / 5 - 10, G.pitch);
            Painter.setPen(Text.meta); Painter.drawText(Meta, Qt::AlignVCenter, fontMetrics().elidedText(RowValue.metadata_, Qt::ElideRight, Meta.width()));
            detail::paintRecordCursor(&Painter, Record, G, appearance_, State, Colors); Y += G.pitch;
        }
        Painter.setPen(Colors[fm::style::Token::Line]); Painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
private:
    detail::RecordGeometry geometry() const { auto G = detail::RecordGeometry::make(true, twoLine_, appearance_);
        const int Extra = qMax(0, fontMetrics().height() + 4 - G.nameHeight); G.nameHeight += Extra; G.pitch += twoLine_ ? Extra * 2 : Extra; return G; }
    QList<Row> rows_; ListAppearance appearance_; bool twoLine_ = false; std::optional<fm::style::ThemeColors> colors_;
};
}
