#pragma once

#include "fmwidgets/Glyph.h"

#include <QFrame>

class QAbstractButton;
class QHBoxLayout;
class QLabel;

namespace fm::ui {

class Button;

/// 배너(목업 .banner) — 여백 12 · 10, 모서리 6, 아이콘 16 + 간격 10, 12.5 px 글(리치 텍스트 — <b> 허용).
/// Info는 테두리 없음, Warn · Danger는 1 px 테두리. 시안2는 네모.
/// 선택: 둘째 줄 설명(detail, 11.5 px --fg2), 그 아래 작은 단추 줄(addAction), 오른쪽 위 닫기(closable).
/// 셋 다 쓰지 않으면 예전과 같은 한 줄 배너다.
class Banner : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(Tone tone READ tone WRITE setTone)
    Q_PROPERTY(fm::ui::glyph::Glyph glyph READ glyph WRITE setGlyph)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QString detail READ detail WRITE setDetail)
    Q_PROPERTY(bool closable READ isClosable WRITE setClosable)
    Q_PROPERTY(IconAlignment iconAlignment READ iconAlignment WRITE setIconAlignment)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    enum Tone { Info, Warn, Danger, Ok };
    Q_ENUM(Tone)
    enum IconAlignment { IconTop, IconCenter };
    Q_ENUM(IconAlignment)

    explicit Banner(QWidget *parent = nullptr);
    Banner(Tone tone, glyph::Glyph glyph, const QString &text, QWidget *parent = nullptr);

    Tone tone() const noexcept { return m_tone; }
    void setTone(Tone tone);
    glyph::Glyph glyph() const noexcept { return m_glyph; }
    void setGlyph(glyph::Glyph glyph);
    QString text() const;
    void setText(const QString &text);
    IconAlignment iconAlignment() const noexcept { return m_iconAlign; }
    void setIconAlignment(IconAlignment alignment);
    QString detail() const;
    // 빈 문자열이면 둘째 줄을 숨긴다.
    void setDetail(const QString &detail);
    // 설명 아래 줄에 작은 단추를 더한다(왼쪽부터). 눌림은 돌려받은 단추의 clicked로 받는다.
    Button *addAction(const QString &text);
    void clearActions();
    bool isClosable() const noexcept { return m_closable; }
    // 오른쪽 위 닫기 단추. 누르면 closeRequested — 숨기기는 받는 쪽이 정한다.
    void setClosable(bool closable);

Q_SIGNALS:
    void closeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyTextColor();

    Tone m_tone = Info;
    glyph::Glyph m_glyph = glyph::Info;
    IconAlignment m_iconAlign = IconCenter;
    QLabel *m_label = nullptr;
    QLabel *m_detail = nullptr;
    QWidget *m_actions = nullptr;
    QHBoxLayout *m_actionLayout = nullptr;
    Button *m_close = nullptr;
    bool m_closable = false;
    bool m_applying = false;
};

} // namespace fm::ui
