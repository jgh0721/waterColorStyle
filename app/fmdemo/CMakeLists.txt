# 파일 관리자 UI 데모 — 메인 창(P4), 이후 대화상자 카탈로그 · 비교 도구가 더해진다(PLAN §9).
qt_add_executable(fmdemo WIN32
    FilePanel.cpp
    FilePanel.h
    MainWindow.cpp
    MainWindow.h
    main.cpp
)

target_link_libraries(fmdemo
    PRIVATE
        Fm::dialogs
        Fm::filelist
        Fm::widgets
        Fm::style
)

fm_set_compile_options(fmdemo)
