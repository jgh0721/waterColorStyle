# FmStyle

파일 관리자용 Qt 스타일(Fusion 기반 QProxyStyle)과 위젯 모음. 디자인 두 가지를 같은 위젯 · 같은 토큰으로 쓴다.

| 디자인 | 스타일 클래스 | 캔버스 |
|---|---|---|
| 시안1 · 기본 (`Design::Standard`) | `fm::style::FmStyle` | 파일 관리자 UI |
| 시안2 · 워터컬러 (`Design::Watercolor`) | `fm::style::WatercolorStyle` | 파일 관리자 UI(워터컬러) |

| 폴더 | 내용 |
|---|---|
| `src/fmstyle` | `Fm::style` — FmStyle, WatercolorStyle, 테마 토큰, ThemeManager, ThemeScope |
| `src/fmwidgets` | `Fm::widgets` — 버튼 · 스위치 · 카드 등 기본 부품, 대화상자 · 설정 · 메인 창 부품 |
| `src/fmfilelist` | `Fm::filelist` — 파일 목록(Qtitan 1줄 · 2줄 밴드 보기), 섬네일(Qt 목록 · Qtitan 카드 두 구현), 모델 · 원본 |
| `src/fmdialogs` | `Fm::dialogs` — 파일 작업 · 관리자 권한 대화상자, 설정 창, 대화상자 변형 카탈로그 |
| `src/fmsettings` | `Fm::settings` — 설정 모델 · 보관소(JSON) |
| `src/designer` | Qt Widgets Designer 플러그인 (`fmdesignerplugin`) |
| `app/fmdemo` | 데모 앱 — 메인 창 · 대화상자 · 설정 · 도구 창(카탈로그 · 섬네일 비교) · 일괄 스냅숏 |
| `third_party/QtitanDataGrid` | QtitanDataGrid 원본 + 패치 묶음(`PATCHES.md`) |
| `examples/designer` | Designer로 만든 `CopyDetails.ui`를 uic로 불러 쓰는 예제 |
| `gallery` | 목업과 비교하는 위젯 갤러리 |
| `tests` | Qt Test — 파일 목록 · 메인 창 · 대화상자 · 설정 · Designer 플러그인 |
| `docs` | 계획(`PLAN.md`) · 명세(`specs`) · 디자인 캔버스 사본 |

## 빌드

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt 키트 경로>
cmake --build build
```

옵션: `FMSTYLE_WITH_QTITAN`, `FMSTYLE_BUILD_GALLERY`, `FMSTYLE_BUILD_EXAMPLES`, `FMSTYLE_BUILD_DESIGNER_PLUGIN`,
`FMSTYLE_BUILD_TESTS` (모두 기본 ON). 플러그인은 Qt6::Designer · Qt6::UiPlugin이 있어야 빌드된다.

Windows에서는 `tools\build.cmd debug|release`가 VS 2026 개발자 환경을 불러 프리셋(`CMakePresets.json`)으로 빌드하고,
`tools\run.cmd <exe> [인수…]`가 Qt 키트의 `bin`을 PATH 앞에 두고 실행한다(개발용 — 배포는 windeployqt).
테스트는 `ctest --test-dir build\debug`로 돌린다(offscreen 플랫폼 · 글꼴 폴더는 CMake가 넣는다).

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
| 섬네일 비교 — Qt 목록 · Qtitan 카드 | 같은 모델을 두 섬네일 구현(QListView IconMode + 델리게이트, Qtitan CardGrid + 카드 그리기 훅)에 물려 좌우에 보인다. 원본은 샘플 · 모의 부하 1만 · 10만 개 · 실제 폴더. 모의 부하는 섬네일이 16 ms마다 400개씩 도착하는 상황을 흉내 낸다. 모델 연결 시간(배치 + 첫 그리기) · 메모리 증가 · 스크롤 40단계의 다시 그리기 시간을 잰다 |

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

### 일괄 스냅숏

```
tools\run.cmd build\release\fmdemo.exe --shot shots
tools\run.cmd build\release\fmdemo.exe --shot shots --only main,copy,settings.keys --themes std-light,wc-navy
```

- 메인 창(보드 기본 상태, 1440 × 900)과 대화상자 변형 76개, 모두 77화면을 테마 5개(`std-light` · `std-dark` ·
  `wc-light` · `wc-dark` · `wc-navy`)로 찍는다. 화면에 띄우지 않고 1배율로 그리므로 화면 배율과 관계없이
  목업 보드의 CSS 픽셀과 같은 크기가 된다.
- 결과: `<폴더>/<테마>/<id>.png`(클라이언트 영역), `<폴더>/<테마>/framed/<id>.png`(목업식 제목 표시줄 틀 —
  시안1은 36 px 제목(메인 32) · 모서리 8, 시안2는 27 px 그라데이션 제목 · 3 px 틀), `<폴더>/index.html`
  (화면 × 테마 표, 틀 켜기 · 끄기와 미리보기 폭 조절, 구현 크기와 목업 크기 비교 — 다르면 빨간색).
- `--only`는 화면 ID 접두어(쉼표 구분, `main` = 메인 창), `--themes`는 테마 ID다.
  전체 770장은 debug 빌드에서 약 3분 걸린다.

## Qt Widgets Designer 플러그인

위젯 상자에 **FmStyle — …** 묶음 다섯 개로 42개 위젯이 들어간다. 프로젝트의 모든 .ui(설정 9페이지 · 파일 작업 대화상자 ·
예제)가 쓰는 위젯을 모두 포함하므로, 어느 .ui든 Designer에서 열면 앱과 같은 모양으로 보인다.

| 묶음 | 위젯 |
|---|---|
| 기본 | `Button`, `Switch`, `SegmentedControl`, `Card`(컨테이너), `ProgressBar`, `TransferGraph`, `Label`, `Tag`, `KeyChip`, `Banner` |
| 대화상자 | `DialogHeader`, `DialogFooter`(컨테이너), `PathEdit`, `RecentTargetsBar`, `ChoiceCard`, `TokenButton`, `FileSummaryList`, `FolderPlanView`, `KeyValueCard`, `ItemListCard`, `ActionCard`, `OptionRadio` |
| 설정 | `SettingRow`(컨테이너 — 넣은 위젯은 오른쪽 컨트롤 칸으로 옮겨진다), `SearchField`, `ThemeModeCard`, `ColorSwatchButton`, `AccentPicker`, `HexColorEdit`, `ToggleChip`, `ColorPickButton`, `CheckListCombo`, `KeyCaptureEdit` |
| 메인 창 | `CommandLine`, `FunctionKeyBar`, `FindBox`, `BreadcrumbBar`, `DriveButton`, `PanelStatusBar`, `PanelTabStrip` |
| 파일 목록 | `fm::filelist::FileListView`, `fm::filelist::ThumbnailView`, `fm::dialogs::RenamePreviewView` — Qtitan을 정적 링크하므로 DLL을 더 배포하지 않는다 |

표에서는 클래스 이름 앞의 `fm::ui::`를 줄였다. Designer 안에서는 목록 · 그래프 · 기능 키 막대 등에 예시 데이터를 넣어
모양을 볼 수 있게 한다(앱이 QUiLoader로 .ui를 읽을 때는 넣지 않는다).

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
- **확인**: `fmstyle_gallery --design watercolor` (창 위쪽 단추로 시안1 · 시안2 전환),
  `fm_designer_example --watercolor [--dark]`.
