// 파일 관리자 UI 데모(P4~) — 메인 창. 도구 모음 오른쪽의 스위치로 시안1 ↔ 시안2, 색 구성표 · 다크 색조를 바꾼다.
//   fmdemo                                         Main 보드 기본 상태
//   fmdemo --design watercolor --scheme navy        시안2 · 다크(남색)
//   fmdemo --left-mode thumb --right-mode thumb     섬네일(왼쪽 Qt 목록 · 오른쪽 Qtitan 카드)
//   fmdemo --sep2 tint --name-below --inv-cursor    표시 변형
//   fmdemo --shot main.png                          스크린샷을 저장하고 끝냄
//   fmdemo --shot shots                             폴더면 메인 창 + 대화상자 변형 전부 × 테마 5 일괄(+ 목업식 틀 · index.html)
//   fmdemo --catalog                                대화상자 카탈로그 창
//   fmdemo --compare 100000 --measure               섬네일 비교 창(sample · 10000 · 100000 · <폴더>), 측정 결과 출력
//   fmdemo --settings my.json                       설정 파일(없으면 %APPDATA%m toolssettings.json, 스냅숏 · --open은 메모리만)

#include "CatalogWindow.h"
#include "FilePanel.h"
#include "MainWindow.h"
#include "Snapshots.h"
#include "ThumbnailCompare.h"

#include <fmdialogs/DialogCatalog.h>
#include <fmfilelist/ListAppearance.h>
#include <fmsettings/SettingsStore.h>
#include <fmstyle/ThemeManager.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QDialog>
#include <QFileInfo>
#include <QScreen>
#include <QTimer>

#include <cstdio>

using namespace Qt::StringLiterals;
namespace fl = fm::filelist;
namespace fs = fm::style;

namespace {

fl::ViewMode modeFrom(const QString &name, fl::ViewMode fallback)
{
    if (name == u"1")
        return fl::ViewMode::OneLine;
    if (name == u"2")
        return fl::ViewMode::TwoLine;
    if (name == u"auto")
        return fl::ViewMode::Auto;
    if (name == u"thumb")
        return fl::ViewMode::Thumbnails;
    return fallback;
}

fl::RecordSeparator separatorFrom(const QString &name, fl::RecordSeparator fallback)
{
    const QStringList names = {u"none"_s, u"zebra"_s, u"line"_s, u"space"_s, u"tint"_s};
    const qsizetype i = names.indexOf(name);
    return i < 0 ? fallback : fl::RecordSeparator(i);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"fmdemo"_s);
    QApplication::setApplicationDisplayName(u"파일 관리자"_s);

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption designOption(u"design"_s, u"standard | watercolor"_s, u"name"_s, u"standard"_s);
    const QCommandLineOption schemeOption(u"scheme"_s, u"system | light | dark | navy"_s, u"name"_s, u"light"_s);
    const QCommandLineOption leftModeOption(u"left-mode"_s, u"1 | 2 | auto | thumb"_s, u"mode"_s);
    const QCommandLineOption rightModeOption(u"right-mode"_s, u"1 | 2 | auto | thumb"_s, u"mode"_s);
    const QCommandLineOption activeOption(u"active"_s, u"left | right"_s, u"side"_s, u"right"_s);
    const QCommandLineOption sep1Option(u"sep1"_s, u"1줄 구분: none | zebra | line | space | tint"_s, u"name"_s);
    const QCommandLineOption sep2Option(u"sep2"_s, u"2줄 구분: none | zebra | line | space | tint"_s, u"name"_s);
    const QCommandLineOption nameBelowOption(u"name-below"_s, u"2줄 레코드에서 이름을 아래 줄에"_s);
    const QCommandLineOption invCursorOption(u"inv-cursor"_s, u"역상 커서"_s);
    const QCommandLineOption invSelOption(u"inv-sel"_s, u"역상 선택"_s);
    const QCommandLineOption sizeOption(u"size"_s, u"창 크기 WxH (기본 1440x900 — 보드의 창)"_s, u"size"_s, u"1440x900"_s);
    const QCommandLineOption shotOption(u"shot"_s, u"스크린샷을 저장하고 끝냅니다. .png면 그 화면 하나, 폴더면 전체 화면 × 테마 5 일괄."_s, u"file|dir"_s);
    const QCommandLineOption delayOption(u"shot-delay"_s, u"스크린샷까지 기다릴 시간(ms)."_s, u"ms"_s, u"900"_s);
    const QCommandLineOption openOption(u"open"_s, u"대화상자 변형을 연다(예: copy.default, delete.permanent). --shot이면 대화상자만 찍는다."_s, u"id"_s);
    const QCommandLineOption listOption(u"list-dialogs"_s, u"대화상자 변형 ID를 출력하고 끝냅니다."_s);
    const QCommandLineOption flowOption(u"flow"_s, u"권한 흐름 시뮬레이션을 시작한다: copy | delete"_s, u"name"_s);
    const QCommandLineOption settingsOption(u"settings"_s, u"설정 파일(JSON). 주지 않으면 기본 위치, --shot · --open이면 메모리에만 둔다."_s, u"file"_s);
    const QCommandLineOption catalogOption(u"catalog"_s, u"대화상자 카탈로그 창을 연다."_s);
    const QCommandLineOption compareOption(u"compare"_s, u"섬네일 비교 창: sample | 10000 | 100000 | <폴더>"_s, u"source"_s);
    const QCommandLineOption measureOption(u"measure"_s, u"--compare와 함께: 연결 · 스크롤을 재고 결과를 출력한 뒤 끝낸다."_s);
    const QCommandLineOption onlyOption(u"only"_s, u"--shot <폴더>: 찍을 화면 ID 접두어(쉼표, main = 메인 창)."_s, u"ids"_s);
    const QCommandLineOption themesOption(u"themes"_s, u"--shot <폴더>: std-light,std-dark,wc-light,wc-dark,wc-navy 중 일부."_s, u"list"_s);
    parser.addOptions({designOption, schemeOption, leftModeOption, rightModeOption, activeOption, sep1Option, sep2Option,
                       nameBelowOption, invCursorOption, invSelOption, sizeOption, shotOption, delayOption, openOption,
                       listOption, flowOption, settingsOption, catalogOption, compareOption, measureOption, onlyOption,
                       themesOption});
    parser.process(app);

    auto &theme = fs::ThemeManager::instance();
    theme.setDesign(parser.value(designOption) == u"watercolor"_s ? fs::Design::Watercolor : fs::Design::Standard);
    const QString scheme = parser.value(schemeOption);
    theme.setDarkTone(scheme == u"navy"_s ? fs::ThemeManager::DarkTone::Navy : fs::ThemeManager::DarkTone::Gray);
    theme.setScheme(scheme == u"system"_s  ? fs::ThemeManager::Scheme::System
                    : scheme == u"light"_s ? fs::ThemeManager::Scheme::Light
                                           : fs::ThemeManager::Scheme::Dark);
    theme.install(app);
    theme.setAlwaysShowMnemonics(true);  // 목업은 액세스 키 밑줄을 항상 보인다(PLAN §11)

    // 설정 보관소 — 스냅숏 · 대화상자 단독 실행은 메모리에만(목업과 같은 상태를 늘 재현), 그 밖에는 설정 파일을 읽고 쓴다
    auto &store = fm::settings::SettingsStore::instance();
    const bool snapshot = parser.isSet(shotOption) || parser.isSet(openOption) || parser.isSet(listOption);
    QString settingsPath = parser.value(settingsOption);
    if (settingsPath.isEmpty() && !snapshot)
        settingsPath = fm::settings::SettingsStore::defaultFilePath();
    bool settingsLoaded = false;
    if (!settingsPath.isEmpty()) {
        store.setFilePath(settingsPath);
        QString error;
        settingsLoaded = QFileInfo::exists(settingsPath) && store.load(&error);
        if (!error.isEmpty())
            std::fprintf(stderr, "settings: %s\n", qPrintable(error));
    }

    if (parser.isSet(listOption)) {
        for (const fm::dialogs::DialogVariant &v : fm::dialogs::dialogVariants())
            std::printf("%s\t%s%s\n", qPrintable(v.id), v.fromMockup ? "" : "제안 · ", qPrintable(v.label));
        return 0;
    }
    // --shot: .png · .jpg면 그 화면 하나, 그 밖(폴더)이면 전체 화면 × 테마 일괄
    const QString shotTarget = parser.value(shotOption);
    const bool shotFile = shotTarget.endsWith(u".png"_s, Qt::CaseInsensitive) || shotTarget.endsWith(u".jpg"_s, Qt::CaseInsensitive);
    if (parser.isSet(shotOption) && !shotFile && !parser.isSet(openOption) && !parser.isSet(catalogOption)
        && !parser.isSet(compareOption)) {
        fm::app::SnapshotOptions options;
        options.dir = shotTarget;
        options.only = parser.value(onlyOption).split(u',', Qt::SkipEmptyParts);
        options.themes = parser.value(themesOption).split(u',', Qt::SkipEmptyParts);
        return fm::app::runSnapshots(options);
    }
    const int shotDelay = parser.value(delayOption).toInt();
    const auto shootAndQuit = [&](QWidget *w) {
        if (!parser.isSet(shotOption))
            return;
        QTimer::singleShot(shotDelay, w, [w, file = shotTarget] {
            w->grab().save(file);
            QApplication::quit();
        });
    };
    if (parser.isSet(catalogOption)) {
        auto *catalog = new fm::app::CatalogWindow;
        catalog->setAttribute(Qt::WA_DeleteOnClose);
        catalog->show();
        shootAndQuit(catalog);
        return app.exec();
    }
    if (parser.isSet(compareOption)) {
        using Source = fm::app::ThumbnailCompare::Source;
        auto *compare = new fm::app::ThumbnailCompare;
        compare->setAttribute(Qt::WA_DeleteOnClose);
        compare->show();
        const QString value = parser.value(compareOption);
        const Source source = value == u"sample"   ? Source::Sample
                              : value == u"10000"  ? Source::Mock10k
                              : value == u"100000" ? Source::Mock100k
                                                   : Source::Folder;
        if (parser.isSet(measureOption)) {
            // 측정은 도착 흉내 없이(같은 조건으로 되풀이할 수 있게) — 다 읽히면 재고 출력하고 끝낸다
            compare->setArrivalSimulation(false);
            QObject::connect(compare, &fm::app::ThumbnailCompare::loaded, compare,
                             [compare, shotDelay, file = parser.isSet(shotOption) ? shotTarget : QString()] {
                                 QTimer::singleShot(shotDelay, compare, [compare, file] {
                                     compare->measureScroll();
                                     std::printf("%s", compare->lastReport().toUtf8().constData());
                                     std::fflush(stdout);
                                     if (!file.isEmpty())
                                         compare->grab().save(file);
                                     QApplication::quit();
                                 });
                             });
        } else {
            shootAndQuit(compare);
        }
        QTimer::singleShot(0, compare, [compare, source, value] { compare->load(source, value); });
        return app.exec();
    }
    if (parser.isSet(openOption)) {
        // 대화상자 하나만 — 목업 보드의 상태로 채운 변형
        QDialog *dialog = fm::dialogs::createDialog(parser.value(openOption));
        if (!dialog) {
            std::fprintf(stderr, "unknown dialog variant: %s\n", qPrintable(parser.value(openOption)));
            return 2;
        }
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        if (parser.isSet(shotOption)) {
            const QString file = parser.value(shotOption);
            QTimer::singleShot(parser.value(delayOption).toInt(), dialog, [dialog, file] {
                dialog->grab().save(file);
                QApplication::quit();
            });
        }
        return app.exec();
    }

    fm::app::MainWindow window;
    window.loadBoardState();

    fl::ListAppearance appearance = window.listAppearance();
    appearance.oneLineSeparator = separatorFrom(parser.value(sep1Option), appearance.oneLineSeparator);
    appearance.twoLineSeparator = separatorFrom(parser.value(sep2Option), appearance.twoLineSeparator);
    appearance.nameBelow = parser.isSet(nameBelowOption);
    appearance.invertCursor = parser.isSet(invCursorOption);
    appearance.invertSelection = parser.isSet(invSelOption);
    window.setListAppearance(appearance);
    if (parser.isSet(leftModeOption))
        window.leftPanel()->setViewMode(modeFrom(parser.value(leftModeOption), fl::ViewMode::OneLine));
    if (parser.isSet(rightModeOption))
        window.rightPanel()->setViewMode(modeFrom(parser.value(rightModeOption), fl::ViewMode::Auto));
    window.setActivePanel(parser.value(activeOption) == u"left"_s ? window.leftPanel() : window.rightPanel());

    using S = fm::settings::Section;
    if (settingsLoaded) {
        // 저장된 설정을 따른다. 명령줄에서 디자인 · 색 구성표를 주었으면 테마만 그대로 둔다.
        fm::settings::Sections sections = S::General | S::Tabs | S::Panel | S::Thumbs | S::Groups | S::Columns | S::FileOps | S::Elevation | S::Keys;
        if (!parser.isSet(designOption) && !parser.isSet(schemeOption))
            sections |= S::Appearance | S::Theme;
        window.applySettings(sections);
    } else {
        // 설정 파일이 없으면 지금 창 상태(보드)가 첫 설정 — 적용할 때까지 파일은 만들지 않는다
        const QString path = store.filePath();
        store.setFilePath(QString());
        window.captureSettings();
        store.setFilePath(path);
    }

    const QStringList wh = parser.value(sizeOption).split(u'x');
    window.resize(wh.value(0).toInt() > 0 ? wh.value(0).toInt() : 1440, wh.value(1).toInt() > 0 ? wh.value(1).toInt() : 900);
    window.show();
    window.activePanel()->focusView();
    if (parser.isSet(flowOption))
        QTimer::singleShot(0, &window, [&window, name = parser.value(flowOption)] { window.startElevationFlow(name == u"delete"_s ? 1 : 0); });

    if (parser.isSet(shotOption)) {
        const QString file = parser.value(shotOption);
        QTimer::singleShot(parser.value(delayOption).toInt(), &window, [&window, file] {
            window.grab().save(file);
            QApplication::quit();
        });
    }
    return app.exec();
}
