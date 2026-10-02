#pragma once

// 앱 설정(docs/specs/04 §4.1 · 05 §4) — 설정 창 9페이지가 고치는 값. 기본값은 목업 기본값이고,
// %APPDATA%\FM Tools\settings.json(JSON)에 저장한다(SettingsStore).

#include <fmfilelist/ColumnSets.h>
#include <fmfilelist/FileGroups.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/ListAppearance.h>
#include <fmstyle/ColorScheme.h>
#include <fmstyle/ThemeManager.h>

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>

namespace fm::settings {

using Scheme = fm::style::ThemeManager::Scheme;
using DarkTone = fm::style::ThemeManager::DarkTone;

struct AppearanceSettings
{
    using Density = fm::filelist::RowDensity;

    Scheme scheme = Scheme::System;                 // 시스템(Y) · 라이트(L) · 다크(K)
    fm::style::Design design = fm::style::Design::Standard;  // (제안) 디자인 — 시안1 · 시안2
    DarkTone darkTone = DarkTone::Gray;             // (제안) 다크 색조 — 워터컬러에서만
    bool darkTitleBar = true;
    bool coloredTitleBar = true;                    // (제안) 워터컬러 제목 표시줄 색
    QString listFontFamily = QStringLiteral("Segoe UI Variable Text");
    int listFontPx = 13;
    QString monoFontFamily = QStringLiteral("Cascadia Mono");
    int monoFontPx = 12;
    Density density = Density::Normal;
    int displayScalePercent = 0;                    // 0 = 시스템 따름 (다시 시작하면 적용)

    bool operator==(const AppearanceSettings &) const = default;
};

struct GeneralSettings
{
    enum class Startup : std::uint8_t { RestoreLastTabs, HomeFolder, SpecificFolders };
    enum class TrayIcon : std::uint8_t { Never, WhileBusy, Always };

    QString language = QStringLiteral("ko");        // "" = 시스템 언어 (다시 시작)
    Startup startup = Startup::RestoreLastTabs;
    QStringList startupFolders;
    bool singleInstance = true;
    bool showCommandLine = true;
    bool showFunctionKeyBar = true;
    TrayIcon trayIcon = TrayIcon::WhileBusy;

    bool operator==(const GeneralSettings &) const = default;
};

struct TabSettings
{
    enum class NewTabPosition : std::uint8_t { RightOfCurrent, End };
    enum class TabTitle : std::uint8_t { FolderName, DriveAndFolder, FullPath };

    NewTabPosition newTabPosition = NewTabPosition::RightOfCurrent;
    TabTitle title = TabTitle::FolderName;
    bool rememberViewPerTab = true;

    bool operator==(const TabSettings &) const = default;
};

struct ThemeSettings
{
    QString schemeId = QStringLiteral("builtin");   // "기본 (내장)" 또는 themes\<id>.json
    fm::style::ColorScheme scheme;                  // 보류 편집 내용(저장 안 된 수정 포함)
    bool editBothVariants = true;                   // 라이트 · 다크 함께(B)

    bool operator==(const ThemeSettings &) const = default;
};

struct PanelSettings
{
    using SizeUnit = fm::filelist::SizeUnit;

    fm::filelist::ViewMode defaultViewMode = fm::filelist::ViewMode::Auto;
    int autoSwitchWidthPx = 640;
    int autoSwitchTruncatedPct = 25;
    fm::filelist::RecordSeparator separator1 = fm::filelist::RecordSeparator::None;
    fm::filelist::RecordSeparator separator2 = fm::filelist::RecordSeparator::Zebra;
    bool nameBelow = false;
    fm::filelist::NameElide nameOverflow = fm::filelist::NameElide::MiddleKeepExtension;
    bool showHidden = false;
    bool showProtectedOs = false;
    bool foldersFirst = true;
    SizeUnit sizeUnit = SizeUnit::Auto;
    QString dateFormat = QStringLiteral("yyyy-MM-dd HH:mm");
    bool inverseCursor = false;
    bool inverseSelection = false;
    bool boldSelection = true;
    fm::filelist::InactiveCursor inactiveCursor = fm::filelist::InactiveCursor::Dashed;

    /// 목록 표시 설정 — 목록 글꼴 · 행 밀도는 일반 · 모양(look)에서.
    fm::filelist::ListAppearance toListAppearance(const AppearanceSettings &look) const;
    fm::filelist::DisplayFormat toDisplayFormat() const { return {sizeUnit, dateFormat}; }
    bool operator==(const PanelSettings &) const = default;
};

struct ThumbSettings
{
    enum class Provider : std::uint8_t { WindowsShell, Builtin, ShellThenBuiltin };
    enum Target : int { Images = 1, Videos = 2, Pdf = 4, Fonts = 8, Documents = 16 };

    int sizePx = 96;
    int nameLines = 2;                              // 1 · 2 · 0(전체)
    fm::filelist::ThumbnailAppearance::Info info = fm::filelist::ThumbnailAppearance::Info::Size;
    bool fill = false;                              // 채움 — 끄면 맞춤
    bool typeBadge = true;
    Provider provider = Provider::WindowsShell;
    int targets = Images | Videos | Pdf | Fonts;
    bool iconsOnlyOnNetworkRemovable = true;
    int concurrency = 4;                            // 1–16
    bool autoThumbnailFolders = true;
    int autoThumbnailPct = 70;

    /// 커서 · 선택 모양은 파일 패널 설정을 따른다.
    fm::filelist::ThumbnailAppearance toThumbnailAppearance(const PanelSettings &panel) const;
    bool operator==(const ThumbSettings &) const = default;
};

struct FileOpsSettings
{
    enum class Conflict : std::uint8_t { Ask, Overwrite, OverwriteIfNewer, Skip, KeepBoth };
    enum class Links : std::uint8_t { CopyAsLink, CopyTarget, Skip };
    enum class DeleteMode : std::uint8_t { RecycleBin, Permanent };
    enum class ReadOnly : std::uint8_t { Ask, Delete, Skip };
    enum class RecordMode : std::uint8_t { Single, Double, Auto };
    enum class RenameProblem : std::uint8_t { Block, SkipProblems };

    Conflict onConflict = Conflict::Ask;
    bool verifyHash = false;
    bool keepAttributes = true;
    bool copyAcl = false;
    bool copyAds = true;
    Links links = Links::CopyAsLink;
    DeleteMode deleteMode = DeleteMode::RecycleBin;
    bool confirmDelete = true;
    ReadOnly readOnly = ReadOnly::Ask;
    int progressDelayMs = 1000;                     // 0 = 바로, 3000, -1 = 표시 안 함
    bool progressDetailed = true;
    bool closeWhenDone = true;
    bool notifyWhenDone = true;
    int maxConcurrentJobs = 1;                      // 0 = 제한 없음
    RecordMode renamePreview = RecordMode::Auto;
    QString renameDefaultPreset = QStringLiteral("사진 · 날짜_장소_번호");
    int renameUndoDepth = 20;                       // 0 = 기록 안 함
    RenameProblem renameOnProblem = RenameProblem::Block;

    bool operator==(const FileOpsSettings &) const = default;
};

struct ElevationSettings
{
    enum class HelperLifetime : std::uint8_t { PerOperation, FiveMinutes, UntilExit };
    enum class Ownership : std::uint8_t { Ask, SkipSilently };
    enum class OwnershipButton : std::uint8_t { Skip, TakeOwnership, Cancel };

    bool preflight = true;
    bool applyToRemainingDefault = true;
    HelperLifetime helperLifetime = HelperLifetime::PerOperation;
    int uacTimeoutSec = 120;                        // 0 = 계속 기다림
    Ownership takeOwnership = Ownership::Ask;
    OwnershipButton ownershipDefault = OwnershipButton::Skip;
    bool backupAclBeforeChange = true;
    bool adminTitleWarning = true;
    QStringList protectedPathsUser = {QStringLiteral("E:\\Backup\\Signed-Releases")};
    int logRetentionDays = 30;                      // 0 = 보관 안 함

    bool operator==(const ElevationSettings &) const = default;
};

struct KeyBindingSettings
{
    enum class ProgressEsc : std::uint8_t { ConfirmCancel, CancelImmediately, HideWindow };

    QString layout = QStringLiteral("default");     // default · totalcmd · explorer
    /// 배열 기본값과 다른 명령만. 빈 목록 = 의도적으로 비움(충돌 해결로 지워진 경우).
    QHash<QString, QList<QKeySequence>> overrides;
    bool alwaysShowMnemonics = false;
    ProgressEsc progressEsc = ProgressEsc::ConfirmCancel;

    bool operator==(const KeyBindingSettings &) const = default;
};

struct DialogState
{
    QString lastPage = QStringLiteral("appearance");
    QByteArray geometry;
    QString themeView = QStringLiteral("basic");

    bool operator==(const DialogState &) const = default;
};

/// 마지막 탭과 폴더(일반 › 시작할 때 = 마지막 탭과 폴더 복원) — 설정이 아니라 끝낼 때 저장하는 상태.
struct SessionState
{
    struct Tab
    {
        bool local = false;  // false = 샘플 데이터(D:)
        QString path;
        fm::filelist::ViewMode mode = fm::filelist::ViewMode::Auto;
        bool modeSet = false;

        bool operator==(const Tab &) const = default;
    };

    QList<Tab> left;
    QList<Tab> right;
    int leftCurrent = 0;
    int rightCurrent = 0;
    bool rightActive = true;

    bool isEmpty() const noexcept { return left.isEmpty() && right.isEmpty(); }
    bool operator==(const SessionState &) const = default;
};

struct AppSettings
{
    int version = 1;
    AppearanceSettings appearance;
    GeneralSettings general;
    TabSettings tabs;
    ThemeSettings theme;
    PanelSettings panel;
    ThumbSettings thumbs;
    fm::filelist::FileGroupSettings groups = fm::filelist::FileGroupSettings::defaults();
    fm::filelist::ColumnSettings columns = fm::filelist::ColumnSettings::defaults();
    FileOpsSettings fileOps;
    ElevationSettings elevation;
    KeyBindingSettings keys;
    DialogState dialog;
    SessionState session;

    bool operator==(const AppSettings &) const = default;

    QJsonObject toJson() const;
    /// 모르는 값은 기본값으로. 버전이 올라가면 여기서 옮긴다.
    static AppSettings fromJson(const QJsonObject &json);
};

/// 설정 구역(적용 단추 · 변경 알림 · 페이지 범위).
enum class Section : std::uint16_t {
    Appearance = 0x1, General = 0x2, Tabs = 0x4, Theme = 0x8, Panel = 0x10, Thumbs = 0x20, Groups = 0x40,
    Columns = 0x80, FileOps = 0x100, Elevation = 0x200, Keys = 0x400, Dialog = 0x800, Session = 0x1000,
};
Q_DECLARE_FLAGS(Sections, Section)
Q_DECLARE_OPERATORS_FOR_FLAGS(Sections)

/// 두 설정이 다른 구역.
Sections differingSections(const AppSettings &a, const AppSettings &b);

/// 보호된 위치의 기본 항목(Known Folder로 계산 — C:\ 하드코딩 금지).
QStringList builtinProtectedPaths();

} // namespace fm::settings
