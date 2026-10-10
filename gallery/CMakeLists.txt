qt_add_executable(fmstyle_gallery WIN32
    main.cpp
)

target_link_libraries(fmstyle_gallery
    PRIVATE
        Fm::style
        Fm::widgets
        Fm::dock
)

fm_set_compile_options(fmstyle_gallery)
