// Qt 표준 위젯 스타일 테스트(두 디자인) — 목업에 없는 Qt 기능: 틀 없는 콤보 · 스핀, 읽기 전용 입력, 납작한 단추,
// 도구 단추 메뉴(분할 칸 폭 · 드롭다운 꺾쇠 자리 · 누름 단추 폭은 그대로), 그룹 상자 제목 정렬 · 납작, 콤보 펼친 목록.

#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>

#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QImage>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTest>
#include <QToolButton>
#include <QVBoxLayout>

#include <memory>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace {

/// 위젯 하나를 담아 보이는 창 — 스타일 polish · 배치가 끝난 뒤 그림을 얻는다.
struct Host
{
    QWidget window;
    QVBoxLayout *layout = new QVBoxLayout(&window);

    template <typename W>
    W *add(W *widget)
    {
        layout->addWidget(widget);
        return widget;
    }
    void show()
    {
        window.resize(320, 240);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
    }
};

QImage shot(QWidget *w)
{
    QImage image = w->grab().toImage();
    image.setDevicePixelRatio(1.0);
    return image;
}

int colorDistance(const QColor &a, const QColor &b)
{
    return qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) + qAbs(a.blue() - b.blue());
}

} // namespace

/// 두 디자인 행 — 테스트마다 같은 데이터.
void addDesigns()
{
    QTest::addColumn<int>("design");
    QTest::newRow("standard") << int(fs::Design::Standard);
    QTest::newRow("watercolor") << int(fs::Design::Watercolor);
}

class TestStyle : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        fs::ThemeManager::instance().setScheme(fs::ThemeManager::Scheme::Light);
        fs::ThemeManager::instance().install(*static_cast<QApplication *>(QCoreApplication::instance()));
    }
    void cleanup() { fs::ThemeManager::instance().setDesign(fs::Design::Standard); }

    void framelessInputs_data() { addDesigns(); }
    void framelessInputs()
    {
        // 틀 없음(setFrame(false))이면 테두리를 그리지 않는다 — 왼쪽 가장자리 가운데가 부모 바탕과 같다
        QFETCH(int, design);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        Host host;
        auto *framedCombo = host.add(new QComboBox);
        auto *combo = host.add(new QComboBox);
        auto *spin = host.add(new QSpinBox);
        for (QComboBox *c : {framedCombo, combo})
            c->addItem(u"항목"_s);
        combo->setFrame(false);
        spin->setFrame(false);
        host.show();
        const QColor background = shot(&host.window).pixelColor(2, 2);
        const auto edge = [](QWidget *w) { return shot(w).pixelColor(0, w->height() / 2); };
        QVERIFY2(colorDistance(edge(framedCombo), background) > 30, "틀 있는 콤보는 테두리가 있다");
        QVERIFY(colorDistance(edge(combo), background) < 12);
        QVERIFY(colorDistance(edge(spin), background) < 12);
    }

    void readOnlyInput_data() { addDesigns(); }
    void readOnlyInput()
    {
        // 읽기 전용은 고칠 수 있는 칸과 바탕이 다르다(시안1 흐린 바탕 · 시안2 창 바탕)
        QFETCH(int, design);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        Host host;
        auto *editable = host.add(new QLineEdit);
        auto *readOnly = host.add(new QLineEdit);
        readOnly->setReadOnly(true);
        auto *spin = host.add(new QSpinBox);
        spin->setReadOnly(true);
        auto *spinEditable = host.add(new QSpinBox);
        host.show();
        const auto inside = [](QWidget *w) { return shot(w).pixelColor(w->width() / 2, 4); };
        QVERIFY(colorDistance(inside(editable), inside(readOnly)) > 6);
        QVERIFY(colorDistance(inside(spinEditable), inside(spin)) > 6);
    }

    void flatPushButton_data() { addDesigns(); }
    void flatPushButton()
    {
        // 납작한 단추는 마우스를 올리기 전에는 바탕 · 테두리가 없다
        QFETCH(int, design);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        Host host;
        auto *normal = host.add(new QPushButton(u"보통"_s));
        auto *flat = host.add(new QPushButton(u"납작"_s));
        flat->setFlat(true);
        host.show();
        const QColor background = shot(&host.window).pixelColor(2, 2);
        const auto edge = [](QWidget *w) { return shot(w).pixelColor(0, w->height() / 2); };
        QVERIFY(colorDistance(edge(normal), background) > 30);
        QVERIFY(colorDistance(edge(flat), background) < 12);
    }

    void toolButtonMenus_data() { addDesigns(); }
    void toolButtonMenus()
    {
        // 분할 단추의 메뉴 칸은 디자인 폭, 드롭다운(바로 · 지연 메뉴)은 꺾쇠 자리만큼 넓다.
        // 메뉴 달린 누름 단추의 폭(PM_MenuButtonIndicator)은 Fusion 값 그대로 — 주소 줄 드라이브 단추 보드 보존.
        QFETCH(int, design);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        Host host;
        QMenu menu;
        menu.addAction(u"이름"_s);
        auto toolButton = [&](QToolButton::ToolButtonPopupMode mode, bool withMenu) {
            auto *t = host.add(new QToolButton);
            t->setText(u"보기"_s);
            t->setToolButtonStyle(Qt::ToolButtonTextOnly);
            if (withMenu)
                t->setMenu(&menu);
            t->setPopupMode(mode);
            return t;
        };
        QToolButton *plain = toolButton(QToolButton::DelayedPopup, false);
        QToolButton *instant = toolButton(QToolButton::InstantPopup, true);
        QToolButton *split = toolButton(QToolButton::MenuButtonPopup, true);
        host.show();
        QVERIFY(instant->sizeHint().width() >= plain->sizeHint().width() + 8);

        QStyleOptionToolButton opt;
        opt.initFrom(split);
        opt.rect = split->rect();
        opt.features = QStyleOptionToolButton::MenuButtonPopup | QStyleOptionToolButton::HasMenu;
        opt.subControls = QStyle::SC_ToolButton | QStyle::SC_ToolButtonMenu;
        const int menuWidth = split->style()->subControlRect(QStyle::CC_ToolButton, &opt, QStyle::SC_ToolButtonMenu, split).width();
        QCOMPARE(menuWidth, design == int(fs::Design::Standard) ? 16 : 13);

        QPushButton push(u"드라이브"_s);
        QStyleOptionButton pushOpt;
        pushOpt.initFrom(&push);
        const std::unique_ptr<QStyle> fusion(QStyleFactory::create(u"Fusion"_s));
        QCOMPARE(push.style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &pushOpt, &push),
                 fusion->pixelMetric(QStyle::PM_MenuButtonIndicator, &pushOpt, &push));
    }

    void groupBoxTitle_data() { addDesigns(); }
    void groupBoxTitle()
    {
        // 제목 정렬(setAlignment) · 오른쪽에서 왼쪽 · 납작(setFlat)
        QFETCH(int, design);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        Host host;
        auto *centered = host.add(new QGroupBox(u"가운데 제목"_s));
        centered->setAlignment(Qt::AlignHCenter);
        auto *rtl = host.add(new QGroupBox(u"앞쪽 제목"_s));
        rtl->setCheckable(true);
        rtl->setLayoutDirection(Qt::RightToLeft);
        auto *flat = host.add(new QGroupBox(u"납작"_s));
        flat->setFlat(true);
        for (QGroupBox *gb : {centered, rtl, flat})
            new QVBoxLayout(gb);
        host.show();

        const auto subRect = [](QGroupBox *gb, QStyle::SubControl sc) {
            QStyleOptionGroupBox opt;
            opt.initFrom(gb);
            opt.text = gb->title();
            opt.textAlignment = gb->alignment();
            opt.features = gb->isFlat() ? QStyleOptionFrame::Flat : QStyleOptionFrame::None;
            opt.subControls = QStyle::SC_GroupBoxFrame | QStyle::SC_GroupBoxLabel
                              | (gb->isCheckable() ? QStyle::SC_GroupBoxCheckBox : QStyle::SC_None);
            return gb->style()->subControlRect(QStyle::CC_GroupBox, &opt, sc, gb);
        };
        const QRect label = subRect(centered, QStyle::SC_GroupBoxLabel);
        QVERIFY2(qAbs(label.center().x() - centered->rect().center().x()) <= 1, "가운데 제목");
        // 오른쪽에서 왼쪽: 체크 상자가 글자 오른쪽, 둘 다 오른쪽 끝에 붙는다
        const QRect check = subRect(rtl, QStyle::SC_GroupBoxCheckBox);
        const QRect rtlLabel = subRect(rtl, QStyle::SC_GroupBoxLabel);
        QVERIFY(check.left() > rtlLabel.left());
        QCOMPARE(check.right(), rtl->rect().right());
        // 납작: 안쪽 영역이 좌우 끝까지
        QCOMPARE(subRect(flat, QStyle::SC_GroupBoxContents).left(), 0);
    }

    void comboPopupSeparator_data() { addDesigns(); }
    void comboPopupSeparator()
    {
        // 펼친 목록의 구분선(insertSeparator)은 폭 전체 — 가운데 짧은 선이 아니다
        QFETCH(int, design);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        Host host;
        auto *combo = host.add(new QComboBox);
        combo->addItems({u"첫째"_s, u"둘째"_s});
        combo->insertSeparator(1);
        host.show();
        QImage image(200, 9, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter p(&image);
        QStyleOption opt;
        opt.rect = image.rect();
        combo->style()->drawPrimitive(QStyle::PE_IndicatorToolBarSeparator, &opt, &p, combo);
        p.end();
        const int y = 4;
        QVERIFY(image.pixelColor(20, y) != QColor(Qt::white));
        QVERIFY(image.pixelColor(180, y) != QColor(Qt::white));
    }
};

QTEST_MAIN(TestStyle)
#include "tst_style.moc"
