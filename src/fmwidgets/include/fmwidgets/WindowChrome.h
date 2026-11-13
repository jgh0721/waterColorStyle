#pragma once

#include <QPointer>
#include <QWidget>

class QToolButton;

namespace fm::ui {

class WindowFrame;

struct WindowChromeOptions
{
    bool minimizeButton = true;
    bool maximizeButton = true;
};

// Main-window chrome keeps native resize, system-menu and Snap handling on Windows (WindowFrame).
// It owns no product commands and can wrap any top-level QWidget. Create it before the window's native handle.
class WindowChrome : public QWidget
{
    Q_OBJECT
public:
    explicit WindowChrome(QWidget* Window, QWidget* Parent = nullptr, WindowChromeOptions Options = {});
    ~WindowChrome() override;
    // A product widget between the title and the caption buttons. The title
    // text already yields the layout's minimum width; nullptr removes it.
    void setAccessoryWidget(QWidget* Widget);
    QWidget* accessoryWidget() const { return accessory_; }

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
    WindowFrame* frame_ = nullptr;
    QToolButton* minimize_ = nullptr;
    QToolButton* maximize_ = nullptr;
    QToolButton* close_ = nullptr;
    QPointer<QWidget> accessory_;
};
}
