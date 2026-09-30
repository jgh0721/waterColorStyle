#pragma once

#include <QWidget>

class QHBoxLayout;
class QTabBar;
class QVBoxLayout;
class QToolButton;

namespace fm::ui {

/// 패널 탭 줄(01 §1.3 A4a · 06 §4.12) — QTabBar + 새 탭 단추 + 아래 선.
/// 시안1: 줄 32 · 탭 28 · 좌우 8 · 새 탭 28 × 28 투명. 시안2: 줄 28 · 탭 23/26 · 좌우 4 · 새 탭 21 × 21 입체.
/// 활성 패널의 선택 탭 위 2 px 강조 띠는 스타일이 fmPaneActive로 그린다.
class PanelTabStrip : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool paneActive READ isPaneActive WRITE setPaneActive)

public:
    explicit PanelTabStrip(QWidget *parent = nullptr);

    QTabBar *tabBar() const noexcept { return m_tabs; }
    QToolButton *newTabButton() const noexcept { return m_add; }

    bool isPaneActive() const;
    void setPaneActive(bool active);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void newTabRequested();
    /// 가운데 단추로 탭을 눌렀다.
    void closeTabRequested(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyMetrics();

    QHBoxLayout *m_layout = nullptr;
    QVBoxLayout *m_addBox = nullptr;
    QTabBar *m_tabs = nullptr;
    QToolButton *m_add = nullptr;
};

} // namespace fm::ui
