# FmStyle

파일 관리자용 Qt 스타일(Fusion 기반 QProxyStyle)과 위젯 모음. 디자인 두 가지를 같은 위젯 · 같은 토큰으로 쓴다.

| 디자인 | 스타일 클래스 | 캔버스 |
|---|---|---|
| 시안1 · 기본 (`Design::Standard`) | `fm::style::FmStyle` | 파일 관리자 UI |
| 시안2 · 워터컬러 (`Design::Watercolor`) | `fm::style::WatercolorStyle` | 파일 관리자 UI(워터컬러) |

| 폴더 | 내용 |
|---|---|
| `src/fmstyle` | `Fm::style` — FmStyle, WatercolorStyle, 테마 토큰, ThemeManager, ThemeScope |
| `src/fmwidgets` | `Fm::widgets` — Button, Switch, SegmentedControl, Card, ProgressBar, TransferGraph |
| `src/designer` | Qt Widgets Designer 플러그인 (`fmdesignerplugin`) |
| `examples/designer` | Designer로 만든 `CopyDetails.ui`를 uic로 불러 쓰는 예제 |
| `gallery` | 목업과 비교하는 위젯 갤러리 |

## 빌드

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt 키트 경로>
cmake --build build
```

옵션: `FMSTYLE_BUILD_GALLERY`, `FMSTYLE_BUILD_EXAMPLES`, `FMSTYLE_BUILD_DESIGNER_PLUGIN` (모두 기본 ON).

## Qt Widgets Designer 플러그인

위젯 상자의 **FmStyle 위젯** 묶음에 6개 위젯이 들어간다.

| 위젯 | 바탕 클래스 | Designer에서 고치는 속성 |
|---|---|---|
| `fm::ui::Button` | QPushButton | `role` (Normal · Primary · Danger · Subtle), `compact` |
| `fm::ui::Switch` | QCheckBox | `onText`, `offText` |
| `fm::ui::SegmentedControl` | QWidget | `items`, `currentIndex` |
| `fm::ui::Card` | QFrame | 컨테이너 — 안에 레이아웃과 위젯을 넣는다 |
| `fm::ui::ProgressBar` | QProgressBar | `state` (Normal · Paused · Error), `compact` (시안2에서 12 px) |
| `fm::ui::TransferGraph` | QWidget | `axis` (Progress · Time), `title`, `showAverage`, `framed`, `speedLimit` (B/s) |

미리 볼 디자인은 Designer를 띄우는 환경 변수로 고른다 — `FMSTYLE_DESIGN=watercolor` (또는 `2`)면 시안2,
없으면 시안1.

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

Designer 미리보기에서는 플러그인이 만든 위젯에만 FmStyle을 걸고 Designer 팔레트의 밝기에 따라
라이트/다크 토큰을 고른다. 팔레트 · 스타일은 .ui에 저장되지 않는다. 앱에서는
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
