#pragma once

#include "fmwidgets/Glyph.h"

#include <QFrame>

class QLabel;

namespace fm::ui {

/// 배너(목업 .banner) — 여백 12 · 10, 모서리 6, 아이콘 16 + 간격 10, 12.5 px 글(리치 텍스트 — <b> 허용).
/// Info는 테두리 없음, Warn · Danger는 1 px 테두리. 시안2는 네모.
class Banner : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(Tone tone READ tone WRITE setTone)
    Q_PROPERTY(fm::ui::glyph::Glyph glyph READ glyph WRITE setGlyph)
    Q_PROPERTY(QString text READ text WRITE setText)
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

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyTextColor();

    Tone m_tone = Info;
    glyph::Glyph m_glyph = glyph::Info;
    IconAlignment m_iconAlign = IconCenter;
    QLabel *m_label = nullptr;
    bool m_applying = false;
};

} // namespace fm::ui
