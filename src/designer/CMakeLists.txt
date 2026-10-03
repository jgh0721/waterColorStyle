find_package(Qt6 6.4 COMPONENTS UiPlugin Designer)
if(NOT Qt6UiPlugin_FOUND OR NOT Qt6Designer_FOUND)
    message(WARNING "Qt6::UiPlugin or Qt6::Designer not found — skipping the Qt Widgets Designer plugin")
    return()
endif()

# Designer loads MODULE libraries only; SHARED makes this a loadable plugin even with a static Qt.
qt_add_plugin(fmdesignerplugin SHARED)

target_sources(fmdesignerplugin
    PRIVATE
        DesignerPreview.cpp
        DesignerPreview.h
        FmDesignerPlugin.cpp
        FmDesignerPlugin.h
        WidgetCatalog.cpp
)

# Qt6::Designer — 위젯 오른쪽 클릭 메뉴(QDesignerTaskMenuExtension)로 미리보기 디자인 · 변형을 바꾼다.
target_link_libraries(fmdesignerplugin
    PRIVATE
        Qt6::UiPlugin
        Qt6::Designer
        Qt6::Widgets
        Fm::style
        Fm::widgets
)

# 파일 목록(Qtitan) · 다중 이름 변경 미리보기도 등록한다 — Qtitan을 정적 링크하므로 DLL을 더 배포하지 않는다.
# QtitanDataGrid 없이 빌드하면(FMSTYLE_WITH_QTITAN=OFF) 두 위젯은 자리 표시로 등록된다.
if(TARGET Fm::filelist AND TARGET Fm::dialogs)
    target_link_libraries(fmdesignerplugin PRIVATE Fm::filelist Fm::dialogs)
    target_compile_definitions(fmdesignerplugin PRIVATE FM_DESIGNER_WITH_FILELIST)
endif()

fm_set_compile_options(fmdesignerplugin)

# <build>/plugins/designer — set QT_PLUGIN_PATH=<build>/plugins to try it without installing.
# The generator expression stops multi-config generators from adding a Release/ subfolder.
set_target_properties(fmdesignerplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/plugins/designer>"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/plugins/designer>"
)

# cmake --install <build> --prefix <Qt kit>   →   <Qt kit>/plugins/designer
install(TARGETS fmdesignerplugin
    LIBRARY DESTINATION "${QT6_INSTALL_PLUGINS}/designer"
    RUNTIME DESTINATION "${QT6_INSTALL_PLUGINS}/designer"
)
