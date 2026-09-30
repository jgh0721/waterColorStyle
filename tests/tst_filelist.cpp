// fmfilelist 단위 테스트 — 표기 규칙, 샘플 데이터의 상태 줄 문구, 정렬 규칙, 레코드 치수, 키 조작, 읽기 전용 실제 폴더.

#include <fmfilelist/FileListModel.h>
#include <fmfilelist/FileListStats.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmfilelist/LocalFileSource.h>
#include <fmfilelist/MockFileSource.h>
#include <fmfilelist/ThumbnailPainter.h>
#include <fmfilelist/ThumbnailView.h>

#include "ListPainting_p.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace fm::filelist;

class TestFileList : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void formatSize_data();
    void formatSize();
    void splitFileName();
    void kinds();
    void attributes();
    void mockStatusLines();
    void sortRules();
    void recordGeometry();
    void tileGeometry();
    void keyboardMarks();
    void autoTwoLine();
    void thumbnailBackendKeepsCursor();
    void localSource();
};

void TestFileList::formatSize_data()
{
    QTest::addColumn<qint64>("bytes");
    QTest::addColumn<QString>("text");
    QTest::newRow("B") << qint64(412) << u"412 B"_s;
    QTest::newRow("1.0 KB") << qint64(1024) << u"1.0 KB"_s;
    QTest::newRow("1.3 KB") << qint64(1370) << u"1.3 KB"_s;
    QTest::newRow("14.6 KB") << qint64(14950) << u"14.6 KB"_s;
    QTest::newRow("88.0 MB") << qint64(88) * 1024 * 1024 << u"88.0 MB"_s;
    QTest::newRow("186 MB") << qint64(186) * 1024 * 1024 << u"186 MB"_s;
    QTest::newRow("3.74 GB") << qint64(3.74 * 1024 * 1024 * 1024) << u"3.74 GB"_s;
    QTest::newRow("312 GB") << qint64(312) * 1024 * 1024 * 1024 << u"312 GB"_s;
    QTest::newRow("1.82 TB") << qint64(1.82 * 1024.0 * 1024 * 1024 * 1024) << u"1.82 TB"_s;
}

void TestFileList::formatSize()
{
    QFETCH(qint64, bytes);
    QFETCH(QString, text);
    QCOMPARE(fm::filelist::formatSize(bytes), text);
}

void TestFileList::splitFileName()
{
    QString stem, ext;
    fm::filelist::splitFileName(u".gitignore"_s, false, &stem, &ext);
    QCOMPARE(stem, u".gitignore"_s);
    QVERIFY(ext.isEmpty());
    fm::filelist::splitFileName(u"vc_redist.x64.exe"_s, false, &stem, &ext);
    QCOMPARE(stem, u"vc_redist.x64"_s);
    QCOMPARE(ext, u"exe"_s);
    fm::filelist::splitFileName(u"folder.name"_s, true, &stem, &ext);
    QCOMPARE(stem, u"folder.name"_s);
    QVERIFY(ext.isEmpty());
    fm::filelist::splitFileName(u"LICENSE"_s, false, &stem, &ext);
    QCOMPARE(stem, u"LICENSE"_s);
    QVERIFY(ext.isEmpty());
}

void TestFileList::kinds()
{
    QCOMPARE(kindForExtension(u"exe"_s), Kind::Exe);
    QCOMPARE(kindForExtension(u"MSI"_s), Kind::Exe);
    QCOMPARE(kindForExtension(u"cmd"_s), Kind::Exe);
    QCOMPARE(kindForExtension(u"pdf"_s), Kind::Pdf);
    QCOMPARE(kindForExtension(u"png"_s), Kind::Img);
    QCOMPARE(kindForExtension(u"mkv"_s), Kind::Img);  // 목업: 영상도 img
    QCOMPARE(kindForExtension(u"7z"_s), Kind::Zip);
    QCOMPARE(kindForExtension(u"natvis"_s), Kind::Code);
    QCOMPARE(kindForExtension(u"md"_s), Kind::Doc);
    QCOMPARE(kindForExtension(QString()), Kind::Doc);
    QCOMPARE(kindForExtension(u"ini"_s, true), Kind::Sys);
    QCOMPARE(kindToken(Kind::Folder), fm::style::Token::Folder);
}

void TestFileList::attributes()
{
    QCOMPARE(attributeText(ReadOnly | Archive), u"ra--"_s);
    QCOMPARE(attributeText(Archive | Hidden | System), u"-ahs"_s);
    QCOMPARE(attributeText(0), u"----"_s);
}

// 01 §1.3 A4d: 보드의 상태 줄 문구가 샘플 데이터에서 그대로 계산되어야 한다.
void TestFileList::mockStatusLines()
{
    FileListModel left(MockFileSource::left().entries);
    const FileListStats l = FileListStats::compute(&left);
    QCOMPARE(l.selectionText(), u"파일 2 / 11개 선택 · 9.9 KB / 41.1 KB"_s);
    QCOMPARE(l.secondaryText(), u"폴더 8개"_s);

    FileListModel right(MockFileSource::right().entries);
    const FileListStats r = FileListStats::compute(&right);
    QCOMPARE(r.selectionText(), u"파일 3 / 11개 선택 · 3.95 GB / 7.00 GB"_s);
    QCOMPARE(r.secondaryText(), u"숨김 1개 표시 중"_s);
}

void TestFileList::sortRules()
{
    FileListModel model(MockFileSource::left().entries);
    FileSortProxy proxy;
    proxy.setSourceModel(&model);

    // 정렬 열 -1: 보드 순서 그대로
    QCOMPARE(proxy.index(19, NameColumn).data(FullNameRole).toString(), u"build_release_x64.cmd"_s);

    auto name = [&](int row) { return proxy.index(row, NameColumn).data(FullNameRole).toString(); };
    auto isDir = [&](int row) { return proxy.index(row, NameColumn).data(IsDirRole).toBool(); };

    proxy.sort(NameColumn, Qt::AscendingOrder);
    QCOMPARE(name(0), u".."_s);
    for (int r = 1; r <= 8; ++r)
        QVERIFY2(isDir(r), qPrintable(name(r)));
    QVERIFY(!isDir(9));
    QCOMPARE(name(8), u"third_party"_s);

    // 내림차순에서도 ".."은 맨 위, 폴더가 먼저
    proxy.sort(NameColumn, Qt::DescendingOrder);
    QCOMPARE(name(0), u".."_s);
    QCOMPARE(name(1), u"third_party"_s);
    QVERIFY(isDir(8));
    QVERIFY(!isDir(9));

    // 크기순: 파일끼리 크기, 폴더는 앞
    proxy.sort(SizeColumn, Qt::AscendingOrder);
    QCOMPARE(name(9), u".editorconfig"_s);  // 412 B

    // 자연 정렬: 숫자는 크기로
    FileEntry a, b, c;
    a.stem = u"file10"_s;
    b.stem = u"file2"_s;
    c.stem = u"File1"_s;
    FileListModel numbers({a, b, c});
    FileSortProxy natural;
    natural.setSourceModel(&numbers);
    natural.sort(NameColumn, Qt::AscendingOrder);
    QCOMPARE(natural.index(0, NameColumn).data(FullNameRole).toString(), u"File1"_s);
    QCOMPARE(natural.index(1, NameColumn).data(FullNameRole).toString(), u"file2"_s);
    QCOMPARE(natural.index(2, NameColumn).data(FullNameRole).toString(), u"file10"_s);

    // 숨김 · 시스템 · 찾기 필터
    FileListModel right(MockFileSource::right().entries);
    FileSortProxy filtered;
    filtered.setSourceModel(&right);
    QCOMPARE(filtered.rowCount(), 12);
    filtered.setShowSystem(false);
    QCOMPARE(filtered.rowCount(), 11);
    filtered.setShowSystem(true);
    filtered.setShowHidden(false);
    QCOMPARE(filtered.rowCount(), 11);
    filtered.setShowHidden(true);
    filtered.setQuickFilter(u"QT"_s);
    QCOMPARE(filtered.rowCount(), 3);  // "..", Qt-6.11.0…, QtitanDataGrid…
}

void TestFileList::recordGeometry()
{
    using detail::RecordGeometry;
    ListAppearance a;
    auto pitch = [&](bool watercolor, bool two, RecordSeparator sep) {
        (two ? a.twoLineSeparator : a.oneLineSeparator) = sep;
        return RecordGeometry::make(watercolor, two, a).pitch;
    };
    // 시안1: 1줄 24 · 여백 26, 2줄 44 · 여백 48 + 2 · 틴트 46
    QCOMPARE(pitch(false, false, RecordSeparator::None), 24);
    QCOMPARE(pitch(false, false, RecordSeparator::Space), 26);
    QCOMPARE(pitch(false, true, RecordSeparator::Line), 44);
    QCOMPARE(pitch(false, true, RecordSeparator::Space), 50);
    QCOMPARE(pitch(false, true, RecordSeparator::Tint), 46);
    // 시안2: 1줄 21, 2줄 38 · 여백 42 + 2 · 틴트 40
    QCOMPARE(pitch(true, false, RecordSeparator::None), 21);
    QCOMPARE(pitch(true, true, RecordSeparator::Zebra), 38);
    QCOMPARE(pitch(true, true, RecordSeparator::Space), 44);
    QCOMPARE(pitch(true, true, RecordSeparator::Tint), 40);

    // 2줄 레코드는 Qtitan 줄 두 개로 나뉘므로 짝수
    for (bool wc : {false, true})
        for (auto sep : {RecordSeparator::None, RecordSeparator::Zebra, RecordSeparator::Line,
                         RecordSeparator::Space, RecordSeparator::Tint})
            QCOMPARE(pitch(wc, true, sep) % 2, 0);

    // 이름 아래: 메타 줄이 위
    a.twoLineSeparator = RecordSeparator::Line;
    a.nameBelow = true;
    const RecordGeometry g = RecordGeometry::make(false, true, a);
    const QRect record(0, 100, 400, 44);
    QVERIFY(g.metaLine(record).top() < g.nameLine(record).top());
    QCOMPARE(g.nameLine(record).height(), 22.0);
    QCOMPARE(g.metaLine(record).height(), 18.0);
    QCOMPARE(g.headerHeight, 45);
}

void TestFileList::tileGeometry()
{
    ThumbnailAppearance a;
    QCOMPARE(a.tileWidth(), 124);
    QCOMPARE(ThumbnailPainter::tileHeight(a), 163);  // 01 §1.7
    a.nameLines = 1;
    QCOMPARE(ThumbnailPainter::tileHeight(a), 147);
    a.size = 64;
    QCOMPARE(a.tileWidth(), 100);
    a.size = 256;
    QCOMPARE(a.tileWidth(), 284);
}

void TestFileList::keyboardMarks()
{
    FileListModel model(MockFileSource::left().entries);
    FileSortProxy proxy;
    proxy.setSourceModel(&model);
    FileListView view;
    view.setModel(&proxy);
    view.resize(900, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.setFocus();
    view.setCursorRow(9);
    QCOMPARE(view.cursorRow(), 9);
    QWidget *target = QApplication::focusWidget() ? QApplication::focusWidget() : view.focusProxy();
    QVERIFY(target);

    auto marked = [&](int row) { return proxy.index(row, NameColumn).data(MarkedRole).toBool(); };
    QVERIFY(!marked(9));
    QTest::keyClick(target, Qt::Key_Insert);
    QVERIFY(marked(9));
    QCOMPARE(view.cursorRow(), 10);
    QTest::keyClick(target, Qt::Key_Space);
    QVERIFY(marked(10));
    QCOMPARE(view.cursorRow(), 11);

    // ".."은 표시되지 않는다
    view.setCursorRow(0);
    QTest::keyClick(target, Qt::Key_Insert);
    QVERIFY(!marked(0));

    QTest::keyClick(target, Qt::Key_A, Qt::ControlModifier);
    const FileListStats all = FileListStats::compute(&proxy);
    QCOMPARE(all.markedFiles, all.files);
    QCOMPARE(all.markedFolders, all.folders);

    // Enter는 activated, Backspace는 upRequested
    QSignalSpy activated(&view, &FileListView::activated);
    QSignalSpy up(&view, &FileListView::upRequested);
    view.setCursorRow(7);
    QTest::keyClick(target, Qt::Key_Return);
    QCOMPARE(activated.size(), 1);
    QCOMPARE(activated.at(0).at(0).value<QModelIndex>().data(FullNameRole).toString(), u"src"_s);
    QTest::keyClick(target, Qt::Key_Backspace);
    QCOMPARE(up.size(), 1);
}

void TestFileList::autoTwoLine()
{
    // 모델은 뷰보다 오래 살아야 한다(Qtitan 편집기가 파괴 중에 인덱스를 읽는다).
    FileListModel model(MockFileSource::left().entries);
    FileListModel longNames(MockFileSource::right().entries);
    FileSortProxy proxy;
    proxy.setSourceModel(&model);
    FileListView view;
    view.setModel(&proxy);
    view.setViewMode(ViewMode::Auto);
    view.resize(900, 500);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTRY_VERIFY(!view.isTwoLine());        // 짧은 이름 · 넓은 폭 → 1줄
    view.resize(600, 500);
    QTRY_VERIFY(view.isTwoLine());         // 폭 < 640 → 2줄
    view.resize(700, 500);
    QTest::qWait(250);
    QVERIFY(view.isTwoLine());             // 되돌림은 80 px 여유(720 이상)
    view.resize(760, 500);
    QTRY_VERIFY(!view.isTwoLine());

    // 긴 이름이 많으면 넓어도 2줄
    proxy.setSourceModel(&longNames);
    QTRY_VERIFY(view.isTwoLine());
    QVERIFY(view.truncatedNameRatio(760) >= 0.25);
}

void TestFileList::thumbnailBackendKeepsCursor()
{
    FileListModel model(MockFileSource::thumbnailPreview().entries);
    FileSortProxy proxy;
    proxy.setSourceModel(&model);
    ThumbnailView view;
    view.setModel(&proxy);
    view.resize(800, 500);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.setCursorRow(5);
    QCOMPARE(view.cursorRow(), 5);
    view.setBackend(ThumbnailView::QtitanCards);
    QTRY_COMPARE(view.cursorRow(), 5);
    view.setBackend(ThumbnailView::QtList);
    QCOMPARE(view.cursorRow(), 5);
}

void TestFileList::localSource()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    for (const QString &name : {u"b10.txt"_s, u"b2.txt"_s, u"a.png"_s}) {
        QFile f(dir.filePath(name));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("fm");
    }
    QVERIFY(QDir(dir.path()).mkdir(u"sub"_s));

    LocalFileSource source;
    FileSortProxy proxy;
    proxy.setSourceModel(source.model());
    proxy.sort(NameColumn, Qt::AscendingOrder);
    source.setPath(dir.path());
    QTRY_COMPARE(proxy.rowCount(), 5);  // ".." + 폴더 1 + 파일 3
    auto name = [&](int row) { return proxy.index(row, NameColumn).data(FullNameRole).toString(); };
    QCOMPARE(name(0), u".."_s);
    QCOMPARE(name(1), u"sub"_s);
    QCOMPARE(name(2), u"a.png"_s);
    QCOMPARE(name(3), u"b2.txt"_s);   // 자연 정렬
    QCOMPARE(name(4), u"b10.txt"_s);
    QCOMPARE(proxy.index(3, SizeColumn).data(SizeBytesRole).toLongLong(), 2);
    QCOMPARE(proxy.index(3, NameColumn).data(KindRole).toInt(), int(Kind::Code));

    // 표시는 이름으로 보관(읽기 전용 원본이라도 표시는 된다)
    QVERIFY(proxy.setData(proxy.index(3, NameColumn), true, MarkedRole));
    QVERIFY(proxy.index(3, NameColumn).data(MarkedRole).toBool());
    QVERIFY(!proxy.setData(proxy.index(0, NameColumn), true, MarkedRole));
    QVERIFY(!(source.model()->flags(source.model()->index(1, NameColumn)) & Qt::ItemIsEditable));

    // 폴더 열기 · 위로 — 위로 올라오면 방금 나온 폴더 이름을 알려 준다
    QSignalSpy changed(&source, &LocalFileSource::pathChanged);
    QVERIFY(source.open(proxy.index(1, NameColumn)));
    QTRY_COMPARE(proxy.rowCount(), 1);  // 빈 폴더: ".."만
    QVERIFY(source.cdUp());
    QCOMPARE(changed.last().at(1).toString(), u"sub"_s);
    QTRY_COMPARE(proxy.rowCount(), 5);
}

QTEST_MAIN(TestFileList)
#include "tst_filelist.moc"
