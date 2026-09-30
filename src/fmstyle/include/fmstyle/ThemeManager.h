#pragma once

#include "fmstyle/ThemeColors.h"

#include <QObject>

class QApplication;
class QStyle;
class QWidget;

namespace fm::style {

/// 앱 전체 테마 상태: 디자인(시안1 · 시안2), 색 구성표(시스템/라이트/다크), 다크 색조(회색/남색), 기준 색, 직접 지정 값.
/// install() 한 번으로 스타일(FmStyle 또는 WatercolorStyle), 팔레트, 기본 글꼴을 적용한다.
class ThemeManager final : public QObject
{
    Q_OBJECT

public:
    enum class Scheme { System, Light, Dark };
    Q_ENUM(Scheme)
    /// 다크 계열일 때의 색조. 남색은 시안2(워터컬러)에서만 쓰이고 시안1은 회색(다크)을 쓴다.
    enum class DarkTone { Gray, Navy };
    Q_ENUM(DarkTone)

    static ThemeManager &instance();

    /// 스타일 설치 · 팔레트와 글꼴 적용 · 시스템 색 구성표 변경 감시.
    void install(QApplication &app);
    bool isInstalled() const noexcept { return m_installed; }

    /// 디자인. install() 전후 언제든 바꿀 수 있고, 설치된 뒤라면 앱 스타일을 바로 바꾼다.
    Design design() const noexcept { return m_design; }
    void setDesign(Design design);

    Scheme scheme() const noexcept { return m_scheme; }
    void setScheme(Scheme scheme);

    DarkTone darkTone() const noexcept { return m_darkTone; }
    void setDarkTone(DarkTone tone);

    /// 지금 화면에 쓰이는 변형 (시스템이면 OS 설정을 따른다). 다크 계열 + 시안2 + 남색 색조면 Navy.
    Variant effectiveVariant() const noexcept { return m_effective; }

    /// 지금 디자인 · 지금 변형의 색.
    const ThemeColors &colors() const noexcept { return colors(m_design, m_effective); }
    /// 지금 디자인의 색.
    const ThemeColors &colors(Variant variant) const noexcept { return colors(m_design, variant); }
    /// 디자인을 지정한 색 — 두 디자인의 색은 늘 함께 계산해 둔다 (나란히 보기 · 미리보기용).
    const ThemeColors &colors(Design design, Variant variant) const noexcept;

    /// 기준 색은 두 디자인에 같이 적용된다.
    ThemeSeeds seeds() const { return m_seeds; }
    void setSeeds(const ThemeSeeds &seeds);

    /// 토큰 하나를 직접 지정한다. std::nullopt 이면 자동(파생 규칙)으로 되돌린다.
    /// 디자인을 주지 않으면 지금 디자인에 적용한다.
    void setOverride(Variant variant, Token token, std::optional<QColor> color);
    void setOverride(Design design, Variant variant, Token token, std::optional<QColor> color);
    void clearOverrides();
    const TokenOverrides &overrides(Variant variant) const noexcept { return overrides(m_design, variant); }
    const TokenOverrides &overrides(Design design, Variant variant) const noexcept;

    /// '다크 테마에서 제목 표시줄도 어둡게' (Windows 전용, 다른 OS에서는 무시).
    bool darkTitleBar() const noexcept { return m_darkTitleBar; }
    void setDarkTitleBar(bool on);

    /// 워터컬러에서 제목 표시줄을 파란 제목 색으로 칠한다 (Windows 11 22000+, 그 밖에서는 무시).
    bool coloredTitleBar() const noexcept { return m_coloredTitleBar; }
    void setColoredTitleBar(bool on);

    /// '액세스 키 밑줄 항상 표시'. 디자인을 바꾸면 스타일 객체가 새로 만들어지므로 여기서 보관하고 다시 건다.
    bool alwaysShowMnemonics() const noexcept { return m_alwaysMnemonics; }
    void setAlwaysShowMnemonics(bool on);

Q_SIGNALS:
    void changed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    ThemeManager();
    Q_DISABLE_COPY_MOVE(ThemeManager)

    void rebuild();
    void apply();
    Variant systemVariant() const;
    QStyle *makeStyle() const;
    void applyTitleBar(QWidget *window) const;

    Design m_design = Design::Standard;
    Scheme m_scheme = Scheme::System;
    Variant m_effective = Variant::Light;
    ThemeSeeds m_seeds;
    DarkTone m_darkTone = DarkTone::Gray;
    TokenOverrides m_overrides[2][kVariantCount];  // [디자인][변형] — 시안1의 남색 칸은 쓰지 않는다(다크 칸으로 대체)
    ThemeColors m_colors[2][kVariantCount];
    bool m_darkTitleBar = true;
    bool m_coloredTitleBar = true;
    bool m_alwaysMnemonics = false;
    bool m_installed = false;
};

/// 디자인에 맞는 스타일을 새로 만든다 (호출한 쪽이 소유).
QStyle *createStyle(Design design);

/// 위젯에 적용할 색: 가장 가까운 ThemeScope, 없으면 ThemeManager의 지금 색.
/// 스타일이 아닌 위젯(속도 그래프 등)이 직접 그릴 때 쓴다.
const ThemeColors &themeColorsFor(const QWidget *widget);

/// 위젯 트리 일부에만 다른 색 구성을 적용한다.
/// 테마 편집 화면의 미리보기, 갤러리의 라이트/다크 나란히 보기에 쓴다.
class ThemeScope
{
public:
    /// applyPalette가 false면 스타일이 쓰는 토큰만 바꾸고 팔레트는 건드리지 않는다
    /// (Qt Designer 플러그인처럼 팔레트가 저장되면 안 되는 곳).
    static void set(QWidget *root, const ThemeColors &colors, bool applyPalette = true);
    static void clear(QWidget *root);
    /// widget 또는 가장 가까운 상위 위젯에 걸린 색. 없으면 nullptr.
    static const ThemeColors *find(const QWidget *widget);
};

} // namespace fm::style
