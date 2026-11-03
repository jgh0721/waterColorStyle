#include <fmfilelist/RecordPainting.h>
#include <QImage>
#include <QPainter>
#include <QTest>

using namespace fm::filelist;
using namespace fm::filelist::detail;

class TestRecordPainting final : public QObject
{
    Q_OBJECT
private slots:
    void selectedRecordGaps_data()
    {
        QTest::addColumn<qreal>("Dpr");
        QTest::addColumn<bool>("TwoLine");
        QTest::addColumn<int>("StateMode");
        QTest::addColumn<int>("Origin");
        for (const qreal Dpr : {1.0, 1.25, 1.5, 1.75, 2.0, 2.5})
            for (const bool TwoLine : {false, true})
                for (int StateMode = 0; StateMode < 4; ++StateMode)
                    for (const int Origin : {0, 3})
                        QTest::newRow(qPrintable(QString("%1-%2-%3-%4")
                            .arg(Dpr).arg(TwoLine).arg(StateMode).arg(Origin)))
                            << Dpr << TwoLine << StateMode << Origin;
    }
    void selectedRecordGaps()
    {
        QFETCH(qreal, Dpr);
        QFETCH(bool, TwoLine);
        QFETCH(int, StateMode);
        QFETCH(int, Origin);
        ListAppearance Appearance;
        Appearance.fontPx = 16; // Paper Fluent one-line pitch 25: fractional device boundaries at 150%.
        Appearance.oneLineSeparator = Appearance.twoLineSeparator = RecordSeparator::None;
        Appearance.invertSelection = StateMode == 1;
        Appearance.invertCursor = StateMode == 2;
        const auto Geometry = RecordGeometry::make(fm::style::Design::PaperFluent, TwoLine, Appearance);
        fm::style::ThemeColors Colors(fm::style::Variant::Dark, fm::style::Design::PaperFluent);
        const auto Surface = Colors[fm::style::Token::Surface];
        const auto Fill = Colors[StateMode == 2 ? fm::style::Token::InvCur
            : StateMode == 1 ? fm::style::Token::InvSel
            : StateMode == 3 ? fm::style::Token::SelIn : fm::style::Token::Sel];
        const int Count = 6;
        QImage Image(qRound(200 * Dpr), qRound((Origin + Count * Geometry.pitch) * Dpr),
            QImage::Format_ARGB32_Premultiplied);
        Image.setDevicePixelRatio(Dpr);
        Image.fill(Surface);
        QPainter Painter(&Image);
        for (int Row = 0; Row < Count; ++Row) {
            Painter.save();
            Painter.setClipRect(QRect(0, Origin + Row * Geometry.pitch, 200, Geometry.pitch));
            Painter.translate(0, Origin + Row * Geometry.pitch);
            paintRecordBackground(&Painter, QRect(0, 0, 200, Geometry.pitch), Geometry, Appearance,
                RecordState{StateMode != 3, StateMode != 2, StateMode == 2, false}, Row % 2, Colors);
            Painter.restore();
        }
        Painter.end();
        const int X = Image.width() / 2;
        for (int Row = 0; Row < Count; ++Row) {
            const int End = qRound((Origin + (Row + 1) * Geometry.pitch) * Dpr);
            QCOMPARE(Image.pixelColor(X, End - 1), Surface);
            QCOMPARE(Image.pixelColor(X, End - 2), Fill);
            QCOMPARE(Image.pixelColor(X, qRound((Origin + Row * Geometry.pitch) * Dpr)), Fill);
        }
    }
};
QTEST_APPLESS_MAIN(TestRecordPainting)
#include "tst_recordpainting.moc"
