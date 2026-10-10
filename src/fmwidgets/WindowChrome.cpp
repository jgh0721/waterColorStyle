#include "fmwidgets/WindowChrome.h"
#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>
#include <QApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>
#include <QToolButton>
#include <QWindow>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <windowsx.h>
#endif

namespace fm::ui {
WindowChrome::WindowChrome(QWidget* Window, QWidget* Parent)
    : QWidget(Parent), window_(Window)
{
    Window->setWindowFlag(Qt::FramelessWindowHint);
    Window->installEventFilter(this);
    qApp->installNativeEventFilter(this);
    setFixedHeight(28);
    auto* Layout = new QHBoxLayout(this);
    Layout->setContentsMargins(4, 3, 4, 3);
    Layout->setSpacing(2);
    Layout->addStretch();
    const auto Add = [this, Layout](const QString& Name, const QString& Text) {
        auto* Button = new QToolButton(this);
        Button->setObjectName(Name); Button->setAccessibleName(Text); Button->setToolTip(Text);
        Button->setFixedSize(22, 22); Button->setFocusPolicy(Qt::NoFocus);
        Layout->addWidget(Button); return Button;
    };
    minimize_ = Add("fm.window.minimize", tr("최소화"));
    maximize_ = Add("fm.window.maximize", tr("최대화 / 복원"));
    close_ = Add("fm.window.close", tr("닫기"));
    connect(minimize_, &QToolButton::clicked, Window, &QWidget::showMinimized);
    connect(maximize_, &QToolButton::clicked, this, &WindowChrome::toggleMaximized);
    connect(close_, &QToolButton::clicked, Window, &QWidget::close);
    connect(Window, &QWidget::windowTitleChanged, this, qOverload<>(&QWidget::update));
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, &WindowChrome::refresh);
    auto* MenuKey = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Space), Window);
    connect(MenuKey, &QShortcut::activated, this, [this] { systemMenu(mapToGlobal(QPoint(0, height()))); });
    refresh();
}
WindowChrome::~WindowChrome() { if (qApp) qApp->removeNativeEventFilter(this); }
void WindowChrome::refresh()
{
    const auto& Colors = fm::style::themeColorsFor(this);
    const auto& Chrome = fm::style::watercolorChrome(Colors.variant());
    const auto Css = QString("QToolButton {background:%1;color:%2;border:1px solid %3;} QToolButton:hover {border-color:%4;}")
        .arg(Chrome.capFill.name(), Chrome.capGlyph.name(), Chrome.capLine.name(), Chrome.capHoverLine.name());
    for (auto* Button : {minimize_, maximize_, close_}) Button->setStyleSheet(Css);
    minimize_->setText(QStringLiteral("−"));
    maximize_->setText(window_ && window_->isMaximized() ? QStringLiteral("❐") : QStringLiteral("□"));
    close_->setIcon(fm::style::glyphIcon(fm::style::Glyph::Close, Chrome.capGlyph, 12));
    update();
}
void WindowChrome::paintEvent(QPaintEvent*)
{
    const auto& Colors = fm::style::themeColorsFor(this);
    const auto& Chrome = fm::style::watercolorChrome(Colors.variant());
    QPainter Painter(this);
    QLinearGradient Gradient(0, 0, width(), 0);
    const bool Active = window_ && window_->isActiveWindow();
    Gradient.setColorAt(0, Active ? Chrome.title : Chrome.titleInactive);
    Gradient.setColorAt(1, Active ? Chrome.titleCap : Chrome.titleInactiveHi);
    Painter.fillRect(rect(), Gradient);
    Painter.setPen(Chrome.titleEdge); Painter.drawRect(rect().adjusted(0, 0, -1, -1));
    fm::style::paintGlyph(&Painter, fm::style::Glyph::App, QRectF(6, 6, 16, 16), Qt::white);
    Painter.setFont(fm::style::pixelFont(font(), 13, QFont::DemiBold)); Painter.setPen(Qt::white);
    const QRect Text(28, 0, std::max(0, width() - 108), height());
    Painter.drawText(Text, Qt::AlignVCenter, Painter.fontMetrics().elidedText(window_ ? window_->windowTitle() : QString{}, Qt::ElideMiddle, Text.width()));
}
void WindowChrome::toggleMaximized()
{
    if (!window_) return;
    window_->isMaximized() ? window_->showNormal() : window_->showMaximized();
}
void WindowChrome::systemMenu(const QPoint& Point)
{
#ifdef Q_OS_WIN
    if (!window_) return;
    const auto Handle = reinterpret_cast<HWND>(window_->winId());
    const auto Menu = GetSystemMenu(Handle, FALSE);
    EnableMenuItem(Menu, SC_RESTORE, MF_BYCOMMAND | (window_->isMaximized() ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(Menu, SC_MAXIMIZE, MF_BYCOMMAND | (window_->isMaximized() ? MF_GRAYED : MF_ENABLED));
    const auto Scale = window_->devicePixelRatioF();
    const auto Local = window_->mapFromGlobal(Point);
    POINT Native{qRound(Local.x() * Scale), qRound(Local.y() * Scale)};
    ClientToScreen(Handle, &Native);
    const auto Command = TrackPopupMenu(Menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, Native.x, Native.y, 0, Handle, nullptr);
    if (Command) PostMessageW(Handle, WM_SYSCOMMAND, Command, 0);
#else
    Q_UNUSED(Point);
#endif
}
void WindowChrome::mousePressEvent(QMouseEvent* Event)
{
    if (!window_) return;
    if (Event->button() == Qt::RightButton) systemMenu(Event->globalPosition().toPoint());
    else if (Event->button() == Qt::LeftButton && window_->windowHandle()) window_->windowHandle()->startSystemMove();
}
void WindowChrome::mouseDoubleClickEvent(QMouseEvent* Event)
{
    if (Event->button() == Qt::LeftButton) toggleMaximized();
}
bool WindowChrome::eventFilter(QObject* Object, QEvent* Event)
{
    if (Object == window_) {
#ifdef Q_OS_WIN
        if (Event->type() == QEvent::Show || Event->type() == QEvent::WinIdChange) {
            const auto Handle = reinterpret_cast<HWND>(window_->winId());
            const auto Flags = GetWindowLongPtrW(Handle, GWL_STYLE);
            SetWindowLongPtrW(Handle, GWL_STYLE, Flags | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
            SetWindowPos(Handle, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
#endif
        if (Event->type() == QEvent::WindowStateChange || Event->type() == QEvent::ActivationChange) refresh();
    }
    return QWidget::eventFilter(Object, Event);
}
bool WindowChrome::nativeEventFilter(const QByteArray&, void* Message, qintptr* Result)
{
#ifdef Q_OS_WIN
    const auto* Msg = static_cast<MSG*>(Message);
    if (!window_ || Msg->hwnd != reinterpret_cast<HWND>(window_->internalWinId())) return false;
    if (Msg->message == WM_NCHITTEST) {
        RECT Bounds; GetWindowRect(Msg->hwnd, &Bounds);
        const int X = GET_X_LPARAM(Msg->lParam), Y = GET_Y_LPARAM(Msg->lParam);
        const int Border = qRound(6 * window_->devicePixelRatioF());
        if (!window_->isMaximized()) {
            const bool Left = X < Bounds.left + Border, Right = X >= Bounds.right - Border;
            const bool Top = Y < Bounds.top + Border, Bottom = Y >= Bounds.bottom - Border;
            if (Top || Bottom || Left || Right) {
                if (Result) *Result = Top ? (Left ? HTTOPLEFT : Right ? HTTOPRIGHT : HTTOP) : Bottom ? (Left ? HTBOTTOMLEFT : Right ? HTBOTTOMRIGHT : HTBOTTOM) : Left ? HTLEFT : HTRIGHT;
                return true;
            }
        }
        // Map native client coordinates (physical pixels) through Qt for mixed-DPI monitors.
        POINT Client{X, Y}; ScreenToClient(Msg->hwnd, &Client);
        const auto Scale = window_->devicePixelRatioF();
        const QPoint Local(qRound(Client.x / Scale), qRound(Client.y / Scale));
        const auto MaxRect = QRect(maximize_->mapTo(window_, QPoint()), maximize_->size());
        if (MaxRect.contains(Local)) { if (Result) *Result = HTMAXBUTTON; return true; }
    }
    // The dispatcher passes no result pointer for queued mouse messages.
    // Synchronous hit-test calls do supply one. Both paths reach this filter.
    if (Msg->message == WM_NCLBUTTONDOWN && Msg->wParam == HTMAXBUTTON) { if (Result) *Result = 0; return true; }
    if (Msg->message == WM_NCLBUTTONUP && Msg->wParam == HTMAXBUTTON) {
        toggleMaximized(); if (Result) *Result = 0; return true;
    }
#else
    Q_UNUSED(Message); Q_UNUSED(Result);
#endif
    return false;
}
}
