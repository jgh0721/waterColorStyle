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

# QtitanDataGrid(Developer Machines 상용 컴포넌트)는 이 저장소에 없다. 라이선스가 있으면 원본 · 패치 · 빌드 연결을
# 담은 비공개 저장소를 third_party/QtitanDataGrid에 두거나 FMSTYLE_QTITAN_DIR로 위치를 알려 준다.
# 없으면 FMSTYLE_WITH_QTITAN이 OFF가 되고, Qtitan을 쓰는 보기(파일 목록 · 다중 이름 변경 미리보기 · 섬네일 카드)는
# 자리 표시만 한다. 코드는 FM_WITH_QTITAN(0 · 1, Fm::filelist의 공개 정의)으로 나눈다.
set(FMSTYLE_QTITAN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/QtitanDataGrid" CACHE PATH
    "QtitanDataGrid 비공개 저장소(원본 + 패치 + CMakeLists.txt) 위치")
if(EXISTS "${FMSTYLE_QTITAN_DIR}/CMakeLists.txt")
    set(_fm_qtitan_default ON)
else()
    set(_fm_qtitan_default OFF)
endif()
option(FMSTYLE_WITH_QTITAN "Build QtitanDataGrid (patched) and use it for the file list views" ${_fm_qtitan_default})
if(FMSTYLE_WITH_QTITAN)
    if(NOT EXISTS "${FMSTYLE_QTITAN_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "FMSTYLE_WITH_QTITAN=ON인데 QtitanDataGrid가 없습니다: ${FMSTYLE_QTITAN_DIR}\n"
                            "FMSTYLE_QTITAN_DIR로 위치를 알려 주거나 -DFMSTYLE_WITH_QTITAN=OFF로 빌드하세요.")
    endif()
    # Qtitan 정적 라이브러리는 Qt 비공개 모듈을 링크 의존으로 넘긴다. 가져온 대상은 디렉터리 범위라
    # 소비자(실행 파일 · 플러그인)가 있는 모든 디렉터리에서 보이도록 여기서 찾는다.
    # 비공개 API는 Qtitan 원본이 쓰는 것이므로 경고를 끈다(qmake 빌드의 CONFIG += private_headers와 같음).
    set(QT_NO_PRIVATE_MODULE_WARNING ON)
    find_package(Qt6 REQUIRED COMPONENTS PrintSupport Sql CorePrivate GuiPrivate WidgetsPrivate)
    add_subdirectory("${FMSTYLE_QTITAN_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/third_party/QtitanDataGrid")
endif()
message(STATUS "QtitanDataGrid: ${FMSTYLE_WITH_QTITAN}")

add_subdirectory(src/fmfilelist)
add_subdirectory(src/fmsettings)
add_subdirectory(src/fmdialogs)
add_subdirectory(app/filelist_lab)
add_subdirectory(app/fmdemo)
if(FMSTYLE_WITH_QTITAN)
    add_subdirectory(app/qtitan_spike)
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
