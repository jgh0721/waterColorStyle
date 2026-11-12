#include "DockInternal_p.h"

#include <QAbstractScrollArea>
#include <QAction>
#include <QEvent>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fm::dock {

DockWidget::DockWidget(const QString &id, const QString &title, QWidget *parent)
    : QFrame(parent)
{
    setObjectName(id);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_toggleViewAction = new QAction(title.isEmpty() ? id : title, this);
    setWindowTitle(title.isEmpty() ? id : title);
    m_toggleViewAction->setCheckable(true);
    m_toggleViewAction->setChecked(true);
    connect(m_toggleViewAction, &QAction::triggered, this, [this](bool on) {
        if (on)
            bringToFront();
        else
            toggleView(false);
    });
}

DockWidget::~DockWidget()
{
    if (m_manager && !m_manager->m_dying)
        m_manager->unregisterDock(this);
    if (m_area && !m_area->m_dying)
        m_area->forgetDockWidget(this);
}

void DockWidget::setWidget(QWidget *content, InsertMode mode)
{
    if (m_content == content)
        return;
    if (QWidget *old = takeWidget())
        old->deleteLater();
    if (!content)
        return;
    const bool scrollable = qobject_cast<QAbstractScrollArea *>(content) != nullptr;
    const bool wrap = mode == InsertMode::ForceScrollArea || (mode == InsertMode::AutoScrollArea && !scrollable);
    m_content = content;
    if (wrap) {
        m_scrollArea = new QScrollArea(this);
        m_scrollArea->setFrameShape(QFrame::NoFrame);
        m_scrollArea->setWidgetResizable(true);
        m_scrollArea->setWidget(content);
        m_layout->addWidget(m_scrollArea);
    } else {
        m_layout->addWidget(content);
    }
    content->show();
    updateGeometry();
}

QWidget *DockWidget::takeWidget()
{
    QWidget *content = m_content;
    if (!content)
        return nullptr;
    if (m_scrollArea) {
        m_scrollArea->takeWidget();
        m_scrollArea->deleteLater();
        m_scrollArea = nullptr;
    } else {
        m_layout->removeWidget(content);
    }
    content->setParent(nullptr);
    m_content = nullptr;
    return content;
}

QString DockWidget::title() const
{
    return windowTitle();
}

void DockWidget::setTitle(const QString &title)
{
    setWindowTitle(title);
}

void DockWidget::setIcon(const QIcon &icon)
{
    m_icon = icon;
    m_toggleViewAction->setIcon(icon);
    if (m_area) {
        if (detail::DockTab *tab = m_area->titleBar()->tabBar()->tabFor(this))
            tab->updateState();
        m_area->titleBar()->tabBar()->relayout();
    }
    Q_EMIT iconChanged();
}

void DockWidget::setFeatures(Features features)
{
    if (m_features == features)
        return;
    m_features = features;
    if (m_area) {
        if (detail::DockTab *tab = m_area->titleBar()->tabBar()->tabFor(this))
            tab->updateState();
        m_area->titleBar()->tabBar()->relayout();
        m_area->updateTitleBar();
    }
    Q_EMIT featuresChanged(features);
}

void DockWidget::setFeature(Feature feature, bool on)
{
    Features f = m_features;
    f.setFlag(feature, on);
    setFeatures(f);
}

DockWidget::Features DockWidget::effectiveFeatures() const
{
    Features f = m_features;
    if (m_manager)
        f &= ~(m_manager->lockedFeatures() & LockableFeatures);
    if (m_central)
        f &= ~(Closable | Movable | Floatable | Pinnable);
    return f;
}

void DockWidget::setDocument(bool on)
{
    if (m_document == on)
        return;
    m_document = on;
    if (m_area) {
        if (detail::DockTab *tab = m_area->titleBar()->tabBar()->tabFor(this))
            tab->updateState();
        m_area->titleBar()->tabBar()->relayout();
    }
}

void DockWidget::setModified(bool on)
{
    if (m_modified == on)
        return;
    m_modified = on;
    if (m_area) {
        if (detail::DockTab *tab = m_area->titleBar()->tabBar()->tabFor(this))
            tab->updateState();
        m_area->titleBar()->tabBar()->relayout();
    }
    Q_EMIT modifiedChanged(on);
}

void DockWidget::setMinimumSizeHintMode(MinimumSizeHintMode mode)
{
    m_minimumSizeHintMode = mode;
    updateGeometry();
}

DockContainer *DockWidget::dockContainer() const
{
    return m_area ? m_area->dockContainer() : nullptr;
}

FloatingWindow *DockWidget::floatingWindow() const
{
    DockContainer *c = dockContainer();
    return c ? c->floatingWindow() : nullptr;
}

bool DockWidget::isFloating() const
{
    return floatingWindow() != nullptr;
}

bool DockWidget::isCurrentTab() const
{
    return m_area && m_area->currentDockWidget() == this;
}

bool DockWidget::isInCentralArea() const
{
    return m_area && m_area->isCentral();
}

QSize DockWidget::minimumSizeHint() const
{
    if (m_minimumSizeHintMode == MinimumSizeHintMode::FromContent && m_content)
        return m_content->minimumSizeHint();
    return {60, 40};
}

void DockWidget::setDockArea(DockArea *area)
{
    m_area = area;
    if (!area)
        return;
    m_lastArea = area;
    m_lastIndex = area->indexOf(this);
    if (DockContainer *c = area->dockContainer(); c && !c->isFloating()) {
        m_dockedArea = area;
        m_dockedIndex = m_lastIndex;
    }
}

void DockWidget::syncToggleAction()
{
    const QSignalBlocker block(m_toggleViewAction);
    m_toggleViewAction->setChecked(!m_closed);
}

void DockWidget::setClosedState(bool closed)
{
    m_closed = closed;
    syncToggleAction();
}

void DockWidget::toggleView(bool open)
{
    if (!open) {
        if (m_closed)
            return;
        setClosedState(true);
        if (isAutoHidden() && m_manager) {
            if (m_manager->expandedAutoHideDockWidget() == this)
                m_manager->collapseAutoHide();
            m_manager->updateSideBars();
        } else if (m_area) {
            hide();
            m_area->dockOpenStateChanged(this);
        }
        if (m_manager)
            m_manager->dockClosedStateChanged(this);
        Q_EMIT viewToggled(false);
        Q_EMIT closed();
        return;
    }
    if (!m_closed && (m_area || isAutoHidden()))
        return;
    setClosedState(false);
    if (isAutoHidden() && m_manager) {
        m_manager->updateSideBars();
    } else if (m_area) {
        m_area->dockOpenStateChanged(this);
        m_area->setCurrentDockWidget(this);
    } else if (m_manager) {
        m_manager->placeClosedDock(this);
    }
    if (m_manager)
        m_manager->dockClosedStateChanged(this);
    Q_EMIT viewToggled(true);
}

void DockWidget::bringToFront()
{
    toggleView(true);
    if (isAutoHidden() && m_manager) {
        m_manager->expandAutoHide(this);
        return;
    }
    if (m_area) {
        m_area->setCurrentDockWidget(this);
        if (FloatingWindow *w = floatingWindow()) {
            w->show();
            w->raise();
        }
    }
}

void DockWidget::setAsCurrentTab()
{
    if (m_area && !m_closed)
        m_area->setCurrentDockWidget(this);
}

void DockWidget::requestClose()
{
    if (!hasFeature(Closable))
        return;
    if (m_features.testFlag(CustomCloseHandling)) {
        Q_EMIT closeRequested();
        return;
    }
    toggleView(false);
    if (m_features.testFlag(DeleteOnClose)) {
        if (m_manager)
            m_manager->removeDockWidget(this);
        deleteLater();
    }
}

void DockWidget::setFloating()
{
    if (!m_manager || !hasFeature(Floatable) || m_manager->testConfigFlag(DockManager::NoTopLevelWindows))
        return;
    if (isFloating() && dockContainer() && dockContainer()->openedDockWidgets().size() == 1)
        return;
    if (m_closed)
        toggleView(true);
    m_manager->floatDocks({this}, m_manager->defaultFloatingGeometry({this}));
}

bool DockWidget::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Show:
    case QEvent::Hide: {
        const bool visible = isVisible();
        if (visible != m_visibleNotified) {
            m_visibleNotified = visible;
            Q_EMIT visibilityChanged(visible);
        }
        break;
    }
    case QEvent::WindowTitleChange:
        if (m_toggleViewAction)
            m_toggleViewAction->setText(windowTitle());
        if (m_area) {
            if (detail::DockTab *tab = m_area->titleBar()->tabBar()->tabFor(this))
                tab->updateState();
            m_area->titleBar()->tabBar()->relayout();
            if (FloatingWindow *w = floatingWindow())
                w->updateTitle();
        }
        if (m_manager)
            m_manager->updateSideBars();
        Q_EMIT titleChanged(windowTitle());
        break;
    default:
        break;
    }
    return QFrame::event(event);
}

} // namespace fm::dock
