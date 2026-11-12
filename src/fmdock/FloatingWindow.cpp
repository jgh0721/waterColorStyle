#include "DockInternal_p.h"

#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCursor>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <dwmapi.h>
#include <windowsx.h>
#endif

#include <algorithm>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace fm::dock {

// =============================================================================================
// FloatingWindow

FloatingWindow::FloatingWindow(DockManager *manager)
    : QWidget(manager->window(), Qt::Window)
    , m_manager(manager)
{
    setObjectName(u"fmFloatingWindow"_s);
    if (useCustomFrame())
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(0);
    m_titleBar = new detail::FloatingTitleBar(this);
    m_container = new DockContainer(manager, this, this);
    m_layout->addWidget(m_titleBar);
    m_layout->addWidget(m_container, 1);
    updateFrame();
    manager->shareShortcuts(this);
}

FloatingWindow::~FloatingWindow() = default;

QList<DockWidget *> FloatingWindow::dockWidgets() const
{
    return m_container->dockWidgets();
}

void FloatingWindow::setFrameGeometryHint(const QRect &rect)
{
    setGeometry(rect);
}

bool FloatingWindow::useCustomFrame() const
{
    return m_manager && !m_manager->testConfigFlag(DockManager::FloatingNativeFrame);
}

void FloatingWindow::updateFrame()
{
    const bool custom = useCustomFrame();
    const int frame = custom && !isMaximized() ? detail::metricsOf(this).floatingFrame : 0;
    m_layout->setContentsMargins(frame, frame, frame, frame);
    m_titleBar->setVisible(custom);
    m_titleBar->updateMaximized();
    update();
}

void FloatingWindow::updateTitle()
{
    DockWidget *shown = m_container->topLevelDockWidget();
    if (!shown) {
        DockWidget *focused = m_manager ? m_manager->focusedDockWidget() : nullptr;
        if (focused && focused->floatingWindow() == this)
            shown = focused;
    }
    if (!shown) {
        const QList<DockWidget *> open = m_container->openedDockWidgets();
        shown = open.isEmpty() ? nullptr : open.first();
    }
    const QString title = shown ? shown->title() : QString();
    const QIcon icon = shown ? shown->icon() : QIcon();
    m_titleBar->setTitle(title, icon);
    setWindowTitle(title);
    if (!icon.isNull())
        setWindowIcon(icon);
}

void FloatingWindow::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    m_titleBar->setActive(active);
    update();
}

void FloatingWindow::containerChanged()
{
    if (!m_manager)
        return;
    if (m_container->dockWidgets().isEmpty()) {
        // 만드는 중(영역을 먼저 넣고 도크를 나중에 넣는다)에도 비어 보인다 — 한 바퀴 뒤에 다시 보고 지운다
        hide();
        QTimer::singleShot(0, this, [this] {
            if (m_manager && m_container->dockWidgets().isEmpty())
                m_manager->floatingWindowEmptied(this);
        });
        return;
    }
    if (m_container->openedDockWidgets().isEmpty()) {
        hide();
        return;
    }
    updateTitle();
    if (!isVisible() && !m_manager->isRestoringState())
        show();
}

void FloatingWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const fs::ThemeColors &tc = detail::colorsOf(this);
    p.fillRect(rect(), tc[fs::Token::Win]);
    if (useCustomFrame() && !isMaximized())
        fs::paintFloatingFrame(&p, rect(), m_active, tc);
}

void FloatingWindow::closeEvent(QCloseEvent *event)
{
    // 창 닫기 = 안의 도크를 닫는다. 닫을 수 없는 도크가 남으면 창만 숨긴다.
    event->ignore();
    const QList<DockWidget *> open = m_container->openedDockWidgets();
    QList<QPointer<DockWidget>> docks(open.cbegin(), open.cend());
    for (const QPointer<DockWidget> &d : docks) {
        if (d && d->hasFeature(DockWidget::Closable))
            d->requestClose();
    }
    if (!m_container->openedDockWidgets().isEmpty())
        hide();
}

void FloatingWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::WindowStateChange:
    case QEvent::StyleChange:
        updateFrame();
        break;
    case QEvent::ActivationChange:
        setActive(isActiveWindow());
        break;
    default:
        break;
    }
}

bool FloatingWindow::event(QEvent *event)
{
#ifdef Q_OS_WIN
    if (event->type() == QEvent::Show && !m_nativeSetupDone && useCustomFrame() && !detail::isHeadlessPlatform()) {
        // 프레임 없는 창에 크기 조절 · 캡션 스타일을 더한다 — Aero 스냅 · 최대화 · DWM 그림자는 OS가 한다
        m_nativeSetupDone = true;
        HWND hwnd = reinterpret_cast<HWND>(winId());
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        style |= WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX;
        SetWindowLongPtrW(hwnd, GWL_STYLE, style);
        const MARGINS margins{1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
#endif
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate)
        setActive(event->type() == QEvent::WindowActivate);
    return QWidget::event(event);
}

bool FloatingWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" && !detail::isHeadlessPlatform()) {
        MSG *msg = static_cast<MSG *>(message);
        HWND hwnd = msg->hwnd;
        switch (msg->message) {
        case WM_NCCALCSIZE:
            if (useCustomFrame() && msg->wParam == TRUE) {
                if (IsZoomed(hwnd)) {
                    // 최대화하면 창이 프레임 두께만큼 화면 밖으로 나간다 — 그만큼 안쪽을 클라이언트로
                    auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
                    const UINT dpi = GetDpiForWindow(hwnd);
                    const int fx = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                    const int fy = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                    params->rgrc[0].left += fx;
                    params->rgrc[0].top += fy;
                    params->rgrc[0].right -= fx;
                    params->rgrc[0].bottom -= fy;
                }
                *result = 0;
                return true;
            }
            break;
        case WM_NCHITTEST:
            if (useCustomFrame()) {
                RECT wr;
                GetWindowRect(hwnd, &wr);
                const int x = GET_X_LPARAM(msg->lParam) - wr.left;
                const int y = GET_Y_LPARAM(msg->lParam) - wr.top;
                const int w = wr.right - wr.left;
                const int h = wr.bottom - wr.top;
                const qreal dpr = devicePixelRatioF();
                const int border = qRound(6 * dpr);
                if (!IsZoomed(hwnd)) {
                    const bool left = x < border;
                    const bool right = x >= w - border;
                    const bool top = y < border;
                    const bool bottom = y >= h - border;
                    LRESULT hit = 0;
                    if (top && left)
                        hit = HTTOPLEFT;
                    else if (top && right)
                        hit = HTTOPRIGHT;
                    else if (bottom && left)
                        hit = HTBOTTOMLEFT;
                    else if (bottom && right)
                        hit = HTBOTTOMRIGHT;
                    else if (left)
                        hit = HTLEFT;
                    else if (right)
                        hit = HTRIGHT;
                    else if (top)
                        hit = HTTOP;
                    else if (bottom)
                        hit = HTBOTTOM;
                    if (hit) {
                        *result = hit;
                        return true;
                    }
                }
                const QPoint local(qRound(x / dpr), qRound(y / dpr));
                if (m_titleBar->isVisible() && m_titleBar->geometry().contains(local)
                    && m_titleBar->isCaptionArea(local - m_titleBar->pos())) {
                    *result = HTCAPTION;
                    return true;
                }
                *result = HTCLIENT;
                return true;
            }
            break;
        case WM_ENTERSIZEMOVE:
            m_moveStart = pos();
            m_sizing = false;
            break;
        case WM_SIZING:
            m_sizing = true;
            break;
        case WM_MOVING:
            if (!m_sizing && m_manager) {
                // OS가 창을 옮기는 동안 — 메인 창 · 다른 떠 있는 창에 붙일 자리를 보인다
                if (!m_moveSession && !detail::ManagerAccess::drag(m_manager)) {
                    const QList<DockWidget *> docks = m_container->dockWidgets();
                    const bool movable = !docks.isEmpty() && std::all_of(docks.cbegin(), docks.cend(), [](DockWidget *d) {
                        return d->hasFeature(DockWidget::Movable);
                    });
                    if (movable) {
                        m_moveSession = new detail::DragSession(m_manager, docks, nullptr, this,
                                                                detail::DragSession::Mode::MoveWindow, QPoint(), size());
                        m_manager->m_drag = m_moveSession;
                    }
                }
                if (m_moveSession)
                    m_moveSession->move(QCursor::pos());
            }
            break;
        case WM_EXITSIZEMOVE:
            if (m_moveSession) {
                const QPointer<detail::DragSession> session = m_moveSession;
                m_moveSession = nullptr;
                if (pos() == m_moveStart)
                    session->cancel();
                else
                    session->drop(QCursor::pos());
            }
            m_sizing = false;
            break;
        default:
            break;
        }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

void FloatingWindow::startOwnMove(const QPoint &globalPos)
{
    // OS 이동을 쓸 수 없는 곳(화면 없는 플랫폼 · 다른 OS) — 창을 직접 옮기며 같은 표시를 보인다
    if (m_moveSession || !m_manager || detail::ManagerAccess::drag(m_manager))
        return;
    const QList<DockWidget *> docks = m_container->dockWidgets();
    m_moveSession = new detail::DragSession(m_manager, docks, nullptr, this, detail::DragSession::Mode::MoveWindow,
                                            globalPos - pos(), size());
    m_manager->m_drag = m_moveSession;
}

namespace detail {

// =============================================================================================
// CaptionButtonWidget

CaptionButtonWidget::CaptionButtonWidget(fs::CaptionButton kind, QWidget *parent)
    : QAbstractButton(parent)
    , m_kind(kind)
{
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
    setKind(kind);
}

void CaptionButtonWidget::setKind(fs::CaptionButton kind)
{
    m_kind = kind;
    const QString tip = kind == fs::CaptionButton::Close      ? QCoreApplication::translate("fm::dock", "닫기")
                      : kind == fs::CaptionButton::Maximize   ? QCoreApplication::translate("fm::dock", "최대화")
                                                              : QCoreApplication::translate("fm::dock", "이전 크기로");
    setToolTip(tip);
    setAccessibleName(tip);
    update();
}

void CaptionButtonWidget::setActiveCaption(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    update();
}

QSize CaptionButtonWidget::sizeHint() const
{
    return metricsOf(this).floatingButton;
}

void CaptionButtonWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    fs::paintCaptionButton(&p, rect(), m_kind, underMouse() && !isDown(), isDown(), m_active, colorsOf(this));
}

bool CaptionButtonWidget::event(QEvent *event)
{
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave)
        update();
    else if (event->type() == QEvent::StyleChange)
        updateGeometry();
    return QAbstractButton::event(event);
}

// =============================================================================================
// FloatingTitleBar

FloatingTitleBar::FloatingTitleBar(FloatingWindow *window)
    : QWidget(window)
    , m_window(window)
{
    setObjectName(u"fmFloatingTitleBar"_s);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_maximize = new CaptionButtonWidget(fs::CaptionButton::Maximize, this);
    m_close = new CaptionButtonWidget(fs::CaptionButton::Close, this);
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->addStretch(1);
    row->addWidget(m_maximize, 0, Qt::AlignVCenter);
    row->addWidget(m_close, 0, Qt::AlignVCenter);
    connect(m_maximize, &QAbstractButton::clicked, this, [this] {
        if (m_window->isMaximized())
            m_window->showNormal();
        else
            m_window->showMaximized();
    });
    connect(m_close, &QAbstractButton::clicked, m_window, &QWidget::close);
    changeEvent(nullptr);
}

void FloatingTitleBar::setTitle(const QString &title, const QIcon &icon)
{
    m_title = title;
    m_icon = icon;
    update();
}

void FloatingTitleBar::setActive(bool active)
{
    m_active = active;
    m_maximize->setActiveCaption(active);
    m_close->setActiveCaption(active);
    update();
}

void FloatingTitleBar::updateMaximized()
{
    m_maximize->setKind(m_window->isMaximized() ? fs::CaptionButton::Restore : fs::CaptionButton::Maximize);
}

bool FloatingTitleBar::isCaptionArea(const QPoint &pos) const
{
    return rect().contains(pos) && !m_maximize->geometry().contains(pos) && !m_close->geometry().contains(pos);
}

QSize FloatingTitleBar::sizeHint() const
{
    return {200, metricsOf(this).floatingTitle};
}

void FloatingTitleBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const fs::ThemeColors &tc = colorsOf(this);
    fs::paintFloatingTitle(&p, rect(), m_active, tc);
    int x = 10;
    if (!m_icon.isNull()) {
        m_icon.paint(&p, QRect(x, (height() - 16) / 2, 16, 16));
        x += 22;
    }
    const int right = std::min(m_maximize->isVisible() ? m_maximize->x() : width(), m_close->x()) - 8;
    fs::paintDockTitleText(&p, QRect(x, 0, std::max(0, right - x), height()), m_title, fs::DockTitleKind::Floating, m_active,
                           tc);
}

void FloatingTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        m_pressGlobal = event->globalPosition().toPoint();
        m_window->activateWindow();
    }
    event->accept();
}

void FloatingTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint global = event->globalPosition().toPoint();
    if (m_window->m_moveSession) {
        m_window->move(global - m_window->m_moveSession->property("offset").toPoint());
        m_window->m_moveSession->move(global);
        event->accept();
        return;
    }
    if (!m_pressed || m_window->isMaximized()
        || (global - m_pressGlobal).manhattanLength() < QApplication::startDragDistance())
        return;
    m_pressed = false;
    m_window->startOwnMove(m_pressGlobal);
    if (m_window->m_moveSession) {
        m_window->m_moveSession->setProperty("offset", m_pressGlobal - m_window->pos());
        m_window->move(global - (m_pressGlobal - m_window->pos()));
        m_window->m_moveSession->move(global);
    }
    event->accept();
}

void FloatingTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_pressed = false;
    if (const QPointer<DragSession> session = m_window->m_moveSession) {
        m_window->m_moveSession = nullptr;
        session->drop(event->globalPosition().toPoint());
    }
    event->accept();
}

void FloatingTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_maximize->click();
    event->accept();
}

void FloatingTitleBar::contextMenuEvent(QContextMenuEvent *event)
{
    if (DockWidget *dock = m_window->dockContainer()->topLevelDockWidget())
        execDockMenu(dock, event->globalPos(), this);
    event->accept();
}

void FloatingTitleBar::changeEvent(QEvent *event)
{
    if (event)
        QWidget::changeEvent(event);
    if (!event || event->type() == QEvent::StyleChange) {
        const fs::DockMetrics m = metricsOf(this);
        if (auto *row = qobject_cast<QHBoxLayout *>(layout())) {
            row->setSpacing(m.floatingButtonGap);
            row->setContentsMargins(0, 0, m.floatingButtonGap > 0 ? 3 : 0, 0);
        }
        m_maximize->updateGeometry();
        m_close->updateGeometry();
        updateGeometry();
    }
}

} // namespace detail

} // namespace fm::dock
