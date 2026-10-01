# Qt Test — 규칙 엔진 · 배치 계산 · 모델. ctest --preset 대신 빌드 폴더에서 ctest 로 실행한다.
find_package(Qt6 REQUIRED COMPONENTS Test)

if(TARGET Fm::filelist)
    qt_add_executable(tst_filelist tst_filelist.cpp)
    target_link_libraries(tst_filelist PRIVATE Fm::filelist Qt6::Test)
    # 비공개 그리기 규칙(ListPainting_p.h)도 검사한다.
    target_include_directories(tst_filelist PRIVATE ${PROJECT_SOURCE_DIR}/src/fmfilelist)
    fm_set_compile_options(tst_filelist)
    add_test(NAME tst_filelist COMMAND tst_filelist)
    # 화면 없이 실행하고, 개발용 Qt 키트의 DLL을 찾게 한다.
    # offscreen 플러그인은 Qt 글꼴 폴더만 읽으므로 Windows 글꼴 폴더를 알려 준다(이름 폭 측정 · 자동 2줄 판정).
    get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qtbin "${_qmake}" DIRECTORY)
    set(_testEnv "QT_QPA_PLATFORM=offscreen" "PATH=${_qtbin}\;$ENV{PATH}")
    if(WIN32)
        list(APPEND _testEnv "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
    endif()
    set_tests_properties(tst_filelist PROPERTIES ENVIRONMENT "${_testEnv}")

    # 메인 창 — 데모 앱의 창 · 패널 소스를 그대로 넣어 검사한다.
    set(_demo ${PROJECT_SOURCE_DIR}/app/fmdemo)
    qt_add_executable(tst_mainwindow tst_mainwindow.cpp
        ${_demo}/FilePanel.cpp ${_demo}/FilePanel.h ${_demo}/MainWindow.cpp ${_demo}/MainWindow.h)
    target_include_directories(tst_mainwindow PRIVATE ${_demo})
    target_link_libraries(tst_mainwindow PRIVATE Fm::dialogs Fm::filelist Fm::widgets Qt6::Test)
    fm_set_compile_options(tst_mainwindow)
    add_test(NAME tst_mainwindow COMMAND tst_mainwindow)
    set_tests_properties(tst_mainwindow PROPERTIES ENVIRONMENT "${_testEnv}")
endif()

if(TARGET Fm::dialogs)
    # 파일 작업 대화상자 — 이름 변경 엔진 · 계획기 · 진행 시뮬레이터 · 변형 전체 생성
    qt_add_executable(tst_dialogs tst_dialogs.cpp)
    target_link_libraries(tst_dialogs PRIVATE Fm::dialogs Qt6::Test)
    fm_set_compile_options(tst_dialogs)
    add_test(NAME tst_dialogs COMMAND tst_dialogs)
    set_tests_properties(tst_dialogs PROPERTIES ENVIRONMENT "${_testEnv}")
endif()

if(TARGET Fm::settings)
    # 설정 모델 — 테마 파생 규칙 · 색 구성표 · AppSettings JSON · 파일 그룹 매처 · 명령 키
    qt_add_executable(tst_settings tst_settings.cpp)
    target_link_libraries(tst_settings PRIVATE Fm::settings Qt6::Test)
    fm_set_compile_options(tst_settings)
    add_test(NAME tst_settings COMMAND tst_settings)
    set_tests_properties(tst_settings PROPERTIES ENVIRONMENT "${_testEnv}")
endif()

if(TARGET fmdesignerplugin)
    # Designer 플러그인 — 적재 · 등록 정보 · 위젯 생성 · 모든 .ui를 QUiLoader로 열기
    find_package(Qt6 REQUIRED COMPONENTS UiTools UiPlugin)
    qt_add_executable(tst_designer tst_designer.cpp)
    target_link_libraries(tst_designer PRIVATE Qt6::Widgets Qt6::UiTools Qt6::UiPlugin Qt6::Test)
    target_compile_definitions(tst_designer PRIVATE
        FM_DESIGNER_PLUGIN_PATH="$<TARGET_FILE:fmdesignerplugin>"
        FM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_dependencies(tst_designer fmdesignerplugin)
    fm_set_compile_options(tst_designer)
    add_test(NAME tst_designer COMMAND tst_designer)
    set_tests_properties(tst_designer PROPERTIES ENVIRONMENT "${_testEnv}")
endif()
