find_package(Qt6 6.4 COMPONENTS UiPlugin)
if(NOT Qt6UiPlugin_FOUND)
    message(WARNING "Qt6::UiPlugin not found — skipping the Qt Widgets Designer plugin")
    return()
endif()

# Designer loads MODULE libraries only; SHARED makes this a loadable plugin even with a static Qt.
qt_add_plugin(fmdesignerplugin SHARED)

target_sources(fmdesignerplugin
    PRIVATE
        FmDesignerPlugin.cpp
        FmDesignerPlugin.h
)

target_link_libraries(fmdesignerplugin
    PRIVATE
        Qt6::UiPlugin
        Qt6::Widgets
        Fm::style
        Fm::widgets
)

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
