#pragma once

// 배치 상태 JSON(07 §4.8). 쓰기 · 읽기 · 검증만 — 위젯에 반영하는 일은 DockManager가 한다.

#include "fmdock/DockGlobals.h"

#include <QByteArray>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>

#include <array>
#include <optional>

namespace fm::dock::detail {

struct LayoutNode
{
    bool split = false;                       // false = 영역
    Qt::Orientation orientation = Qt::Horizontal;
    QList<int> sizes;                         // 분할 — 자식 크기(px)
    QList<LayoutNode> children;               // 분할 — 자식
    bool central = false;                     // 중앙 영역(문서 영역)의 뿌리
    QStringList docks;                        // 영역 — 도크 id(탭 순서)
    QString current;                          // 영역 — 선택 탭

    bool isEmpty() const { return split ? children.isEmpty() : docks.isEmpty(); }
};

struct FloatingState
{
    QString screen;                           // QScreen::name()
    QRect rect;                               // 그 화면 왼쪽 위 기준 창 사각형(장치 독립 px)
    bool maximized = false;
    LayoutNode tree;
};

struct AutoHideState
{
    QString dock;
    int size = 0;
};

struct DockState
{
    int userVersion = 0;
    bool hasMain = false;
    LayoutNode main;
    QList<FloatingState> floating;
    std::array<QList<AutoHideState>, 4> autoHide;   // Left · Top · Right · Bottom
    QStringList closed;
    QStringList locked;                       // "closable" · "movable" · "floatable" · "pinnable"
    QString focus;
    QString expanded;                         // 펼친 자동 숨김 도크
};

/// 상태 형식 · 판.
inline constexpr char kStateFormat[] = "fmdock";
inline constexpr int kStateVersion = 2;

/// 검증 상한 — 신뢰하지 않는 입력으로 다룬다(07 §4.8-3).
struct StateLimits
{
    qsizetype maxBytes = 1024 * 1024;
    int maxDepth = 32;
    int maxNodes = 2000;
    int maxFloating = 32;
    int maxIdLength = 256;
};

QByteArray encodeState(const DockState &state);
/// 형식 · 판 · 사용자 판 · 상한이 맞지 않으면 nullopt(error에 이유).
std::optional<DockState> decodeState(const QByteArray &bytes, int userVersion, QString *error = nullptr,
                                     const StateLimits &limits = {});

/// 노드 안의 도크 id 전부(깊이 우선).
QStringList dockIds(const LayoutNode &node);

/// 상태의 사용자 판(JSON "user", 읽을 수 없으면 0) — 기본 배치 · 이름 붙인 배치를 다시 읽을 때.
int stateUserVersion(const QByteArray &bytes);

/// id가 든 영역 노드(main · floating 어디든). 없으면 nullptr.
const LayoutNode *findAreaNode(const DockState &state, const QString &id);

} // namespace fm::dock::detail
