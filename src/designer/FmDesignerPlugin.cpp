#include "FmDesignerPlugin.h"

#include <fmstyle/ThemeManager.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/Card.h>
#include <fmwidgets/ProgressBar.h>
#include <fmwidgets/SegmentedControl.h>
#include <fmwidgets/Switch.h>
#include <fmwidgets/TransferGraph.h>

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>

#include <cmath>
#include <numbers>

using namespace Qt::StringLiterals;

namespace fm::designer {

namespace {

// 미리 볼 디자인. 환경 변수 FMSTYLE_DESIGN=watercolor (또는 2) 로 Designer를 띄우면 시안2.
fm::style::Design previewDesign()
{
    static const fm::style::Design design = [] {
        const QString v = qEnvironmentVariable("FMSTYLE_DESIGN").trimmed().toLower();
        return (v == u"watercolor" || v == u"2" || v == u"시안2") ? fm::style::Design::Watercolor
                                                                   : fm::style::Design::Standard;
    }();
    return design;
}

// 미리 볼 변형 — FMSTYLE_VARIANT=light | dark | navy. 없으면 Designer 팔레트의 밝기로 고른다(prepare).
// 남색은 시안2 전용이라 시안1에서는 다크 색이 쓰인다.
fm::style::Variant previewVariant()
{
    const QString v = qEnvironmentVariable("FMSTYLE_VARIANT").trimmed().toLower();
    if (v == u"navy")
        return fm::style::Variant::Navy;
    if (v == u"dark")
        return fm::style::Variant::Dark;
    return fm::style::Variant::Light;
}

// Designer 안에서도 목업처럼 보이도록 만든 위젯에만 스타일을 건다.
// 팔레트는 건드리지 않는다 — 팔레트를 바꾸면 .ui에 저장될 수 있다.
QStyle *designerStyle()
{
    static QStyle *style = fm::style::createStyle(previewDesign());  // Designer가 끝날 때까지 쓴다
    return style;
}

QWidget *prepare(QWidget *w)
{
    // 앱이 이미 파일 관리자 스타일을 쓰면(예: 앱에서 QUiLoader로 .ui를 읽을 때) 그대로 둔다.
    // 플러그인은 fmstyle을 따로 정적 링크하므로 qobject_cast 대신 클래스 이름으로 본다.
    if (const QStyle *app = QApplication::style();
        app && (app->inherits("fm::style::FmStyle") || app->inherits("fm::style::WatercolorStyle")))
        return w;

    fm::style::Variant variant = previewVariant();
    if (!qEnvironmentVariableIsSet("FMSTYLE_VARIANT")) {
        const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
        variant = dark ? fm::style::Variant::Dark : fm::style::Variant::Light;
    }
    fm::style::ThemeScope::set(w, fm::style::ThemeManager::instance().colors(previewDesign(), variant), false);
    w->setStyle(designerStyle());
    const auto children = w->findChildren<QWidget *>();
    for (QWidget *c : children)
        c->setStyle(designerStyle());
    return w;
}

QIcon glyph(int kind)
{
    const qreal dpr = 2.0;
    QPixmap pm(QSize(22, 22) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor accent(0x1F, 0x5F, 0xD1);
    const QColor line(0x64, 0x6A, 0x75);
    switch (kind) {
    case 0:  // 버튼
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawRoundedRect(QRectF(2, 6, 18, 10), 2.5, 2.5);
        break;
    case 1:  // 스위치
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawRoundedRect(QRectF(2, 6, 18, 10), 5, 5);
        p.setBrush(Qt::white);
        p.drawEllipse(QPointF(15, 11), 3.2, 3.2);
        break;
    case 2:  // 세그먼트
        p.setPen(QPen(line, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(1.5, 6.5, 19, 9), 2, 2);
        p.drawLine(QPointF(8, 6.5), QPointF(8, 15.5));
        p.drawLine(QPointF(14, 6.5), QPointF(14, 15.5));
        p.fillRect(QRectF(8.5, 7, 5, 8), accent.lighter(170));
        break;
    case 3:  // 카드
        p.setPen(QPen(line, 1));
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(2.5, 4.5, 17, 13), 3, 3);
        p.drawLine(QPointF(5, 11), QPointF(17, 11));
        break;
    case 4:  // 진행 막대
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xEC, 0xEE, 0xF1));
        p.drawRoundedRect(QRectF(2, 9, 18, 4), 2, 2);
        p.setBrush(accent);
        p.drawRoundedRect(QRectF(2, 9, 11, 4), 2, 2);
        break;
    default: {  // 속도 그래프
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
        p.setPen(QPen(accent, 1.4));
        p.drawLine(QPointF(2, 9.5), QPointF(20, 9.5));
        break;
    }
    }
    return QIcon(pm);
}

QString dom(const QString &className, const QString &objectName, const QString &displayName,
            const QString &properties)
{
    return u"<ui language=\"c++\" displayname=\"%3\">\n <widget class=\"%1\" name=\"%2\">\n%4 </widget>\n</ui>\n"_s
        .arg(className, objectName, displayName, properties);
}

QString geometry(int w, int h)
{
    return u"  <property name=\"geometry\"><rect><x>0</x><y>0</y><width>%1</width><height>%2</height></rect></property>\n"_s
        .arg(w)
        .arg(h);
}

// 디자이너에서 빈 그래프 대신 모양을 볼 수 있도록 넣는 예시 기록
void fillSample(fm::ui::TransferGraph *g)
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

} // namespace

// ---------------------------------------------------------------------------------------------

WidgetPlugin::WidgetPlugin(WidgetInfo info, QObject *parent)
    : QObject(parent)
    , m_info(std::move(info))
{
}

QString WidgetPlugin::group() const
{
    return u"FmStyle 위젯"_s;
}

QWidget *WidgetPlugin::createWidget(QWidget *parent)
{
    return prepare(m_info.create(parent));
}

void WidgetPlugin::initialize(QDesignerFormEditorInterface *core)
{
    Q_UNUSED(core)
    m_initialized = true;
}

// ---------------------------------------------------------------------------------------------

FmWidgetCollection::FmWidgetCollection(QObject *parent)
    : QObject(parent)
{
    const auto add = [this](WidgetInfo info) { m_widgets.append(new WidgetPlugin(std::move(info), this)); };

    add({u"fm::ui::Button"_s, u"fmwidgets/Button.h"_s, u"역할(보통 · 기본 · 위험 · 투명)을 고르는 버튼"_s,
         u"role 속성으로 기본(강조색) · 위험 · 투명 버튼을 고르고, compact로 작은 버튼을 만듭니다."_s,
         dom(u"fm::ui::Button"_s, u"button"_s, u"버튼"_s,
             u"  <property name=\"text\"><string>확인</string></property>\n"_s),
         glyph(0), false, [](QWidget *parent) { return new fm::ui::Button(parent); }});

    add({u"fm::ui::Switch"_s, u"fmwidgets/Switch.h"_s, u"켬 / 끔 스위치"_s,
         u"QCheckBox와 같게 쓰는 스위치. onText · offText를 주면 상태에 따라 글자가 바뀝니다."_s,
         dom(u"fm::ui::Switch"_s, u"switchWidget"_s, u"스위치"_s,
             u"  <property name=\"text\"><string>끔</string></property>\n"_s),
         glyph(1), false, [](QWidget *parent) { return new fm::ui::Switch(parent); }});

    add({u"fm::ui::SegmentedControl"_s, u"fmwidgets/SegmentedControl.h"_s, u"이어 붙인 버튼 중 하나를 고르는 컨트롤"_s,
         u"items에 항목을 넣고 currentIndex · currentIndexChanged로 고른 항목을 다룹니다."_s,
         dom(u"fm::ui::SegmentedControl"_s, u"segmentedControl"_s, u"세그먼트 컨트롤"_s,
             u"  <property name=\"items\"><stringlist><string>1줄</string><string>2줄</string>"
             "<string>자동</string></stringlist></property>\n"
             "  <property name=\"currentIndex\"><number>0</number></property>\n"_s),
         glyph(2), false, [](QWidget *parent) { return new fm::ui::SegmentedControl(parent); }});

    add({u"fm::ui::Card"_s, u"fmwidgets/Card.h"_s, u"카드 — 설정 항목 묶음"_s,
         u"목록 바탕색과 구분선 테두리를 가진 컨테이너. 안에 레이아웃과 위젯을 넣습니다."_s,
         dom(u"fm::ui::Card"_s, u"card"_s, u"카드"_s, geometry(320, 120)),
         glyph(3), true, [](QWidget *parent) { return new fm::ui::Card(parent); }});

    add({u"fm::ui::ProgressBar"_s, u"fmwidgets/ProgressBar.h"_s, u"진행 막대 (보통 · 일시 정지 · 오류)"_s,
         u"state 속성으로 일시 정지 · 오류 색을 고릅니다."_s,
         dom(u"fm::ui::ProgressBar"_s, u"progressBar"_s, u"진행 막대"_s,
             geometry(240, 16) + u"  <property name=\"value\"><number>62</number></property>\n"_s),
         glyph(4), false, [](QWidget *parent) { return new fm::ui::ProgressBar(parent); }});

    add({u"fm::ui::TransferGraph"_s, u"fmwidgets/TransferGraph.h"_s, u"처리 속도 그래프"_s,
         u"진행 창의 속도 그래프. 디자이너에서는 예시 기록으로 그리고, 실행 중에는 addSample()로 채웁니다."_s,
         dom(u"fm::ui::TransferGraph"_s, u"transferGraph"_s, u"처리 속도 그래프"_s, geometry(560, 140)),
         glyph(5), false, [](QWidget *parent) {
             auto *g = new fm::ui::TransferGraph(parent);
             fillSample(g);
             return g;
         }});
}

} // namespace fm::designer
