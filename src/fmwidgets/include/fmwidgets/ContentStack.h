#pragma once
#include <QStackedWidget>

namespace fm::ui {
// Hidden pages must not impose their form size on a scrollable current page.
class ContentStack : public QStackedWidget
{
    Q_OBJECT
public:
    explicit ContentStack(QWidget* Parent = nullptr);
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int Width) const override;
protected:
    bool event(QEvent* Event) override;
};
}
