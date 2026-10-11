qt_add_library(fmwidgets STATIC
    include/fmwidgets/Banner.h
    include/fmwidgets/BarListCard.h
    include/fmwidgets/BreadcrumbBar.h
    include/fmwidgets/Button.h
    include/fmwidgets/Card.h
    include/fmwidgets/CommandLine.h
    include/fmwidgets/ContentDialog.h
    include/fmwidgets/DialogCards.h
    include/fmwidgets/DialogChrome.h
    include/fmwidgets/DialogFooter.h
    include/fmwidgets/DialogHeader.h
    include/fmwidgets/DialogWidgets.h
    include/fmwidgets/DriveButton.h
    include/fmwidgets/FindBox.h
    include/fmwidgets/FlexLayout.h
    include/fmwidgets/FlowLayout.h
    include/fmwidgets/FunctionKeyBar.h
    include/fmwidgets/Glyph.h
    include/fmwidgets/KeyChip.h
    include/fmwidgets/Label.h
    include/fmwidgets/LaneLadder.h
    include/fmwidgets/LayoutBoxes.h
    include/fmwidgets/Menu.h
    include/fmwidgets/PairedTimeline.h
    include/fmwidgets/PanelStatusBar.h
    include/fmwidgets/PanelTabStrip.h
    include/fmwidgets/ProgressBar.h
    include/fmwidgets/ProgressRing.h
    include/fmwidgets/SegmentedControl.h
    include/fmwidgets/SettingsWidgets.h
    include/fmwidgets/Switch.h
    include/fmwidgets/Tag.h
    include/fmwidgets/Toast.h
    include/fmwidgets/ToolTip.h
    include/fmwidgets/TransferGraph.h
    BarListCard.cpp
    ContentDialog.cpp
    Controls.cpp
    DialogCards.cpp
    DialogHeader.cpp
    DialogParts.cpp
    DialogWidgets.cpp
    FlexLayout.cpp
    FlowLayout.cpp
    Label.cpp
    LaneLadder.cpp
    LayoutBoxes.cpp
    MainBars.cpp
    Menu.cpp
    PairedTimeline.cpp
    PanelChrome.cpp
    ProgressRing.cpp
    SegmentedControl.cpp
    SettingsWidgets.cpp
    Toast.cpp
    ToolTip.cpp
    TransferGraph.cpp
)

add_library(Fm::widgets ALIAS fmwidgets)

target_include_directories(fmwidgets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 공개 헤더가 fmstyle 헤더(Glyphs.h 등)를 쓰므로 Fm::style을 공개로 링크한다.
target_link_libraries(fmwidgets
    PUBLIC
        Qt6::Widgets
        Fm::style
)

fm_set_compile_options(fmwidgets)

# The static library is also linked into the Designer plugin (a shared library).
set_target_properties(fmwidgets PROPERTIES POSITION_INDEPENDENT_CODE ON)
