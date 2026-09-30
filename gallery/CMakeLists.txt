qt_add_executable(fmstyle_gallery WIN32
    main.cpp
)

target_link_libraries(fmstyle_gallery
    PRIVATE
        Fm::style
        Fm::widgets
)

target_compile_options(fmstyle_gallery
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4 /permissive- /Zc:__cplusplus>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)
