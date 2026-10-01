# Fm::dialogs — 파일 작업 대화상자(.ui + 코드), 진행 창, 다중 이름 변경, 규칙 엔진.
qt_add_library(fmdialogs STATIC
    include/fmdialogs/DialogCatalog.h
    include/fmdialogs/ElevationDialog.h
    include/fmdialogs/ElevationFlow.h
    include/fmdialogs/ElevationPrompt.h
    include/fmdialogs/FileOpContext.h
    include/fmdialogs/FileOpDialogs.h
    include/fmdialogs/MultiRenameDialog.h
    include/fmdialogs/Planners.h
    include/fmdialogs/ProgressDialog.h
    include/fmdialogs/ProgressSimulator.h
    include/fmdialogs/RenameEngine.h
    include/fmdialogs/RenamePreview.h
    include/fmdialogs/SettingsDialog.h
    DialogCatalog.cpp
    ElevationDialog.cpp
    ElevationFlow.cpp
    ElevationPrompt.cpp
    FileOpContext.cpp
    FileOpDialogs.cpp
    MultiRenameDialog.cpp
    Planners.cpp
    ProgressDialog.cpp
    ProgressSimulator.cpp
    RenameEngine.cpp
    RenamePreview.cpp
    SettingsAppearancePage.cpp
    SettingsColumnsPage.cpp
    SettingsDialog.cpp
    SettingsElevationPage.cpp
    SettingsFileOpsPage.cpp
    SettingsGroupsPage.cpp
    SettingsKeysPage.cpp
    SettingsPages_p.h
    SettingsPanelPage.cpp
    SettingsThemePage.cpp
    SettingsThumbsPage.cpp
    ui/CopyDialog.ui
    ui/DeleteDialog.ui
    ui/MoveRenameDialog.ui
    ui/MultiRenameDialog.ui
    ui/NewFileDialog.ui
    ui/NewFolderDialog.ui
    ui/ProgressDialog.ui
    ui/SettingsAppearancePage.ui
    ui/SettingsColumnsPage.ui
    ui/SettingsElevationPage.ui
    ui/SettingsFileOpsPage.ui
    ui/SettingsGroupsPage.ui
    ui/SettingsKeysPage.ui
    ui/SettingsPanelPage.ui
    ui/SettingsThemePage.ui
    ui/SettingsThumbsPage.ui
)

add_library(Fm::dialogs ALIAS fmdialogs)

set_target_properties(fmdialogs PROPERTIES
    AUTOUIC ON
    AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

target_include_directories(fmdialogs
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fmdialogs
    PUBLIC
        Qt6::Widgets
        Fm::widgets
        Fm::filelist
        Fm::settings
    PRIVATE
        Qtitan::Grid
        Qt6::Svg
)

fm_set_compile_options(fmdialogs)
set_target_properties(fmdialogs PROPERTIES POSITION_INDEPENDENT_CODE ON)
