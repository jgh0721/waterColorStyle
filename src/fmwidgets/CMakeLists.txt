qt_add_library(fmwidgets STATIC
    include/fmwidgets/Button.h
    include/fmwidgets/Card.h
    include/fmwidgets/ProgressBar.h
    include/fmwidgets/SegmentedControl.h
    include/fmwidgets/Switch.h
    include/fmwidgets/TransferGraph.h
    Controls.cpp
    SegmentedControl.cpp
    TransferGraph.cpp
)

add_library(Fm::widgets ALIAS fmwidgets)

target_include_directories(fmwidgets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fmwidgets
    PUBLIC
        Qt6::Widgets
    PRIVATE
        Fm::style
)

target_compile_options(fmwidgets
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4 /permissive- /Zc:__cplusplus>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

# The static library is also linked into the Designer plugin (a shared library).
set_target_properties(fmwidgets PROPERTIES POSITION_INDEPENDENT_CODE ON)
