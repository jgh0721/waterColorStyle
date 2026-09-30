#pragma once

#include <QLabel>

namespace fm::ui {

/// 목업의 글자 역할(필드 라벨 · 도움말 · 통계 값 등)을 글꼴 · 색으로 적용하는 QLabel.
/// 색은 테마 토큰에서 오므로 디자인 · 색 구성표를 바꾸면 다시 적용된다. 버디 · 니모닉은 QLabel 그대로.
class Label : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(TextRole textRole READ textRole WRITE setTextRole)
    Q_PROPERTY(Tone tone READ tone WRITE setTone)
    Q_PROPERTY(bool monospace READ isMonospace WRITE setMonospace)
    Q_PROPERTY(bool tabularNumbers READ tabularNumbers WRITE setTabularNumbers)
    Q_PROPERTY(Qt::TextElideMode elideMode READ elideMode WRITE setElideMode)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum. 목업 CSS 클래스는 주석.
    enum TextRole {
        Body,        // 기본 13 px · --fg
        FieldLabel,  // .lbl — 12 px 600 · --fg2
        Help,        // .help — 12 px · --fg3 · 줄바꿈
        Meta,        // .meta — 12 px · --fg2
        PathMeta,    // .path.mono — mono 12 px · --fg3
        StatLabel,   // .stats dt — 12 px · --fg3
        StatValue,   // .stats dd — 15 px 600 · --fg · 숫자 너비 고정
        BigNumber,   // 전체 % — 26 px 600 · --fg · 숫자 너비 고정
        Caption,     // .gt-t — 12 px 600 · --fg2
        Minor,       // .gt-s — 12 px · --fg3
        Summary,     // 버튼 영역 요약 — 12.5 px · --fg2 · 숫자 너비 고정
        Heading,     // .h1 — 15 px 600 · --fg
        Description  // .desc — 13 px · --fg2 · 줄바꿈
    };
    Q_ENUM(TextRole)

    /// 역할 기본색을 바꾼다 (오류 도움말 → Danger 등).
    enum Tone { Default, Secondary, Muted, Accent, Danger, Warn, Ok };
    Q_ENUM(Tone)

    explicit Label(QWidget *parent = nullptr);
    explicit Label(const QString &text, QWidget *parent = nullptr);
    Label(const QString &text, TextRole role, QWidget *parent = nullptr);

    TextRole textRole() const noexcept { return m_role; }
    void setTextRole(TextRole role);
    Tone tone() const noexcept { return m_tone; }
    void setTone(Tone tone);
    bool isMonospace() const noexcept { return m_mono; }
    void setMonospace(bool on);
    bool tabularNumbers() const noexcept { return m_tnum; }
    void setTabularNumbers(bool on);

    /// ElideNone(기본)이 아니면 한 줄로 줄여 그리고, 줄였을 때 전체 글을 도구 설명으로 보인다.
    Qt::TextElideMode elideMode() const noexcept { return m_elide; }
    void setElideMode(Qt::TextElideMode mode);

    QSize minimumSizeHint() const override;

protected:
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void apply();

    TextRole m_role = Body;
    Tone m_tone = Default;
    bool m_mono = false;
    bool m_tnum = false;
    Qt::TextElideMode m_elide = Qt::ElideNone;
    bool m_applying = false;
};

} // namespace fm::ui
