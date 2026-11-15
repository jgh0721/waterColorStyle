#include <fmstyle/ThemeManager.h>
#include <fmwidgets/BreadcrumbBar.h>
#include <fmwidgets/ReorderList.h>
#include <fmwidgets/ValueInputs.h>
#include <fmwidgets/DataInterpreter.h>
#include <fmwidgets/HexBufferEditor.h>
#include <fmwidgets/PathEntryDelegate.h>
#include <fmwidgets/ScrollPanel.h>
#include <fmwidgets/ContentStack.h>
#include <fmwidgets/Label.h>
#include <fmwidgets/SegmentedControl.h>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QtEndian>
#include <limits>
#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>

class RegistryWidgetTests final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        fm::style::ThemeManager::instance().install(*qobject_cast<QApplication*>(QCoreApplication::instance()));
    }
    void clearSegmentSelection()
    {
        fm::ui::SegmentedControl Control({"one", "two"});
        QSignalSpy Changed(&Control, &fm::ui::SegmentedControl::currentIndexChanged);
        Control.setCurrentIndex(1);
        QCOMPARE(Control.currentIndex(), 1);
        Control.setCurrentIndex(-1);
        QCOMPARE(Control.currentIndex(), -1);
        for (auto* Button : Control.findChildren<QPushButton*>())
            QVERIFY(!Button->isChecked());
        QCOMPARE(Changed.count(), 2);
        Control.setCurrentIndex(-1);
        QCOMPARE(Changed.count(), 2);
        Control.setCurrentIndex(0);
        QCOMPARE(Control.currentIndex(), 0);
        int Checked = 0;
        for (auto* Button : Control.findChildren<QPushButton*>())
            Checked += Button->isChecked() ? 1 : 0;
        QCOMPARE(Checked, 1);
    }
    void contentStackUsesCurrentPage()
    {
        fm::ui::ScrollPanel Panel;
        auto* Stack = new fm::ui::ContentStack;
        auto* Small = new QWidget; Small->setMinimumSize(100, 40);
        auto* Large = new QWidget; Large->setMinimumSize(100, 700);
        Stack->addWidget(Small); Stack->addWidget(Large);
        Panel.setContentWidget(Stack); Panel.resize(300, 200); Panel.show();
        QTRY_COMPARE(Panel.verticalScrollBar()->maximum(), 0);
        QCOMPARE(Stack->minimumSizeHint().height(), 40);
        Stack->setCurrentWidget(Large);
        QTRY_VERIFY(Panel.verticalScrollBar()->maximum() > 0);
        Stack->setCurrentWidget(Small);
        QTRY_COMPARE(Panel.verticalScrollBar()->maximum(), 0);
        Large->setMinimumHeight(900);
        QTRY_COMPARE(Panel.verticalScrollBar()->maximum(), 0);
    }
    void scrollPanelBounds()
    {
        fm::ui::ScrollPanel Panel;
        auto* Body = new QWidget; auto* Layout = new QVBoxLayout(Body);
        auto* Text = new fm::ui::Label(QString("long detail line\n").repeated(200), fm::ui::Label::Description, Body);
        Text->setWordWrap(true); Layout->addWidget(Text);
        Panel.setContentWidget(Body); Panel.resize(300, 200); Panel.show();
        QTRY_VERIFY(Panel.verticalScrollBar()->maximum() > 0);
        QCOMPARE(Panel.minimumSizeHint(), QSize(0, 0)); QCOMPARE(Panel.size(), QSize(300, 200));
        QCOMPARE(Panel.horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
        Panel.verticalScrollBar()->setValue(Panel.verticalScrollBar()->maximum());
        QVERIFY(Panel.verticalScrollBar()->value() > 0);
        Text->setText("short"); QTRY_COMPARE(Panel.verticalScrollBar()->maximum(), 0);
    }
    void breadcrumbOptions()
    {
        fm::ui::BreadcrumbBar Bar; Bar.resize(700, 32); Bar.setSegments({"HKCU", "Software"});
        Bar.setFullPath("HKCU\\Software"); Bar.show(); Bar.activateWindow(); QTRY_VERIFY(Bar.isActiveWindow());
        QSignalSpy Entered(&Bar, &fm::ui::BreadcrumbBar::pathEntered);
        QTest::mouseClick(&Bar, Qt::LeftButton, Qt::NoModifier, QPoint(690, 15));
        QVERIFY(!Bar.findChild<QLineEdit*>());
        Bar.setSingleClickEdit(true);
        QTest::mouseClick(&Bar, Qt::LeftButton, Qt::NoModifier, QPoint(690, 15));
        auto* Editor = Bar.findChild<QLineEdit*>(); QVERIFY(Editor); QVERIFY(Editor->isVisible());
        QCOMPARE(Editor->text(), QString("HKCU\\Software")); QTRY_VERIFY(Editor->hasFocus());
        QTest::keyClick(Editor, Qt::Key_Escape); QTRY_VERIFY(!Editor->isVisible());
        const auto SavedClipboard = QApplication::clipboard()->text();
        QApplication::clipboard()->setText("  HKLM\\Software  ");
        QTest::mouseClick(&Bar, Qt::MiddleButton, Qt::NoModifier, QPoint(690, 15));
        QCOMPARE(Entered.size(), 0);
        Bar.setMiddleClickPaste(true);
        QTest::mouseClick(&Bar, Qt::MiddleButton, Qt::NoModifier, QPoint(690, 15));
        const auto Count = Entered.size(); const auto Path = Count ? Entered.at(0).at(0).toString() : QString();
        QApplication::clipboard()->setText(SavedClipboard);
        QCOMPARE(Count, 1); QCOMPARE(Path, QString("HKLM\\Software"));
    }
    void pathRows()
    {
        fm::ui::ReorderListWidget List; auto* Delegate = new fm::ui::PathEntryDelegate(&List); List.setItemDelegate(Delegate);
        auto* Missing = new QListWidgetItem("%DOES_NOT_EXIST%\\bin", &List);
        Missing->setData(fm::ui::PathEntryDelegate::PathStatusRole, fm::ui::PathEntryDelegate::Unresolved);
        Missing->setData(fm::ui::PathEntryDelegate::PathWarningRole, "unresolved variable");
        auto* Existing = new QListWidgetItem("C:\\Windows", &List);
        Existing->setData(fm::ui::PathEntryDelegate::PathStatusRole, fm::ui::PathEntryDelegate::Exists);
        Existing->setData(fm::ui::PathEntryDelegate::ExpandedPathRole, "C:\\Windows");
        List.SetMoveModifiers(Qt::ControlModifier); List.resize(500, 240); List.show(); List.setCurrentRow(1);
        QTest::keyClick(&List, Qt::Key_Up, Qt::ControlModifier); QCOMPARE(List.item(0), Existing);
        QStyleOptionViewItem Option; Option.fontMetrics = QFontMetrics(List.font());
        QVERIFY(Delegate->sizeHint(Option, List.model()->index(0, 0)).height() >= 48);
        QVERIFY(!List.grab().isNull());
    }
    void hexEditing()
    {
        fm::ui::HexBufferEditor Editor; Editor.resize(800, 240); Editor.show();
        QVERIFY(Editor.setData(QByteArray::fromHex("000102")));
        QTest::keyClicks(&Editor, "AF"); QCOMPARE(Editor.data(), QByteArray::fromHex("af0102")); QCOMPARE(Editor.cursorOffset(), qsizetype(1));
        Editor.undo(); QCOMPARE(Editor.data(), QByteArray::fromHex("a00102"));
        Editor.undo(); QCOMPARE(Editor.data(), QByteArray::fromHex("000102"));
        Editor.redo(); Editor.redo(); QCOMPARE(Editor.data(), QByteArray::fromHex("af0102"));
        Editor.setSelection(1, 2); QVERIFY(Editor.replaceSelection(QByteArray::fromHex("ff00")));
        QCOMPARE(Editor.data(), QByteArray::fromHex("afff00")); Editor.undo(); QCOMPARE(Editor.data(), QByteArray::fromHex("af0102"));
        const auto Clipboard = QApplication::clipboard()->text();
        QApplication::clipboard()->setText("1 F2"); QSignalSpy Rejected(&Editor, &fm::ui::HexBufferEditor::editRejected);
        QTest::keyClick(&Editor, Qt::Key_V, Qt::ControlModifier);
        const auto RejectedCount = Rejected.size(); const auto AfterRejected = Editor.data();
        QApplication::clipboard()->setText("FE 00"); Editor.setSelection(1, 2);
        QTest::keyClick(&Editor, Qt::Key_V, Qt::ControlModifier); const auto AfterPaste = Editor.data();
        QApplication::clipboard()->setText(Clipboard);
        QCOMPARE(RejectedCount, 1); QCOMPARE(AfterRejected, QByteArray::fromHex("af0102")); QCOMPARE(AfterPaste, QByteArray::fromHex("affe00"));
        Editor.setReadOnly(true); Editor.setSelection(0, 3); QTest::keyClick(&Editor, Qt::Key_Delete); Editor.undo();
        QCOMPARE(Editor.data(), QByteArray::fromHex("affe00")); QVERIFY(!Editor.replaceSelection("x"));
        Editor.setReadOnly(false); Editor.setCursorOffset(Editor.data().size()); QTest::keyClicks(&Editor, "10");
        QCOMPARE(Editor.data(), QByteArray::fromHex("affe0010"));
    }
    void hexInsertMode()
    {
        fm::ui::HexBufferEditor Editor; QVERIFY(Editor.setData(QByteArray::fromHex("1234")));
        QSignalSpy Mode(&Editor, &fm::ui::HexBufferEditor::insertModeChanged);
        QTest::keyClick(&Editor, Qt::Key_Insert); QVERIFY(Editor.isInsertMode()); QCOMPARE(Mode.size(), 1);
        QTest::keyClicks(&Editor, "AB"); QCOMPARE(Editor.data(), QByteArray::fromHex("ab1234"));
        Editor.undo(); Editor.undo(); QCOMPARE(Editor.data(), QByteArray::fromHex("1234"));
        QTest::keyClick(&Editor, Qt::Key_Insert); QVERIFY(!Editor.isInsertMode());
        Editor.setCursorOffset(0); QTest::keyClicks(&Editor, "EF"); QCOMPARE(Editor.data(), QByteArray::fromHex("ef34"));
        const auto Clipboard = QApplication::clipboard()->text();
        Editor.setCursorOffset(0); QApplication::clipboard()->setText("AA BB CC");
        QTest::keyClick(&Editor, Qt::Key_V, Qt::ControlModifier); const auto Overwrite = Editor.data();
        Editor.setInsertMode(true); Editor.setCursorOffset(1); QApplication::clipboard()->setText("01");
        QTest::keyClick(&Editor, Qt::Key_V, Qt::ControlModifier); const auto Insert = Editor.data();
        QApplication::clipboard()->setText(Clipboard);
        QCOMPARE(Overwrite, QByteArray::fromHex("aabbcc")); QCOMPARE(Insert, QByteArray::fromHex("aa01bbcc"));
    }
    void hexLargeBuffer()
    {
        fm::ui::HexBufferEditor Editor; Editor.resize(800, 240); Editor.show();
        QVERIFY(Editor.setData(QByteArray(16 * 1024 * 1024, '\x55')));
        QVERIFY(Editor.verticalScrollBar()->maximum() > 1000);
        Editor.setCursorOffset(Editor.data().size() - 1); QTest::keyClicks(&Editor, "AA");
        QCOMPARE(quint8(Editor.data().back()), quint8(0xaa)); Editor.undo(); Editor.undo();
        QCOMPARE(quint8(Editor.data().back()), quint8(0x55)); QVERIFY(!Editor.grab().isNull());
    }
    void interpreter()
    {
        const auto Short = fm::ui::DataInterpreter::Interpret(QByteArray::fromHex("01"));
        QVERIFY(!Short[0].second.startsWith("0x")); QVERIFY(!Short[1].second.startsWith("0x"));
        const auto Rows = fm::ui::DataInterpreter::Interpret(QByteArray::fromHex("0102030405060708090a0b0c0d0e0f10"));
        QVERIFY(Rows[0].second.startsWith("0x04030201")); QCOMPARE(Rows[1].second, QString("0x0807060504030201"));
        QCOMPARE(Rows[4].second, QString("{04030201-0605-0807-090A-0B0C0D0E0F10}"));
        const auto Sid = fm::ui::DataInterpreter::Interpret(QByteArray::fromHex("01020000000000052000000020020000"));
        QCOMPARE(Sid[5].second, QString("S-1-5-32-544"));
        const auto Epoch = fm::ui::DataInterpreter::Interpret(QByteArray::fromHex("00803ed5deb19d01"));
        QCOMPARE(Epoch[2].second, QString("1970-01-01 00:00:00.000 UTC"));
        const auto Unicode = fm::ui::DataInterpreter::Interpret(QByteArray::fromHex("410042000000"));
        QCOMPARE(Unicode[3].second, QString("AB"));
    }
    void unsignedNumbers()
    {
        fm::ui::UnsignedNumberEdit Editor;
        QVERIFY(Editor.setValue(std::numeric_limits<quint32>::max()));
        QCOMPARE(Editor.draft(), QString("FFFFFFFF"));
        QVERIFY(Editor.setRadix(10)); QCOMPARE(Editor.draft(), QString("4294967295"));
        Editor.setDraft("4294967296"); QVERIFY(!Editor.value());
        QVERIFY(!Editor.setRadix(16)); QCOMPARE(Editor.draft(), QString("4294967296")); QCOMPARE(Editor.radix(), 10);
        Editor.setDraft("-1"); QVERIFY(!Editor.value());
        Editor.setBitWidth(64); QVERIFY(Editor.setValue(std::numeric_limits<quint64>::max()));
        QCOMPARE(Editor.draft(), QString("18446744073709551615"));
        QVERIFY(Editor.setRadix(16)); QCOMPARE(Editor.draft(), QString("FFFFFFFFFFFFFFFF"));
        Editor.setDraft("10000000000000000"); QVERIFY(!Editor.value());
        Editor.setDraft("0xabcdef"); QVERIFY(Editor.value()); QCOMPARE(*Editor.value(), quint64(0xabcdef));
        Editor.setDraft("invalid"); QVERIFY(Editor.setValue(std::numeric_limits<quint64>::max()));
        Editor.setBitWidth(32); QVERIFY(!Editor.value());
        QVERIFY(!Editor.setValue(std::numeric_limits<quint64>::max()));
    }
    void inlineDraft()
    {
        fm::ui::InlineNameEditor Editor; Editor.setText("draft"); Editor.show();
        QSignalSpy Commit(&Editor, &fm::ui::InlineNameEditor::commitRequested);
        QSignalSpy Cancel(&Editor, &fm::ui::InlineNameEditor::cancelRequested);
        QTest::keyClick(&Editor, Qt::Key_Return); QCOMPARE(Commit.size(), 1);
        QTest::keyClick(&Editor, Qt::Key_Escape); QCOMPARE(Cancel.size(), 1); QCOMPARE(Editor.text(), QString("draft"));
        fm::ui::MultilineField Multiline; Multiline.setPlainText("a\n\nb\n"); QCOMPARE(Multiline.toPlainText(), QString("a\n\nb\n"));
    }
    void reorderModifiers()
    {
        fm::ui::ReorderListWidget List; List.addItems({"A", "B", "C"}); List.resize(250, 200); List.show(); List.setFocus();
        List.setCurrentRow(1); QTest::keyClick(&List, Qt::Key_Up, Qt::AltModifier);
        QCOMPARE(List.item(0)->text(), QString("B")); QCOMPARE(List.currentRow(), 0);
        List.SetMoveModifiers(Qt::ControlModifier);
        List.setCurrentRow(1); QTest::keyClick(&List, Qt::Key_Up);
        QCOMPARE(List.currentRow(), 0); QCOMPARE(List.item(0)->text(), QString("B"));
        List.setCurrentRow(1); QTest::keyClick(&List, Qt::Key_Up, Qt::ControlModifier);
        QCOMPARE(List.item(0)->text(), QString("A")); QCOMPARE(List.currentRow(), 0);
    }
};
QTEST_MAIN(RegistryWidgetTests)
#include "tst_registrywidgets.moc"