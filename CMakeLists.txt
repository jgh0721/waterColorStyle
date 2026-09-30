cmake_minimum_required(VERSION 3.21)

project(FmStyle
    VERSION 0.1.0
    DESCRIPTION "File manager style: QProxyStyle over Fusion driven by theme tokens"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 6.4 is the minimum only because the Linux test environment ships it.
# The product targets Qt 6.11; 6.6+ (QPalette::Accent) and 6.8+ (QStyleHints::setColorScheme)
# features are used behind QT_VERSION checks.
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_subdirectory(src/fmstyle)
add_subdirectory(src/fmwidgets)

option(FMSTYLE_BUILD_GALLERY "Build the widget gallery used to compare against the mockups" ON)
if(FMSTYLE_BUILD_GALLERY)
    add_subdirectory(gallery)
endif()

# Qt Widgets Designer plugin — puts the fm::ui widgets in Designer's widget box.
# Build it in Release with the same Qt version as the Designer that will load it.
option(FMSTYLE_BUILD_DESIGNER_PLUGIN "Build the Qt Widgets Designer plugin" ON)
if(FMSTYLE_BUILD_DESIGNER_PLUGIN)
    add_subdirectory(src/designer)
endif()

option(FMSTYLE_BUILD_EXAMPLES "Build the example that uses a Designer-made .ui form" ON)
if(FMSTYLE_BUILD_EXAMPLES)
    add_subdirectory(examples/designer)
endif()
