#pragma once

#include "fmwidgets/Glyph.h"

#include <QWidget>

namespace fm::ui {

/// 대화상자 머리 블록(목업 .head) — 배지 40 × 40 + 제목(15 px 600) + 부제(12 px, 한 줄 말줄임).
/// titleTo가 있으면 "제목 → 대상"으로 그린다(진행 창). 시안2는 배지 바탕이 투명하고 모서리가 없다.
class DialogHeader : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QString titleTo READ titleTo WRITE setTitleTo)
    Q_PROPERTY(QString subtitle READ subtitle WRITE setSubtitle)
    Q_PROPERTY(fm::ui::glyph::Glyph glyph READ glyph WRITE setGlyph)
    Q_PROPERTY(Tone tone READ tone WRITE setTone)
    Q_PROPERTY(Qt::TextElideMode subtitleElide READ subtitleElide WRITE setSubtitleElide)
    Q_PROPERTY(bool subtitleWrap READ subtitleWrap WRITE setSubtitleWrap)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    enum Tone { Info, Danger, Mute, Warn, Ok };
    Q_ENUM(Tone)

    explicit DialogHeader(QWidget *parent = nullptr);

    QString title() const { return m_title; }
    void setTitle(const QString &title);
    QString titleTo() const { return m_titleTo; }
    void setTitleTo(const QString &titleTo);
    QString subtitle() const { return m_subtitle; }
    void setSubtitle(const QString &subtitle);
    glyph::Glyph glyph() const noexcept { return m_glyph; }
    void setGlyph(glyph::Glyph glyph);
    Tone tone() const noexcept { return m_tone; }
    void setTone(Tone tone);
    /// 부제 말줄임 — 경로는 가운데(ElideMiddle)를 권장.
    Qt::TextElideMode subtitleElide() const noexcept { return m_elide; }
    void setSubtitleElide(Qt::TextElideMode mode);
    /// 부제를 여러 줄로(권한 대화상자의 설명 .desc — 13 px, 줄 19).
    bool subtitleWrap() const noexcept { return m_wrap; }
    void setSubtitleWrap(bool wrap);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override { return m_wrap; }
    int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void refreshAccessibility();
    QFont titleFont() const;
    QFont subtitleFont() const;
    int textLeft() const { return 40 + 14; }

    QString m_title;
    QString m_titleTo;
    QString m_subtitle;
    glyph::Glyph m_glyph = glyph::None;
    Tone m_tone = Info;
    Qt::TextElideMode m_elide = Qt::ElideRight;
    bool m_wrap = false;
};

} // namespace fm::ui
