# P3 파일 목록 실험실 — FileListView · 섬네일 두 구현 · 샘플/실제 폴더 원본 비교.
qt_add_executable(filelist_lab WIN32
    main.cpp
)

target_link_libraries(filelist_lab
    PRIVATE
        Fm::filelist
        Fm::widgets
        Fm::style
)

fm_set_compile_options(filelist_lab)
