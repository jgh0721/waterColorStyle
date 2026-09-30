// 파일 관리자 UI 데모(P4~) — 메인 창. 도구 모음 오른쪽의 스위치로 시안1 ↔ 시안2, 색 구성표 · 다크 색조를 바꾼다.
//   fmdemo                                         Main 보드 기본 상태
//   fmdemo --design watercolor --scheme navy        시안2 · 다크(남색)
//   fmdemo --left-mode thumb --right-mode thumb     섬네일(왼쪽 Qt 목록 · 오른쪽 Qtitan 카드)
//   fmdemo --sep2 tint --name-below --inv-cursor    표시 변형
//   fmdemo --shot main.png                          스크린샷을 저장하고 끝냄

#include "FilePanel.h"
#include "MainWindow.h"

#include <fmfilelist/ListAppearance.h>
#include <fmstyle/ThemeManager.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QScreen>
#include <QTimer>

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
    const QCommandLineOption shotOption(u"shot"_s, u"스크린샷을 저장하고 끝냅니다."_s, u"file"_s);
    const QCommandLineOption delayOption(u"shot-delay"_s, u"스크린샷까지 기다릴 시간(ms)."_s, u"ms"_s, u"900"_s);
    parser.addOptions({designOption, schemeOption, leftModeOption, rightModeOption, activeOption, sep1Option, sep2Option,
                       nameBelowOption, invCursorOption, invSelOption, sizeOption, shotOption, delayOption});
    parser.process(app);

    auto &theme = fs::ThemeManager::instance();
    theme.setDesign(parser.value(designOption) == u"watercolor"_s ? fs::Design::Watercolor : fs::Design::Standard);
    const QString scheme = parser.value(schemeOption);
    theme.setDarkTone(scheme == u"navy"_s ? fs::ThemeManager::DarkTone::Navy : fs::ThemeManager::DarkTone::Gray);
    theme.setScheme(scheme == u"system"_s  ? fs::ThemeManager::Scheme::System
                    : scheme == u"light"_s ? fs::ThemeManager::Scheme::Light
                                           : fs::ThemeManager::Scheme::Dark);
    theme.install(app);

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

    const QStringList wh = parser.value(sizeOption).split(u'x');
    window.resize(wh.value(0).toInt() > 0 ? wh.value(0).toInt() : 1440, wh.value(1).toInt() > 0 ? wh.value(1).toInt() : 900);
    window.show();
    window.activePanel()->focusView();

    if (parser.isSet(shotOption)) {
        const QString file = parser.value(shotOption);
        QTimer::singleShot(parser.value(delayOption).toInt(), &window, [&window, file] {
            window.grab().save(file);
            QApplication::quit();
        });
    }
    return app.exec();
}
