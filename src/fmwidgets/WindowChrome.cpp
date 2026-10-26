#include "fmwidgets/WindowChrome.h"
#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/PaperFluentChrome.h>
#include <fmstyle/WatercolorChrome.h>
#include <QApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QShortcut>
#include <QToolButton>
#include <QWindow>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <windowsx.h>
#endif

namespace fm::ui {
namespace {
// 시안3 캡션 단추 — Fluent 기호를 제목 글자색으로 직접 그린다(워터컬러는 글자 · 닫기 기호).
enum class Caption { Minimize, Maximize, Restore, Close };
QIcon captionIcon(Caption Kind, const QColor& Color)
{
    QIcon Icon;
    for (const qreal Dpr : {1.0, 1.25, 1.5, 2.0}) {
        QPixmap Pixmap(qCeil(10 * Dpr), qCeil(10 * Dpr)); Pixmap.setDevicePixelRatio(Dpr); Pixmap.fill(Qt::transparent);
        QPainter Painter(&Pixmap); Painter.setRenderHint(QPainter::Antialiasing, Kind == Caption::Close);
        QPen Pen(Color, 1.0); Pen.setCosmetic(false); Painter.setPen(Pen);
        switch (Kind) {
        case Caption::Minimize: Painter.drawLine(QPointF(0, 5.5), QPointF(10, 5.5)); break;
        case Caption::Maximize: Painter.drawRect(QRectF(0.5, 0.5, 9, 9)); break;
        case Caption::Restore: Painter.drawRect(QRectF(0.5, 2.5, 7, 7)); Painter.drawPolyline(QPolygonF{QPointF(2.5, 2.5), QPointF(2.5, 0.5), QPointF(9.5, 0.5), QPointF(9.5, 7.5), QPointF(7.5, 7.5)}); break;
        case Caption::Close: Painter.drawLine(QPointF(0.5, 0.5), QPointF(9.5, 9.5)); Painter.drawLine(QPointF(9.5, 0.5), QPointF(0.5, 9.5)); break;
        }
        Painter.end(); Icon.addPixmap(Pixmap);
    }
    return Icon;
}
int chromeHeight(const QWidget* Widget)
{
    // 시안3 32 px(보드 "제목 표시줄 32"), 워터컬러 28
    const bool Paper = fm::style::themeColorsFor(Widget).isPaperFluent();
    return qMax(Paper ? 32 : 28, Widget->fontMetrics().height() + 10);
}
}

WindowChrome::WindowChrome(QWidget* Window, QWidget* Parent, WindowChromeOptions Options)
    : QWidget(Parent), window_(Window), options_(Options)
{
    Window->setWindowFlag(Qt::FramelessWindowHint);
    Window->installEventFilter(this);
    installEventFilter(this);
    qApp->installNativeEventFilter(this);
    setFixedHeight(chromeHeight(this));
    auto* Layout = new QHBoxLayout(this);
    Layout->setContentsMargins(4, 3, 4, 3);
    Layout->setSpacing(2);
    Layout->addStretch();
    const auto Add = [this, Layout](const QString& Name, const QString& Text) {
        auto* Button = new QToolButton(this);
        Button->setObjectName(Name); Button->setAccessibleName(Text); Button->setToolTip(Text);
        Button->setFixedSize(22, 22); Button->setFocusPolicy(Qt::StrongFocus);
        Layout->addWidget(Button); return Button;
    };
    minimize_ = Add("fm.window.minimize", tr("최소화"));
    maximize_ = Add("fm.window.maximize", tr("최대화 / 복원"));
    close_ = Add("fm.window.close", tr("닫기"));
    minimize_->setVisible(options_.minimizeButton);
    maximize_->setVisible(options_.maximizeButton);
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
    setFixedHeight(chromeHeight(this));
    auto* Layout = static_cast<QHBoxLayout*>(layout());
    if (Colors.isPaperFluent()) {
        // 시안3: 단추는 제목 줄 높이 전체 · 46 px, 바탕 없이 올림 · 누름만 밝은 겹침, 닫기는 위험색
        const auto Paper = fm::style::paperFluentChrome(Colors);
        Layout->setContentsMargins(0, 0, 0, 0); Layout->setSpacing(0);
        const auto Fg = Paper.titleFg;
        const auto Css = QString("QToolButton {background:transparent;border:0;} QToolButton:hover {background:rgba(%1,%2,%3,36);} QToolButton:pressed {background:rgba(%1,%2,%3,60);}")
            .arg(Fg.red()).arg(Fg.green()).arg(Fg.blue());
        for (auto* Button : {minimize_, maximize_}) { Button->setStyleSheet(Css); Button->setFixedSize(46, height()); Button->setText({}); }
        close_->setStyleSheet(Css + QString(" QToolButton#fm\\.window\\.close:hover {background:%1;} QToolButton#fm\\.window\\.close:pressed {background:%2;}")
            .arg(Colors[fm::style::Token::DangerFill].name(), Colors[fm::style::Token::DangerFill].darker(115).name()));
        close_->setFixedSize(46, height());
        minimize_->setIcon(captionIcon(Caption::Minimize, Fg));
        maximize_->setIcon(captionIcon(window_ && window_->isMaximized() ? Caption::Restore : Caption::Maximize, Fg));
        close_->setIcon(captionIcon(Caption::Close, Fg));
        for (auto* Button : {minimize_, maximize_, close_}) Button->setIconSize(QSize(10, 10));
        update();
        return;
    }
    Layout->setContentsMargins(4, 3, 4, 3); Layout->setSpacing(2);
    for (auto* Button : {minimize_, maximize_, close_}) { Button->setFixedSize(22, 22); Button->setIcon({}); }
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
    QPainter Painter(this);
    if (Colors.isPaperFluent()) {
        // 시안3: 세로 파란 워시(--title-1 → --title-2), 비활성 창은 창 바탕 쪽으로 옅게
        const auto Paper = fm::style::paperFluentChrome(Colors);
        const bool Active = window_ && window_->isActiveWindow();
        const auto Win = Colors[fm::style::Token::Win];
        QLinearGradient Wash(0, 0, 0, height());
        Wash.setColorAt(0, Active ? Paper.titleTop : QColor::fromRgbF(Paper.titleTop.redF() * 0.45 + Win.redF() * 0.55, Paper.titleTop.greenF() * 0.45 + Win.greenF() * 0.55, Paper.titleTop.blueF() * 0.45 + Win.blueF() * 0.55));
        Wash.setColorAt(1, Active ? Paper.titleBottom : QColor::fromRgbF(Paper.titleBottom.redF() * 0.45 + Win.redF() * 0.55, Paper.titleBottom.greenF() * 0.45 + Win.greenF() * 0.55, Paper.titleBottom.blueF() * 0.45 + Win.blueF() * 0.55));
        Painter.fillRect(rect(), Wash);
        auto Fg = Paper.titleFg; if (!Active) Fg.setAlphaF(0.85f);
        fm::style::paintGlyph(&Painter, fm::style::Glyph::App, QRectF(12, (height() - 16) / 2.0, 16, 16), Fg);
        auto TitleFont = font(); TitleFont.setPixelSize(12); TitleFont.setWeight(QFont::DemiBold); Painter.setFont(TitleFont); Painter.setPen(Fg);
        const QRect Text(38, 0, std::max(0, width() - (38 + layout()->minimumSize().width() + 8)), height());
        Painter.drawText(Text, Qt::AlignVCenter, Painter.fontMetrics().elidedText(window_ ? window_->windowTitle() : QString{}, Qt::ElideMiddle, Text.width()));
        return;
    }
    const auto& Chrome = fm::style::watercolorChrome(Colors.variant());
    QLinearGradient Gradient(0, 0, width(), 0);
    const bool Active = window_ && window_->isActiveWindow();
    Gradient.setColorAt(0, Active ? Chrome.title : Chrome.titleInactive);
    Gradient.setColorAt(1, Active ? Chrome.titleCap : Chrome.titleInactiveHi);
    Painter.fillRect(rect(), Gradient);
    Painter.setPen(Chrome.titleEdge); Painter.drawRect(rect().adjusted(0, 0, -1, -1));
    fm::style::paintGlyph(&Painter, fm::style::Glyph::App, QRectF(6, (height() - 16) / 2, 16, 16), Qt::white);
    auto TitleFont = font(); TitleFont.setWeight(QFont::DemiBold); Painter.setFont(TitleFont); Painter.setPen(Qt::white);
    const QRect Text(28, 0, std::max(0, width() - (28 + layout()->minimumSize().width())), height());
    Painter.drawText(Text, Qt::AlignVCenter, Painter.fontMetrics().elidedText(window_ ? window_->windowTitle() : QString{}, Qt::ElideMiddle, Text.width()));
}
void WindowChrome::toggleMaximized()
{
    if (!window_ || !options_.maximizeButton) return;
    window_->isMaximized() ? window_->showNormal() : window_->showMaximized();
}
void WindowChrome::systemMenu(const QPoint& Point)
{
#ifdef Q_OS_WIN
    if (!window_) return;
    const auto Handle = reinterpret_cast<HWND>(window_->winId());
    const auto Menu = GetSystemMenu(Handle, FALSE);
    EnableMenuItem(Menu, SC_RESTORE, MF_BYCOMMAND | (window_->isMaximized() ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(Menu, SC_MAXIMIZE, MF_BYCOMMAND | (!options_.maximizeButton || window_->isMaximized() ? MF_GRAYED : MF_ENABLED));
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
    if (Object == this && Event->type() == QEvent::FontChange) setFixedHeight(chromeHeight(this));
    if (Object == window_) {
#ifdef Q_OS_WIN
        if (Event->type() == QEvent::Show || Event->type() == QEvent::WinIdChange) {
            const auto Handle = reinterpret_cast<HWND>(window_->winId());
            const auto Flags = GetWindowLongPtrW(Handle, GWL_STYLE);
            SetWindowLongPtrW(Handle, GWL_STYLE, (Flags | WS_THICKFRAME | WS_SYSMENU) | (options_.minimizeButton ? WS_MINIMIZEBOX : 0) | (options_.maximizeButton ? WS_MAXIMIZEBOX : 0));
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
        if (options_.maximizeButton && MaxRect.contains(Local)) { if (Result) *Result = HTMAXBUTTON; return true; }
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
