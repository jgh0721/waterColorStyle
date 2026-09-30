#pragma once

// FmStyle · WatercolorStyle이 같이 쓰는 도우미 — 동적 속성 읽기, 미리보기 상태, 활성 패널 판단.

#include "fmstyle/StyleProps.h"

#include <QAbstractScrollArea>
#include <QCheckBox>
#include <QRectF>
#include <QStringList>
#include <QStyle>
#include <QStyleOption>
#include <QVariant>
#include <QWidget>

namespace fm::style::detail {

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

} // namespace fm::style::detail
