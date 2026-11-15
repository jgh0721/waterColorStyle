#pragma once
#include <QScrollArea>

namespace fm::ui {
// Frameless, wrapping detail/form surface. Large content never increases the
// enclosing panel's minimum size; scrollbars use the installed fm style.
class ScrollPanel : public QScrollArea
{
    Q_OBJECT
public:
    explicit ScrollPanel(QWidget* Parent = nullptr);
    void setContentWidget(QWidget* Content);
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
};
}