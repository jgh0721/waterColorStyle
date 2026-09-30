#include "fmstyle/ThemeManager.h"

#include "fmstyle/FmStyle.h"
#include "fmstyle/WatercolorChrome.h"
#include "fmstyle/WatercolorStyle.h"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QHash>
#include <QSettings>
#include <QStyleHints>
#include <QWidget>

#include <memory>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <dwmapi.h>
#endif

using namespace Qt::StringLiterals;

namespace fm::style {

namespace {

constexpr int variantIndex(Variant v) { return v == Variant::Light ? 0 : 1; }
constexpr int designIndex(Design d) { return d == Design::Standard ? 0 : 1; }

struct ScopeEntry
{
    std::shared_ptr<const ThemeColors> colors;
    bool palette = true;
};

QHash<const QWidget *, ScopeEntry> &scopes()
{
    static QHash<const QWidget *, ScopeEntry> table;
    return table;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// ThemeManager

ThemeManager &ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

ThemeManager::ThemeManager()
{
    // 시스템 구성표는 QApplication이 있어야 읽을 수 있으므로 install()에서 정한다.
    rebuild();
}

void ThemeManager::install(QApplication &app)
{
    if (m_installed)
        return;
    m_installed = true;

    // 목업의 기본 글꼴: 13 px. 픽셀 크기는 장치 독립 픽셀이라 배율에 따라 커진다.
    QFont font = QApplication::font();
    font.setFamilies({u"Segoe UI Variable Text"_s, u"Segoe UI"_s, u"Malgun Gothic"_s,
                      u"Noto Sans CJK KR"_s});
    font.setPixelSize(13);
    QApplication::setFont(font);

    QApplication::setStyle(createStyle(m_design));  // QApplication이 소유
    app.installEventFilter(this);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) {
                if (m_scheme == Scheme::System)
                    apply();
            });
#endif
    apply();
}

void ThemeManager::setDesign(Design design)
{
    if (m_design == design)
        return;
    m_design = design;
    if (m_installed)
        QApplication::setStyle(createStyle(design));  // 이전 스타일은 QApplication이 지운다
    apply();
}

void ThemeManager::setScheme(Scheme scheme)
{
    if (m_scheme == scheme)
        return;
    m_scheme = scheme;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // 네이티브 대화상자와 제목 표시줄도 같은 구성표를 쓰게 한다.
    // 시스템으로 되돌릴 때 Unknown을 넣어야 colorScheme()이 다시 OS 값을 돌려준다.
    const Qt::ColorScheme qtScheme = scheme == Scheme::Light ? Qt::ColorScheme::Light
                                   : scheme == Scheme::Dark  ? Qt::ColorScheme::Dark
                                                             : Qt::ColorScheme::Unknown;
    QGuiApplication::styleHints()->setColorScheme(qtScheme);
#endif
    apply();
}

const ThemeColors &ThemeManager::colors(Design design, Variant variant) const noexcept
{
    return m_colors[designIndex(design)][variantIndex(variant)];
}

void ThemeManager::setSeeds(const ThemeSeeds &seeds)
{
    if (m_seeds == seeds)
        return;
    m_seeds = seeds;
    rebuild();
    apply();
}

void ThemeManager::setOverride(Variant variant, Token token, std::optional<QColor> color)
{
    setOverride(m_design, variant, token, std::move(color));
}

void ThemeManager::setOverride(Design design, Variant variant, Token token, std::optional<QColor> color)
{
    m_overrides[designIndex(design)][variantIndex(variant)][indexOf(token)] = std::move(color);
    rebuild();
    apply();
}

void ThemeManager::clearOverrides()
{
    for (auto &perDesign : m_overrides) {
        for (auto &table : perDesign)
            table = {};
    }
    rebuild();
    apply();
}

const TokenOverrides &ThemeManager::overrides(Design design, Variant variant) const noexcept
{
    return m_overrides[designIndex(design)][variantIndex(variant)];
}

void ThemeManager::setDarkTitleBar(bool on)
{
    if (m_darkTitleBar == on)
        return;
    m_darkTitleBar = on;
    if (m_installed) {
        const auto windows = QApplication::topLevelWidgets();
        for (QWidget *w : windows)
            applyTitleBar(w);
    }
}

void ThemeManager::setColoredTitleBar(bool on)
{
    if (m_coloredTitleBar == on)
        return;
    m_coloredTitleBar = on;
    if (m_installed) {
        const auto windows = QApplication::topLevelWidgets();
        for (QWidget *w : windows)
            applyTitleBar(w);
    }
}

void ThemeManager::rebuild()
{
    for (const Design d : {Design::Standard, Design::Watercolor}) {
        for (const Variant v : {Variant::Light, Variant::Dark})
            m_colors[designIndex(d)][variantIndex(v)] =
                deriveColors(v, m_seeds, m_overrides[designIndex(d)][variantIndex(v)], d);
    }
}

void ThemeManager::apply()
{
    m_effective = m_scheme == Scheme::Light ? Variant::Light
                : m_scheme == Scheme::Dark  ? Variant::Dark
                                            : systemVariant();
    if (m_installed) {
        QApplication::setPalette(colors().toPalette());
        const auto windows = QApplication::topLevelWidgets();
        for (QWidget *w : windows) {
            applyTitleBar(w);
            w->update();
        }
    }
    Q_EMIT changed();
}

Variant ThemeManager::systemVariant() const
{
    if (!qApp)
        return Variant::Light;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? Variant::Dark
                                                                                 : Variant::Light;
#elif defined(Q_OS_WIN)
    const QSettings personalize(
        u"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"_s,
        QSettings::NativeFormat);
    return personalize.value(u"AppsUseLightTheme"_s, 1).toInt() == 0 ? Variant::Dark
                                                                      : Variant::Light;
#else
    return Variant::Light;
#endif
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event)
{
    if ((event->type() == QEvent::Show || event->type() == QEvent::WinIdChange)
        && watched->isWidgetType()) {
        auto *w = static_cast<QWidget *>(watched);
        if (w->isWindow())
            applyTitleBar(w);
    }
    return false;
}

void ThemeManager::applyTitleBar(QWidget *window) const
{
#ifdef Q_OS_WIN
    if (!window || !window->isWindow())
        return;
    const Qt::WindowType type = window->windowType();
    // Qt::Desktop은 Qt 6에서 쓰이지 않는 값이라(C4996) 비교하지 않는다.
    if (type == Qt::Popup || type == Qt::ToolTip || type == Qt::SplashScreen)
        return;
    const WId id = window->internalWinId();  // 네이티브 창을 새로 만들지 않도록 winId() 대신
    if (!id)
        return;
    const HWND hwnd = reinterpret_cast<HWND>(id);
    constexpr DWORD kUseImmersiveDarkMode = 20;  // DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 20H1+)
    constexpr DWORD kBorderColor = 34;           // DWMWA_BORDER_COLOR   (Windows 11 22000+)
    constexpr DWORD kCaptionColor = 35;          // DWMWA_CAPTION_COLOR
    constexpr DWORD kTextColor = 36;             // DWMWA_TEXT_COLOR
    constexpr COLORREF kColorDefault = 0xFFFFFFFF;  // DWMWA_COLOR_DEFAULT — 시스템 색으로 되돌림

    // 워터컬러: 창 틀과 같은 파란색 제목 표시줄 + 흰 글자. 단추 기호도 흰색이 되도록 어두운 모드를 켠다.
    const bool tinted = m_design == Design::Watercolor && m_coloredTitleBar;
    const BOOL dark = (tinted || (m_effective == Variant::Dark && m_darkTitleBar)) ? TRUE : FALSE;
    ::DwmSetWindowAttribute(hwnd, kUseImmersiveDarkMode, &dark, sizeof(dark));

    COLORREF caption = kColorDefault;
    COLORREF text = kColorDefault;
    COLORREF border = kColorDefault;
    if (tinted) {
        const QColor frame = watercolorChrome(m_effective).frame;
        caption = border = RGB(frame.red(), frame.green(), frame.blue());
        text = RGB(0xFF, 0xFF, 0xFF);
    }
    // Windows 10에서는 실패하고 아무 일도 일어나지 않는다.
    ::DwmSetWindowAttribute(hwnd, kCaptionColor, &caption, sizeof(caption));
    ::DwmSetWindowAttribute(hwnd, kTextColor, &text, sizeof(text));
    ::DwmSetWindowAttribute(hwnd, kBorderColor, &border, sizeof(border));
#else
    Q_UNUSED(window)
#endif
}

// ---------------------------------------------------------------------------------------------

QStyle *createStyle(Design design)
{
    if (design == Design::Watercolor)
        return new WatercolorStyle;
    return new FmStyle;
}

const ThemeColors &themeColorsFor(const QWidget *widget)
{
    if (const ThemeColors *scoped = ThemeScope::find(widget))
        return *scoped;
    return ThemeManager::instance().colors();
}

// ---------------------------------------------------------------------------------------------
// ThemeScope

void ThemeScope::set(QWidget *root, const ThemeColors &colors, bool applyPalette)
{
    if (!root)
        return;
    auto &table = scopes();
    const bool fresh = !table.contains(root);
    table.insert(root, {std::make_shared<const ThemeColors>(colors), applyPalette});
    if (fresh)
        QObject::connect(root, &QObject::destroyed, [root] { scopes().remove(root); });
    if (applyPalette)
        root->setPalette(colors.toPalette());
    root->update();
}

void ThemeScope::clear(QWidget *root)
{
    auto &table = scopes();
    const auto it = table.find(root);
    if (it == table.end())
        return;
    const bool palette = it->palette;
    table.erase(it);
    if (palette)
        root->setPalette(QPalette());
    root->update();
}

const ThemeColors *ThemeScope::find(const QWidget *widget)
{
    const auto &table = scopes();
    if (table.isEmpty())
        return nullptr;
    for (const QWidget *w = widget; w; w = w->parentWidget()) {
        const auto it = table.constFind(w);
        if (it != table.constEnd())
            return it->colors.get();
    }
    return nullptr;
}

} // namespace fm::style
