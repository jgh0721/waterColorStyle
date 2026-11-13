#include "fmwidgets/WindowFrame.h"
#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QWidget>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <windowsx.h>
#endif

namespace fm::ui {
namespace {
// offscreen and minimal platforms have no real window behind winId().
bool nativeWindows() { return QGuiApplication::platformName() == QLatin1String("windows"); }
}

WindowFrame::WindowFrame(QWidget* Window, WindowFrameOptions Options)
    : QObject(Window), window_(Window), options_(Options)
{
    if (Window->testAttribute(Qt::WA_WState_Created)) {
        // Changing flags now would recreate the native window. A frameless one keeps Qt's full-window client area.
        inner_ = Window->windowFlags().testFlag(Qt::FramelessWindowHint);
    } else if (nativeWindows()) {
        Window->setWindowFlag(Qt::FramelessWindowHint, false);
    } else {
        // Headless platforms have no native frame to keep; stay frameless as the title bar always was.
        Window->setWindowFlag(Qt::FramelessWindowHint);
        inner_ = true;
    }
    Window->installEventFilter(this);
    qApp->installNativeEventFilter(this);
    applyFrame();
}

WindowFrame::~WindowFrame()
{
    if (qApp) qApp->removeNativeEventFilter(this);
}

int WindowFrame::resizeBorderWidth(const QWidget* Window)
{
#ifdef Q_OS_WIN
    UINT Dpi = Window && Window->internalWinId() && nativeWindows() ? GetDpiForWindow(reinterpret_cast<HWND>(Window->internalWinId())) : 0;
    if (!Dpi) Dpi = UINT(qRound(96 * (Window ? Window->devicePixelRatioF() : 1.0)));
    return GetSystemMetricsForDpi(SM_CXSIZEFRAME, Dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, Dpi);
#else
    Q_UNUSED(Window);
    return 0;
#endif
}

bool WindowFrame::resizable() const
{
    return window_ && !window_->windowFlags().testFlag(Qt::MSWindowsFixedSizeDialogHint)
        && window_->minimumSize() != window_->maximumSize();
}

bool WindowFrame::eventFilter(QObject* Object, QEvent* Event)
{
    // Qt sends WinIdChange from QWidget::create(), before native children exist and before the first show.
    if (Object == window_ && (Event->type() == QEvent::WinIdChange || Event->type() == QEvent::Show)) applyFrame();
    return QObject::eventFilter(Object, Event);
}

void WindowFrame::applyFrame()
{
#ifdef Q_OS_WIN
    if (!window_ || !nativeWindows() || !window_->internalWinId() || window_->isFullScreen()) return;
    const auto Handle = reinterpret_cast<HWND>(window_->internalWinId());
    const auto Style = GetWindowLongPtrW(Handle, GWL_STYLE);
    // The thick frame gives resize borders, Snap and the DWM shadow; the system menu and boxes back Alt+Space,
    // Win+arrow keys and the taskbar menu.
    auto Wanted = (Style & ~LONG_PTR(WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) | WS_THICKFRAME | WS_SYSMENU
        | (options_.minimizeBox ? WS_MINIMIZEBOX : 0) | (options_.maximizeBox ? WS_MAXIMIZEBOX : 0);
    if (!inner_) Wanted |= WS_CAPTION;
    const bool First = framedHandle_ != quintptr(Handle);
    if (Wanted == Style && !First) return;
    RECT Client{};
    GetClientRect(Handle, &Client);
    MapWindowPoints(Handle, HWND_DESKTOP, reinterpret_cast<POINT*>(&Client), 2);
    const bool Normal = !IsZoomed(Handle) && !IsIconic(Handle);
    // WM_NCCALCSIZE turns the caption into client area from here on.
    framedHandle_ = quintptr(Handle);
    SetWindowLongPtrW(Handle, GWL_STYLE, Wanted);
    SetWindowPos(Handle, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (!First || !Normal || inner_) return;
    // Regrow the window around the client rect Qt placed, so a given or restored geometry does not move up by the
    // caption height. Qt reads its frame margins back from the window and client rects (calculateFullFrameMargins).
    RECT Outer{}, Inner{};
    GetWindowRect(Handle, &Outer);
    GetClientRect(Handle, &Inner);
    MapWindowPoints(Handle, HWND_DESKTOP, reinterpret_cast<POINT*>(&Inner), 2);
    const RECT Target{Client.left - (Inner.left - Outer.left), Client.top - (Inner.top - Outer.top),
                      Client.right + (Outer.right - Inner.right), Client.bottom + (Outer.bottom - Inner.bottom)};
    if (!EqualRect(&Target, &Outer))
        SetWindowPos(Handle, nullptr, Target.left, Target.top, Target.right - Target.left, Target.bottom - Target.top,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
#endif
}

#ifdef Q_OS_WIN
long WindowFrame::hitTest(void* RootHandle, long X, long Y) const
{
    const auto Root = static_cast<HWND>(RootHandle);
    RECT Inner{};
    GetClientRect(Root, &Inner);
    MapWindowPoints(Root, HWND_DESKTOP, reinterpret_cast<POINT*>(&Inner), 2);
    const bool Sizing = resizable() && (GetWindowLongPtrW(Root, GWL_STYLE) & WS_THICKFRAME) && !IsZoomed(Root)
        && !IsIconic(Root) && !window_->isFullScreen();
    if (Sizing) {
        // Left, right and bottom borders are non-client; the top one is a band inside the client area.
        // A frameless handle (inner_) keeps every band inside its client area.
        const int Band = resizeBorderWidth(window_), Side = inner_ ? Band : 0;
        const bool Left = X < Inner.left + Side, Right = X >= Inner.right - Side;
        const bool Top = Y < Inner.top + Band, Bottom = Y >= Inner.bottom - Side;
        if (Left || Right || Top || Bottom) {
            // Corners reach one band further along each edge, like the native frame's.
            const bool NearLeft = X < Inner.left + Side + Band, NearRight = X >= Inner.right - Side - Band;
            const bool NearTop = Y < Inner.top + 2 * Band, NearBottom = Y >= Inner.bottom - Side - Band;
            if ((Top && NearLeft) || (Left && NearTop)) return HTTOPLEFT;
            if ((Top && NearRight) || (Right && NearTop)) return HTTOPRIGHT;
            if ((Bottom && NearLeft) || (Left && NearBottom)) return HTBOTTOMLEFT;
            if ((Bottom && NearRight) || (Right && NearBottom)) return HTBOTTOMRIGHT;
            return Left ? HTLEFT : Right ? HTRIGHT : Top ? HTTOP : HTBOTTOM;
        }
    }
    // The frame of a fixed-size or maximized window.
    if (!PtInRect(&Inner, POINT{X, Y})) return HTBORDER;
    if (captionHitTest_) {
        // Native client coordinates are physical pixels; Qt's scale also covers mixed-DPI monitors.
        const qreal Scale = window_->devicePixelRatioF();
        if (const int Code = captionHitTest_(QPoint(qRound((X - Inner.left) / Scale), qRound((Y - Inner.top) / Scale))))
            return Code;
    }
    return HTCLIENT;
}
#endif

bool WindowFrame::nativeEventFilter(const QByteArray&, void* Message, qintptr* Result)
{
#ifdef Q_OS_WIN
    const auto* Msg = static_cast<MSG*>(Message);
    if (Msg->message != WM_NCHITTEST && Msg->message != WM_NCCALCSIZE && Msg->message != WM_NCLBUTTONDOWN
        && Msg->message != WM_NCLBUTTONUP)
        return false;
    if (!window_ || !framedHandle_ || framedHandle_ != quintptr(window_->internalWinId())) return false;
    const auto Root = reinterpret_cast<HWND>(framedHandle_);
    if (Msg->hwnd != Root) {
        // Windows asks the deepest child window under the cursor. A same-thread child over the top band or a caption
        // button answers HTTRANSPARENT, so Windows asks the windows beneath it and finally this window.
        if (Msg->message != WM_NCHITTEST || GetAncestor(Msg->hwnd, GA_ROOT) != Root
            || hitTest(Root, GET_X_LPARAM(Msg->lParam), GET_Y_LPARAM(Msg->lParam)) == HTCLIENT)
            return false;
        if (Result) *Result = HTTRANSPARENT;
        return true;
    }
    switch (Msg->message) {
    case WM_NCCALCSIZE: {
        if (inner_) return false;
        auto* Proposed = Msg->wParam ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(Msg->lParam)->rgrc[0] : reinterpret_cast<RECT*>(Msg->lParam);
        const RECT Outer = *Proposed;
        const LRESULT Default = DefWindowProcW(Root, WM_NCCALCSIZE, Msg->wParam, Msg->lParam);
        // Keep the left, right and bottom borders; the caption becomes client area. A maximized window hangs its
        // frame off the monitor on every side, so its top keeps a border as wide as the bottom one.
        Proposed->top = Outer.top + (IsZoomed(Root) ? Outer.bottom - Proposed->bottom : 0);
        if (Result) *Result = Default;
        return true;
    }
    case WM_NCHITTEST:
        if (Result) *Result = hitTest(Root, GET_X_LPARAM(Msg->lParam), GET_Y_LPARAM(Msg->lParam));
        return true;
    // The dispatcher passes no result pointer for queued mouse messages; synchronous calls supply one.
    // DefWindowProc would track and draw a classic caption button.
    case WM_NCLBUTTONDOWN:
        if (Msg->wParam != HTMAXBUTTON) return false;
        if (Result) *Result = 0;
        return true;
    case WM_NCLBUTTONUP:
        if (Msg->wParam != HTMAXBUTTON) return false;
        if (maximizeButton_) maximizeButton_();
        if (Result) *Result = 0;
        return true;
    default:
        break;
    }
#else
    Q_UNUSED(Message);
    Q_UNUSED(Result);
#endif
    return false;
}

}
