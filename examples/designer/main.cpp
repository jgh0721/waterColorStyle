// Qt Widgets Designer로 만든 폼(CopyDetails.ui)을 uic로 불러 쓰는 예제.
//
//   fm_designer_example                 창을 띄움
//   fm_designer_example --dark          다크 테마로
//   fm_designer_example --watercolor    시안2(워터컬러) 디자인으로
//   fm_designer_example --shot a.png    잠깐 돌린 뒤 스크린샷을 저장하고 끝냄
//   fm_designer_example --layouts       LayoutDemo.ui(배치 상자 · 승격한 메뉴 막대)를 띄움

#include "ui_CopyDetails.h"
#include "ui_LayoutDemo.h"

#include <fmstyle/ThemeManager.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QMainWindow>
#include <QRandomGenerator>
#include <QTimer>

#include <cmath>

using namespace Qt::StringLiterals;

namespace {

constexpr double MB = 1024.0 * 1024.0;
constexpr qint64 kTotal = qint64(6.24 * 1024 * 1024 * 1024);
constexpr qint64 kStepMs = 250;

class CopyDetails : public QWidget
{
public:
    explicit CopyDetails(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        ui.setupUi(this);

        // .ui 에서 정한 값(축 · 역할 · 켬/끔 글자)은 setupUi가 이미 넣었다. 여기서는 동작만 잇는다.
        connect(ui.axisSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, [this](int i) {
            ui.transferGraph->setAxis(i == 1 ? fm::ui::TransferGraph::Time : fm::ui::TransferGraph::Progress);
        });
        connect(ui.pauseButton, &QPushButton::toggled, this, [this](bool paused) {
            m_paused = paused;
            ui.pauseButton->setText(paused ? u"계속"_s : u"일시 정지"_s);
            ui.progressBar->setState(paused ? fm::ui::ProgressBar::Paused : fm::ui::ProgressBar::Normal);
            ui.transferGraph->setPaused(paused);
        });
        connect(ui.limitSwitch, &QCheckBox::toggled, this, [this](bool on) {
            ui.transferGraph->setSpeedLimit(on ? 40.0 * MB : 0.0);
        });
        connect(ui.cancelButton, &QPushButton::clicked, this, &QWidget::close);

        ui.transferGraph->start(kTotal);
        ui.transferGraph->addSample(0, 0);
        m_timer.setInterval(int(kStepMs));
        connect(&m_timer, &QTimer::timeout, this, &CopyDetails::step);
        m_timer.start();
    }

    void preroll(int steps)
    {
        for (int i = 0; i < steps; ++i)
            step();
    }

private:
    void step()
    {
        m_ms += kStepMs;
        if (!m_paused) {
            const double t = m_ms / 1000.0;
            double v = 44.0 + 6.0 * std::sin(t / 2.3) + 3.0 * std::sin(t / 0.9 + 1.0);
            v += (m_rng.generateDouble() - 0.5) * 3.0;
            if (ui.limitSwitch->isChecked())
                v = std::min(v, 40.0);
            m_bytes = std::min(kTotal, m_bytes + qint64(std::max(0.0, v) * MB * kStepMs / 1000.0));
        }
        ui.transferGraph->addSample(m_bytes, m_ms);
        const int percent = int(m_bytes * 100 / kTotal);
        ui.progressBar->setValue(percent);
        ui.percentLabel->setText(u"%1%  ·  %2"_s.arg(percent).arg(
            fm::ui::TransferGraph::formatRate(ui.transferGraph->currentSpeed())));
        if (m_bytes >= kTotal)
            m_timer.stop();
    }

    Ui::CopyDetails ui;
    QTimer m_timer;
    QRandomGenerator m_rng{7};
    qint64 m_bytes = 0;
    qint64 m_ms = 0;
    bool m_paused = false;
};

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption darkOption(u"dark"_s, u"다크 테마로 띄움"_s);
    const QCommandLineOption watercolorOption(u"watercolor"_s, u"시안2(워터컬러) 디자인으로 띄움"_s);
    const QCommandLineOption shotOption(u"shot"_s, u"스크린샷을 저장하고 끝냄"_s, u"file"_s);
    const QCommandLineOption layoutsOption(u"layouts"_s, u"LayoutDemo.ui(배치 상자 · 승격한 메뉴 막대)를 띄움"_s);
    parser.addOptions({darkOption, watercolorOption, shotOption, layoutsOption});
    parser.process(app);

    auto &theme = fm::style::ThemeManager::instance();
    if (parser.isSet(watercolorOption))
        theme.setDesign(fm::style::Design::Watercolor);  // install() 전에 정하면 처음부터 워터컬러
    theme.install(app);
    if (parser.isSet(darkOption))
        theme.setScheme(fm::style::ThemeManager::Scheme::Dark);

    if (parser.isSet(layoutsOption)) {
        // 배치 상자는 uic가 만든 자식을 스스로 배치하고(itemOrder · flexGrow 따름), 메뉴 막대 · 메뉴는 fm::ui로 승격돼 있다
        QMainWindow demo;
        Ui::LayoutDemo ui;
        ui.setupUi(&demo);
        demo.show();
        if (parser.isSet(shotOption)) {
            const QString file = parser.value(shotOption);
            QTimer::singleShot(600, &demo, [&demo, file] {
                demo.grab().save(file);
                QApplication::quit();
            });
        }
        return app.exec();
    }

    CopyDetails window;
    window.show();

    if (parser.isSet(shotOption)) {
        window.preroll(90);
        const QString file = parser.value(shotOption);
        QTimer::singleShot(900, &window, [&window, file] {
            window.grab().save(file);
            QApplication::quit();
        });
    }
    return app.exec();
}
