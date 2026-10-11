// 떠 있는 위젯 · 진행 고리 테스트(두 디자인) — ProgressRing(범위 · 글 · 바쁨), ToolTip(붙이기 · 숨기기 · 배치 · 꼬리 ·
// 전역 설치 · 항목 보기 설명), Menu · MenuBar(그림자 판 · 자리 · 글리프 · 항목 높이 · 막대 아이콘 + 글),
// ContentDialog(단추 · 기본 단추 · 결과 · Esc · 덮는 층 · 시안2 제목 표시줄 닫기).

#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/ContentDialog.h>
#include <fmwidgets/Menu.h>
#include <fmwidgets/ProgressRing.h>
#include <fmwidgets/ToolTip.h>

#include <QApplication>
#include <QHelpEvent>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStyleOptionMenuItem>
#include <QStyleOptionTitleBar>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;
namespace fs = fm::style;
namespace ui = fm::ui;

namespace {

void addDesigns()
{
    QTest::addColumn<int>("design");
    QTest::newRow("standard") << int(fs::Design::Standard);
    QTest::newRow("watercolor") << int(fs::Design::Watercolor);
}

void sendToolTipEvent(QWidget *target, const QPoint &pos)
{
    QHelpEvent help(QEvent::ToolTip, pos, target->mapToGlobal(pos));
    QApplication::sendEvent(target, &help);
}

} // namespace

class TestPopups : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        fs::ThemeManager::instance().setScheme(fs::ThemeManager::Scheme::Light);
        fs::ThemeManager::instance().install(*static_cast<QApplication *>(QCoreApplication::instance()));
    }
    void cleanup()
    {
        ui::ToolTip::hideText();
        fs::ThemeManager::instance().setDesign(fs::Design::Standard);
    }

    // ------------------------------------------------------------------ 진행 고리

    void progressRing()
    {
        ui::ProgressRing ring;
        QSignalSpy busy(&ring, &ui::ProgressRing::busyChanged);
        ring.setRange(0, 80);
        ring.setValue(200);
        QCOMPARE(ring.value(), 80);  // 범위 안으로
        ring.setValue(20);
        QCOMPARE(ring.text(), u"25%"_s);
        ring.setValueDisplay(ui::ProgressRing::Actual);
        QCOMPARE(ring.text(), u"20"_s);
        QVERIFY(!ring.isBusy());
        ring.setRange(0, 0);  // 진행률 모름
        QVERIFY(ring.isBusy());
        QCOMPARE(ring.text(), QString());
        QCOMPARE(busy.count(), 1);
        ring.setRange(0, 100);
        ring.setBusy(true);
        QVERIFY(ring.isBusy());
        QCOMPARE(ring.heightForWidth(64), 64);
        QCOMPARE(ring.sizeHint(), QSize(48, 48));
    }

    void progressRingPaints_data() { addDesigns(); }
    void progressRingPaints()
    {
        // 72 %: 12시 바로 오른쪽(채움)은 강조색 계열, 9시(안 채움)는 아니다
        QFETCH(int, design);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        ui::ProgressRing ring;
        ring.setTextVisible(false);
        ring.resize(64, 64);
        ring.setValue(72);
        const QImage image = ring.grab().toImage();
        const auto blueish = [&](const QColor &c) { return c.alpha() > 0 && c.blue() > c.red() + 40; };
        const int ringY = design == int(fs::Design::Standard) ? 4 : 6;
        QVERIFY2(blueish(image.pixelColor(36, ringY)), qPrintable(image.pixelColor(36, ringY).name()));
        QVERIFY(!blueish(image.pixelColor(ringY, 32)));
    }

    // ------------------------------------------------------------------ 도구 설명

    void toolTipAttached_data() { addDesigns(); }
    void toolTipAttached()
    {
        QFETCH(int, design);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        QWidget window;
        auto *target = new QPushButton(u"대상"_s, &window);
        target->setGeometry(100, 100, 120, 30);
        window.resize(400, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        ui::ToolTip *tip = ui::ToolTip::attach(target, u"선택한 항목을 복사합니다"_s, u"복사"_s, ui::glyph::Copy);
        tip->setPlacement(ui::ToolTip::Below);
        tip->setTailVisible(true);
        tip->setHideDelay(0);
        sendToolTipEvent(target, QPoint(10, 10));
        QTRY_VERIFY(tip->isVisible());
        QVERIFY(tip->isWindow());
        // 대상 아래 · 꼬리는 위(대상 쪽)
        const QRect panel(tip->mapToGlobal(tip->panelRect().topLeft()), tip->panelRect().size());
        const QRect anchor(target->mapToGlobal(QPoint(0, 0)), target->size());
        QVERIFY(panel.top() > anchor.bottom());
        QCOMPARE(tip->tailEdge(), Qt::TopEdge);
        QVERIFY(qAbs(panel.center().x() - anchor.center().x()) <= 2);

        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(target, &leave);
        QTRY_VERIFY(!tip->isVisible());
        // 대상이 지워지면 함께 지워진다
        QPointer<ui::ToolTip> guard(tip);
        delete target;
        QVERIFY(guard.isNull());
    }

    void toolTipWrapsLongText()
    {
        ui::ToolTip tip;
        tip.setMaximumTextWidth(160);
        tip.setText(QString(u"아주 긴 설명 "_s).repeated(12));
        tip.showAt(QPoint(50, 50));
        QVERIFY(tip.isVisible());
        QVERIFY(tip.panelRect().width() <= 160 + 2 * 12 + 2);
        QVERIFY(tip.panelRect().height() > 40);  // 여러 줄
        tip.hideTip();
        QTRY_VERIFY(!tip.isVisible());
    }

    void toolTipGlobal()
    {
        auto &app = *static_cast<QApplication *>(QCoreApplication::instance());
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *button = new QPushButton(u"버튼"_s);
        button->setToolTip(u"위젯 도구 설명"_s);
        auto *list = new QListWidget;
        auto *item = new QListWidgetItem(u"항목"_s, list);
        item->setToolTip(u"항목 도구 설명"_s);
        layout->addWidget(button);
        layout->addWidget(list);
        window.resize(300, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        ui::ToolTip::installGlobal(app);
        QVERIFY(ui::ToolTip::isGlobalInstalled());
        sendToolTipEvent(button, QPoint(5, 5));
        QTRY_VERIFY(ui::ToolTip::shared()->isVisible());
        QCOMPARE(ui::ToolTip::shared()->text(), u"위젯 도구 설명"_s);

        const QRect itemRect = list->visualItemRect(item);
        sendToolTipEvent(list->viewport(), itemRect.center());
        QCOMPARE(ui::ToolTip::shared()->text(), u"항목 도구 설명"_s);

        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(list->viewport(), &leave);
        QTRY_VERIFY(!ui::ToolTip::shared()->isVisible());
        ui::ToolTip::uninstallGlobal(app);
        QVERIFY(!ui::ToolTip::isGlobalInstalled());
    }

    // ------------------------------------------------------------------ 메뉴

    void menu_data() { addDesigns(); }
    void menu()
    {
        QFETCH(int, design);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        ui::Menu menu;
        QVERIFY(menu.property(fs::props::kOwnPanel).toBool());
        QAction *copy = menu.addAction(ui::glyph::Copy, u"복사"_s, QKeySequence(u"F5"_s));
        QVERIFY(!copy->icon().isNull());
        QCOMPARE(copy->shortcut(), QKeySequence(u"F5"_s));
        ui::Menu *sub = menu.addMenu(ui::glyph::ViewOneLine, u"정렬"_s);
        QVERIFY(sub);
        QCOMPARE(sub->title(), u"정렬"_s);
        QCOMPARE(menu.contentsMargins(), menu.shadowMargins());

        // 띄우면 판의 왼쪽 위가 QMenu가 정한 자리(그림자만큼 옮김)
        menu.popup(QPoint(200, 150));
        QTRY_VERIFY(menu.isVisible());
        const QPoint panelTopLeft = menu.mapToGlobal(menu.panelRect().topLeft());
        QVERIFY2((panelTopLeft - QPoint(200, 150)).manhattanLength() <= 1,
                 qPrintable(u"%1,%2"_s.arg(panelTopLeft.x()).arg(panelTopLeft.y())));
        // 판 바깥(그림자 자리 모서리)은 투명, 판 안은 불투명
        const QImage image = menu.grab().toImage();
        const QRect panel = menu.panelRect();
        QVERIFY(image.pixelColor(panel.center().x(), panel.top() + 2).alpha() == 255);
        QVERIFY(image.pixelColor(image.width() - 1, 0).alpha() < 255);
        menu.hide();

        menu.setItemHeight(36);
        QCOMPARE(menu.actionGeometry(copy).height(), 36);
    }

    void menuBar_data() { addDesigns(); }
    void menuBar()
    {
        QFETCH(int, design);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        ui::MenuBar bar;
        bar.setNativeMenuBar(false);
        ui::Menu *file = bar.addMenu(u"파일"_s);
        QVERIFY(file);
        QAction *refresh = bar.addAction(ui::glyph::Refresh, u"새로 고침"_s);
        QAction *plain = bar.QMenuBar::addAction(u"새로 고침"_s);
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        // 아이콘 + 글 항목은 글만 있는 같은 이름 항목보다 아이콘만큼 넓다(Qt 기본은 아이콘만)
        QVERIFY(bar.actionGeometry(refresh).width() >= bar.actionGeometry(plain).width() + 16);
    }

    // ------------------------------------------------------------------ 내용 대화상자

    void contentDialog_data() { addDesigns(); }
    void contentDialog()
    {
        QFETCH(int, design);
        fs::ThemeManager::instance().setDesign(fs::Design(design));
        QWidget window;
        window.resize(500, 400);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        ui::ContentDialog dialog(&window);
        dialog.setTitle(u"영구 삭제할까요?"_s);
        dialog.setText(u"되돌릴 수 없습니다."_s);
        dialog.setPrimaryButtonText(u"삭제"_s);
        dialog.setCloseButtonText(u"취소"_s);
        QVERIFY(dialog.button(ui::ContentDialog::PrimaryButton)->isVisibleTo(&dialog));
        QVERIFY(!dialog.button(ui::ContentDialog::SecondaryButton)->isVisibleTo(&dialog));  // 글이 비면 숨김
        QVERIFY(dialog.button(ui::ContentDialog::PrimaryButton)->isDefault());
        dialog.setDefaultButton(ui::ContentDialog::CloseButton);
        QVERIFY(dialog.button(ui::ContentDialog::CloseButton)->isDefault());
        QVERIFY(!dialog.button(ui::ContentDialog::PrimaryButton)->isDefault());

        // 기본 단추 → Primary, 그동안 부모 창을 덮는 층이 있다
        bool smokeSeen = false;
        QTimer::singleShot(0, &dialog, [&] {
            smokeSeen = window.findChild<QWidget *>(u"fmContentDialogSmoke"_s) != nullptr;
            dialog.button(ui::ContentDialog::PrimaryButton)->click();
        });
        QCOMPARE(dialog.exec(), int(ui::ContentDialog::Primary));
        QVERIFY(smokeSeen);
        QTRY_VERIFY(!window.findChild<QWidget *>(u"fmContentDialogSmoke"_s));

        // Esc = None
        QTimer::singleShot(0, &dialog, [&] { QTest::keyClick(&dialog, Qt::Key_Escape); });
        QCOMPARE(dialog.exec(), int(ui::ContentDialog::None));

        // 시안2: 제목 표시줄의 닫기 단추 = None, 시안1은 제목 표시줄이 없다
        if (design == int(fs::Design::Watercolor)) {
            QSignalSpy closed(&dialog, &ui::ContentDialog::closeButtonClicked);
            QTimer::singleShot(0, &dialog, [&] {
                // 스타일이 정한 제목 표시줄 닫기 단추 자리를 눌렀다 뗀다
                QStyleOptionTitleBar opt;
                opt.initFrom(&dialog);
                opt.rect = dialog.captionRect();
                opt.titleBarFlags = Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint;
                opt.subControls = QStyle::SC_TitleBarLabel | QStyle::SC_TitleBarCloseButton;
                const QPoint close = dialog.style()->subControlRect(QStyle::CC_TitleBar, &opt, QStyle::SC_TitleBarCloseButton, &dialog).center();
                QTest::mousePress(&dialog, Qt::LeftButton, Qt::NoModifier, close);
                QTest::mouseRelease(&dialog, Qt::LeftButton, Qt::NoModifier, close);
            });
            QTimer::singleShot(2000, &dialog, [&] { dialog.reject(); });  // 실패해도 멈추지 않게
            QCOMPARE(dialog.exec(), int(ui::ContentDialog::None));
            QCOMPARE(closed.count(), 1);
        } else {
            QVERIFY(dialog.captionRect().isEmpty());
        }
    }

    void contentDialogAsk()
    {
        QWidget window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTimer::singleShot(0, &window, [] {
            for (QWidget *w : QApplication::topLevelWidgets()) {
                if (auto *d = qobject_cast<ui::ContentDialog *>(w); d && d->isVisible())
                    d->button(ui::ContentDialog::SecondaryButton)->click();
            }
        });
        const auto r = ui::ContentDialog::ask(&window, u"제목"_s, u"글"_s, u"예"_s, u"아니요"_s);
        QCOMPARE(r, ui::ContentDialog::Secondary);
    }
};

QTEST_MAIN(TestPopups)
#include "tst_popups.moc"
