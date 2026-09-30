#pragma once

#include "fmwidgets/Glyph.h"

#include <QWidget>

namespace fm::ui {

/// 태그(목업 .tag) — 높이 22(compact 20), 알약 모양, 11.5 px 600. 시안2는 네모.
/// 표 안에서는 fm::style::paintTag()를 직접 쓴다.
class Tag : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Tone tone READ tone WRITE setTone)
    Q_PROPERTY(fm::ui::glyph::Glyph glyph READ glyph WRITE setGlyph)
    Q_PROPERTY(bool compact READ isCompact WRITE setCompact)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum. Bad = 바탕 없는 위험 글자.
    enum Tone { Ok, Info, Mute, Bad, Warn, Danger };
    Q_ENUM(Tone)

    explicit Tag(QWidget *parent = nullptr);
    Tag(const QString &text, Tone tone, QWidget *parent = nullptr);

    QString text() const { return m_text; }
    void setText(const QString &text);
    Tone tone() const noexcept { return m_tone; }
    void setTone(Tone tone);
    glyph::Glyph glyph() const noexcept { return m_glyph; }
    void setGlyph(glyph::Glyph glyph);
    bool isCompact() const noexcept { return m_compact; }
    void setCompact(bool compact);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    Tone m_tone = Mute;
    glyph::Glyph m_glyph = glyph::None;
    bool m_compact = false;
};

} // namespace fm::ui
