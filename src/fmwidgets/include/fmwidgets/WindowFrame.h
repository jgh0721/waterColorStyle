#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPoint>
#include <QPointer>

#include <functional>

class QWidget;

namespace fm::ui {

struct WindowFrameOptions
{
    bool minimizeBox = true;
    bool maximizeBox = true;
};

// A custom title bar over the native Windows frame, sized like other Windows apps.
// The caption turns into client area. The left, right and bottom resize borders stay non-client, outside the
// visible edge, so child windows (native widgets, other processes' windows) cannot cover them. The top edge is a
// band inside the client area: same-thread child windows under it answer HTTRANSPARENT and the window answers HTTOP.
// Create it before the window's native handle exists; it clears Qt::FramelessWindowHint.
class WindowFrame final : public QObject, public QAbstractNativeEventFilter
{
public:
    // Classifies a window-local (device-independent) point that is not on a resize border, e.g. HTMAXBUTTON or
    // HTCAPTION. 0 means client area.
    using CaptionHitTest = std::function<int(const QPoint& Local)>;

    explicit WindowFrame(QWidget* Window, WindowFrameOptions Options = {});
    ~WindowFrame() override;

    void setCaptionHitTest(CaptionHitTest Test) { captionHitTest_ = std::move(Test); }
    // Runs on a left click released on a point the caption test reported as HTMAXBUTTON.
    void setMaximizeButtonHandler(std::function<void()> Handler) { maximizeButton_ = std::move(Handler); }

    // Physical width of the resize border at the window's DPI (SM_CXSIZEFRAME + SM_CXPADDEDBORDER).
    static int resizeBorderWidth(const QWidget* Window);

    bool nativeEventFilter(const QByteArray& Type, void* Message, qintptr* Result) override;

protected:
    bool eventFilter(QObject* Object, QEvent* Event) override;

private:
    bool resizable() const;
    void applyFrame();
#ifdef Q_OS_WIN
    long hitTest(void* Root, long X, long Y) const;
#endif

    QPointer<QWidget> window_;
    WindowFrameOptions options_;
    CaptionHitTest captionHitTest_;
    std::function<void()> maximizeButton_;
    quintptr framedHandle_ = 0;
    // The window already had a frameless native handle: keep Qt's full-window client area and use inner bands.
    bool inner_ = false;
};

}
