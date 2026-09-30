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
# Svg: 목업 아이콘(fmstyle Glyphs)을 SVG 경로 그대로 그린다.
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Svg)
qt_standard_project_setup()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FmCompileOptions)

add_subdirectory(src/fmstyle)
add_subdirectory(src/fmwidgets)

# QtitanDataGrid — third_party/QtitanDataGrid/upstream 원본 + patches 를 정적 라이브러리로 빌드한다.
option(FMSTYLE_WITH_QTITAN "Build QtitanDataGrid (patched) and the targets that use it" ON)
if(FMSTYLE_WITH_QTITAN)
    # Qtitan 정적 라이브러리는 Qt 비공개 모듈을 링크 의존으로 넘긴다. 가져온 대상은 디렉터리 범위라
    # 소비자(실행 파일 · 플러그인)가 있는 모든 디렉터리에서 보이도록 여기서 찾는다.
    # 비공개 API는 Qtitan 원본이 쓰는 것이므로 경고를 끈다(qmake 빌드의 CONFIG += private_headers와 같음).
    set(QT_NO_PRIVATE_MODULE_WARNING ON)
    find_package(Qt6 REQUIRED COMPONENTS PrintSupport Sql CorePrivate GuiPrivate WidgetsPrivate)
    add_subdirectory(third_party/QtitanDataGrid)
    add_subdirectory(src/fmfilelist)
    add_subdirectory(src/fmdialogs)
    add_subdirectory(app/qtitan_spike)
    add_subdirectory(app/filelist_lab)
    add_subdirectory(app/fmdemo)
endif()

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

option(FMSTYLE_BUILD_TESTS "Build the Qt Test unit tests" ON)
if(FMSTYLE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
