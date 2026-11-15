#include "fmwidgets/ScrollPanel.h"
#include <QSizePolicy>

namespace fm::ui {
ScrollPanel::ScrollPanel(QWidget* Parent) : QScrollArea(Parent)
{
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setBackgroundRole(QPalette::Window);
    viewport()->setBackgroundRole(QPalette::Window);
}
void ScrollPanel::setContentWidget(QWidget* Content)
{
    if (Content) {
        Content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        Content->setBackgroundRole(QPalette::Window);
    }
    setWidget(Content);
}
QSize ScrollPanel::minimumSizeHint() const { return {0, 0}; }
QSize ScrollPanel::sizeHint() const { return {480, 320}; }
}