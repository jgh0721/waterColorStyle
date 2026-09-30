qt_add_library(fmstyle STATIC
    include/fmstyle/FmStyle.h
    include/fmstyle/StyleProps.h
    include/fmstyle/ThemeColors.h
    include/fmstyle/ThemeManager.h
    include/fmstyle/ThemeTokens.h
    include/fmstyle/WatercolorChrome.h
    include/fmstyle/WatercolorStyle.h
    ColorMath_p.h
    StyleCommon_p.h
    FmStyle.cpp
    StyleProps.cpp
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
        # DwmSetWindowAttribute (dark title bar)
        $<$<PLATFORM_ID:Windows>:dwmapi>
)

target_compile_options(fmstyle
    PRIVATE
        # Korean string literals are UTF-8; MSVC needs /utf-8 to read them correctly.
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4 /permissive- /Zc:__cplusplus>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

# The static library is also linked into the Designer plugin (a shared library).
set_target_properties(fmstyle PROPERTIES POSITION_INDEPENDENT_CODE ON)
