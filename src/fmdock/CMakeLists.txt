qt_add_library(fmdock STATIC
    include/fmdock/DockManager.h
    DockManager_p.h
    AutoHide.cpp
    DockDrag.cpp
    DockManager.cpp
    DockTitleBar.cpp
)

add_library(Fm::dock ALIAS fmdock)

target_include_directories(fmdock
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 모양은 fm::style(두 디자인)이 그린다 — 끌어 놓기 표시 · 사이드바 바탕은 StylePaint 도우미.
target_link_libraries(fmdock
    PUBLIC
        Qt6::Widgets
    PRIVATE
        Fm::style
)

fm_set_compile_options(fmdock)
