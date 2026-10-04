// Designer에 등록할 위젯 목록 — 묶음 · 헤더 · 설명 · 끌어 놓을 때의 기본 속성(domXml) · 아이콘 · 만드는 함수.
// .ui에서 쓰는 위젯(설정 · 대화상자 .ui 전부)과 PLAN §6.3의 부품, 파일 목록 두 종(Qtitan)을 담는다.

#include "FmDesignerPlugin.h"

#include <fmstyle/Glyphs.h>
#include <fmwidgets/Banner.h>
#include <fmwidgets/BarListCard.h>
#include <fmwidgets/BreadcrumbBar.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/Card.h>
#include <fmwidgets/CommandLine.h>
#include <fmwidgets/DialogCards.h>
#include <fmwidgets/DialogFooter.h>
#include <fmwidgets/DialogHeader.h>
#include <fmwidgets/DialogWidgets.h>
#include <fmwidgets/DriveButton.h>
#include <fmwidgets/FindBox.h>
#include <fmwidgets/FunctionKeyBar.h>
#include <fmwidgets/KeyChip.h>
#include <fmwidgets/Label.h>
#include <fmwidgets/PanelStatusBar.h>
#include <fmwidgets/PanelTabStrip.h>
#include <fmwidgets/ProgressBar.h>
#include <fmwidgets/SegmentedControl.h>
#include <fmwidgets/SettingsWidgets.h>
#include <fmwidgets/Switch.h>
#include <fmwidgets/Tag.h>
#include <fmwidgets/TransferGraph.h>

#ifdef FM_DESIGNER_WITH_FILELIST
#include <fmdialogs/RenameEngine.h>
#include <fmdialogs/RenamePreview.h>
#include <fmfilelist/FileListModel.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmfilelist/MockFileSource.h>
#include <fmfilelist/ThumbnailView.h>
#endif

#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <QTabBar>

#include <cmath>
#include <numbers>

using namespace Qt::StringLiterals;

namespace fm::designer {

namespace {

namespace fs = fm::style;

const QString kBase = u"FmStyle — 기본"_s;
const QString kDialog = u"FmStyle — 대화상자"_s;
const QString kSettings = u"FmStyle — 설정"_s;
const QString kMain = u"FmStyle — 메인 창"_s;
const QString kList = u"FmStyle — 파일 목록"_s;

const QColor kAccent(0x1F, 0x5F, 0xD1);
const QColor kLine(0x64, 0x6A, 0x75);

// ---------------------------------------------------------------- 아이콘(위젯 상자, 22 px)

QPixmap canvas(QPainter &p)
{
    QPixmap pm(QSize(22, 22) * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    p.begin(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    return pm;
}

enum class Shape { Button, Switch, Segment, Card, Progress, Graph, Pill, Footer, Radio, Swatch };

QIcon shapeIcon(Shape shape)
{
    QPainter p;
    QPixmap pm = canvas(p);
    switch (shape) {
    case Shape::Button:
        p.setPen(Qt::NoPen);
        p.setBrush(kAccent);
        p.drawRoundedRect(QRectF(2, 6, 18, 10), 2.5, 2.5);
        break;
    case Shape::Switch:
        p.setPen(Qt::NoPen);
        p.setBrush(kAccent);
        p.drawRoundedRect(QRectF(2, 6, 18, 10), 5, 5);
        p.setBrush(Qt::white);
        p.drawEllipse(QPointF(15, 11), 3.2, 3.2);
        break;
    case Shape::Segment:
        p.setPen(QPen(kLine, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(1.5, 6.5, 19, 9), 2, 2);
        p.drawLine(QPointF(8, 6.5), QPointF(8, 15.5));
        p.drawLine(QPointF(14, 6.5), QPointF(14, 15.5));
        p.fillRect(QRectF(8.5, 7, 5, 8), kAccent.lighter(170));
        break;
    case Shape::Card:
        p.setPen(QPen(kLine, 1));
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(2.5, 4.5, 17, 13), 3, 3);
        p.drawLine(QPointF(5, 11), QPointF(17, 11));
        break;
    case Shape::Progress:
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xEC, 0xEE, 0xF1));
        p.drawRoundedRect(QRectF(2, 9, 18, 4), 2, 2);
        p.setBrush(kAccent);
        p.drawRoundedRect(QRectF(2, 9, 11, 4), 2, 2);
        break;
    case Shape::Graph: {
        QPainterPath path;
        path.moveTo(2, 18);
        path.lineTo(5, 10);
        path.lineTo(9, 8);
        path.lineTo(12, 12);
        path.lineTo(16, 6);
        path.lineTo(20, 8);
        path.lineTo(20, 18);
        path.closeSubpath();
        p.fillPath(path, QColor(0x1F, 0x5F, 0xD1, 70));
        p.setPen(QPen(kAccent, 1.4));
        p.drawLine(QPointF(2, 9.5), QPointF(20, 9.5));
        break;
    }
    case Shape::Pill:
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xE3, 0xF1, 0xE8));
        p.drawRoundedRect(QRectF(2, 7, 18, 8), 4, 4);
        p.setBrush(QColor(0x1C, 0x7A, 0x4A));
        p.drawEllipse(QPointF(6, 11), 1.6, 1.6);
        break;
    case Shape::Footer:
        p.fillRect(QRectF(1, 12, 20, 9), QColor(0xF0, 0xF1, 0xF3));
        p.setPen(QPen(kLine, 1));
        p.drawLine(QPointF(1, 12), QPointF(21, 12));
        p.setPen(Qt::NoPen);
        p.setBrush(kAccent);
        p.drawRoundedRect(QRectF(8, 14.5, 6, 4), 1, 1);
        p.setBrush(kLine);
        p.drawRoundedRect(QRectF(15, 14.5, 5, 4), 1, 1);
        break;
    case Shape::Radio:
        p.setPen(QPen(kAccent, 1.4));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(7, 11), 4, 4);
        p.setBrush(kAccent);
        p.drawEllipse(QPointF(7, 11), 1.8, 1.8);
        p.setPen(QPen(kLine, 1.2));
        p.drawLine(QPointF(13, 9), QPointF(20, 9));
        p.drawLine(QPointF(13, 13), QPointF(18, 13));
        break;
    case Shape::Swatch:
        p.setPen(Qt::NoPen);
        for (const auto &[x, c] : {std::pair{5.0, kAccent}, std::pair{11.0, QColor(0x0F, 0x7A, 0x6E)}, std::pair{17.0, QColor(0x9A, 0x5B, 0x00)}}) {
            p.setBrush(c);
            p.drawEllipse(QPointF(x, 11), 3.2, 3.2);
        }
        break;
    }
    p.end();
    return QIcon(pm);
}

QIcon glyphIcon(fs::Glyph glyph)
{
    return fs::glyphIcon(glyph, kAccent, 22);
}

QIcon textIcon(const QString &text)
{
    QPainter p;
    QPixmap pm = canvas(p);
    p.setPen(QPen(kLine, 1));
    p.setBrush(Qt::white);
    p.drawRoundedRect(QRectF(1.5, 4.5, 19, 13), 2.5, 2.5);
    QFont f = p.font();
    f.setPixelSize(text.size() > 2 ? 7 : 9);
    f.setBold(true);
    p.setFont(f);
    p.setPen(kAccent);
    p.drawText(QRectF(1.5, 4.5, 19, 13), Qt::AlignCenter, text);
    p.end();
    return QIcon(pm);
}

// ---------------------------------------------------------------- domXml

QString dom(const QString &className, const QString &objectName, const QString &displayName, const QString &properties = {})
{
    return u"<ui language=\"c++\" displayname=\"%3\">\n <widget class=\"%1\" name=\"%2\">\n%4 </widget>\n</ui>\n"_s.arg(className, objectName,
                                                                                                               displayName, properties);
}

QString geometry(int w, int h)
{
    return u"  <property name=\"geometry\"><rect><x>0</x><y>0</y><width>%1</width><height>%2</height></rect></property>\n"_s.arg(w).arg(h);
}

QString prop(const QString &name, const QString &value)
{
    return u"  <property name=\"%1\"><string>%2</string></property>\n"_s.arg(name, value);
}

QString enumProp(const QString &name, const QString &value)
{
    return u"  <property name=\"%1\"><enum>%2</enum></property>\n"_s.arg(name, value);
}

QString boolProp(const QString &name, bool value)
{
    return u"  <property name=\"%1\"><bool>%2</bool></property>\n"_s.arg(name, value ? u"true"_s : u"false"_s);
}

QString listProp(const QString &name, const QStringList &values)
{
    QString items;
    for (const QString &v : values)
        items += u"<string>%1</string>"_s.arg(v);
    return u"  <property name=\"%1\"><stringlist>%2</stringlist></property>\n"_s.arg(name, items);
}

// ---------------------------------------------------------------- 예시 데이터(Designer 안)

void fillGraph(fm::ui::TransferGraph *g)
{
    constexpr double MB = 1024.0 * 1024.0;
    constexpr double pi = std::numbers::pi;
    g->start(qint64(1200) * 1024 * 1024);  // 예시 기록이 폭의 60 %쯤 차도록
    qint64 bytes = 0;
    for (int i = 0; i <= 80; ++i) {
        const double t = i * 0.25;
        double v = 38.0 + 4.0 * std::sin(t / 1.7) + 1.5 * std::sin(t / 0.8);
        if (t > 7.0 && t < 10.0)
            v -= 9.0 * std::sin((t - 7.0) / 3.0 * pi);
        v *= std::min(1.0, t / 1.2);
        g->addSample(bytes, qint64(t * 1000.0));
        bytes += qint64(v * MB * 0.25);
    }
}

void fillBarList(fm::ui::BarListCard *w)
{
    w->setSeries({{u"사용"_s, fs::Token::Accent}, {u"예약"_s, fs::Token::Accent, 0.47}});
    w->setRows({{u"C:"_s, u"시스템"_s, {182, 24}},
                {u"D:"_s, u"작업"_s, {96, 0}, {}, u"2"_s},
                {u"E:"_s, u"백업"_s, {240, 60}}});
    w->setBadgeLegend(u"읽지 못한 폴더"_s);
}

template <typename T>
std::function<QWidget *(QWidget *, bool)> plain()
{
    return [](QWidget *parent, bool) { return new T(parent); };
}

} // namespace

QList<WidgetInfo> widgetCatalog()
{
    using namespace fm::ui;
    QList<WidgetInfo> list;
    const auto add = [&list](WidgetInfo info) { list.append(std::move(info)); };

    // ================================================================ 기본
    add({u"fm::ui::Button"_s, kBase, u"fmwidgets/Button.h"_s, u"역할(보통 · 기본 · 위험 · 투명 · 링크)을 고르는 버튼"_s,
         u"role 속성으로 기본(강조색) · 위험 · 투명 · 링크 버튼을 고르고, compact로 작은 버튼, keyHint로 키 칩을 붙입니다."_s,
         dom(u"fm::ui::Button"_s, u"button"_s, u"버튼"_s, prop(u"text"_s, u"확인"_s)), shapeIcon(Shape::Button), false,
         plain<Button>()});
    add({u"fm::ui::Switch"_s, kBase, u"fmwidgets/Switch.h"_s, u"켬 / 끔 스위치"_s,
         u"QCheckBox와 같게 쓰는 스위치. onText · offText를 주면 상태에 따라 글자가 바뀝니다(글자는 스위치 왼쪽)."_s,
         dom(u"fm::ui::Switch"_s, u"switchWidget"_s, u"스위치"_s, prop(u"onText"_s, u"켬"_s) + prop(u"offText"_s, u"끔"_s)),
         shapeIcon(Shape::Switch), false, plain<Switch>()});
    add({u"fm::ui::SegmentedControl"_s, kBase, u"fmwidgets/SegmentedControl.h"_s, u"이어 붙인 버튼 중 하나를 고르는 컨트롤"_s,
         u"items에 항목을 넣고 currentIndex · currentIndexChanged로 고른 항목을 다룹니다. size로 크기(Mini · Small · Normal · Compact)."_s,
         dom(u"fm::ui::SegmentedControl"_s, u"segmentedControl"_s, u"세그먼트 컨트롤"_s,
             listProp(u"items"_s, {u"1줄"_s, u"2줄"_s, u"자동"_s}) + u"  <property name=\"currentIndex\"><number>0</number></property>\n"_s),
         shapeIcon(Shape::Segment), false, plain<SegmentedControl>()});
    add({u"fm::ui::Card"_s, kBase, u"fmwidgets/Card.h"_s, u"카드 — 설정 항목 묶음"_s,
         u"목록 바탕색과 구분선 테두리를 가진 컨테이너. 안에 레이아웃과 위젯(SettingRow 등)을 넣습니다."_s,
         dom(u"fm::ui::Card"_s, u"card"_s, u"카드"_s, geometry(320, 120)), shapeIcon(Shape::Card), true, plain<Card>()});
    add({u"fm::ui::ProgressBar"_s, kBase, u"fmwidgets/ProgressBar.h"_s, u"진행 막대(보통 · 일시 정지 · 오류, 두께 3종)"_s,
         u"state로 일시 정지 · 오류 색, thickness로 두께를 고릅니다. 범위를 0–0으로 두면 진행률 모름 움직임."_s,
         dom(u"fm::ui::ProgressBar"_s, u"progressBar"_s, u"진행 막대"_s,
             geometry(240, 16) + u"  <property name=\"value\"><number>62</number></property>\n"_s),
         shapeIcon(Shape::Progress), false, plain<ProgressBar>()});
    add({u"fm::ui::TransferGraph"_s, kBase, u"fmwidgets/TransferGraph.h"_s, u"처리 속도 그래프"_s,
         u"진행 창의 속도 그래프. Designer에서는 예시 기록으로 그리고, 실행 중에는 addSample()로 채웁니다."_s,
         dom(u"fm::ui::TransferGraph"_s, u"transferGraph"_s, u"처리 속도 그래프"_s, geometry(560, 140)), shapeIcon(Shape::Graph), false,
         [](QWidget *parent, bool samples) {
             auto *g = new TransferGraph(parent);
             if (samples)
                 fillGraph(g);
             return g;
         }});
    add({u"fm::ui::BarListCard"_s, kBase, u"fmwidgets/BarListCard.h"_s, u"가로 누적 막대 목록 카드"_s,
         u"setSeries()로 구간(이름 · 색 토큰), setRows()로 줄(코드 · 이름 · 구간 값 · 배지)을 넣습니다. 막대 길이는 가장 긴 줄에 견줍니다."_s,
         dom(u"fm::ui::BarListCard"_s, u"barListCard"_s, u"누적 막대 목록"_s, geometry(420, 100)), shapeIcon(Shape::Progress), false,
         [](QWidget *parent, bool samples) {
             auto *w = new BarListCard(parent);
             if (samples)
                 fillBarList(w);
             return w;
         }});
    add({u"fm::ui::Label"_s, kBase, u"fmwidgets/Label.h"_s, u"글자 역할을 고르는 레이블"_s,
         u"textRole로 목업의 글자 역할(본문 · 도움말 · 메타 · 제목 · 설정 페이지 제목 · 구역 제목 등), tone으로 색, elideMode로 말줄임."_s,
         dom(u"fm::ui::Label"_s, u"label"_s, u"레이블"_s, prop(u"text"_s, u"구역 제목"_s) + enumProp(u"textRole"_s, u"fm::ui::Label::SectionTitle"_s)),
         textIcon(u"Aa"_s), false, plain<Label>()});
    add({u"fm::ui::Tag"_s, kBase, u"fmwidgets/Tag.h"_s, u"태그(정상 · 정보 · 흐림 · 경고 · 위험)"_s, u"tone으로 색조, compact로 높이 20."_s,
         dom(u"fm::ui::Tag"_s, u"tag"_s, u"태그"_s, prop(u"text"_s, u"정상"_s) + enumProp(u"tone"_s, u"fm::ui::Tag::Ok"_s)), shapeIcon(Shape::Pill),
         false, plain<Tag>()});
    add({u"fm::ui::KeyChip"_s, kBase, u"fmwidgets/KeyChip.h"_s, u"키 칩"_s, u"keys에 \"Ctrl+M\"처럼 키를 적습니다."_s,
         dom(u"fm::ui::KeyChip"_s, u"keyChip"_s, u"키 칩"_s, prop(u"keys"_s, u"F5"_s)), textIcon(u"F5"_s), false, plain<KeyChip>()});
    add({u"fm::ui::Banner"_s, kBase, u"fmwidgets/Banner.h"_s, u"안내 배너(정보 · 경고 · 위험 · 정상)"_s, u"tone으로 색조와 아이콘, text로 글."_s,
         dom(u"fm::ui::Banner"_s, u"banner"_s, u"배너"_s,
             geometry(420, 36) + prop(u"text"_s, u"휴지통에서 언제든 원래 위치로 복원할 수 있습니다."_s)),
         glyphIcon(fs::Glyph::Info), false, plain<Banner>()});

    // ================================================================ 대화상자
    add({u"fm::ui::DialogHeader"_s, kDialog, u"fmwidgets/DialogHeader.h"_s, u"대화상자 머리(배지 · 제목 · 부제)"_s,
         u"tone으로 배지 색, glyph로 배지 아이콘, title · titleTo(→ 대상) · subtitle."_s,
         dom(u"fm::ui::DialogHeader"_s, u"header"_s, u"대화상자 머리"_s,
             prop(u"title"_s, u"3개 항목을 휴지통으로 옮길까요?"_s) + prop(u"subtitle"_s, u"D:\\Downloads · 합계 3.95 GB"_s)),
         glyphIcon(fs::Glyph::Trash), false, plain<DialogHeader>()});
    add({u"fm::ui::DialogFooter"_s, kDialog, u"fmwidgets/DialogFooter.h"_s, u"대화상자 버튼 영역"_s,
         u"--foot 바탕 영역. 안에 가로 레이아웃과 버튼을 넣습니다(오른쪽 정렬은 늘어나는 빈칸으로)."_s,
         dom(u"fm::ui::DialogFooter"_s, u"footer"_s, u"버튼 영역"_s, geometry(540, 62)), shapeIcon(Shape::Footer), true, plain<DialogFooter>()});
    add({u"fm::ui::PathEdit"_s, kDialog, u"fmwidgets/DialogWidgets.h"_s, u"경로 입력(최근 대상 · 찾아보기)"_s,
         u"path · history · historyVisible · browseVisible. 경로는 고정폭 글꼴."_s,
         dom(u"fm::ui::PathEdit"_s, u"pathEdit"_s, u"경로 입력"_s, geometry(420, 32) + prop(u"path"_s, u"D:\\Backup\\2026-09"_s)),
         glyphIcon(fs::Glyph::FolderOutline), false, plain<PathEdit>()});
    add({u"fm::ui::RecentTargetsBar"_s, kDialog, u"fmwidgets/DialogWidgets.h"_s, u"최근 대상 칩 줄"_s, u"targets에 경로 목록을 넣습니다."_s,
         dom(u"fm::ui::RecentTargetsBar"_s, u"recentTargets"_s, u"최근 대상"_s,
             geometry(420, 28) + listProp(u"targets"_s, {u"D:\\Backup"_s, u"E:\\Archive"_s, u"\\\\nas01\\share"_s})),
         glyphIcon(fs::Glyph::Clock), false, plain<RecentTargetsBar>()});
    add({u"fm::ui::ChoiceCard"_s, kDialog, u"fmwidgets/DialogWidgets.h"_s, u"고르는 카드(라디오 · 파일 종류 아이콘 · 설명)"_s,
         u"text · detail · keyHint · fileKind. 같은 부모의 카드끼리 하나만 고릅니다."_s,
         dom(u"fm::ui::ChoiceCard"_s, u"choiceCard"_s, u"고르는 카드"_s,
             geometry(240, 64) + prop(u"text"_s, u"텍스트 문서"_s) + prop(u"detail"_s, u".txt"_s)),
         glyphIcon(fs::Glyph::Check), false, plain<ChoiceCard>()});
    add({u"fm::ui::TokenButton"_s, kDialog, u"fmwidgets/DialogWidgets.h"_s, u"자리표시자 단추(다중 이름 변경)"_s, u"token(\"[N]\") · caption(\"이름\")."_s,
         dom(u"fm::ui::TokenButton"_s, u"tokenButton"_s, u"자리표시자 단추"_s, prop(u"token"_s, u"[N]"_s) + prop(u"caption"_s, u"이름"_s)),
         textIcon(u"[N]"_s), false, plain<TokenButton>()});
    add({u"fm::ui::FileSummaryList"_s, kDialog, u"fmwidgets/DialogWidgets.h"_s, u"파일 요약 목록(최대 N행 + \"… 외 N개\")"_s,
         u"setItems()로 채웁니다. maxVisibleRows보다 많으면 마지막 행이 \"… 외 N개\"가 됩니다."_s,
         dom(u"fm::ui::FileSummaryList"_s, u"fileSummary"_s, u"파일 요약 목록"_s, geometry(480, 84)), glyphIcon(fs::Glyph::File), false,
         [](QWidget *parent, bool samples) {
             auto *w = new FileSummaryList(parent);
             if (samples)
                 w->setItems({{u"Qt-6.11.0-windows-x64-msvc2026-offline-installer.exe"_s, u"3.74 GB"_s, ChoiceCard::ExeKind},
                              {u"QtitanDataGrid-9.3.0-Windows-MSVC2026-x64-Setup.exe"_s, u"186 MB"_s, ChoiceCard::ExeKind},
                              {u"vc_redist.x64.exe"_s, u"24.4 MB"_s, ChoiceCard::ExeKind}});
             return w;
         }});
    add({u"fm::ui::FolderPlanView"_s, kDialog, u"fmwidgets/DialogWidgets.h"_s, u"새 폴더 계획(있음 · 새로 · 잘못됨)"_s,
         u"setPlan()으로 경로 조각마다 상태를 넣습니다."_s,
         dom(u"fm::ui::FolderPlanView"_s, u"folderPlan"_s, u"새 폴더 계획"_s, geometry(420, 112)), glyphIcon(fs::Glyph::NewFolder), false,
         [](QWidget *parent, bool samples) {
             auto *w = new FolderPlanView(parent);
             using N = FolderPlanView::Node;
             if (samples)
                 w->setPlan({{u"D:\\Work"_s, N::Current}, {u"fm-core"_s, N::Existing}, {u"docs"_s, N::New}, {u"design"_s, N::New}});
             return w;
         }});
    add({u"fm::ui::KeyValueCard"_s, kDialog, u"fmwidgets/DialogCards.h"_s, u"키-값 카드(권한 대화상자)"_s,
         u"setRows()로 라벨 · 값(아이콘 · 고정폭 · 굵게 · 보조 글자 · 태그)을 넣습니다."_s,
         dom(u"fm::ui::KeyValueCard"_s, u"keyValueCard"_s, u"키-값 카드"_s, geometry(480, 90)), textIcon(u"k:v"_s), false,
         [](QWidget *parent, bool samples) {
             auto *w = new KeyValueCard(parent);
             if (samples) {
                 KeyValueCard::Row target{u"대상"_s, u"C:\\Program Files\\FM Tools"_s, KeyValueCard::Mono, IconSpec::folder()};
                 KeyValueCard::Row item{u"항목"_s, u"settings.ini"_s, KeyValueCard::Normal, IconSpec::file(fs::Token::KDoc), u"4.0 KB"_s};
                 KeyValueCard::Row need{u"필요"_s, QString(), KeyValueCard::Normal, {}, QString(), u"관리자 권한"_s, Tag::Warn};
                 w->setRows({target, item, need});
             }
             return w;
         }});
    add({u"fm::ui::ItemListCard"_s, kDialog, u"fmwidgets/DialogCards.h"_s, u"항목 목록 카드(최대 5행 + \"… 외 N개\")"_s,
         u"setItems()로 고정폭 항목과 오른쪽 태그를 넣습니다."_s,
         dom(u"fm::ui::ItemListCard"_s, u"itemListCard"_s, u"항목 목록 카드"_s, geometry(480, 90)), glyphIcon(fs::Glyph::Lock), false,
         [](QWidget *parent, bool samples) {
             auto *w = new ItemListCard(parent);
             if (samples)
                 w->setItems({{u"C:\\Program Files\\FM Tools\\redist"_s, u"권한 필요"_s, Tag::Warn},
                              {u"C:\\Program Files\\FM Tools\\settings.ini"_s, u"권한 필요"_s, Tag::Warn},
                              {u"C:\\Program Files\\WindowsApps\\Fabrikam.PhotoTools"_s, u"TrustedInstaller"_s, Tag::Mute}});
             return w;
         }});
    add({u"fm::ui::ActionCard"_s, kDialog, u"fmwidgets/DialogCards.h"_s, u"대안 동작 카드"_s, u"text · detail · glyph. 누르면 clicked."_s,
         dom(u"fm::ui::ActionCard"_s, u"actionCard"_s, u"대안 동작 카드"_s,
             geometry(480, 56) + prop(u"text"_s, u"다른 위치에 복사"_s) + prop(u"detail"_s, u"권한이 필요 없는 폴더를 고릅니다"_s)),
         glyphIcon(fs::Glyph::ArrowRight), false, plain<ActionCard>()});
    add({u"fm::ui::OptionRadio"_s, kDialog, u"fmwidgets/DialogCards.h"_s, u"설명 있는 라디오"_s, u"text · description · checked."_s,
         dom(u"fm::ui::OptionRadio"_s, u"optionRadio"_s, u"설명 있는 라디오"_s,
             prop(u"text"_s, u"속성별로 합치기"_s) + prop(u"description"_s, u"그 항목을 정한 가장 위 그룹의 값"_s)),
         shapeIcon(Shape::Radio), false, plain<OptionRadio>()});

    // ================================================================ 설정
    add({u"fm::ui::SettingRow"_s, kSettings, u"fmwidgets/SettingsWidgets.h"_s, u"설정 행(이름 · 설명 · 기본값과 다름 점 · 오른쪽 컨트롤)"_s,
         u"title · description · modified · rowHeight. 안에 넣은 위젯은 처음 표시될 때 오른쪽 컨트롤 칸으로 옮겨집니다. 이름에 &가 있으면 첫 컨트롤이 버디."_s,
         dom(u"fm::ui::SettingRow"_s, u"settingRow"_s, u"설정 행"_s,
             geometry(420, 52) + prop(u"title"_s, u"숨김 파일 표시"_s) + prop(u"description"_s, u"Ctrl+H"_s)),
         shapeIcon(Shape::Switch), true, plain<SettingRow>()});
    add({u"fm::ui::SearchField"_s, kSettings, u"fmwidgets/SettingsWidgets.h"_s, u"돋보기 검색 상자"_s,
         u"Enter · Esc를 먹어 대화상자 기본 단추로 새지 않습니다."_s,
         dom(u"fm::ui::SearchField"_s, u"searchField"_s, u"검색 상자"_s, prop(u"placeholderText"_s, u"설정 찾기"_s)),
         glyphIcon(fs::Glyph::Search), false, plain<SearchField>()});
    add({u"fm::ui::ThemeModeCard"_s, kSettings, u"fmwidgets/SettingsWidgets.h"_s, u"테마 카드(시스템 · 라이트 · 다크)"_s,
         u"mode로 그림, accent로 그림 첫 줄 색. 같은 부모의 카드끼리 하나만 고릅니다."_s,
         dom(u"fm::ui::ThemeModeCard"_s, u"themeCard"_s, u"테마 카드"_s,
             geometry(150, 120) + prop(u"text"_s, u"시스템(&amp;Y)"_s) + enumProp(u"mode"_s, u"fm::ui::ThemeModeCard::System"_s)),
         glyphIcon(fs::Glyph::App), false, plain<ThemeModeCard>()});
    add({u"fm::ui::ColorSwatchButton"_s, kSettings, u"fmwidgets/SettingsWidgets.h"_s, u"원형 색 견본"_s, u"color · addButton(점선 \"＋\")."_s,
         dom(u"fm::ui::ColorSwatchButton"_s, u"swatch"_s, u"색 견본"_s), shapeIcon(Shape::Swatch), false, plain<ColorSwatchButton>()});
    add({u"fm::ui::AccentPicker"_s, kSettings, u"fmwidgets/SettingsWidgets.h"_s, u"강조색 견본 묶음"_s,
         u"목업의 6색. accentChosen(std::optional<QColor>) — 첫 견본은 기준 색 없음(내장 강조)."_s,
         dom(u"fm::ui::AccentPicker"_s, u"accentPicker"_s, u"강조색 견본"_s), shapeIcon(Shape::Swatch), false, plain<AccentPicker>()});
    add({u"fm::ui::HexColorEdit"_s, kSettings, u"fmwidgets/SettingsWidgets.h"_s, u"16진수 색 입력"_s, u"#RRGGBB · #AARRGGBB. colorEdited(QColor)."_s,
         dom(u"fm::ui::HexColorEdit"_s, u"hexEdit"_s, u"16진수 색 입력"_s, geometry(96, 30)), textIcon(u"#"_s), false, plain<HexColorEdit>()});
    add({u"fm::ui::ToggleChip"_s, kSettings, u"fmwidgets/SettingsWidgets.h"_s, u"켜고 끄는 작은 단추(글꼴 효과)"_s, u"checkable 단추. 켜지면 강조 테두리."_s,
         dom(u"fm::ui::ToggleChip"_s, u"toggleChip"_s, u"토글 칩"_s, prop(u"text"_s, u"굵게(&amp;B)"_s) + boolProp(u"checkable"_s, true)),
         textIcon(u"B"_s), false, plain<ToggleChip>()});
    add({u"fm::ui::ColorPickButton"_s, kSettings, u"fmwidgets/SettingsWidgets.h"_s, u"색 단추(견본 · 16진수 · 팝업)"_s,
         u"color(std::optional<QColor>) · allowNone. 팝업: 추천 색 · 지정 안 함 · 사용자 지정…"_s,
         dom(u"fm::ui::ColorPickButton"_s, u"colorButton"_s, u"색 단추"_s), shapeIcon(Shape::Swatch), false, plain<ColorPickButton>()});
    add({u"fm::ui::CheckListCombo"_s, kSettings, u"fmwidgets/SettingsWidgets.h"_s, u"체크 목록 콤보"_s,
         u"setCheckItems() · checkedMask. 고른 항목을 \" · \"로 이어 보입니다."_s,
         dom(u"fm::ui::CheckListCombo"_s, u"checkListCombo"_s, u"체크 목록 콤보"_s, geometry(204, 30)), glyphIcon(fs::Glyph::ChevronDown), false,
         [](QWidget *parent, bool samples) {
             auto *w = new CheckListCombo(parent);
             if (samples) {
                 w->setCheckItems({u"이미지"_s, u"동영상"_s, u"PDF"_s, u"글꼴"_s});
                 w->setCheckedMask(0b0111);
             }
             return w;
         }});
    add({u"fm::ui::KeyCaptureEdit"_s, kSettings, u"fmwidgets/SettingsWidgets.h"_s, u"단축키 입력 칸"_s,
         u"startCapture()로 키를 받습니다. Esc 취소 · Backspace 지우기 · Tab도 입력."_s,
         dom(u"fm::ui::KeyCaptureEdit"_s, u"keyCapture"_s, u"단축키 입력 칸"_s), textIcon(u"Ctrl"_s), false, plain<KeyCaptureEdit>()});

    // ================================================================ 메인 창
    add({u"fm::ui::CommandLine"_s, kMain, u"fmwidgets/CommandLine.h"_s, u"명령줄(프롬프트 + 입력)"_s, u"prompt에 지금 경로. commandEntered(QString)."_s,
         dom(u"fm::ui::CommandLine"_s, u"commandLine"_s, u"명령줄"_s, geometry(720, 30) + prop(u"prompt"_s, u"D:\\Downloads"_s)), textIcon(u">_"_s),
         false, plain<CommandLine>()});
    add({u"fm::ui::FunctionKeyBar"_s, kMain, u"fmwidgets/FunctionKeyBar.h"_s, u"기능 키 막대"_s, u"addKey(키, 글자)로 칸을 넣습니다. triggered(keys)."_s,
         dom(u"fm::ui::FunctionKeyBar"_s, u"functionKeys"_s, u"기능 키 막대"_s, geometry(720, 32)), textIcon(u"F2"_s), false,
         [](QWidget *parent, bool samples) {
             auto *w = new FunctionKeyBar(parent);
             if (samples) {
                 for (const auto &[key, text] : {std::pair{u"F2"_s, u"이름 변경"_s}, std::pair{u"F3"_s, u"보기"_s}, std::pair{u"F5"_s, u"복사"_s},
                                                 std::pair{u"F6"_s, u"이동"_s}, std::pair{u"F7"_s, u"새 폴더"_s}, std::pair{u"F8"_s, u"삭제"_s}})
                     w->addKey(key, text);
             }
             return w;
         }});
    add({u"fm::ui::FindBox"_s, kMain, u"fmwidgets/FindBox.h"_s, u"현재 폴더 찾기 상자"_s, u"돋보기 · 자리표시 · Ctrl+F 칩."_s,
         dom(u"fm::ui::FindBox"_s, u"findBox"_s, u"찾기 상자"_s), glyphIcon(fs::Glyph::Search), false, plain<FindBox>()});
    add({u"fm::ui::BreadcrumbBar"_s, kMain, u"fmwidgets/BreadcrumbBar.h"_s, u"경로 이동 줄"_s,
         u"segments에 드라이브 다음 조각들. 조각을 누르면 segmentClicked, 빈 곳 두 번 누르면 직접 입력."_s,
         dom(u"fm::ui::BreadcrumbBar"_s, u"breadcrumbs"_s, u"경로 이동 줄"_s, geometry(360, 28) + listProp(u"segments"_s, {u"Work"_s, u"fm-core"_s})),
         glyphIcon(fs::Glyph::ChevronRight), false, plain<BreadcrumbBar>()});
    add({u"fm::ui::DriveButton"_s, kMain, u"fmwidgets/DriveButton.h"_s, u"드라이브 단추"_s, u"text에 드라이브 이름(\"D:\"), 메뉴를 붙여 씁니다."_s,
         dom(u"fm::ui::DriveButton"_s, u"driveButton"_s, u"드라이브 단추"_s, prop(u"text"_s, u"D:"_s)), glyphIcon(fs::Glyph::Drive), false,
         plain<DriveButton>()});
    add({u"fm::ui::PanelStatusBar"_s, kMain, u"fmwidgets/PanelStatusBar.h"_s, u"패널 상태 줄"_s, u"leftText(선택 · 합계) · rightText(폴더 수 등)."_s,
         dom(u"fm::ui::PanelStatusBar"_s, u"statusBar"_s, u"패널 상태 줄"_s,
             geometry(720, 26) + prop(u"leftText"_s, u"파일 3 / 11개 선택 · 3.95 GB / 7.00 GB"_s) + prop(u"rightText"_s, u"숨김 1개 표시 중"_s)),
         glyphIcon(fs::Glyph::More), false, plain<PanelStatusBar>()});
    add({u"fm::ui::PanelTabStrip"_s, kMain, u"fmwidgets/PanelTabStrip.h"_s, u"패널 탭 줄"_s,
         u"tabBar()에 탭을 넣고, newTabRequested · closeTabRequested를 받습니다. paneActive로 활성 패널 강조."_s,
         dom(u"fm::ui::PanelTabStrip"_s, u"tabStrip"_s, u"패널 탭 줄"_s, geometry(720, 36)), textIcon(u"Tab"_s), false,
         [](QWidget *parent, bool samples) {
             auto *w = new PanelTabStrip(parent);
             if (samples) {
                 w->tabBar()->addTab(u"fm-core"_s);
                 w->tabBar()->addTab(u"qtitan-samples"_s);
                 w->setPaneActive(true);
             }
             return w;
         }});

#ifdef FM_DESIGNER_WITH_FILELIST
    // ================================================================ 파일 목록(Qtitan)
    namespace fl = fm::filelist;
    add({u"fm::filelist::FileListView"_s, kList, u"fmfilelist/FileListView.h"_s, u"파일 목록(1줄 · 2줄 · 자동, Qtitan 밴드 보기)"_s,
         u"FileRoles 규약 모델(보통 FileSortProxy)을 setModel()로 줍니다. Designer에서는 샘플 폴더(D:\\Downloads)를 보입니다."_s,
         dom(u"fm::filelist::FileListView"_s, u"fileList"_s, u"파일 목록"_s, geometry(720, 360)), glyphIcon(fs::Glyph::ViewOneLine), false,
         [](QWidget *parent, bool samples) {
             auto *view = new fl::FileListView(parent);
             if (samples) {
                 const fl::MockFolder folder = fl::MockFileSource::right();
                 auto *model = new fl::FileListModel(folder.entries, view);
                 auto *proxy = new fl::FileSortProxy(view);
                 proxy->setSourceModel(model);
                 proxy->sort(-1);
                 view->setModel(proxy);
                 view->setSortIndicator(fl::NameColumn, Qt::AscendingOrder, false);
                 view->setPaneActive(true);
                 view->setCursorRow(folder.cursor);
             }
             return view;
         }});
    add({u"fm::filelist::ThumbnailView"_s, kList, u"fmfilelist/ThumbnailView.h"_s, u"섬네일 보기(Qt 목록 · Qtitan 카드 두 구현)"_s,
         u"backend로 구현을 고릅니다. 모델 규약은 FileListView와 같습니다. Designer에서는 샘플 사진 폴더를 보입니다."_s,
         dom(u"fm::filelist::ThumbnailView"_s, u"thumbnails"_s, u"섬네일 보기"_s, geometry(560, 420)), glyphIcon(fs::Glyph::ViewThumbnails), false,
         [](QWidget *parent, bool samples) {
             auto *view = new fl::ThumbnailView(parent);
             if (samples) {
                 const fl::MockFolder folder = fl::MockFileSource::thumbnailPreview();
                 auto *model = new fl::FileListModel(folder.entries, view);
                 auto *proxy = new fl::FileSortProxy(view);
                 proxy->setSourceModel(model);
                 proxy->sort(-1);
                 view->setModel(proxy);
                 view->setSortText(u"이름 ↑"_s);
                 view->setPaneActive(true);
                 view->setCursorRow(folder.cursor);
             }
             return view;
         }});
    add({u"fm::dialogs::RenamePreviewView"_s, kList, u"fmdialogs/RenamePreview.h"_s, u"다중 이름 변경 미리보기(Qtitan)"_s,
         u"RenamePreviewModel을 setModel()로 줍니다. 1줄 · 2줄 · 자동(긴 이름이 있으면 2줄)."_s,
         dom(u"fm::dialogs::RenamePreviewView"_s, u"renamePreview"_s, u"이름 변경 미리보기"_s, geometry(720, 220)),
         glyphIcon(fs::Glyph::MultiRename), false, [](QWidget *parent, bool samples) {
             auto *view = new fm::dialogs::RenamePreviewView(parent);
             if (samples) {
                 using Row = fm::dialogs::RenamePreviewRow;
                 const QDateTime date(QDate(2026, 9, 21), QTime(14, 2));
                 auto *model = new fm::dialogs::RenamePreviewModel(view);
                 model->setRows({{1, u"IMG_2041.heic"_s, u"2026-09-21_제주_001.heic"_s, Row::Ok, false, 3565158, date},
                                 {2, u"IMG_2042.heic"_s, u"2026-09-21_제주_002.heic"_s, Row::Ok, false, 3250585, date},
                                 {3, u"DSC04417.arw"_s, u"2026-09-21_제주_003.arw"_s, Row::Exists, false, 25795994, date}});
                 view->setModel(model);
             }
             return view;
         }});
#endif
    return list;
}

} // namespace fm::designer
