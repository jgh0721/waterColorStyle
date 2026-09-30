#pragma once

// FmStyle이 읽는 동적 속성. 위젯 서브클래스를 만들지 않고 목업의 변형(기본 버튼, 스위치,
// 세그먼트, 카드 등)을 표시하려고 쓴다. 이름을 직접 쓰기보다 아래 도우미 함수를 권장.

#include <QList>
#include <QString>

class QAbstractButton;
class QWidget;

namespace fm::style {

namespace props {
inline constexpr char kRole[] = "fmRole";                  // "primary" | "danger" | "subtle"
inline constexpr char kSize[] = "fmSize";                  // "small"
inline constexpr char kSwitch[] = "fmSwitch";              // bool — QCheckBox를 스위치로
inline constexpr char kSegment[] = "fmSegment";            // "first" | "middle" | "last" | "only"
inline constexpr char kCard[] = "fmCard";                  // bool — QFrame을 카드로
inline constexpr char kPaneActive[] = "fmPaneActive";      // bool — 활성 패널 (탭 줄 · 목록)
inline constexpr char kProgressState[] = "fmProgress";     // "paused" | "error"
inline constexpr char kPreviewState[] = "fmPreviewState";  // "hover,pressed,focus,on" — 미리보기 전용
} // namespace props

enum class ButtonRole { Normal, Primary, Danger, Subtle };

/// 기본(강조색) · 위험 · 투명 버튼. QPushButton, QToolButton에 쓴다.
void setButtonRole(QWidget *button, ButtonRole role);
ButtonRole buttonRole(const QWidget *button);

/// 작은 버튼 (시안1 높이 30 px · 여백 12 px, 시안2 24 px · 8 px). 시안2의 진행 막대에 쓰면 12 px로 얇아진다.
void setSmall(QWidget *widget, bool enabled = true);

/// QCheckBox를 스위치(시안1 40 × 20, 시안2 52 × 22)로 그린다. 글자는 스위치 왼쪽.
void setSwitch(QWidget *checkBox, bool on = true);

/// 버튼들을 이어 붙인 세그먼트 컨트롤로 만든다. 레이아웃 간격은 0, 배타 선택은
/// QButtonGroup으로 호출하는 쪽에서 묶는다.
void setSegments(const QList<QAbstractButton *> &buttons);

/// QFrame을 카드(목록 바탕 + 구분선 테두리, 시안1 모서리 6 px · 시안2 네모)로.
void setCard(QWidget *frame, bool on = true);

/// 듀얼 패널에서 활성 패널 표시. 탭 줄은 선택 탭 위에 강조색 선, 목록은 선택 색이 달라진다.
/// 지정하지 않으면 목록은 포커스를 기준으로 판단한다.
void setPaneActive(QWidget *widget, bool active);

/// 진행 막대 상태 — 빈 문자열(보통), "paused", "error".
void setProgressState(QWidget *progressBar, const QString &state);

/// 미리보기 · 갤러리 전용. 마우스 올림, 누름, 키보드 포커스 등을 강제로 그린다.
void setPreviewState(QWidget *widget, const QString &states);

} // namespace fm::style
