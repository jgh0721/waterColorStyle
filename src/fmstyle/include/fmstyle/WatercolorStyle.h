#pragma once

#include "fmstyle/WatercolorChrome.h"

#include <QProxyStyle>

namespace fm::style {

class ThemeColors;

/// 시안2 "파일 관리자 UI(워터컬러)"를 그리는 스타일. FmStyle과 같은 방식으로 Fusion 위에 얹고,
/// 색은 테마 토큰(워터컬러 값) + 워터컬러 입체 색(WatercolorChrome)에서 가져온다.
///
/// 직접 그리는 것: 입체 버튼(보통 · 기본 · 위험 · 투명 · 토글 묶음), 도구 버튼, 들어간 입력 · 콤보 · 스핀 상자,
/// 체크 상자 · 라디오 · 스위치, 탭, 머리글, 목록 선택 · 점선 커서, 화살표 단추가 있는 스크롤 막대,
/// 블록 진행 막대, 메뉴 · 메뉴 막대, 노란 도구 설명, 카드 · 그룹 상자 · 틀, 도구 모음, 상태 표시줄 칸,
/// MDI 제목 표시줄. 나머지는 Fusion이 팔레트로 그린다.
///
/// 최상위 창의 제목 표시줄은 운영체제가 그린다 — ThemeManager가 Windows 11에서 제목 색을 입힌다.
class WatercolorStyle : public QProxyStyle
{
    Q_OBJECT

public:
    WatercolorStyle();
    ~WatercolorStyle() override;

    /// 위젯에 적용할 워터컬러 색: 가장 가까운 ThemeScope, 없으면 ThemeManager의 현재 변형.
    static const ThemeColors &colorsFor(const QWidget *widget);
    /// 같은 변형의 입체 색.
    static const WatercolorChrome &chromeFor(const QWidget *widget);

    /// '액세스 키 밑줄 항상 표시'. 끄면 Windows처럼 Alt를 누르는 동안만 밑줄이 보인다.
    void setAlwaysShowMnemonics(bool on);
    bool alwaysShowMnemonics() const noexcept { return m_alwaysMnemonics; }

    void polish(QWidget *widget) override;
    void unpolish(QWidget *widget) override;
    void polish(QApplication *app) override;
    void unpolish(QApplication *app) override;
    using QProxyStyle::polish;
    using QProxyStyle::unpolish;

    QPalette standardPalette() const override;

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                            QPainter *painter, const QWidget *widget = nullptr) const override;

    QRect subElementRect(SubElement element, const QStyleOption *option,
                         const QWidget *widget = nullptr) const override;
    QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                         SubControl subControl, const QWidget *widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize,
                           const QWidget *widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void drawButtonPanel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawToolPanel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawSegment(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawFocus(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawTabShape(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawHeaderSection(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawMenuItem(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawMenuBarItem(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawProgress(ControlElement element, const QStyleOption *option, QPainter *painter,
                      const QWidget *widget) const;
    void drawShapedFrame(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawScrollBar(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    void drawTitleBar(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;

    void setAltDown(bool down, QObject *source);

    bool m_alwaysMnemonics = false;
    bool m_altDown = false;
};

} // namespace fm::style
