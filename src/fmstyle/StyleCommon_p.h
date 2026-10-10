#pragma once

// FmStyle · WatercolorStyle이 같이 쓰는 도우미 — 동적 속성 읽기, 미리보기 상태, 활성 패널 판단.

#include "fmstyle/StyleProps.h"

#include <QAbstractScrollArea>
#include <QRegularExpression>
#include <QCheckBox>
#include <QRectF>
#include <QStringList>
#include <QStyle>
#include <QStyleOption>
#include <QVariant>
#include <QWidget>

namespace fm::style::detail {

using namespace Qt::StringLiterals;

inline QString stringProp(const QWidget *w, const char *name)
{
    return w ? w->property(name).toString() : QString();
}

inline bool boolProp(const QWidget *w, const char *name)
{
    return w && w->property(name).toBool();
}

inline bool isSmall(const QWidget *w) { return stringProp(w, props::kSize) == u"small"; }
inline QString segmentOf(const QWidget *w) { return stringProp(w, props::kSegment); }
inline QString keyHintOf(const QWidget *w) { return stringProp(w, props::kKeyHint); }
inline bool isFooter(const QWidget *w) { return boolProp(w, props::kFooter); }

// 표 머리글 평면 — 머리글 자신 또는 그 표(부모)에 건 속성, 또는 대화상자 · 설정 밀도.
inline bool flatHeader(const QWidget *w)
{
    if (!w)
        return false;
    if (stringProp(w, props::kHeader) == u"flat" || stringProp(w->parentWidget(), props::kHeader) == u"flat")
        return true;
    return density(w) != Density::Normal;
}

// 오류 모양 — 위젯 또는 부모(콤보 · 스핀 상자 안의 입력)
inline bool isInvalid(const QWidget *w)
{
    return boolProp(w, props::kInvalid) || (w && boolProp(w->parentWidget(), props::kInvalid));
}

// 키 칩을 붙일 때의 단추 글자(니모닉 & 제외) 폭.
inline QString plainText(QString text)
{
    text.remove(QRegularExpression(u"&(?!&)"_s));
    return text.replace(u"&&"_s, u"&"_s);
}

inline bool isSwitch(const QWidget *w)
{
    return qobject_cast<const QCheckBox *>(w) && boolProp(w, props::kSwitch);
}

inline QStyle::State previewState(const QWidget *w)
{
    QStyle::State state;
    if (!w)
        return state;
    const QVariant value = w->property(props::kPreviewState);
    if (!value.isValid())
        return state;
    const QStringList parts = value.toString().split(u',', Qt::SkipEmptyParts);
    for (const QString &raw : parts) {
        const QString part = raw.trimmed();
        if (part == u"hover")
            state |= QStyle::State_MouseOver;
        else if (part == u"pressed")
            state |= QStyle::State_Sunken;
        else if (part == u"focus")
            state |= QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange;
        else if (part == u"on")
            state |= QStyle::State_On;
    }
    return state;
}

// 미리보기 상태는 위젯이 이번 그리기를 위해 만든 옵션 객체(상수가 아닌 지역 변수)에 더한다.
inline void applyPreviewState(const QStyleOption *option, const QWidget *w)
{
    const QStyle::State forced = previewState(w);
    if (forced.toInt() == 0)
        return;
    auto *mutableOption = const_cast<QStyleOption *>(option);
    mutableOption->state |= forced;
    if (forced & QStyle::State_Sunken) {
        if (auto *tb = qstyleoption_cast<QStyleOptionToolButton *>(mutableOption))
            tb->activeSubControls |= QStyle::SC_ToolButton;
    }
    // 슬라이더는 손잡이 위에 있을 때만 마우스 올림 · 누름 모양이다.
    if ((forced & (QStyle::State_MouseOver | QStyle::State_Sunken)) && w->inherits("QSlider")) {
        if (auto *sl = qstyleoption_cast<QStyleOptionSlider *>(mutableOption))
            sl->activeSubControls |= QStyle::SC_SliderHandle;
    }
}

// 그리기를 대신 맡은 숨은 위젯이면 보이는 가장 가까운 상위 위젯. Qtitan 그리드는 셀 · 행을 그릴 때
// 그리드의 자식인 숨은 대리 QTableView를 넘긴다(third_party/QtitanDataGrid 패치 Q1).
inline const QWidget *paintingWidget(const QWidget *w)
{
    while (w && w->isHidden() && !w->isWindow() && w->parentWidget())
        w = w->parentWidget();
    return w;
}

// 활성 패널 판단: 명시 속성(위젯 또는 상위 — 패널 컨테이너에 한 번 걸어 둘 수 있다) → 창 활성 + 포커스.
/// 위젯 또는 조상에 건 fmPaneActive(없으면 false) — 포커스로 짐작하지 않는다(탭 줄의 강조 띠).
inline bool paneActiveProperty(const QWidget *w)
{
    for (const QWidget *p = w; p; p = p->parentWidget()) {
        const QVariant v = p->property(props::kPaneActive);
        if (v.isValid())
            return v.toBool();
        if (p->isWindow())
            break;
    }
    return false;
}

inline bool paneActive(const QStyleOption *option, const QWidget *w)
{
    for (const QWidget *p = w; p; p = p->parentWidget()) {
        const QVariant v = p->property(props::kPaneActive);
        if (v.isValid())
            return v.toBool();
        if (p->isWindow())
            break;
    }
    if (!(option->state & QStyle::State_Active))
        return false;
    if (!w)
        return true;
    if (w->window() && w->window()->windowType() == Qt::Popup)
        return true;
    const QWidget *painting = paintingWidget(w);
    return painting->hasFocus() || painting->isAncestorOf(painting->window()->focusWidget());
}

inline QRectF crisp(const QRect &r, qreal width = 1.0)
{
    const qreal h = width / 2.0;
    return QRectF(r).adjusted(h, h, -h, -h);
}

inline bool keyboardFocus(QStyle::State s)
{
    return (s & QStyle::State_HasFocus) && (s & QStyle::State_KeyboardFocusChange);
}

inline const QAbstractScrollArea *scrollAreaOf(const QWidget *w)
{
    // 스크롤 막대 → 컨테이너 → 스크롤 영역
    for (int depth = 0; w && depth < 3; ++depth, w = w->parentWidget()) {
        if (auto *area = qobject_cast<const QAbstractScrollArea *>(w))
            return area;
    }
    return nullptr;
}

/// 단추 아이콘 · 글자 간격(목업 .btn gap 8 — Qt 기본은 4, docs/specs/03 §4.3).
inline constexpr int kButtonIconGap = 8;

/// 아이콘과 글자가 모두 있는 단추의 라벨을 "아이콘 + 8 + 글자"로 가운데에 그린다. 그렸으면 true.
/// 아이콘 모드는 Fusion 규칙과 같다(사용 불가 → Disabled, State_HasFocus → Active, State_On → On).
inline bool drawIconTextButtonLabel(const QStyle *style, const QStyleOptionButton &b, QPainter *p, const QWidget *w)
{
    if (b.icon.isNull() || b.text.isEmpty() || (b.features & QStyleOptionButton::HasMenu))
        return false;
    const bool enabled = b.state & QStyle::State_Enabled;
    const QIcon::Mode mode = !enabled ? QIcon::Disabled : (b.state & QStyle::State_HasFocus) ? QIcon::Active : QIcon::Normal;
    const QIcon::State state = (b.state & QStyle::State_On) ? QIcon::On : QIcon::Off;
    const bool mnemonic = style->styleHint(QStyle::SH_UnderlineShortcut, &b, w);
    const int textFlags = Qt::AlignLeft | Qt::AlignVCenter | (mnemonic ? Qt::TextShowMnemonic : Qt::TextHideMnemonic);
    const int textWidth = b.fontMetrics.size(Qt::TextShowMnemonic, b.text).width();
    const QSize icon = b.iconSize;
    const int total = icon.width() + kButtonIconGap + textWidth;
    const int left = b.rect.left() + std::max(0, (b.rect.width() - total) / 2);
    const QRect iconRect(left, b.rect.top() + (b.rect.height() - icon.height()) / 2, icon.width(), icon.height());
    b.icon.paint(p, iconRect, Qt::AlignCenter, mode, state);
    const QRect textRect(iconRect.right() + 1 + kButtonIconGap, b.rect.top(), b.rect.right() - iconRect.right() - kButtonIconGap,
                         b.rect.height());
    style->drawItemText(p, textRect, textFlags, b.palette, enabled, b.text, QPalette::ButtonText);
    return true;
}

} // namespace fm::style::detail
