#pragma once

#include <QStringList>
#include <QWidget>

class QLineEdit;

namespace fm::ui {

/// 경로 이동 줄(01 §1.3 A4b-2 · §10 · 06 §4.14) — 조각마다 앞에 꺾쇠, 마지막 조각은 굵게.
/// 시안1: 조각 26 · 좌우 6 · 모서리 4 · 13 px --fg2(마지막 --fg 600), 마우스 올림은 투명 단추 규칙.
/// 시안2: 들어간 칸 24 안에 조각 20 · 좌우 4, 마우스 올림 = --accent 바탕 + 흰 글자.
/// 넘치면 앞쪽 조각부터 숨기고 "…" 조각을 둔다. 빈 곳을 두 번 누르면 경로를 직접 입력한다.
class BreadcrumbBar : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList segments READ segments WRITE setSegments)

public:
    explicit BreadcrumbBar(QWidget *parent = nullptr);

    /// 드라이브 다음 조각들("Work", "fm-core"). 드라이브 조각은 드라이브 단추가 대신한다.
    QStringList segments() const { return m_segments; }
    void setSegments(const QStringList &segments);
    /// 직접 입력할 때 처음 보일 전체 경로.
    void setFullPath(const QString &path) { m_fullPath = path; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    /// index번째 조각을 눌렀다(0 = 첫 조각). -1 = 드라이브 루트("…"를 누름).
    void segmentClicked(int index);
    /// 경로를 직접 입력하고 Enter.
    void pathEntered(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Crumb
    {
        int index;      // 조각 번호, -1 = "…"
        QRect rect;     // 조각 칸(꺾쇠 제외)
        QRect chevron;
    };
    void layoutCrumbs();
    int crumbAt(const QPoint &pos) const;
    void startEditing();
    bool watercolor() const;

    QStringList m_segments;
    QString m_fullPath;
    QList<Crumb> m_crumbs;
    int m_hover = -2;
    int m_pressed = -2;
    QLineEdit *m_editor = nullptr;
};

} // namespace fm::ui
