#pragma once

#include "fmwidgets/Glyph.h"

#include <QPointer>
#include <QWidget>

class QApplication;
class QLabel;
class QTimer;
class QVariantAnimation;

namespace fm::ui {

/// 도구 설명 — 두 디자인에 맞춘 떠 있는 설명(ElaToolTip · Windows 11 ToolTip · XP 풍선 도움말).
///
/// - 시안1: Surface 바탕 · 1 px 선 · 모서리 6 · 부드러운 그림자, 12 px 글. 꼬리를 켜면 대상 쪽으로 작은 삼각형.
/// - 시안2: XP 노란 칸(tipBg · tipLine) · 오른쪽 아래 그림자. 제목이나 꼬리가 있으면 XP 풍선(둥근 모서리 + 꼬리).
/// - 글(리치 텍스트 허용, 최대 폭에서 줄 바꿈), 제목 + 아이콘(풍선 도움말), 또는 사용자 위젯(setCustomWidget).
///
/// 대상 위젯에 붙이면(생성자 · setTarget) 대상의 도움말 이벤트(QEvent::ToolTip — 스타일의 대기 시간)에 뜨고,
/// 마우스가 떠나거나 누르면 숨는다. Qt 기본 도구 설명은 뜨지 않는다. 앱 전체에서 QWidget::toolTip()과
/// 항목 보기의 Qt::ToolTipRole을 이 모양으로 띄우려면 installGlobal().
class ToolTip : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(fm::ui::glyph::Glyph glyph READ glyph WRITE setGlyph)
    Q_PROPERTY(Placement placement READ placement WRITE setPlacement)
    Q_PROPERTY(bool tailVisible READ isTailVisible WRITE setTailVisible)
    Q_PROPERTY(int showDelay READ showDelay WRITE setShowDelay)
    Q_PROPERTY(int hideDelay READ hideDelay WRITE setHideDelay)
    Q_PROPERTY(int duration READ duration WRITE setDuration)
    Q_PROPERTY(int maximumTextWidth READ maximumTextWidth WRITE setMaximumTextWidth)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    /// Cursor = 마우스 아래(시스템 도구 설명 자리), 나머지는 대상 위젯 둘레(가운데 맞춤, 자리가 없으면 반대쪽).
    enum Placement { Cursor, Below, Above, Left, Right };
    Q_ENUM(Placement)

    explicit ToolTip(QWidget *target = nullptr);
    ToolTip(const QString &text, QWidget *target);
    ~ToolTip() override;

    /// 대상에 새 도구 설명을 붙인다(대상이 지우면 함께 지워진다).
    static ToolTip *attach(QWidget *target, const QString &text, const QString &title = {},
                           glyph::Glyph glyph = glyph::None);
    /// 앱 전체의 도구 설명을 이 모양으로 — 위젯의 toolTip() · 항목 보기의 ToolTipRole. 붙인 ToolTip이 있는 위젯은 그쪽이 맡는다.
    static void installGlobal(QApplication &app);
    static void uninstallGlobal(QApplication &app);
    static bool isGlobalInstalled();
    /// QToolTip::showText처럼 — 앱이 함께 쓰는 하나를 띄운다. 글이 비면 숨긴다.
    static void showText(const QPoint &globalPos, const QString &text, QWidget *owner = nullptr);
    static void hideText();
    /// 함께 쓰는 하나(installGlobal · showText) — 테스트 · 미리보기용.
    static ToolTip *shared();

    QWidget *target() const { return m_target; }
    void setTarget(QWidget *target);

    QString text() const { return m_text; }
    void setText(const QString &text);
    QString title() const { return m_title; }
    void setTitle(const QString &title);
    glyph::Glyph glyph() const noexcept { return m_glyph; }
    void setGlyph(glyph::Glyph glyph);
    /// 글 대신 보일 위젯(소유권을 가져간다). nullptr이면 글로 돌아간다.
    void setCustomWidget(QWidget *widget);
    QWidget *customWidget() const { return m_custom; }

    Placement placement() const noexcept { return m_placement; }
    void setPlacement(Placement placement);
    bool isTailVisible() const noexcept { return m_tail; }
    void setTailVisible(bool visible);
    /// -1 = 스타일의 대기 시간(도움말 이벤트), 0 이상 = 마우스가 들어온 뒤 그 ms.
    int showDelay() const noexcept { return m_showDelay; }
    void setShowDelay(int ms);
    /// 마우스가 떠난 뒤 숨기까지 ms.
    int hideDelay() const noexcept { return m_hideDelay; }
    void setHideDelay(int ms);
    /// 보이는 시간 ms — 0 = 글 길이에 따라(10초 + 글자당 40 ms), -1 = 떠날 때까지.
    int duration() const noexcept { return m_duration; }
    void setDuration(int ms);
    int maximumTextWidth() const noexcept { return m_maxTextWidth; }
    void setMaximumTextWidth(int px);

    /// 대상(없으면 globalPos)에 맞춰 띄운다. Cursor 배치면 globalPos 아래.
    void showAt(const QPoint &globalPos);
    /// 흐려지며 숨는다.
    void hideTip();
    /// 꼬리가 가리키는 쪽 — 띄운 뒤 정해진다(Cursor 배치 · 꼬리 끔이면 Qt::Edge 0).
    Qt::Edge tailEdge() const noexcept { return m_tailEdge; }
    /// 띄우지 않고 쓸 때(창 안에 묻은 설명 · 미리보기) 꼬리 쪽과 위치(그 변을 따라 px, -1 = 가운데)를 정한다.
    /// showAt이 다시 정한다.
    void setTail(Qt::Edge edge, int offset = -1);
    /// 그림자 · 꼬리를 뺀 풍선 영역(위젯 좌표).
    QRect panelRect() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    void rebuild();
    void applyColors();
    QMargins chromeMargins() const;
    bool balloon() const;
    void placeNear(const QPoint &globalPos);
    void fadeTo(qreal opacity);
    /// 색을 읽을 위젯 — 범위(ThemeScope)를 따르도록 주인 · 대상, 없으면 자신.
    const QWidget *colorSource() const
    {
        return m_scope ? m_scope.data() : m_target ? m_target.data() : static_cast<const QWidget *>(this);
    }

    QPointer<QWidget> m_target;
    QPointer<QWidget> m_scope;  // 함께 쓰는 도구 설명의 주인(색 범위만)
    QString m_text;
    QString m_title;
    glyph::Glyph m_glyph = glyph::None;
    QPointer<QWidget> m_custom;
    QLabel *m_icon = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_body = nullptr;
    Placement m_placement = Cursor;
    bool m_tail = false;
    int m_showDelay = -1;
    int m_hideDelay = 120;
    int m_duration = 0;
    int m_maxTextWidth = 360;
    Qt::Edge m_tailEdge = Qt::Edge(0);
    int m_tailOffset = 0;  // 꼬리 가운데의 위치(꼬리 쪽 변을 따라)
    QPoint m_cursor;
    QTimer *m_showTimer = nullptr;
    QTimer *m_hideTimer = nullptr;
    QTimer *m_lifeTimer = nullptr;
    QVariantAnimation *m_fade = nullptr;
};

} // namespace fm::ui
