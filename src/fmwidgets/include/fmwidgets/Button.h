#pragma once

#include <QPushButton>

namespace fm::ui {

/// 역할(보통 · 기본 · 위험 · 투명)과 작은 크기를 속성으로 가진 버튼.
/// 일반 QPushButton에 fm::style::setButtonRole()을 쓰는 것과 같고, Qt Designer에서 고르기 쉽게 한 것.
class Button : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(Role role READ role WRITE setRole)
    Q_PROPERTY(bool compact READ isCompact WRITE setCompact)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    enum Role { Normal, Primary, Danger, Subtle };
    Q_ENUM(Role)

    explicit Button(QWidget *parent = nullptr);
    explicit Button(const QString &text, QWidget *parent = nullptr);

    Role role() const noexcept { return m_role; }
    void setRole(Role role);

    /// 작은 버튼 — 시안1 높이 30 px · 여백 12 px, 시안2 높이 24 px · 여백 8 px. 최소 폭 없음.
    bool isCompact() const noexcept { return m_compact; }
    void setCompact(bool compact);

private:
    Role m_role = Normal;
    bool m_compact = false;
};

} // namespace fm::ui
