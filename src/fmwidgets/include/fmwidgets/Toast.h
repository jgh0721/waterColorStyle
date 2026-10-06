#pragma once

#include "fmwidgets/Glyph.h"

#include <QColor>
#include <QString>
#include <QWidget>

class QVariantAnimation;

namespace fm::ui {

/// 알림(토스트) — 일을 끝낸 뒤 잠깐 떠 있다 스스로 사라지는 쪽지.
///
/// - 바탕 창(host) 안쪽 아래 가운데에 떠 있고, 여러 개면 위로 쌓인다(새것이 아래).
/// - 아이콘 16 + 간격 10 + 12.5 px 글(리치 텍스트 — <b> 허용), 여백 12 · 10, 모서리 6. Banner와 같은 치수다.
/// - 들어올 때 180 ms, 나갈 때 220 ms 동안 흐려지며 아래에서 6 px 올라온다.
/// - 누르면 바로 닫힌다. 마우스를 올린 동안은 시간이 멈춘다 — 읽는 중에 사라지지 않게.
/// - 단추가 있는 알림은 닫히지 않는다(action이 비지 않으면 머문다). 단추를 누르면 actionTriggered가 나간다.
///
/// 바탕 창의 자식이라 창을 옮기거나 크기를 바꿔도 따라다니고, 창이 닫히면 함께 사라진다.
/// 색은 FmStyle 테마 토큰에서 가져오므로 라이트 · 다크를 따로 설정할 필요가 없다.
class Toast : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(Tone tone READ tone WRITE setTone)
    Q_PROPERTY(fm::ui::glyph::Glyph glyph READ glyph WRITE setGlyph)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QString actionText READ actionText WRITE setActionText)
    Q_PROPERTY(int duration READ duration WRITE setDuration)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    enum Tone { Info, Ok, Warn, Danger };
    Q_ENUM(Tone)

    /// 기본 머무는 시간. 글이 길면 showOver()가 글자 수에 따라 늘린다.
    static constexpr int kDefaultMs = 4000;
    /// 한 바탕 창에 동시에 두는 최대 개수 — 넘으면 가장 오래된 것부터 닫는다.
    static constexpr int kMaxStack = 4;

    explicit Toast(QWidget *parent = nullptr);
    Toast(Tone tone, glyph::Glyph glyph, const QString &text, QWidget *parent = nullptr);
    ~Toast() override;

    /// 만들고 띄운다 — 부르는 쪽이 수명을 들지 않아도 된다(닫히면 스스로 지워진다).
    /// ms가 0이면 kDefaultMs에서 글 길이만큼 늘린 값을, 음수면 「스스로 닫지 않는다」를 뜻한다.
    static Toast *showOver(QWidget *host, Tone tone, const QString &text, int ms = 0);
    static Toast *showOver(QWidget *host, Tone tone, glyph::Glyph glyph, const QString &text, int ms = 0);
    /// 그 바탕 창에 떠 있는 알림을 모두 닫는다.
    static void dismissAll(QWidget *host);
    /// 그 바탕 창에 떠 있는 알림 — 아래에서 위로.
    static QList<Toast *> toastsOn(QWidget *host);

    Tone tone() const noexcept { return m_tone; }
    void setTone(Tone tone);
    glyph::Glyph glyph() const noexcept { return m_glyph; }
    void setGlyph(glyph::Glyph glyph);
    QString text() const { return m_text; }
    void setText(const QString &text);

    /// 오른쪽에 둘 단추의 글. 비면 단추가 없다. 단추가 있으면 스스로 닫히지 않는다.
    QString actionText() const { return m_actionText; }
    void setActionText(const QString &text);

    int duration() const noexcept { return m_duration; }
    void setDuration(int ms);

    /// 바탕 창 안쪽 아래 가운데에 띄운다. host가 null이면 parentWidget()을 쓴다.
    void showOn(QWidget *host = nullptr);
    /// 흐려지며 닫는다(바로 지우지 않는다 — 애니메이션이 끝나면 deleteLater).
    void dismiss();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void dismissed();
    void actionTriggered();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QRect actionRect() const;
    QRect textRect() const;
    int wrappedTextHeight(int width) const;
    void startTimer();
    void relayout();
    void fadeTo(qreal target, int ms, bool thenDelete);
    static void restack(QWidget *host);

    Tone m_tone = Info;
    glyph::Glyph m_glyph = glyph::Info;
    QString m_text;
    QString m_actionText;
    int m_duration = kDefaultMs;
    int m_timerId = 0;
    qreal m_opacity = 0.0;
    qreal m_rise = 6.0;  // 아래에서 올라오는 남은 거리
    bool m_hovering = false;
    bool m_closing = false;
    bool m_actionHot = false;
    QWidget *m_host = nullptr;
    QVariantAnimation *m_fade = nullptr;
};

} // namespace fm::ui
