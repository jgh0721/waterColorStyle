#pragma once

#include "fmwidgets/Glyph.h"

#include <QPushButton>

namespace fm::ui {

/// 역할(보통 · 기본 · 위험 · 투명 · 링크)과 작은 크기를 속성으로 가진 버튼.
/// 일반 QPushButton에 fm::style::setButtonRole()을 쓰는 것과 같고, Qt Designer에서 고르기 쉽게 한 것.
class Button : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(Role role READ role WRITE setRole)
    Q_PROPERTY(bool compact READ isCompact WRITE setCompact)
    Q_PROPERTY(QString keyHint READ keyHint WRITE setKeyHint)
    Q_PROPERTY(fm::ui::glyph::Glyph glyph READ glyph WRITE setGlyph)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    // Link: 바탕 · 테두리 없는 강조색 글자 단추(12.5 px, 아이콘 12) — 진행 창 '간단히 보기'.
    enum Role { Normal, Primary, Danger, Subtle, Link };
    Q_ENUM(Role)

    explicit Button(QWidget *parent = nullptr);
    explicit Button(const QString &text, QWidget *parent = nullptr);

    Role role() const noexcept { return m_role; }
    void setRole(Role role);

    /// 작은 버튼 — 시안1 높이 30 px(대화상자 밀도 28) · 여백 12 px, 시안2 높이 24 px · 여백 8 px. 최소 폭 없음.
    bool isCompact() const noexcept { return m_compact; }
    void setCompact(bool compact);

    /// 글자 뒤 키 칩(예: "F2") — 스타일 속성 fmKeyHint.
    QString keyHint() const { return m_keyHint; }
    void setKeyHint(const QString &keys);

    /// 테마 색 아이콘 — 역할의 글자색으로 칠하고, 디자인 · 색 구성표가 바뀌면 다시 만든다.
    glyph::Glyph glyph() const noexcept { return m_glyph; }
    void setGlyph(glyph::Glyph glyph);

protected:
    void changeEvent(QEvent *event) override;

private:
    void refreshIcon();

    Role m_role = Normal;
    bool m_compact = false;
    QString m_keyHint;
    glyph::Glyph m_glyph = glyph::None;
};

} // namespace fm::ui
