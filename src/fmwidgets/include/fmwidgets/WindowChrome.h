#pragma once

#include <QAbstractNativeEventFilter>
#include <QPointer>
#include <QWidget>

class QToolButton;

namespace fm::ui {

struct WindowChromeOptions
{
    bool minimizeButton = true;
    bool maximizeButton = true;
};

// Main-window chrome keeps native resize, system-menu and Snap handling on Windows.
// It owns no product commands and can wrap any top-level QWidget.
class WindowChrome : public QWidget, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit WindowChrome(QWidget* Window, QWidget* Parent = nullptr, WindowChromeOptions Options = {});
    ~WindowChrome() override;
    bool nativeEventFilter(const QByteArray& Type, void* Message, qintptr* Result) override;

protected:
    void paintEvent(QPaintEvent* Event) override;
    bool eventFilter(QObject* Object, QEvent* Event) override;
    void mousePressEvent(QMouseEvent* Event) override;
    void mouseDoubleClickEvent(QMouseEvent* Event) override;

private:
    void refresh();
    void toggleMaximized();
    void systemMenu(const QPoint& Point);
    QPointer<QWidget> window_;
    WindowChromeOptions options_;
    QToolButton* minimize_ = nullptr;
    QToolButton* maximize_ = nullptr;
    QToolButton* close_ = nullptr;
};
}
