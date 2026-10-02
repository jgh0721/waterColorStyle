# 파일 관리자 UI 데모 — 메인 창(P4) · 설정(P7) · 대화상자 카탈로그 · 섬네일 비교 창 · 일괄 스냅숏(P9, PLAN §9).
qt_add_executable(fmdemo WIN32
    CatalogWindow.cpp
    CatalogWindow.h
    FilePanel.cpp
    FilePanel.h
    MainWindow.cpp
    MainWindow.h
    Snapshots.cpp
    Snapshots.h
    ThumbnailCompare.cpp
    ThumbnailCompare.h
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

# 섬네일 비교 창의 메모리 측정(GetProcessMemoryInfo)
if(WIN32)
    target_link_libraries(fmdemo PRIVATE psapi)
endif()
