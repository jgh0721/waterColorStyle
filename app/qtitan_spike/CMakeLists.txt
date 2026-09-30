# P1 시험 앱 — 패치한 QtitanDataGrid에 FmStyle · WatercolorStyle이 적용되는 범위를 판정한다.
qt_add_executable(qtitan_spike WIN32
    main.cpp
)

target_link_libraries(qtitan_spike
    PRIVATE
        Fm::style
        Qtitan::Grid
)

fm_set_compile_options(qtitan_spike)
