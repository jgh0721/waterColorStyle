# Fm::settings — 앱 설정(AppSettings) · JSON 저장(SettingsStore) · 테마 적용 · 명령 목록.
qt_add_library(fmsettings STATIC
    include/fmsettings/AppSettings.h
    include/fmsettings/Commands.h
    include/fmsettings/SettingsStore.h
    AppSettings.cpp
    Commands.cpp
    SettingsStore.cpp
)

add_library(Fm::settings ALIAS fmsettings)

target_include_directories(fmsettings
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fmsettings
    PUBLIC
        Qt6::Widgets
        Fm::style
        Fm::filelist
)

fm_set_compile_options(fmsettings)
set_target_properties(fmsettings PROPERTIES POSITION_INDEPENDENT_CODE ON)
