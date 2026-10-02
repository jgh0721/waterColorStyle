# 파일 관리자 UI 데모 — 메인 창(P4) · 설정(P7) · 대화상자 카탈로그 · 섬네일 비교 창 · 일괄 스냅숏(P9, PLAN §9).
find_package(Qt6 REQUIRED COMPONENTS Network)  # 창 하나만 실행(QLocalServer)
qt_add_executable(fmdemo WIN32
    CatalogWindow.cpp
    CatalogWindow.h
    FilePanel.cpp
    FilePanel.h
    MainWindow.cpp
    MainWindow.h
    SingleInstance.cpp
    SingleInstance.h
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
        Qt6::Network
)

fm_set_compile_options(fmdemo)

# 섬네일 비교 창의 메모리 측정(GetProcessMemoryInfo) · 화면 배율 설정(GetDpiForMonitor)
if(WIN32)
    target_link_libraries(fmdemo PRIVATE psapi shcore)
endif()
