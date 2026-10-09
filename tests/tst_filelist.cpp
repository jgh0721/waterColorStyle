// fmfilelist 단위 테스트 — 표기 규칙, 샘플 데이터의 상태 줄 문구, 정렬 규칙, 레코드 치수, 키 조작, 읽기 전용 실제 폴더.

#include <fmfilelist/ColumnValues.h>
#include <fmfilelist/FileGroups.h>
#include <fmfilelist/FileListModel.h>
#include <fmfilelist/FileListStats.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmfilelist/LocalFileSource.h>
#include <fmfilelist/MockFileSource.h>
#include <fmfilelist/ThumbnailPainter.h>
#include <fmfilelist/ThumbnailProvider.h>
#include <fmfilelist/ThumbnailView.h>

#include "ListPainting_p.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <array>

using namespace Qt::StringLiterals;
using namespace fm::filelist;

class TestFileList : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void formatSize_data();
    void formatSize();
    void sizeUnits_data();
    void sizeUnits();
    void dateFormats();
    void modelDisplayFormat();
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
    void thumbnailTargets();
    void columnSets();
    void propertyReader();
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

void TestFileList::sizeUnits_data()
{
    // 설정 › 파일 패널 › 크기 단위 — 바이트는 자리 구분, KB · MB는 올림(탐색기와 같음)
    QTest::addColumn<int>("unit");
    QTest::addColumn<qint64>("bytes");
    QTest::addColumn<QString>("text");
    QTest::newRow("auto") << int(SizeUnit::Auto) << qint64(1370) << u"1.3 KB"_s;
    QTest::newRow("bytes") << int(SizeUnit::Bytes) << qint64(1234567) << u"1,234,567 B"_s;
    QTest::newRow("kb 0") << int(SizeUnit::KB) << qint64(0) << u"0 KB"_s;
    QTest::newRow("kb 1 B") << int(SizeUnit::KB) << qint64(1) << u"1 KB"_s;
    QTest::newRow("kb grouped") << int(SizeUnit::KB) << qint64(1234567) << u"1,206 KB"_s;
    QTest::newRow("mb small") << int(SizeUnit::MB) << qint64(4096) << u"0.1 MB"_s;
    QTest::newRow("mb") << int(SizeUnit::MB) << qint64(186) * 1024 * 1024 << u"186.0 MB"_s;
    QTest::newRow("mb grouped") << int(SizeUnit::MB) << qint64(3.74 * 1024 * 1024 * 1024) << u"3,829.8 MB"_s;
    QTest::newRow("folder") << int(SizeUnit::KB) << qint64(-1) << QString();
}

void TestFileList::sizeUnits()
{
    QFETCH(int, unit);
    QFETCH(qint64, bytes);
    QFETCH(QString, text);
    QCOMPARE(fm::filelist::formatSize(bytes, SizeUnit(unit)), text);
}

void TestFileList::dateFormats()
{
    const QDateTime t(QDate(2026, 9, 27), QTime(23, 14, 5));
    QCOMPARE(formatDate(t, u"yyyy-MM-dd HH:mm"_s), u"2026-09-27 23:14"_s);
    QCOMPARE(formatDate(t, u"yyyy-MM-dd HH:mm:ss"_s), u"2026-09-27 23:14:05"_s);
    QCOMPARE(formatDate(t, QString::fromUtf16(DisplayFormat::kSystemShort)), QLocale::system().toString(t, QLocale::ShortFormat));
    QCOMPARE(formatDate(QDateTime(), u"yyyy"_s), QString());
    // 섬네일 정보 줄 — 시각 부분을 뺀 날짜
    QCOMPARE(formatDay(t, u"yyyy-MM-dd HH:mm"_s), u"2026-09-27"_s);
    QCOMPARE(formatDay(t, u"dd.MM.yyyy, hh:mm AP"_s), u"27.09.2026"_s);
    QCOMPARE(formatDay(t, u"yyyy년 M월 d일 HH:mm"_s), u"2026년 9월 27일"_s);
    QCOMPARE(formatDay(t, u"'at' HH:mm"_s), u"2026-09-27"_s);  // 날짜 기호가 없으면 기본
    QCOMPARE(formatDay(t, QString::fromUtf16(DisplayFormat::kSystemLong)), QLocale::system().toString(t.date(), QLocale::ShortFormat));
}

void TestFileList::modelDisplayFormat()
{
    // 모델마다 표시 형식 — 크기 · 날짜 열과 섬네일 정보 줄 역할이 따르고, 바꾸면 dataChanged
    FileEntry file;
    file.stem = u"report"_s;
    file.ext = u"pdf"_s;
    file.kind = Kind::Pdf;
    file.size = 1234567;
    file.modified = QDateTime(QDate(2026, 9, 25), QTime(16, 11));
    FileEntry folder;
    folder.stem = u"docs"_s;
    folder.kind = Kind::Folder;
    FileListModel model({file, folder});
    QCOMPARE(model.index(0, SizeColumn).data().toString(), u"1.2 MB"_s);
    QCOMPARE(model.index(0, ModifiedColumn).data().toString(), u"2026-09-25 16:11"_s);

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    model.setDisplayFormat({SizeUnit::KB, u"yy/MM/dd HH:mm"_s});
    QCOMPARE(changed.count(), 1);
    model.setDisplayFormat({SizeUnit::KB, u"yy/MM/dd HH:mm"_s});  // 같으면 알리지 않는다
    QCOMPARE(changed.count(), 1);
    QCOMPARE(model.index(0, SizeColumn).data().toString(), u"1,206 KB"_s);
    QCOMPARE(model.index(0, ModifiedColumn).data().toString(), u"26/09/25 16:11"_s);
    QCOMPARE(model.index(0, NameColumn).data(SizeTextRole).toString(), u"1,206 KB"_s);
    QCOMPARE(model.index(0, NameColumn).data(DateTextRole).toString(), u"26/09/25"_s);
    QCOMPARE(model.index(1, SizeColumn).data().toString(), QString());  // 폴더는 크기 없음
    QCOMPARE(model.index(1, NameColumn).data(SizeTextRole).toString(), QString());
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

    // 행 밀도(04 §2.1.2): 시안1 1줄 22 · 24 · 28, 2줄 40 · 44 · 52 — 시안2도 같은 차이. 2줄은 여전히 짝수.
    a = ListAppearance{};
    a.twoLineSeparator = RecordSeparator::Line;
    const std::pair<RowDensity, std::array<int, 4>> densities[] = {
        {RowDensity::Compact, {22, 40, 19, 34}},
        {RowDensity::Normal, {24, 44, 21, 38}},
        {RowDensity::Relaxed, {28, 52, 25, 46}},
    };
    for (const auto &[density, expected] : densities) {
        a.density = density;
        QCOMPARE(pitch(false, false, RecordSeparator::None), expected[0]);
        QCOMPARE(pitch(false, true, RecordSeparator::Line), expected[1]);
        QCOMPARE(pitch(true, false, RecordSeparator::None), expected[2]);
        QCOMPARE(pitch(true, true, RecordSeparator::Line), expected[3]);
        for (auto sep : {RecordSeparator::Space, RecordSeparator::Tint})
            QCOMPARE(pitch(false, true, sep) % 2, 0);
    }
    // 목록 글꼴 크기: 13 px 기준 1 px마다 1줄 +1, 2줄 +2(이름 · 메타 줄이 같이 늘고 위아래 여백은 그대로)
    a.density = RowDensity::Normal;
    a.fontPx = 15;
    QCOMPARE(pitch(false, false, RecordSeparator::None), 26);
    QCOMPARE(pitch(false, true, RecordSeparator::Line), 48);
    const RecordGeometry big = RecordGeometry::make(false, true, a);
    QCOMPARE(big.nameHeight, 24);
    QCOMPARE(big.metaHeight, 20);
    QCOMPARE(big.padTop, RecordGeometry::make(false, true, ListAppearance{a.oneLineSeparator, a.twoLineSeparator}).padTop);
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
#if !FM_WITH_QTITAN
    QSKIP("QtitanDataGrid 없는 빌드 — FileListView는 자리 표시라 키 조작이 없다");
#else
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
#endif
}

void TestFileList::autoTwoLine()
{
#if !FM_WITH_QTITAN
    QSKIP("QtitanDataGrid 없는 빌드 — FileListView는 자리 표시라 자동 2줄 판정을 하지 않는다");
#else
    // 모델은 뷰보다 오래 살아야 한다(Qtitan 편집기가 파괴 중에 인덱스를 읽는다).
    FileListModel model(MockFileSource::left().entries);
    auto LongEntries = MockFileSource::right().entries;
    // Ensure overflow independently of platform font metrics and demo names.
    for (auto& Entry : LongEntries) {
        if (!Entry.isUp()) Entry.stem = QString(256, QLatin1Char('W'));
    }
    FileListModel longNames(LongEntries);
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
    proxy.setSourceModel(&model);
    QTRY_VERIFY(!view.isTwoLine());
#endif
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

void TestFileList::thumbnailTargets()
{
    // 설정 › 섬네일 보기 › 대상 · 네트워크/이동식 드라이브는 아이콘만
    using P = ThumbnailProvider;
    QCOMPARE(P::targetOf(u"PNG"_s), int(P::Images));
    QCOMPARE(P::targetOf(u"heic"_s), int(P::Images));
    QCOMPARE(P::targetOf(u"mkv"_s), int(P::Videos));
    QCOMPARE(P::targetOf(u"pdf"_s), int(P::Pdf));
    QCOMPARE(P::targetOf(u"otf"_s), int(P::Fonts));
    QCOMPARE(P::targetOf(u"docx"_s), int(P::Documents));
    QCOMPARE(P::targetOf(u"exe"_s), 0);

    ThumbnailProvider provider;
    QVERIFY(provider.isTarget(u"pdf"_s));
    QVERIFY(!provider.isTarget(u"docx"_s));  // 기본 대상: 이미지 · 동영상 · PDF · 글꼴
    QSignalSpy changed(&provider, &ThumbnailProvider::settingsChanged);
    provider.setTargets(P::Images | P::Documents);
    provider.setTargets(P::Images | P::Documents);  // 같으면 알리지 않는다
    QCOMPARE(changed.count(), 1);
    QVERIFY(!provider.isTarget(u"pdf"_s));
    QVERIFY(provider.isTarget(u"docx"_s));

    QVERIFY(P::isSlowVolume(u"\\\\server\\share\\a.png"_s));
    QVERIFY(!P::isSlowVolume(QDir::tempPath() + u"/a.png"_s));
    provider.setSkipSlowVolumes(true);
    QCOMPARE(changed.count(), 2);
    QVERIFY(provider.thumbnail(u"\\\\server\\share\\a.png"_s, QDateTime(), 1).isNull());
    QVERIFY(!provider.isPending(u"\\\\server\\share\\a.png"_s, QDateTime(), 1));  // 요청하지 않는다

    // 실제 폴더 원본은 대상이 바뀌면 섬네일 역할을 다시 알리고, 대상이 아닌 파일은 종류 아이콘(Art::None)
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile pdf(dir.filePath(u"report.pdf"_s));
    QVERIFY(pdf.open(QIODevice::WriteOnly));
    pdf.write("%PDF-1.4");
    pdf.close();
    LocalFileSource source;
    source.setPath(dir.path());
    QTRY_COMPARE(source.model()->rowCount(), 2);  // ".." + report.pdf
    const QModelIndex file = source.model()->index(1, NameColumn);
    QSignalSpy roles(source.model(), &QAbstractItemModel::dataChanged);
    source.thumbnails()->setTargets(P::Images);
    QVERIFY(roles.count() >= 1);
    QVERIFY(roles.last().at(2).value<QList<int>>().contains(ArtRole));
    QCOMPARE(file.data(ArtRole).toInt(), int(Art::None));
}

void TestFileList::columnSets()
{
    // 설정 › 열 · 사용자 정의 열 — 폴더마다 세트 고르기, 메인 창 열 배치, 추가 열 값(05 §2.2)
    const ColumnSettings settings = ColumnSettings::defaults();
    auto indexOf = [&](const QString &id) {
        for (int i = 0; i < settings.sets.size(); ++i)
            if (settings.sets.at(i).id == id)
                return i;
        return -1;
    };
    // 경로 와일드카드(D:\Work\*) · 알려진 폴더(실제 다운로드 폴더만) · 맞는 것이 없으면 기본
    QCOMPARE(resolveColumnSet(settings, u"D:\\Work\\fm-core"_s, false, nullptr, nullptr), indexOf(u"source"_s));
    QCOMPARE(resolveColumnSet(settings, u"d:\\work\\fm-core\\src"_s, false, nullptr, nullptr), indexOf(u"source"_s));
    QCOMPARE(resolveColumnSet(settings, u"D:\\Downloads"_s, false, nullptr, nullptr), 0);
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QCOMPARE(resolveColumnSet(settings, QDir::toNativeSeparators(downloads), true, nullptr, nullptr), indexOf(u"downloads"_s));
    QCOMPARE(resolveColumnSet(settings, QDir::toNativeSeparators(downloads), false, nullptr, nullptr), 0);  // 샘플은 아님

    // 그룹 비율 — 이미지 · 영상 그룹이 60 % 이상(폴더 · ".." 제외)
    auto file = [](const QString &name, const QString &ext) {
        FileEntry e;
        e.stem = name;
        e.ext = ext;
        e.kind = kindForExtension(ext);
        e.size = 1000;
        return e;
    };
    FileEntry up;
    up.kind = Kind::Up;
    FileEntry folder;
    folder.stem = u"raw"_s;
    folder.kind = Kind::Folder;
    FileListModel photos({up, folder, file(u"a"_s, u"jpg"_s), file(u"b"_s, u"heic"_s), file(u"c"_s, u"mp4"_s), file(u"notes"_s, u"txt"_s)});
    const FileGroupMatcher groups(FileGroupSettings::defaults());
    QCOMPARE(resolveColumnSet(settings, u"E:\\Photos"_s, true, &groups, &photos), indexOf(u"photos"_s));  // 3/4 = 75 %
    FileListModel mixed({file(u"a"_s, u"jpg"_s), file(u"x"_s, u"txt"_s), file(u"y"_s, u"txt"_s)});
    QCOMPARE(resolveColumnSet(settings, u"E:\\Mixed"_s, true, &groups, &mixed), 0);  // 1/3
    ColumnSettings off = settings;
    off.sets[indexOf(u"photos"_s)].autoApply = false;
    QCOMPARE(resolveColumnSet(off, u"E:\\Photos"_s, true, &groups, &photos), 0);

    // 열 배치 — 기본 세트는 목록 기본 배치 그대로, 사진 세트는 기본 열(크기) + 추가 열(속성 · 식)
    QList<ColumnDef> extras;
    QCOMPARE(mainLayoutForSet(settings.sets.first(), &extras), ListColumnLayout::standard());
    QVERIFY(extras.isEmpty());
    const ListColumnLayout layout = mainLayoutForSet(settings.sets.at(indexOf(u"photos"_s)), &extras);
    QStringList extraIds;
    for (const ColumnDef &d : std::as_const(extras))
        extraIds.append(d.id);
    QCOMPARE(extraIds, (QStringList{u"taken"_s, u"resolution"_s, u"duration"_s, u"camera"_s}));
    QVERIFY(layout.find(SizeColumn));
    QVERIFY(!layout.find(ExtColumn));  // 확장자 열이 없으면 이름 칸에 확장자를 붙인다
    QVERIFY(!layout.find(ModifiedColumn));  // 1줄 · 2줄 모두 숨김 → 열 없음
    const ListColumn *camera = layout.find(ColumnCount + 3);
    QVERIFY(camera);
    QVERIFY(!camera->oneLine);  // 1줄에 보이지 않음 · 2줄 행 1
    QCOMPARE(camera->caption, u"카메라"_s);
    QCOMPARE(layout.requiredColumns(), ColumnCount + 4);

    // 추가 열 값 — 기본 필드 · 식 · 빈 값 규칙(— · 비움 · 다른 열로 대신)
    FileEntry report = file(u"report"_s, u"pdf"_s);
    report.modified = QDateTime(QDate(2026, 9, 25), QTime(16, 11));
    ColumnDef expr;
    expr.kind = ColumnDef::Kind::Expression;
    expr.source = u"[이름] ([확장자]) · [크기]"_s;
    QCOMPARE(extraColumnText(expr, {}, report, nullptr, {}), u"report (pdf) · 1000 B"_s);
    const QList<ColumnDef> photoColumns = settings.sets.at(indexOf(u"photos"_s)).columns;
    const ColumnDef taken = photoColumns.at(1);  // 촬영 날짜 — 없으면 수정한 날짜로
    QCOMPARE(extraColumnText(taken, photoColumns, report, nullptr, {}), u"2026-09-25 16:11"_s);
    QCOMPARE(extraColumnText(photoColumns.at(4), photoColumns, report, nullptr, {}), u"—"_s);     // 재생 시간: —
    QCOMPARE(extraColumnText(photoColumns.at(3), photoColumns, report, nullptr, {}), QString());   // 해상도: 비움

    // 모델의 추가 열 — 열 수 · 머리글 · 값, 바꾸면 reset
    FileListModel model({report});
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    model.setExtraColumns({extras, photoColumns, nullptr});
    QCOMPARE(reset.count(), 1);
    QCOMPARE(model.columnCount(), ColumnCount + 4);
    QCOMPARE(model.headerData(ColumnCount, Qt::Horizontal).toString(), u"촬영 날짜"_s);
    QCOMPARE(model.index(0, ColumnCount).data().toString(), u"2026-09-25 16:11"_s);
    QCOMPARE(model.index(0, ColumnCount + 2).data().toString(), u"—"_s);
    QCOMPARE(model.index(0, ColumnCount + 2).data(FullNameRole).toString(), u"report.pdf"_s);  // 그 밖의 역할은 항목 값
}

void TestFileList::propertyReader()
{
    // Windows 속성 — 작업 스레드에서 읽어 캐시, 끝나면 ready(path)
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(u"note.txt"_s);
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("hello");
    f.close();
    const QDateTime modified = QFileInfo(path).lastModified();
    PropertyReader reader;
    reader.setProperties({u"System.Size"_s, u"System.FileExtension"_s});
    QSignalSpy ready(&reader, &PropertyReader::ready);
    QCOMPARE(reader.value(path, modified, u"System.FileExtension"_s), QString());  // 처음에는 요청만
    QVERIFY(reader.isPending(path, modified));
    QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 10000);
    QCOMPARE(reader.value(path, modified, u"System.FileExtension"_s), u".txt"_s);
    QCOMPARE(reader.rawValue(path, modified, u"System.Size"_s), u"5"_s);  // 식에는 단위 없는 값
    QVERIFY(!reader.value(path, modified, u"System.Size"_s).isEmpty());
    QVERIFY(!reader.isPending(path, modified));
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
