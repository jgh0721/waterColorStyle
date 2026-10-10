#pragma once

// FmStyle이 읽는 동적 속성. 위젯 서브클래스를 만들지 않고 목업의 변형(기본 버튼, 스위치,
// 세그먼트, 카드 등)을 표시하려고 쓴다. 이름을 직접 쓰기보다 아래 도우미 함수를 권장.

#include <QList>
#include <QString>

class QAbstractButton;
class QWidget;

namespace fm::style {

namespace props {
inline constexpr char kRole[] = "fmRole";                  // "primary" | "danger" | "subtle" | "link"
inline constexpr char kSize[] = "fmSize";                  // "small" | "mini" | "thin" | "thick"
inline constexpr char kSwitch[] = "fmSwitch";              // bool — QCheckBox를 스위치로
inline constexpr char kSegment[] = "fmSegment";            // "first" | "middle" | "last" | "only"
inline constexpr char kCard[] = "fmCard";                  // bool — QFrame을 카드로
inline constexpr char kPaneActive[] = "fmPaneActive";      // bool — 활성 패널 (탭 줄 · 목록)
inline constexpr char kProgressState[] = "fmProgress";     // "paused" | "error"
inline constexpr char kPreviewState[] = "fmPreviewState";  // "hover,pressed,focus,on" — 미리보기 전용
inline constexpr char kDensity[] = "fmDensity";            // "dialog" | "settings" — 상위 위젯에 한 번 건다
inline constexpr char kFooter[] = "fmFooter";              // bool — QFrame을 대화상자 버튼 영역으로
inline constexpr char kKeyHint[] = "fmKeyHint";            // "F2" — 단추 · 라디오 · 체크 글자 뒤 키 칩
inline constexpr char kHeader[] = "fmHeader";              // "flat" — 표 머리글을 평면으로(시안2 대화상자 · 설정 표)
inline constexpr char kInvalid[] = "fmInvalid";            // bool — 입력 · 콤보를 오류 모양으로
inline constexpr char kBusyPhase[] = "fmBusyPhase";        // qreal 0 ~ 1 — 불확정 진행 막대의 위치(fm::ui::ProgressBar가 움직임)
inline constexpr char kStyledFont[] = "fmStyledFont";      // bool — 스타일이 polish에서 준 글꼴(unpolish에서 되돌림)
inline constexpr char kDockButton[] = "fmDockButton";      // "close" | "float" | "pin" | "unpin" | "menu" — 도크 제목 줄 단추
inline constexpr char kDockExtraButtons[] = "fmDockExtraButtons";  // int — 사용자 제목 줄이 닫기 · 떼어 내기 밖에 둔 단추 수
} // namespace props

/// Link: 바탕 · 테두리 없는 글자 단추(강조 글자색) — 진행 창 '간단히 보기'.
enum class ButtonRole { Normal, Primary, Danger, Subtle, Link };

/// 크기 변형(fmSize). Small: 작은 단추 · 26 px 세그먼트 · 시안2 얇은 진행 막대. Mini: 20 px 세그먼트.
/// Thin · Thick: 진행 막대 4 · 8 px(시안1), 12 · 16 px(시안2).
/// Compact: 메인 창 주소 줄의 보기 세그먼트 — 시안1 28 · 좌우 10, 시안2 24.
enum class SizeVariant { Normal, Small, Mini, Thin, Thick, Compact };

/// 컨트롤 밀도(fmDensity). 창 · 대화상자 루트에 한 번 걸면 안의 컨트롤이 따른다.
/// Dialog: 파일 작업 대화상자(시안1 입력 32, 세그먼트 32, 작은 단추 28). Settings: 설정 창(입력 30, 세그먼트 30).
/// 두 밀도 모두 표 머리글은 28 px 평면. 시안2 입력 · 단추는 밀도와 상관없이 25 · 27.
enum class Density { Normal, Dialog, Settings };

/// 기본(강조색) · 위험 · 투명 버튼. QPushButton, QToolButton에 쓴다.
void setButtonRole(QWidget *button, ButtonRole role);
ButtonRole buttonRole(const QWidget *button);

/// 작은 버튼 (시안1 높이 30 px · 여백 12 px, 시안2 24 px · 8 px). 시안2의 진행 막대에 쓰면 12 px로 얇아진다.
void setSmall(QWidget *widget, bool enabled = true);

/// 크기 변형 — setSmall(true)는 SizeVariant::Small과 같다.
void setSizeVariant(QWidget *widget, SizeVariant variant);
SizeVariant sizeVariant(const QWidget *widget);

/// 밀도 — 위젯 자신 또는 가장 가까운 상위 위젯의 값.
void setDensity(QWidget *root, Density density);
Density density(const QWidget *widget);

/// QFrame을 대화상자 버튼 영역으로(시안1: --foot 바탕 + 위 1 px --line, 시안2: 선 없음).
void setFooter(QWidget *frame, bool on = true);

/// 단추 · 라디오 · 체크 글자 뒤에 키 칩(예: "F2", "Del"). 빈 문자열이면 없앤다.
void setKeyHint(QWidget *widget, const QString &keys);

/// 표 머리글을 평면(--head 바탕 · 아래 --line · --fg2 글자)으로. 표 또는 머리글에 건다.
void setFlatHeader(QWidget *viewOrHeader, bool on = true);

/// 입력 · 콤보 · 스핀 상자를 오류 모양(위험 테두리)으로.
void setInvalid(QWidget *widget, bool invalid = true);

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

/// 도크 제목 줄 단추 — kind = "close" · "float" · "pin"(자동 숨김으로) · "unpin"(도크로) · "menu".
/// 스타일이 단추 바탕과 기호를 함께 그린다(시안1 겹침 단추, 시안2 캡션 단추). QDockWidget 기본 단추는 스타일이 붙인다.
void setDockButton(QWidget *button, const QString &kind);

} // namespace fm::style
