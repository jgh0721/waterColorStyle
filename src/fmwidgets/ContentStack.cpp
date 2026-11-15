#include "fmwidgets/ContentStack.h"
#include <QEvent>
#include <QLayout>

namespace fm::ui {
ContentStack::ContentStack(QWidget* Parent) : QStackedWidget(Parent)
{
    layout()->setSizeConstraint(QLayout::SetNoConstraint);
    connect(this, &QStackedWidget::currentChanged, this, [this] { updateGeometry(); });
}
QSize ContentStack::minimumSizeHint() const
{
    return currentWidget() ? currentWidget()->minimumSizeHint().expandedTo(currentWidget()->minimumSize()) : QSize(0, 0);
}
QSize ContentStack::sizeHint() const
{
    return currentWidget() ? currentWidget()->sizeHint().expandedTo(minimumSizeHint()) : QSize(0, 0);
}
bool ContentStack::hasHeightForWidth() const { return currentWidget() && currentWidget()->hasHeightForWidth(); }
int ContentStack::heightForWidth(int Width) const
{
    return hasHeightForWidth() ? currentWidget()->heightForWidth(Width) : sizeHint().height();
}
bool ContentStack::event(QEvent* Event)
{
    const bool Result = QStackedWidget::event(Event);
    if (Event->type() == QEvent::LayoutRequest) updateGeometry();
    return Result;
}
}
