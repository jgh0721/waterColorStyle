# FmStyle

파일 관리자용 Qt 스타일(Fusion 기반 QProxyStyle)과 위젯 모음. 디자인 두 가지를 같은 위젯 · 같은 토큰으로 쓴다.

| 디자인 | 스타일 클래스 | 캔버스 |
|---|---|---|
| 시안1 · 기본 (`Design::Standard`) | `fm::style::FmStyle` | 파일 관리자 UI |
| 시안2 · 워터컬러 (`Design::Watercolor`) | `fm::style::WatercolorStyle` | 파일 관리자 UI(워터컬러) |

| 폴더 | 내용 |
|---|---|
| `src/fmstyle` | `Fm::style` — FmStyle, WatercolorStyle, 테마 토큰, ThemeManager, ThemeScope |
| `src/fmwidgets` | `Fm::widgets` — 버튼 · 스위치 · 카드 등 기본 부품, 대화상자 · 설정 · 메인 창 부품, 떠 있는 알림(`Toast`), 진행 고리 · 도구 설명 · 메뉴 · 메뉴 막대 · 내용 대화상자, 웹 배치(`FlowLayout` · `FlexLayout`) |
| `src/fmfilelist` | `Fm::filelist` — 파일 목록(Qtitan 1줄 · 2줄 밴드 보기), 섬네일(Qt 목록 · Qtitan 카드 두 구현), 모델 · 원본. Qtitan 없이 빌드하면 파일 목록은 자리 표시, 섬네일은 Qt 목록만 |
| `src/fmdialogs` | `Fm::dialogs` — 파일 작업 · 관리자 권한 대화상자, 설정 창, 대화상자 변형 카탈로그 |
| `src/fmsettings` | `Fm::settings` — 설정 모델 · 보관소(JSON) |
| `src/fmdock` | `Fm::dock` — 도킹 관리자(`DockManager`): QMainWindow · QDockWidget 위에 끌어 놓기 표시 · 자동 숨김 사이드바 · 이름 붙인 배치 등을 얹는다(아래 [도크](#도크--fmdock)) |
| `src/designer` | Qt Widgets Designer 플러그인 (`fmdesignerplugin`) |
| `app/fmdemo` | 데모 앱 — 메인 창 · 도크(폴더 트리 · 미리보기 · 속성 · 작업 대기열) · 대화상자 · 설정 · 도구 창(카탈로그 · 섬네일 비교) · 일괄 스냅숏 |
| `third_party/QtitanDataGrid` | 저장소에 없음 — QtitanDataGrid 비공개 저장소를 두는 자리(아래 [QtitanDataGrid](#qtitandatagrid--상용-컴포넌트-선택)) |
| `examples/designer` | Designer로 만든 `CopyDetails.ui` · `LayoutDemo.ui`(배치 상자 · 승격한 메뉴 막대)를 uic로 불러 쓰는 예제 |
| `gallery` | 목업과 비교하는 위젯 갤러리 |
| `tests` | Qt Test — 파일 목록 · 메인 창 · 대화상자 · 설정 · Designer 플러그인 · 도크 · Qt 표준 위젯 스타일 · 웹 배치 · 떠 있는 위젯 |

## 빌드

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt 키트 경로>
cmake --build build
```

옵션: `FMSTYLE_BUILD_GALLERY`, `FMSTYLE_BUILD_EXAMPLES`, `FMSTYLE_BUILD_DESIGNER_PLUGIN`, `FMSTYLE_BUILD_TESTS`(모두 기본 ON),
`FMSTYLE_WITH_QTITAN`(QtitanDataGrid가 있으면 ON — 아래 절). 플러그인은 Qt6::Designer · Qt6::UiPlugin이 있어야 빌드된다.

Windows에서는 `tools\build.cmd debug|debug-noqtitan|release`가 VS 2026 개발자 환경을 불러 프리셋(`CMakePresets.json`)으로
빌드하고, `tools\run.cmd <exe> [인수…]`가 Qt 키트의 `bin`을 PATH 앞에 두고 실행한다(개발용 — 배포는 windeployqt).
테스트는 `ctest --test-dir build\debug`로 돌린다(offscreen 플랫폼 · 글꼴 폴더는 CMake가 넣는다).

### QtitanDataGrid — 상용 컴포넌트, 선택

파일 목록(`FileListView`) · 다중 이름 변경 미리보기(`RenamePreviewView`) · 섬네일 카드 구현(`ThumbnailCardView`)은
Developer Machines의 상용 컴포넌트 QtitanDataGrid(9.2.0, 패치 적용)를 쓴다. 라이선스 조건 때문에 원본과 패치는 이 저장소에 없다.

- **없을 때(기본)** — `FMSTYLE_WITH_QTITAN=OFF`. 위 세 보기는 안내 문구만 보이는 자리 표시로 빌드되고, 섬네일은 Qt 목록 구현만 쓴다.
  나머지(스타일 · 위젯 · 대화상자 · 설정 · 데모 앱 · Designer 플러그인 · 테스트)는 그대로 빌드되고 돈다.
  Qtitan에 기대는 테스트 몇 개는 건너뛴다(`QSKIP`).
- **있을 때** — 원본(`upstream/<버전>`) · 패치(`patches/`) · `CMakeLists.txt`를 담은 비공개 저장소를 `third_party/QtitanDataGrid`에
  두면 CMake가 찾아 `FMSTYLE_WITH_QTITAN=ON`으로 빌드한다. 다른 곳에 두었으면 `-DFMSTYLE_QTITAN_DIR=<경로>`로 알려 준다.
  그 폴더는 `.gitignore`에 있어 이 저장소에 올라가지 않는다.

코드는 `FM_WITH_QTITAN`(0 · 1)으로 나눈다. CMake가 `Fm::filelist`의 공개 정의로 넣으므로 이 라이브러리를 쓰는 대상은 모두 같은 값을 본다.
Qtitan 쪽 코드는 `#if FM_WITH_QTITAN … #else(자리 표시) … #endif` 안에 있다.

## 데모 앱 — fmdemo

메인 창(두 패널 · 탭 · 주소 줄 · 명령줄 · 기능 키), 파일 작업 · 관리자 권한 대화상자, 설정 창(9페이지)을 묶은 데모다.
실제 폴더는 읽기만 하고, 복사 · 이동 · 삭제는 진행 창과 권한 흐름의 시뮬레이션이다.

```
tools\run.cmd build\release\fmdemo.exe --design watercolor --scheme navy
```

**도구** 메뉴의 비교 도구:

| 메뉴 | 내용 |
|---|---|
| 대화상자 카탈로그 (Ctrl+Shift+G) | 대화상자 변형 76개를 묶음별 트리로 보이고, 고른 변형을 목업 보드의 상태로 연다(모덜리스, 여러 개 동시). 오른쪽의 디자인 · 색 구성표 · 다크 색조를 바꾸면 열린 대화상자에도 바로 반영된다. 출처 "제안"은 목업에 없는 상태를 보이려고 더한 변형이다 |
| 섬네일 비교 — Qt 목록 · Qtitan 카드 | 같은 모델을 두 섬네일 구현(QListView IconMode + 델리게이트, Qtitan CardGrid + 카드 그리기 훅)에 물려 좌우에 보인다. 원본은 샘플 · 모의 부하 1만 · 10만 개 · 실제 폴더. 모의 부하는 섬네일이 16 ms마다 400개씩 도착하는 상황을 흉내 낸다. 모델 연결 시간(배치 + 첫 그리기) · 메모리 증가 · 스크롤 40단계의 다시 그리기 시간을 잰다. Qtitan 없이 빌드하면 오른쪽(카드)은 안내 문구 |

명령줄:

| 옵션 | 내용 |
|---|---|
| `--design standard\|watercolor` · `--scheme system\|light\|dark\|navy` | 시작 디자인 · 색 구성표 |
| `--left-mode` · `--right-mode 1\|2\|auto\|thumb` · `--active left\|right` | 패널 보기 방식 · 활성 패널 |
| `--sep1` · `--sep2` · `--name-below` · `--inv-cursor` · `--inv-sel` · `--size WxH` | 행 구분 방식 · 2줄 레코드의 이름 위치 · 역상 커서 · 역상 선택 · 창 크기(기본 1440x900) |
| `--settings <file>` | 설정 파일(JSON). 주지 않으면 `%APPDATA%\FM Tools\settings.json`, `--shot` · `--open`이면 메모리에만 둔다 |
| `--open <id>` · `--list-dialogs` | 대화상자 변형 하나를 연다 · 변형 ID 목록을 출력한다 |
| `--flow copy\|delete` | 관리자 권한 흐름 시뮬레이션을 시작한다 |
| `--catalog` · `--compare sample\|10000\|100000\|<폴더>` | 도구 창을 바로 연다 |
| `--measure` | `--compare`와 함께 — 도착 흉내를 끄고 연결 · 스크롤을 재어 결과를 출력한 뒤 끝낸다 |
| `--shot <file.png>` · `--shot-delay <ms>` | 지금 화면(`--open`이면 그 대화상자, `--catalog` · `--compare`면 그 창)을 찍고 끝낸다 |
| `--shot <폴더>` · `--only <ids>` · `--themes <list>` | 일괄 스냅숏(아래) |
| `--docks` | 도크 넷을 연 채로 시작한다(아래) |
| `[폴더]` | 그 실제 폴더를 새 탭으로 연다. 설정 "창을 하나만 실행"(기본 켬)이면 이미 뜬 창에 넘기고 끝난다 |

### 도크

**보기 › 도크**에서 켜고 끈다. 처음에는 모두 닫혀 있어 메인 창은 보드 그대로다. 도크는 두 패널 영역에만 붙는다
(아래 도크도 명령줄 · 기능 키 막대 위).

| 도크 | 내용 |
|---|---|
| 폴더 트리(왼쪽) | 이 PC의 드라이브 · 폴더(폴더만). 누르면 활성 패널이 그 폴더로 가고, 활성 패널이 실제 폴더면 그 자리를 따라 펼친다 |
| 미리보기(오른쪽) | 커서 항목 — 실제 그림 · 글 파일 앞부분, 샘플은 가짜 섬네일 · 종류 아이콘, 아래에 이름과 정보 줄 |
| 속성(오른쪽, 미리보기와 탭 묶음) | 이름 · 종류 · 크기(바이트) · 날짜 · 속성 · 위치 · 원본, 실제 그림은 픽셀 크기 |
| 작업 대기열(아래) | 진행 창마다 한 줄 — 작업 · 진행 막대(일시 중지 색) · 상태. 두 번 누르면 그 진행 창을 앞으로 |

제목 줄을 끌면 끌어 놓기 표시가 나오고(가장자리 넷 · 도크 십자, Esc 취소), 압정으로 사이드바에 접는다.
배치는 **보기 › 도크 › 배치**로 이름 붙여 저장하고, 지금 배치와 저장한 배치는 끝낼 때 설정 파일(`session.docks` ·
`session.dockLayouts`)에 남아 다음 시작에 되살아난다(시작할 때 설정과 무관).

### 설정의 반영

설정 창에서 적용하면 바로 반영되고(화면 배율 · 언어는 다시 시작한 뒤), 설정 파일은 `%APPDATA%\FM Tools\settings.json`이다.
`--shot` · `--open`은 설정 파일을 읽지 않고 목업 보드 상태로 뜬다.

| 설정 | 앱에서 |
|---|---|
| 일반 · 모양 | 디자인 · 색 구성표 · 다크 색조, 목록 글꼴 · 행 밀도(파일 목록), 고정폭 글꼴(명령줄 · 경로 · 식), 시작할 때(마지막 탭 복원 · 홈 · 지정한 폴더), 창 하나만 실행, 알림 영역 아이콘, 화면 배율, 언어(Qt 표준 문자열), 명령줄 · 기능 키 막대, 폴더 탭(새 탭 위치 · 탭 이름 · 탭마다 기억) |
| 파일 패널 · 섬네일 보기 | 표시 방식 · 구분 · 역상 · 커서, 크기 단위 · 날짜 형식, 숨김 · 보호된 OS 파일, 섬네일 모양 · 만드는 방법 · 대상 · 동시 개수 · 자동 섬네일 |
| 파일 그룹 · 열 | 행 글자색 · 배경 · 글꼴 효과, 폴더마다 열 세트 자동 적용(보기 › 열 세트 바꾸기 Ctrl+Shift+C로 탭마다 고름) |
| 파일 작업 · 관리자 권한 · 키보드 | 대화상자 처음 값, 진행 창(표시 지연 · 자세히 · 완료되면 닫기 · 동시 작업 수 · 대기열 · 완료 알림 · Esc), 기본 삭제 방식 · 삭제 전 확인, 권한 흐름(사전 확인 · UAC 대기 · 도우미 수명 · 소유권 창), 단축키 · 기능 키 막대 글자 |

### 일괄 스냅숏

```
tools\run.cmd build\release\fmdemo.exe --shot shots
tools\run.cmd build\release\fmdemo.exe --shot shots --only main,copy,settings.keys --themes std-light,wc-navy
```

- 메인 창(보드 기본 상태, 1440 × 900), 도크 넷을 연 메인 창(`main.docks` — 제안), 대화상자 변형 76개, 모두 78화면을
  테마 5개(`std-light` · `std-dark` · `wc-light` · `wc-dark` · `wc-navy`)로 찍는다. 화면에 띄우지 않고 1배율로 그리므로
  화면 배율과 관계없이 목업 보드의 CSS 픽셀과 같은 크기가 된다.
- 결과: `<폴더>/<테마>/<id>.png`(클라이언트 영역), `<폴더>/<테마>/framed/<id>.png`(목업식 제목 표시줄 틀 —
  시안1은 36 px 제목(메인 32) · 모서리 8, 시안2는 27 px 그라데이션 제목 · 3 px 틀), `<폴더>/index.html`
  (화면 × 테마 표, 틀 켜기 · 끄기와 미리보기 폭 조절, 구현 크기와 목업 크기 비교 — 다르면 빨간색).
- `--only`는 화면 ID 접두어(쉼표 구분, `main` = 메인 창 둘), `--themes`는 테마 ID다.
  전체 780장은 debug 빌드에서 약 3분 걸린다.

## Qt Widgets Designer 플러그인

위젯 상자에 **FmStyle — …** 묶음 다섯 개로 50개 위젯이 들어간다. 프로젝트의 모든 .ui(설정 9페이지 · 파일 작업 대화상자 ·
예제)가 쓰는 위젯을 모두 포함하므로, 어느 .ui든 Designer에서 열면 앱과 같은 모양으로 보인다.

| 묶음 | 위젯 |
|---|---|
| 기본 | `Button`, `Switch`, `SegmentedControl`, `Card`(컨테이너), `ProgressBar`, `ProgressRing`, `TransferGraph`, `BarListCard`, `LaneLadder`, `PairedTimeline`, `Label`, `Tag`, `KeyChip`, `Banner`, `FlowBox`(컨테이너), `FlexBox`(컨테이너) |
| 대화상자 | `DialogHeader`, `DialogFooter`(컨테이너), `PathEdit`, `RecentTargetsBar`, `ChipButton`, `ChoiceCard`, `TokenButton`, `FileSummaryList`, `FolderPlanView`, `KeyValueCard`, `ItemListCard`, `ActionCard`, `OptionRadio` |
| 설정 | `SettingRow`(컨테이너 — 넣은 위젯은 오른쪽 컨트롤 칸으로 옮겨진다), `SearchField`, `ThemeModeCard`, `ColorSwatchButton`, `AccentPicker`, `HexColorEdit`, `ToggleChip`, `ColorPickButton`, `CheckListCombo`, `KeyCaptureEdit` |
| 메인 창 | `CommandLine`, `FunctionKeyBar`, `FunctionKeyButton`, `FindBox`, `BreadcrumbBar`, `DriveButton`, `PanelStatusBar`, `PanelTabStrip` |
| 승격 대상 | `MenuBar`(← QMenuBar), `Menu`(← QMenu) — 위젯 상자가 아니라 메뉴 막대 · 메뉴를 오른쪽 클릭 → **승격 대상**에 바로 보인다 |
| 파일 목록 | `fm::filelist::FileListView`, `fm::filelist::ThumbnailView`, `fm::dialogs::RenamePreviewView` — Qtitan을 정적 링크하므로 DLL을 더 배포하지 않는다(Qtitan 없는 빌드는 자리 표시) |

표에서는 클래스 이름 앞의 `fm::ui::`를 줄였다. Designer 안에서는 목록 · 그래프 · 기능 키 막대 등에 예시 데이터를 넣어
모양을 볼 수 있게 한다(앱이 QUiLoader로 .ui를 읽을 때는 넣지 않는다).

### 배치 상자 — Designer에서 FlowLayout · FlexLayout 쓰기

Designer에는 **사용자 QLayout을 넣을 길이 없다**. 레이아웃 도구 모음은 가로 · 세로 · 격자 · 양식 · 분할기로 고정이고,
플러그인 인터페이스(QDesignerCustomWidgetInterface)는 위젯만 만든다. 그래서 레이아웃을 품은 컨테이너 위젯 둘을 등록했다.

- `FlowBox`(FlowLayout) · `FlexBox`(FlexLayout)를 놓고 그 안에 위젯을 끌어 넣으면 상자가 스스로 배치한다.
  Designer는 상자 안 배치를 "자기가 관리하지 않는 레이아웃"으로 보아 자유 배치처럼 다루고, 상자가 자리를 정한다.
- 상자 속성: `FlowBox` — `horizontalSpacing` · `verticalSpacing` · `lineAlignment` · `margin`,
  `FlexBox` — `direction` · `wrap` · `justifyContent` · `alignItems` · `alignContent` · `rowGap` · `columnGap` · `margin`.
- 항목별 flex 값은 자식 위젯의 **동적 속성**(속성 창 + 단추)으로 준다: `flexGrow` · `flexShrink`(실수),
  `flexBasis` · `flexOrder`(정수), `flexAlignSelf`(문자열 start · end · center · stretch).
- 차례: 자식을 다른 자식 위로 끌어 놓으면 그 자리로 옮겨지고(되돌리기 가능), 차례는 `itemOrder`(자식 이름 목록)로 .ui에 남는다.
- 상자에 Designer 레이아웃(가로 · 세로 · 격자)을 걸지 않는다. 상자 바깥(상자를 놓는 부모)에는 평소처럼 건다.
- 디자인 중에는 상자에 점선 테두리가 보이고(실행 중에는 없음), uic · QUiLoader 모두 같은 배치를 만든다
  (예제 `examples/designer/LayoutDemo.ui`, `fm_designer_example --layouts`).

### 승격 — 메뉴 · 메뉴 막대

`fm::ui::MenuBar` · `fm::ui::Menu`(그림자 판 · 글리프 아이콘)는 위젯 상자에 넣지 않는다 — Designer가 메뉴 막대 · 메뉴를
자기 편집기로 다루기 때문이다. 대신 플러그인이 둘을 **승격 대상으로 미리 등록**해 두므로, 메뉴 막대나 메뉴를 오른쪽 클릭 →
승격 대상 → `fm::ui::MenuBar` / `fm::ui::Menu`를 고르면 된다(헤더를 손으로 적지 않는다). 메뉴는 Designer에서 그대로 편집하고,
uic가 만든 앱에서는 fm::ui 클래스로 만들어진다(QUiLoader는 기반 클래스 QMenuBar · QMenu로 만든다 — 모양은 스타일이 같게 그린다).
등록은 Designer 비공개 API(Qt6::DesignerPrivate)를 쓰며, 그 모듈이 없는 설치에서는 등록만 건너뛴다(승격 대화상자에서 직접 추가).

`ToolTip` · `Toast`(떠 있는 창) · `ContentDialog`(대화상자 자체)는 놓는 위젯이 아니라 등록하지 않는다. 도구 설명은
위젯의 toolTip 속성을 Designer에서 적고 앱에서 `ToolTip::installGlobal(app)`을 부르면 이 모양으로 뜬다. ContentDialog의 내용은
보통 위젯 폼으로 만들어 `setContentWidget`으로 넣는다.

### 미리보기 디자인 · 변형

플러그인 위젯을 **오른쪽 클릭 → 미리보기 디자인 / 미리보기 변형**에서 시안1 · 시안2와 Designer 밝기 따라가기 · 라이트 ·
다크 · 남색(시안2)을 고른다. 열려 있는 모든 미리보기에 바로 반영되고, 고른 값은 다음 실행에도 남는다.
처음 값은 환경 변수로도 줄 수 있다 — `FMSTYLE_DESIGN=watercolor`, `FMSTYLE_VARIANT=light|dark|navy`(있으면 저장값보다 우선).
스타일 · 색은 미리보기 위젯(과 그 안에 넣은 위젯)에만 걸며 .ui에 저장되지 않는다.

### 설치

플러그인은 **그것을 읽을 Designer와 같은 Qt 버전 · 같은 컴파일러 · Release**로 빌드해야 한다.
(Windows Qt 키트의 designer.exe는 Release 빌드라 Debug 플러그인은 읽지 않는다.)

- 설치: `cmake --install build --prefix <Qt 키트 경로>` → `<Qt 키트>/plugins/designer/fmdesignerplugin.dll`
- 설치 없이 써 보기: 환경 변수 `QT_PLUGIN_PATH=<build>/plugins` 로 Designer를 띄운다.
- 확인: Designer의 *Help → About Plugins*에 `fmdesignerplugin`이 보이면 된다.
- Qt Creator에 내장된 디자이너는 **Qt Creator를 빌드한 Qt 버전**과 맞아야 읽는다. 버전이 다르면
  Qt 키트의 designer.exe를 따로 쓴다.

### 앱에서 쓰기

.ui 파일에는 `<customwidgets>`로 클래스 이름과 헤더(`fmwidgets/Button.h` 등)만 적힌다.
앱은 `Fm::widgets`를 링크하고 AUTOUIC(또는 uic)로 .ui를 넣으면 된다 — 실행에는 플러그인이 필요 없다.

```cmake
qt_add_executable(app main.cpp CopyDetails.ui)
target_link_libraries(app PRIVATE Fm::style Fm::widgets)
```

앱에서는
`ThemeManager::instance().install(app)`이 스타일과 팔레트를 적용한다.

## 시안2 · 워터컬러

```cpp
auto &theme = fm::style::ThemeManager::instance();
theme.setDesign(fm::style::Design::Watercolor);  // install() 전에 부르면 처음부터 워터컬러
theme.install(app);
// 실행 중에도 theme.setDesign(...)으로 바꿀 수 있다 — 앱 스타일 · 팔레트 · 제목 표시줄이 같이 바뀐다.
```

- **색**: 51개 토큰은 이름이 같고 값만 워터컬러 값이다 (`builtinColor(token, variant, Design::Watercolor)`).
  버튼 빗면 · 제목 표시줄 · 탭 같은 입체 색은 `WatercolorChrome`(`watercolorChrome(variant)`)에 고정 값으로 있다.
  기준 색(강조색)은 두 디자인에 같이 적용되고, 직접 지정 값은 디자인마다 따로 둔다
  (`setOverride(Design, Variant, Token, color)`). 색 구성표는 시안1과 같이 시스템 · 라이트 · 다크.
- **크기** (캔버스 시안2의 CSS 값):

  | 컨트롤 | 크기 |
  |---|---|
  | 누름 단추 | 높이 27 · 좌우 12 · 최소 폭 80, 작게 24 · 8 |
  | 입력 · 콤보 · 스핀 상자 | 높이 25 · 안쪽 단추 16 |
  | 체크 상자 · 라디오 | 13 × 13, 글자와 7 |
  | 스위치 | 52 × 22, 글자와 10 |
  | 토글 단추 묶음(세그먼트) | 높이 24, 이웃과 테두리 1 px 공유 |
  | 탭 | 23 (선택 26) |
  | 목록 머리글 · 행 | 22 · 21 |
  | 스크롤 막대 | 17, 양 끝 화살표 단추 |
  | 진행 막대 | 16 (compact 12), 7 px 블록 + 2 px 틈 |
  | 메뉴 항목 · 메뉴 막대 | 24 · 22 |
  | MDI 창 제목 표시줄 | 27, 모자이크 + 21 × 20 단추 |

- **시안1과 다르게 움직이는 곳**: 기본 역할 버튼뿐 아니라 Enter 기본 단추에도 검은 테두리가 한 겹 더 붙는다 ·
  누르면 글자가 1 px 내려간다 · 활성 패널의 선택 행은 흰 글자, 커서는 점선이고 비활성 패널에는 커서가 없다 ·
  편집할 수 없는 콤보 상자에 포커스가 있으면 글자를 선택 색으로 칠한다 · 도구 설명은 노란 칸.
- **제목 표시줄**: 최상위 창의 제목 표시줄은 운영체제가 그린다. Windows 11(22000+)에서는 ThemeManager가
  창 틀과 같은 파란색 + 흰 글자로 칠한다 (`setColoredTitleBar(false)`로 끔). Windows 10은 시스템 기본 색.
  캔버스의 그라데이션 · 모자이크 제목 표시줄은 QMdiSubWindow에서 스타일이 그린다.
- **위젯**: `fm::ui::*` 위젯은 두 디자인에서 그대로 쓴다. 속도 그래프도 워터컬러에서는 모서리가 네모나고
  도구 설명이 노란 칸이 된다. 위젯이 직접 그릴 때 색은 `fm::style::themeColorsFor(widget)`로 얻는다.
- **Qt 표준 위젯**: 목업에 없는 기본 위젯도 두 디자인이 직접 그린다 — 탭 네 방향 · 닫기 단추, 슬라이더 ·
  다이얼, 도구 상자, 달력(내비게이션 줄 · 주말 글자), MDI 제목 표시줄 · 메뉴 막대 단추 묶음, 단색 표준 아이콘
  (화살표 · 창 단추 · 도구 모음 확장 · 새로 고침 — 그릴 때 테마 색을 정해 다크 · 실행 중 전환을 따른다),
  항목 보기(마우스 올림 · 끌어 놓기 표시 · 칸 편집기 테두리 · 가운데 맞춤 머리글의 정렬 표시 · 열 보기 화살표와
  손잡이, 시안2 트리 점선). 선택한 항목의 아이콘은 강조색으로 물들이지 않는다.
  입력 · 단추의 Qt 기능도 두 디자인이 그린다 — 틀 없는 콤보 · 스핀 상자, 읽기 전용 입력 바탕(시안1 흐린 바탕 · 밑줄 없음,
  시안2 XP처럼 창 바탕), 스핀 상자 ± 기호, 납작한 누름 단추, 켠 채 사용 안 함인 토글, 도구 단추 메뉴(분할 단추는 두 칸 ·
  구분선, 바로 · 지연 메뉴는 오른쪽 꺾쇠), 그룹 상자 제목 정렬 · 납작(선만), 콤보 펼친 목록(시안1 메뉴처럼 둥근 겹침,
  시안2 XP 드롭다운, 구분선 폭 전체). 항목 대리자가 그리는 진행 막대도 `styleObject`의 `fmProgress`로 일시 중지 · 오류 색을 낸다.
  시안1은 Windows 11 컨트롤, 시안2는 XP 컨트롤을 토큰 색으로 옮겼다. 컬러 표준 아이콘 등은 아직 Fusion이 팔레트로 그린다.
- **도크(QDockWidget)**: 제목 줄 · 단추 · 떠 있는 창 틀 · 분할선을 두 디자인이 그린다. 시안1은 `--win` 바탕 · 12 px 굵은 글자 ·
  단추 22, 활성 도크는 위쪽 2 px 강조선. 시안2는 MDI 제목처럼 파란 그라데이션 · 흰 굵은 글자 · 캡션 단추 16, 떠 있는 창은
  4 px 창 틀, 분할선은 마우스를 올리면 잡이 점. 세로 제목 줄(`DockWidgetVerticalTitleBar`)도 같다.
- **확인**: `fmstyle_gallery --design watercolor` (창 위쪽 단추로 시안1 · 시안2 전환, 맨 아래 Qt 표준 위젯 구역),
  `fmstyle_gallery --qt-widgets` (Qt 표준 위젯 구역만), `fmstyle_gallery --controls` (입력 · 단추 · 묶음 · 글자 구역만),
  `fmstyle_gallery --docks` (도크 · 도킹 관리자 구역만), `fmstyle_gallery --layouts` (배치 비교 구역만),
  `fmstyle_gallery --extras` (진행 고리 · 도구 설명 · 메뉴 · 내용 대화상자 구역만),
  `fm_designer_example --watercolor [--dark]`.

## 도크 — fmdock

`Fm::dock`의 `fm::dock::DockManager`는 QMainWindow · QDockWidget을 그대로 쓰면서 KDDockWidgets의 기능을 옮겨 얹는다
(코드는 가져오지 않고 동작을 다시 구현 — KDDockWidgets는 GPL). 모양은 `Fm::style`의 두 디자인이 그린다.

```cpp
auto *docks = new fm::dock::DockManager(mainWindow);
QDockWidget *tree = docks->addDock(u"folders"_s, u"폴더"_s, new QTreeView, Qt::LeftDockWidgetArea);
docks->addDock(u"log"_s, u"로그"_s, new QPlainTextEdit, Qt::BottomDockWidgetArea);
docks->setAutoHidden(docks->dock(u"log"_s), true);   // 아래 사이드바 탭으로 접기
docks->populateMenu(viewMenu->addMenu(u"도크"_s));   // 켜기 · 끄기 + 배치 저장 · 적용 · 삭제
settings.setValue("docks", docks->saveState());       // restoreState로 되살린다(자동 숨김 포함)
```

| 기능 | 내용 |
|---|---|
| 제목 줄 | 압정(자동 숨김) · 떼어 내기 · 닫기 단추, 두 번 눌러 떼기 · 붙이기, 포커스가 들어 있는 도크를 활성으로 강조 |
| 끌어 놓기 | 메인 창 가장자리 넷 + 마우스 아래 도크의 십자(왼 · 위 · 오른 · 아래 · 탭), 놓일 자리 미리보기, Esc 취소, 표시 밖에 놓으면 떠 있는 창. 마우스 없이 `dropOnto` · `dropToEdge` |
| 자동 숨김 | 창 가장자리 사이드바의 탭, 누르면 내용 위로 펼침(바깥을 누르거나 Esc면 접힘), 펼친 창의 안쪽 가장자리로 폭 조절 |
| 도크 탭 | 탭 묶음에 닫기 단추, 활성 도크 탭 강조 |
| 상태 · 배치 | `saveState` · `restoreState`(판이 다르면 거절), 이름 붙인 배치(`saveLayout` · `applyLayout` · `layouts`) |

## 진행 고리 · 도구 설명 · 메뉴 · 내용 대화상자

`Fm::widgets`의 네 가지는 두 디자인에 맞춰 그린다(ElaWidgetTools의 ElaProgressRing · ElaToolTip · ElaMenu ·
ElaMenuBar · ElaContentDialog와 Windows 11 컨트롤을 참고). `fmstyle_gallery --extras`가 모두 보인다.

```cpp
auto *ring = new fm::ui::ProgressRing;          // 값 · 백분율, setBusy(true) 또는 범위 0–0이면 도는 호
ring->setValue(72);

fm::ui::ToolTip::attach(copyButton, u"선택한 항목을 복사합니다"_s, u"복사"_s, fm::ui::glyph::Copy);
fm::ui::ToolTip::installGlobal(app);            // 앱 전체의 toolTip() · Qt::ToolTipRole을 이 모양으로

auto *bar = new fm::ui::MenuBar;                // 하위 메뉴는 fm::ui::Menu(그림자 판)
fm::ui::Menu *edit = bar->addMenu(u"편집(&E)"_s);
edit->addAction(fm::ui::glyph::Copy, u"복사"_s, QKeySequence(u"F5"_s));

const auto r = fm::ui::ContentDialog::ask(this, u"파일 3개를 영구 삭제할까요?"_s, u"되돌릴 수 없습니다."_s,
                                          u"영구 삭제"_s, u"휴지통으로"_s, u"취소"_s);
if (r == fm::ui::ContentDialog::Primary) { /* … */ }
```

| 위젯 | 시안1 (Windows 11) | 시안2 (XP) |
|---|---|---|
| `ProgressRing` | 옅은 홈 고리 + 둥근 끝 강조색 호, 바쁨은 늘었다 줄며 도는 호 | 들어간 홈 고리 + XP 진행 막대 칸(관 모양 그라데이션), 바쁨은 밝은 칸 셋이 돈다 |
| `ToolTip` | Surface · 1 px 선 · 모서리 6 · 부드러운 그림자, 꼬리(선택) | 노란 칸 · 오른쪽 아래 그림자, 제목 · 꼬리가 있으면 둥근 풍선 도움말 |
| `Menu` · `MenuBar` | 둥근 판 · 그림자, 막대 항목에 아이콘 + 글 | 네모 판 · menuLine 테두리 · 오른쪽 아래 그림자, 강조 바탕이면 아이콘도 흰색 |
| `ContentDialog` | 부모 창을 덮는 층(30 %) · 둥근 카드 · 20 px 제목 · 단추 줄(Win 바탕, 같은 폭, 기본 단추만 강조색) | 스타일이 그린 파란 제목 표시줄(닫기 · 끌어 옮기기) · 창 틀 · 오른쪽 단추(기본 단추 검은 테두리) |

- `ProgressRing` — `minimum` · `maximum` · `value`, `busy`, `textVisible` · `valueDisplay`(Percent · Actual),
  `state`(Normal · Paused · Error), `trackVisible`, `thickness`. 정사각형 안에 그리고(heightForWidth), 숨으면 움직임을 멈춘다.
- `ToolTip` — 대상에 붙이면 대상의 도움말 이벤트에 뜨고 마우스가 떠나거나 누르면 숨는다(Qt 기본 설명은 뜨지 않음).
  `placement`(Cursor · Below · Above · Left · Right, 자리가 없으면 반대쪽), `tailVisible`, `title` · `glyph`(풍선),
  `setCustomWidget`, `showDelay` · `hideDelay` · `duration`, `maximumTextWidth`(넘으면 줄 바꿈). `showText`는 QToolTip::showText처럼.
- `Menu` — `addAction(glyph, 글, 단축키)` · `addMenu(…)`(fm::ui::Menu), `itemHeight`, `animated`(시스템 메뉴 효과가 꺼져 있을 때 흐려짐).
  그림자는 창 안 여백에 그리고 띄울 때 그만큼 옮겨 판이 QMenu가 정한 자리에 온다. 스타일은 `fmOwnPanel`이면 판을 건너뛴다.
- `ContentDialog` — `title` · `text` 또는 `setContentWidget`, `primaryButtonText` · `secondaryButtonText` · `closeButtonText`
  (빈 단추는 숨김), `defaultButton`, `smokeVisible`. `exec()` = `Result`(None · Primary · Secondary, Esc · 닫기 = None),
  신호 `primaryButtonClicked` 등. 시안1은 열 때 167 ms · 닫을 때 120 ms 흐려짐.

## 웹 배치 — FlowLayout · FlexLayout

`Fm::widgets`의 두 QLayout은 웹 문서의 배치 방식을 Qt로 옮겼다. QBoxLayout · QGridLayout처럼 아무 위젯에나 건다.

```cpp
// 태그 · 칩: 제 크기로 놓다가 줄이 차면 다음 줄
auto *tags = new fm::ui::FlowLayout(6, 6, panel);   // 가로 · 세로 간격
tags->setLineAlignment(Qt::AlignLeft | Qt::AlignVCenter);
for (const QString &name : names)
    tags->addWidget(new fm::ui::Tag(name, fm::ui::Tag::Info));

// 도구 줄: 찾기 칸이 남는 폭을 다 받고, 좁아지면 다음 줄로
auto *bar = new fm::ui::FlexLayout(fm::ui::FlexLayout::Direction::Row, toolbar);
bar->setWrap(fm::ui::FlexLayout::Wrap::Wrap);
bar->setAlignItems(fm::ui::FlexLayout::Align::Center);
bar->setGap(8);
bar->addWidget(backButton);
bar->addWidget(findBox, /*grow*/ 1, /*shrink*/ 1, /*basis*/ 200);
bar->addWidget(settingsButton);
```

| | 흐름 `FlowLayout` | 유연 상자 `FlexLayout` | 격자 `QGridLayout` (Qt) |
|---|---|---|---|
| 웹에서 | 인라인 흐름(글줄) | CSS flexbox | CSS grid(일부) |
| 축 | 1차원 — 줄 단위 | 1차원 — 주 축(가로 · 세로 · 거꾸로), 줄 바꿈하면 줄마다 따로 | 2차원 — 행과 열을 함께 |
| 항목 크기 | sizeHint 그대로(늘이지 않음) | 기준(basis 또는 sizeHint)에서 남는 · 모자란 길이를 grow · shrink로 나눔, 최소 · 최대 고정 | 열 폭 · 행 높이를 칸 sizeHint · 늘이기 비율(stretch)로 정함 |
| 줄 바꿈 | 늘 함 | 고를 수 있음(NoWrap · Wrap · WrapReverse) | 없음 — 좁으면 칸이 줄어든다 |
| 정렬 | 줄 정렬(앞 · 가운데 · 뒤 · 양쪽) + 줄 안 세로 정렬 | justify-content 여섯 · align-items · align-self · align-content | 칸 안 정렬(Qt::Alignment) |
| 같은 열 맞추기 | 안 됨 | 안 됨(줄마다 따로 나눔) | 됨 — 같은 열은 폭이, 같은 행은 높이가 같다 |
| 칸 합치기 · 순서 | — · 넣은 순서 | — · `order` | row · column span · 칸 좌표 |
| 높이 | 폭으로 정해짐(heightForWidth) | 가로 + 줄 바꿈이면 폭으로 정해짐 | 칸 높이의 합 |
| 쓰임 | 태그 · 칩 · 필터 단추 묶음 | 도구 줄 · 카드 줄 · 대화상자 단추 줄 · 반응형 양식 | 설정 양식(이름 · 값 열) · 표 모양 배치 |

`fmstyle_gallery --layouts`가 세 배치를 같은 항목 · 같은 폭으로 나란히 보인다.
