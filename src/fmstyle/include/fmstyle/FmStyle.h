#pragma once

#include <QProxyStyle>

namespace fm::style {

class ThemeColors;

/// Fusion 위에 얹는 프록시 스타일. 색은 모두 테마 토큰(ThemeColors)에서 가져오고,
/// 크기 · 모서리 · 상태 표현은 디자인 목업을 따른다.
///
/// 직접 그리는 것: 버튼(보통 · 기본 · 위험 · 투명 · 세그먼트), 도구 버튼, 입력 · 콤보 · 스핀 상자,
/// 체크 상자 · 라디오 · 스위치, 탭(네 방향 · 닫기 단추), 머리글, 목록 선택 · 커서, 스크롤 막대, 진행 막대,
/// 슬라이더 · 다이얼, 도구 상자, 메뉴, 메뉴 막대, 도구 설명, 카드 · 그룹 상자, 분할선, MDI 제목 표시줄 ·
/// 단추 묶음, 창 단추 · 화살표 표준 아이콘. 나머지는 Fusion이 팔레트로 그린다.
class FmStyle : public QProxyStyle
{
    Q_OBJECT

public:
    FmStyle();
    ~FmStyle() override;

    /// 위젯에 적용할 시안1 색: 가장 가까운 ThemeScope, 없으면 ThemeManager의 현재 변형.
    /// 디자인과 상관없이 위젯이 직접 그릴 때는 fm::style::themeColorsFor()를 쓴다.
    static const ThemeColors &colorsFor(const QWidget *widget);

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
    QIcon standardIcon(StandardPixmap standardIcon, const QStyleOption *option = nullptr,
                       const QWidget *widget = nullptr) const override;

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
    void drawToolBoxTab(ControlElement element, const QStyleOption *option, QPainter *painter,
                        const QWidget *widget) const;
    void drawSlider(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    void drawDial(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    void drawTitleBar(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    void drawMdiControls(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;

    void setAltDown(bool down, QObject *source);

    bool m_alwaysMnemonics = false;
    bool m_altDown = false;
};

} // namespace fm::style
