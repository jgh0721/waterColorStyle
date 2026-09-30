# Qt Widgets Designer로 만든 CopyDetails.ui를 AUTOUIC(uic)로 불러 쓰는 예제.
# 플러그인이 없어도 빌드된다 — uic는 .ui의 <customwidgets>에 적힌 헤더만 본다.
qt_add_executable(fm_designer_example WIN32
    main.cpp
    CopyDetails.ui
)

target_link_libraries(fm_designer_example
    PRIVATE
        Fm::style
        Fm::widgets
)

fm_set_compile_options(fm_designer_example)
