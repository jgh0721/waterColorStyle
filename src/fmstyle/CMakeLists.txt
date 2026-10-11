qt_add_library(fmstyle STATIC
    include/fmstyle/ColorScheme.h
    include/fmstyle/FmStyle.h
    include/fmstyle/Glyphs.h
    include/fmstyle/StylePaint.h
    include/fmstyle/StyleProps.h
    include/fmstyle/ThemeColors.h
    include/fmstyle/ThemeManager.h
    include/fmstyle/ThemeTokens.h
    include/fmstyle/WatercolorChrome.h
    include/fmstyle/WatercolorStyle.h
    ColorMath_p.h
    StyleCommon_p.h
    StyleShared_p.h
    ColorScheme.cpp
    FmStyle.cpp
    Glyphs.cpp
    StylePaint.cpp
    StyleProps.cpp
    StyleShared.cpp
    ThemeColors.cpp
    ThemeManager.cpp
    ThemeTokens.cpp
    WatercolorChrome.cpp
    WatercolorStyle.cpp
)

add_library(Fm::style ALIAS fmstyle)

target_include_directories(fmstyle
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(fmstyle
    PUBLIC
        Qt6::Widgets
    PRIVATE
        Qt6::Svg
    PRIVATE
        # DwmSetWindowAttribute (dark title bar)
        $<$<PLATFORM_ID:Windows>:dwmapi>
)

fm_set_compile_options(fmstyle)

# The static library is also linked into the Designer plugin (a shared library).
set_target_properties(fmstyle PROPERTIES POSITION_INDEPENDENT_CODE ON)
