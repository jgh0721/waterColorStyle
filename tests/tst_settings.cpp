// 설정 모델(P7) 테스트 — 테마 파생 규칙(기준 색 11역할 · 내장 값 보존), 색 구성표 JSON, AppSettings JSON 왕복,
// 파일 그룹 매처(05 §2.1.4 샘플 · 합치기 규칙), 크기 · 기간 해석, 명령 키 충돌(범위).

#include <fmfilelist/ColumnSets.h>
#include <fmfilelist/FileGroups.h>
#include <fmfilelist/FileRoles.h>
#include <fmsettings/AppSettings.h>
#include <fmsettings/Commands.h>
#include <fmsettings/SettingsStore.h>
#include <fmstyle/ColorScheme.h>

#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
namespace fs = fm::style;
namespace fl = fm::filelist;
namespace st = fm::settings;
using fs::Token;

class TestSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void deriveKeepsBuiltin();
    void deriveAccent();
    void deriveRoles();
    void colorSchemeJson();
    void appSettingsJson();
    void storeFile();
    void groupMatcher();
    void groupParsing();
    void columnSets();
    void keyConflicts();
};

void TestSettings::deriveKeepsBuiltin()
{
    // 시드가 없으면 손으로 맞춘 내장 값 그대로(두 디자인 · 모든 변형)
    for (const fs::Design d : {fs::Design::Standard, fs::Design::Watercolor}) {
        for (const fs::Variant v : {fs::Variant::Light, fs::Variant::Dark, fs::Variant::Navy}) {
            const fs::DerivedTheme t = fs::deriveTheme(v, fs::ThemeSeeds{}, {}, d);
            QCOMPARE(t.colors, fs::ThemeColors(v, d));
            QVERIFY(t.adjusted.none());
        }
    }
    // 내장 값과 같은 시드는 규칙을 부르지 않는다(내장 값 보존 원칙)
    fs::ThemeSeeds seeds;
    seeds.variantSeeds(fs::Design::Standard, fs::Variant::Light).seed[size_t(fs::SeedRole::Win)] = QColor(u"#F3F4F6"_s);
    QCOMPARE(fs::deriveTheme(fs::Variant::Light, seeds).colors, fs::ThemeColors(fs::Variant::Light));
}

void TestSettings::deriveAccent()
{
    fs::ThemeSeeds seeds;
    seeds.accent = QColor(u"#0F7A6E"_s);  // 청록
    const fs::ThemeColors light = fs::deriveColors(fs::Variant::Light, seeds);
    QCOMPARE(light[Token::Accent], QColor(u"#0F7A6E"_s));
    QVERIFY(fs::contrastRatio(light[Token::AccentFg], light[Token::Surface]) >= 4.5);
    QCOMPARE(light[Token::OnAccent], QColor(Qt::white));
    QCOMPARE(light[Token::InvSel], light[Token::Accent]);
    QVERIFY(fs::contrastRatio(light[Token::Fg], light[Token::Sel]) >= 4.5);
    // 다크: 기준 색에 흰색 8 %
    const fs::ThemeColors dark = fs::deriveColors(fs::Variant::Dark, seeds);
    QCOMPARE(fs::seedColor(fs::SeedRole::Accent, dark, seeds), QColor(0x22, 0x85, 0x7A));
    // 워터컬러: 선택 = 강조 채움, 포커스는 규칙 없음(글자색 점선)
    const fs::ThemeColors wc = fs::deriveColors(fs::Variant::Light, seeds, {}, fs::Design::Watercolor);
    QCOMPARE(wc[Token::Sel], wc[Token::Accent]);
    QCOMPARE(wc[Token::Focus], fs::ThemeColors(fs::Variant::Light, fs::Design::Watercolor)[Token::Focus]);
    // 흰 글자 대비가 모자란 강조색은 어둡게 보정되고 "보정됨"
    fs::ThemeSeeds pale;
    pale.accent = QColor(u"#9FD3FF"_s);
    const fs::DerivedTheme t = fs::deriveTheme(fs::Variant::Light, pale);
    QVERIFY(t.adjusted.test(fs::indexOf(Token::Accent)));
    QVERIFY(fs::contrastRatio(Qt::white, t.colors[Token::Accent]) >= 4.5);
}

void TestSettings::deriveRoles()
{
    // 다크 구분선: 그리드 = 구분선과 목록 바탕 사이
    fs::ThemeSeeds seeds;
    auto &dark = seeds.variantSeeds(fs::Design::Standard, fs::Variant::Dark);
    dark.seed[size_t(fs::SeedRole::Line)] = QColor(u"#404550"_s);
    fs::ThemeColors c = fs::deriveColors(fs::Variant::Dark, seeds);
    QCOMPARE(c[Token::Line], QColor(u"#404550"_s));
    QCOMPARE(c[Token::Grid], QColor(0x2B, 0x2F, 0x36));  // mix(#404550, #16181B, .5)
    // 목록 바탕을 바꾸면 그 바탕을 읽는 규칙(경고 바탕 · 위험 바탕 · 강조 연한 바탕)이 따라온다
    fs::ThemeSeeds s2;
    s2.variantSeeds(fs::Design::Standard, fs::Variant::Light).seed[size_t(fs::SeedRole::Surface)] = QColor(u"#FAF7F0"_s);
    c = fs::deriveColors(fs::Variant::Light, s2);
    const fs::ThemeColors builtin(fs::Variant::Light);
    QCOMPARE(c[Token::Surface], QColor(u"#FAF7F0"_s));
    QCOMPARE(c[Token::Field], c[Token::Surface]);
    QVERIFY(c[Token::WarnBg] != builtin[Token::WarnBg]);
    QVERIFY(c[Token::AccentSoft] != builtin[Token::AccentSoft]);
    QVERIFY(fs::contrastRatio(c[Token::Fg3], c[Token::Surface]) >= 4.5);
    // 위험: 흰 글자 대비 4.5 이상 채움, 글자 위 색
    fs::ThemeSeeds s3;
    s3.variantSeeds(fs::Design::Standard, fs::Variant::Light).seed[size_t(fs::SeedRole::Danger)] = QColor(u"#FF6B6B"_s);
    const fs::DerivedTheme t = fs::deriveTheme(fs::Variant::Light, s3);
    QVERIFY(fs::contrastRatio(Qt::white, t.colors[Token::DangerFill]) >= 4.5);
    QVERIFY(t.adjusted.test(fs::indexOf(Token::DangerFill)));
    // 선택 연동을 끄고 시드를 주면 그 색 그대로
    fs::ThemeSeeds s4;
    auto &l4 = s4.variantSeeds(fs::Design::Standard, fs::Variant::Light);
    l4.selFollowsAccent = false;
    l4.seed[size_t(fs::SeedRole::Sel)] = QColor(u"#FFE9A8"_s);
    QCOMPARE(fs::deriveColors(fs::Variant::Light, s4)[Token::Sel], QColor(u"#FFE9A8"_s));
    // 직접 지정이 이긴다
    fs::TokenOverrides ov{};
    ov[fs::indexOf(Token::Grid)] = QColor(u"#123456"_s);
    QCOMPARE(fs::deriveColors(fs::Variant::Dark, seeds, ov)[Token::Grid], QColor(u"#123456"_s));
    // 메타데이터
    QCOMPARE(fs::roleOfToken(Token::WarnLine), std::optional(fs::SeedRole::Warn));
    QVERIFY(!fs::roleOfToken(Token::KPdf));
    QVERIFY(fs::tokenSource(Token::Surface, fs::Design::Standard).isSeed);
    QCOMPARE(fs::tokenSource(Token::Head, fs::Design::Standard).rule(fs::Variant::Dark), u"창보다 한 단계 밝게"_s);
    QCOMPARE(fs::tokenDisplayValue(Token::TintSel, fs::ThemeColors(fs::Variant::Dark)[Token::TintSel]), u"흰색 7 %"_s);
    int total = 0;
    for (const fs::SeedRoleInfo &r : fs::seedRoles())
        total += r.tokenCount;
    QCOMPARE(total, 39);  // 11개가 토큰 39개를 정함
}

void TestSettings::colorSchemeJson()
{
    fs::ColorScheme scheme;
    scheme.id = u"my-blue"_s;
    scheme.name = u"내 파랑"_s;
    scheme.seeds.accent = QColor(u"#0F7A6E"_s);
    scheme.seeds.fixContrast = false;
    auto &vs = scheme.seeds.variantSeeds(fs::Design::Watercolor, fs::Variant::Navy);
    vs.seed[size_t(fs::SeedRole::Surface)] = QColor(u"#0F172C"_s);
    vs.invSelFollowsAccent = false;
    scheme.overridesFor(fs::Design::Standard, fs::Variant::Light)[fs::indexOf(Token::KPdf)] = QColor(u"#C4382D"_s);
    scheme.overridesFor(fs::Design::Standard, fs::Variant::Dark)[fs::indexOf(Token::TintSel)] = QColor(0xFF, 0xFF, 0xFF, 0x20);
    QVERIFY(!scheme.isPristine());
    const QJsonObject json = scheme.toJson();
    QCOMPARE(json.value(u"format"_s).toString(), u"fm-color-scheme"_s);
    const auto back = fs::ColorScheme::fromJson(json);
    QVERIFY(back);
    QCOMPARE(*back, scheme);
    QVERIFY(fs::ColorScheme().isPristine());
    QString error;
    QVERIFY(!fs::ColorScheme::fromJson(QJsonObject{{u"format"_s, u"x"_s}}, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(fs::parseColorHex(u"1f5fd1"_s), QColor(u"#1F5FD1"_s));
    QVERIFY(!fs::parseColorHex(u"#12345"_s).isValid());
}

void TestSettings::appSettingsJson()
{
    st::AppSettings s;
    s.appearance.scheme = st::Scheme::Dark;
    s.appearance.design = fs::Design::Watercolor;
    s.appearance.density = st::AppearanceSettings::Density::Relaxed;
    s.general.trayIcon = st::GeneralSettings::TrayIcon::Always;
    s.tabs.title = st::TabSettings::TabTitle::FullPath;
    s.theme.scheme.seeds.accent = QColor(u"#6D3FC0"_s);
    s.panel.separator2 = fl::RecordSeparator::Tint;
    s.panel.showHidden = true;
    s.thumbs.info = fl::ThumbnailAppearance::Info::SizeDate;
    s.thumbs.targets = st::ThumbSettings::Images | st::ThumbSettings::Documents;
    s.groups.merge = fl::FileGroupSettings::Merge::FirstOnly;
    s.groups.groups[5].style.italic = true;
    s.columns.sets[1].columns[3].align = Qt::AlignRight;
    s.fileOps.onConflict = st::FileOpsSettings::Conflict::KeepBoth;
    s.elevation.protectedPathsUser.append(u"D:\\Secure"_s);
    s.keys.overrides.insert(u"columnSet"_s, {QKeySequence(u"Ctrl+M"_s)});
    s.keys.overrides.insert(u"multiRename"_s, {});  // 의도적으로 비움
    s.dialog.lastPage = u"theme"_s;
    const st::AppSettings back = st::AppSettings::fromJson(s.toJson());
    QCOMPARE(back, s);
    QVERIFY(back.keys.overrides.contains(u"multiRename"_s));
    QVERIFY(back.keys.overrides.value(u"multiRename"_s).isEmpty());
    // 기본값 · 구역 비교
    QCOMPARE(st::AppSettings::fromJson(QJsonObject()), st::AppSettings());
    QCOMPARE(st::differingSections(st::AppSettings(), s) & st::Section::Panel, st::Section::Panel);
    QVERIFY(!(st::differingSections(s, s)));
    QCOMPARE(st::builtinProtectedPaths().size(), 5);
}

void TestSettings::storeFile()
{
    QTemporaryDir dir;
    auto &store = st::SettingsStore::instance();
    const QString path = dir.filePath(u"FM Tools/settings.json"_s);
    store.setFilePath(path);
    QVERIFY(store.load());  // 파일이 없으면 기본값
    st::AppSettings s = store.settings();
    s.panel.inverseCursor = true;
    QSignalSpy spy(&store, &st::SettingsStore::changed);
    store.setSettings(s);
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.first().first().value<st::Sections>(), st::Sections(st::Section::Panel));
    QVERIFY(QFile::exists(path));
    // 모르는 키는 보존
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        root[u"futureSection"_s] = QJsonObject{{u"x"_s, 1}};
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QJsonDocument(root).toJson());
    }
    QVERIFY(store.load());
    QVERIFY(store.settings().panel.inverseCursor);
    s.panel.boldSelection = false;
    store.setSettings(s);
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(QJsonDocument::fromJson(f.readAll()).object().contains(u"futureSection"_s));
    store.setFilePath(QString());
    store.setSettings(st::AppSettings());
}

void TestSettings::groupMatcher()
{
    const fl::FileGroupSettings settings = fl::FileGroupSettings::defaults();
    QCOMPARE(settings.groups.size(), 10);
    const QDateTime now = QDateTime::currentDateTime();
    auto facts = [&](const QString &name, qint64 size, int attributes = 0, int hoursAgo = 72) {
        fl::FileFacts f;
        f.name = name;
        f.ext = name.contains(u'.') ? name.section(u'.', -1) : QString();
        f.size = size;
        f.attributes = attributes;
        f.modified = now.addSecs(-3600LL * hoursAgo);
        return f;
    };
    const fl::FileGroupMatcher matcher(settings);
    // 05 §2.1.4 — 속성별로 합치기
    fl::ResolvedGroupStyle r = matcher.resolve(facts(u"BandedPanelView.cpp"_s, 24678, 0, 2), now);
    QCOMPARE(r.groups, (QStringList{u"소스 코드"_s, u"최근 24시간에 바뀜"_s}));
    QVERIFY(r.bold);
    QCOMPARE(*r.textLight, QColor(u"#9A5B00"_s));
    QCOMPARE(*r.backLight, QColor(u"#FFF1C9"_s));
    r = matcher.resolve(facts(u"Qt-6.11.0-windows-x64-msvc2026-offline-installer-with-debug-symbols.exe"_s, qint64(3.74 * (1LL << 30))), now);
    QCOMPARE(r.groups, QStringList{u"실행 파일"_s});  // 큰 파일은 줄 것이 없다(글자색은 이미 정해짐)
    r = matcher.resolve(facts(u"placeholder-notes.txt"_s, 0), now);
    QCOMPARE(r.groups, (QStringList{u"문서"_s, u"빈 파일"_s}));
    QVERIFY(r.strike);
    r = matcher.resolve(facts(u"Qt Creator 18.lnk"_s, 1434), now);
    QCOMPARE(r.groups, QStringList{u"링크 · 바로 가기"_s});
    QVERIFY(r.underline && !r.textLight);
    r = matcher.resolve(facts(u"desktop.ini"_s, 282, fl::Hidden | fl::System), now);
    QCOMPARE(r.groups, QStringList{u"숨김 · 시스템"_s});
    QVERIFY(r.italic);
    QVERIFY(matcher.resolve(facts(u"notes"_s, 12), now).isEmpty());
    // 위 그룹 하나만
    fl::FileGroupSettings first = settings;
    first.merge = fl::FileGroupSettings::Merge::FirstOnly;
    const fl::FileGroupMatcher firstOnly(first);
    r = firstOnly.resolve(facts(u"BandedPanelView.cpp"_s, 24678, 0, 2), now);
    QCOMPARE(r.groups, QStringList{u"소스 코드"_s});
    QVERIFY(!r.backLight);
    // 모두 맞아야 — 해석 안 되는 조건은 빠진다
    fl::FileGroupSettings all;
    fl::FileGroup g;
    g.name = u"큰 영상"_s;
    g.matchAll = true;
    g.conditions = {{fl::GroupCondition::Field::Extension, fl::GroupCondition::Op::AnyOf, u"mkv"_s},
                    {fl::GroupCondition::Field::Size, fl::GroupCondition::Op::AtLeast, u"1 GB"_s},
                    {fl::GroupCondition::Field::Size, fl::GroupCondition::Op::AtLeast, u"많이"_s}};
    g.style.bold = true;
    all.groups = {g};
    const fl::FileGroupMatcher strict(all);
    QVERIFY(strict.resolve(facts(u"a.mkv"_s, 2LL << 30), now).bold);
    QVERIFY(strict.resolve(facts(u"a.mkv"_s, 100), now).isEmpty());
    // 요약 줄
    QCOMPARE(fl::groupSummary(settings.groups.at(1)), u"*.exe  *.msi  *.cmd  *.bat  *.ps1 …"_s);
    QCOMPARE(fl::groupSummary(settings.groups.at(0)), u"속성에 H 또는 S"_s);
    QCOMPARE(fl::groupSummary(settings.groups.at(8)), u"크기 0 바이트 · 취소선만"_s);
    QCOMPARE(fl::groupSummary(settings.groups.at(9)), u"수정한 날짜 24시간 이내 · 배경만"_s);
}

void TestSettings::groupParsing()
{
    QCOMPARE(fl::parseSizeText(u"1 GB"_s), std::optional<qint64>(1LL << 30));
    QCOMPARE(fl::parseSizeText(u"0 바이트"_s), std::optional<qint64>(0));
    QCOMPARE(fl::parseSizeText(u"1.5MB"_s), std::optional<qint64>(1572864));
    QVERIFY(!fl::parseSizeText(u"크다"_s));
    QCOMPARE(fl::parseDurationText(u"24시간 이내"_s), std::optional<qint64>(86400));
    QCOMPARE(fl::parseDurationText(u"2주"_s), std::optional<qint64>(14 * 86400));
    QString error;
    QVERIFY(!fl::validateCondition({fl::GroupCondition::Field::Name, fl::GroupCondition::Op::Regex, u"("_s}, &error));
    QVERIFY(error.startsWith(u"정규식 오류"_s));
    QVERIFY(fl::validateCondition({fl::GroupCondition::Field::Size, fl::GroupCondition::Op::Between, u"1 MB ~ 10 MB"_s}));
    // 다크 자동: 어두운 바탕 대비 4.5 이상
    const QColor dark = fl::darkFromLightColor(QColor(u"#9A5B00"_s), QColor(u"#16181B"_s));
    QVERIFY(fs::contrastRatio(dark, QColor(u"#16181B"_s)) >= 4.5);
}

void TestSettings::columnSets()
{
    const fl::ColumnSettings s = fl::ColumnSettings::defaults();
    QCOMPARE(s.sets.size(), 5);
    QVERIFY(s.sets.first().isDefault());
    const fl::ColumnSet &photos = s.sets.at(1);
    QCOMPARE(photos.columns.size(), 7);
    QCOMPARE(fl::columnSetTag(photos), u"자동"_s);
    QCOMPARE(fl::columnSetRuleSummary(photos, {u"이미지 · 영상"_s}, {u"media"_s}), u"이미지 · 영상 그룹이 60% 이상"_s);
    QCOMPARE(fl::columnSetRuleSummary(s.sets.at(2)), u"경로가 D:\\Work\\* 와 일치"_s);
    QCOMPARE(fl::columnSetRuleSummary(s.sets.at(3)), u"경로가 사용자 다운로드 폴더"_s);
    QCOMPARE(fl::columnSetTag(s.sets.at(4)), u"수동"_s);
    // 2줄 배치: 아이콘 · 이름 + 행 1 열 5개(촬영 날짜 136 · 크기 84 · 해상도 104 · 재생 시간 72 · 카메라 140) + 채움
    QList<int> rowColumns;
    const fl::ListColumnLayout layout = fl::bandPreviewLayout(photos, &rowColumns);
    QCOMPARE(layout.columns.size(), 8);
    QCOMPARE(rowColumns, (QList<int>{1, 2, 3, 4, 5}));
    QCOMPARE(layout.columns.at(2).width, 136);
    QCOMPARE(layout.columns.at(4).align, Qt::Alignment(Qt::AlignHCenter));
    QCOMPARE(fl::columnEmptyLabel(photos.columns.at(1), photos.columns), u"수정한 날짜로 대신"_s);
}

void TestSettings::keyConflicts()
{
    st::KeyBindingSettings keys;
    QCOMPARE(st::effectiveKeys(keys, u"delete"_s).size(), 2);  // F8 · Del
    // Ctrl+M(파일 목록) ↔ 열 세트 바꾸기(패널): 범위가 겹쳐 충돌
    QCOMPARE(st::findConflict(keys, u"columnSet"_s, QKeySequence(u"Ctrl+M"_s)), std::optional<QString>(u"multiRename"_s));
    // F2(파일 목록) ↔ 대기열에 추가(복사 · 이동 대화상자): 충돌 아님
    QVERIFY(!st::findConflict(keys, u"queue"_s, QKeySequence(Qt::Key_F2)));
    // 충돌 해결로 비운 명령은 키가 없다
    keys.overrides.insert(u"multiRename"_s, {});
    QVERIFY(!st::findConflict(keys, u"columnSet"_s, QKeySequence(u"Ctrl+M"_s)));
    QVERIFY(st::isForbiddenKey(QKeySequence(Qt::ALT | Qt::Key_F4)));
    QVERIFY(st::isForbiddenKey(QKeySequence(Qt::Key_F10)));
    QVERIFY(!st::isForbiddenKey(QKeySequence(Qt::Key_Tab)));
    QCOMPARE(st::layoutKeys(u"rename"_s, u"totalcmd"_s), QList<QKeySequence>{QKeySequence(Qt::SHIFT | Qt::Key_F6)});
}

QTEST_MAIN(TestSettings)
#include "tst_settings.moc"
