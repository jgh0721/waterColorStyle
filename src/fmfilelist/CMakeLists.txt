# Fm::filelist — 파일 목록 모델(샘플 · 읽기 전용 실제 폴더), FileListView(Qtitan 밴드 보기), 섬네일 두 구현.
qt_add_library(fmfilelist STATIC
    include/fmfilelist/ColumnSets.h
    include/fmfilelist/FileGroups.h
    include/fmfilelist/FileIconPainter.h
    include/fmfilelist/FileListModel.h
    include/fmfilelist/FileListStats.h
    include/fmfilelist/FileListView.h
    include/fmfilelist/FileRoles.h
    include/fmfilelist/FileSortProxy.h
    include/fmfilelist/ListAppearance.h
    include/fmfilelist/ListColumns.h
    include/fmfilelist/LocalFileSource.h
    include/fmfilelist/MockFileSource.h
    include/fmfilelist/ThumbnailPainter.h
    include/fmfilelist/ThumbnailProvider.h
    include/fmfilelist/ThumbnailView.h
    ColumnSets.cpp
    FileGroups.cpp
    FileIconPainter.cpp
    FileListModel.cpp
    FileListStats.cpp
    FileListView.cpp
    FileRoles.cpp
    FileSortProxy.cpp
    ListColumns.cpp
    ListPainting.cpp
    ListPainting_p.h
    LocalFileSource.cpp
    MockFileSource.cpp
    ThumbnailPainter.cpp
    ThumbnailProvider.cpp
    ThumbnailView.cpp
)

add_library(Fm::filelist ALIAS fmfilelist)

target_include_directories(fmfilelist
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fmfilelist
    PUBLIC
        Qt6::Widgets
        Fm::style
        Fm::widgets
    PRIVATE
        Qtitan::Grid
)

if(WIN32)
    # 셸 섬네일(IShellItemImageFactory)
    target_link_libraries(fmfilelist PRIVATE shell32 ole32)
endif()

fm_set_compile_options(fmfilelist)

# Designer 플러그인(공유 라이브러리)에도 링크한다.
set_target_properties(fmfilelist PROPERTIES POSITION_INDEPENDENT_CODE ON)
