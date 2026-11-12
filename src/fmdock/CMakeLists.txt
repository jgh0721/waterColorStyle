# fmdock 2 — 컨테이너 · 분할 · 영역(탭 묶음) · 떠 있는 창 · 자동 숨김을 직접 관리하는 도킹(docs/specs/07-docking.md).
qt_add_library(fmdock STATIC
    include/fmdock/DockArea.h
    include/fmdock/DockContainer.h
    include/fmdock/DockGlobals.h
    include/fmdock/DockManager.h
    include/fmdock/DockWidget.h
    include/fmdock/FloatingWindow.h
    DockInternal_p.h
    DropResolver_p.h
    StateCodec_p.h
    AutoHide.cpp
    DockArea.cpp
    DockCommon.cpp
    DockContainer.cpp
    DockManager.cpp
    DockTabBar.cpp
    DockWidget.cpp
    DragDrop.cpp
    DropResolver.cpp
    FloatingWindow.cpp
    StateCodec.cpp
)

add_library(Fm::dock ALIAS fmdock)

target_include_directories(fmdock
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 모양은 fm::style(세 디자인)이 그린다 — 탭 · 단추 · 표시 · 제목 줄은 QStyle 요소와 StylePaint 도우미.
# 메뉴는 fm::ui::Menu(Fm::widgets).
target_link_libraries(fmdock
    PUBLIC
        Qt6::Widgets
    PRIVATE
        Fm::style
        Fm::widgets
)
if(WIN32)
    # 떠 있는 창 — 프레임 없는 창의 DWM 그림자 · 스냅(07 §4.5)
    target_link_libraries(fmdock PRIVATE dwmapi)
endif()

fm_set_compile_options(fmdock)
