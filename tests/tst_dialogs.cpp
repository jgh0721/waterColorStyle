// 파일 작업 · 권한 대화상자(P5 · P6) 테스트 — 다중 이름 변경 엔진(02 §9.5 표의 값), 이름 검사 · 이동 해석 · 새 폴더 계획,
// 진행 시뮬레이터, 대화상자 변형 전체 생성, 다중 이름 변경 · 진행 창 동작, 권한 대화상자 8종 · 흐름 시뮬레이션,
// 설정 창(P7) 페이지 동작.

#include <fmdialogs/DialogCatalog.h>
#include <fmdialogs/ElevationDialog.h>
#include <fmdialogs/ElevationFlow.h>
#include <fmdialogs/FileOpContext.h>
#include <fmdialogs/FileOpDialogs.h>
#include <fmdialogs/MultiRenameDialog.h>
#include <fmdialogs/Planners.h>
#include <fmdialogs/ProgressDialog.h>
#include <fmdialogs/ProgressSimulator.h>
#include <fmdialogs/RenameEngine.h>
#include <fmdialogs/RenamePreview.h>
#include <fmdialogs/SettingsDialog.h>
#include <fmsettings/Commands.h>
#include <fmsettings/SettingsStore.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/DialogCards.h>
#include <fmwidgets/SegmentedControl.h>
#include <fmwidgets/SettingsWidgets.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

#include <memory>

using namespace Qt::StringLiterals;
using namespace fm::dialogs;

class TestDialogs : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void renameSummaries_data();
    void renameSummaries();
    void renameMockupComparison();
    void renameRules();
    void nameChecks();
    void moveRenamePlans();
    void folderPlans();
    void simulator();
    void allVariants();
    void multiRename();
    void progress();
    void elevationDialogs_data();
    void elevationDialogs();
    void elevationChoices();
    void elevationFlows();
    void fileOpSettings();
    void elevationSettings();
    void settingsKeys();
    void settingsPages();
};

// ------------------------------------------------------------------------------------ 규칙 엔진

void TestDialogs::renameSummaries_data()
{
    QTest::addColumn<QString>("mask");
    QTest::addColumn<int>("extCase");
    QTest::addColumn<QString>("find");
    QTest::addColumn<QString>("replace");
    QTest::addColumn<int>("changed");
    QTest::addColumn<int>("bad");
    QTest::addColumn<int>("same");

    const QString board = u"[Y]-[M]-[D]_제주_[C]"_s;
    const int lower = int(ExtCase::Lower);
    // 02 §9.5 — Windows 권장(대소문자 무시) 값
    QTest::newRow("default") << board << lower << QString() << QString() << 10 << 1 << 1;
    QTest::newRow("extKeep") << board << int(ExtCase::Keep) << QString() << QString() << 10 << 1 << 1;
    QTest::newRow("extUpper") << board << int(ExtCase::Upper) << QString() << QString() << 11 << 1 << 0;
    QTest::newRow("tokenAppend") << board + u"[t]"_s << lower << QString() << QString() << 12 << 0 << 0;
    QTest::newRow("duplicates") << u"[Y]-[M]-[D]"_s << lower << QString() << QString() << 3 << 9 << 0;
    QTest::newRow("invalid") << u"[Y]:[M]"_s << lower << QString() << QString() << 0 << 12 << 0;
    QTest::newRow("findReplace") << u"[N]"_s << lower << u"IMG_"_s << u"제주_"_s << 8 << 0 << 4;
}

void TestDialogs::renameSummaries()
{
    QFETCH(QString, mask);
    QFETCH(int, extCase);
    QFETCH(QString, find);
    QFETCH(QString, replace);
    RenameRules rules;
    rules.mask = mask;
    rules.extCase = ExtCase(extCase);
    rules.find = find;
    rules.replace = replace;
    const QList<RenamePreviewRow> rows =
        previewRename(RenameSample::files(), rules, RenameSample::folderName(), RenameSample::existing());
    const RenameSummary s = summarizeRename(rows);
    QCOMPARE(s.total, 12);
    QTEST(s.changed, "changed");
    QTEST(s.bad, "bad");
    QTEST(s.same, "same");
    QCOMPARE(s.longCount, 4);  // 1 · 3 · 8 · 12
}

// 목업 JS는 대소문자를 구분한다 — 두 결과를 모두 기록(PLAN §11)
void TestDialogs::renameMockupComparison()
{
    RenameRules rules = RenameSample::boardRules();
    const auto files = RenameSample::files();
    const auto folder = RenameSample::folderName();
    const auto existing = RenameSample::existing();

    const QList<RenamePreviewRow> board = previewRename(files, rules, folder, existing, Qt::CaseSensitive);
    QCOMPARE(summarizeRename(board).text(), u"12개 중 10개 변경 · 충돌/오류 1 · 변경 없음 1"_s);
    QCOMPARE(board.at(0).newName, u"2026-09-14_제주_001.mp4"_s);
    QCOMPARE(board.at(6).state, RenamePreviewRow::Exists);
    QCOMPARE(board.at(10).state, RenamePreviewRow::Same);
    QCOMPARE(board.at(11).newName, u"2026-09-15_제주_012.jpg"_s);

    rules.extCase = ExtCase::Keep;  // 7행 "…_007.JPG" — 대소문자를 구분하면 충돌이 아니다
    RenameSummary s = summarizeRename(previewRename(files, rules, folder, existing, Qt::CaseSensitive));
    QCOMPARE(s.changed, 11);
    QCOMPARE(s.bad, 0);
    QCOMPARE(s.same, 1);

    rules.extCase = ExtCase::Upper;  // 11행도 ".JPG"로 바뀐다
    s = summarizeRename(previewRename(files, rules, folder, existing, Qt::CaseSensitive));
    QCOMPARE(s.changed, 12);
    QCOMPARE(s.bad, 0);
    QCOMPARE(s.same, 0);
}

void TestDialogs::renameRules()
{
    const RenameFile file = RenameSample::files().at(1);  // IMG_20260914_101522.JPG, 2026-09-14 10:15:22
    RenameRules r;
    r.mask = u"[N]_[C]"_s;
    QCOMPARE(applyRenameRules(file, 1, r, u"폴더"_s), u"IMG_20260914_101522_002.JPG"_s);
    r.counterStart = 10;
    r.counterStep = 5;
    r.counterDigits = 4;
    QCOMPARE(applyRenameRules(file, 2, r, u"폴더"_s), u"IMG_20260914_101522_0020.JPG"_s);

    r = RenameRules();
    r.mask = u"[P] [Y][M][D]-[t]"_s;
    r.extCase = ExtCase::Lower;
    QCOMPARE(applyRenameRules(file, 0, r, u"2026-09 제주"_s), u"2026-09 제주 20260914-101522.jpg"_s);

    // 찾기 · 바꾸기: 대소문자 무시(기본) · 구분, 정규식 캡처 $1
    r = RenameRules();
    r.find = u"img_"_s;
    r.replace = u"사진_"_s;
    QCOMPARE(applyRenameRules(file, 0, r, {}), u"사진_20260914_101522.JPG"_s);
    r.caseSensitive = true;
    QCOMPARE(applyRenameRules(file, 0, r, {}), file.name);
    r = RenameRules();
    r.regex = true;
    r.find = u"^IMG_(\\d{4})(\\d{4})_"_s;
    r.replace = u"$1-$2 "_s;
    QCOMPARE(applyRenameRules(file, 0, r, {}), u"2026-0914 101522.JPG"_s);

    // 이름 대소문자 · 확장자 마스크
    r = RenameRules();
    r.nameCase = NameCase::TitleCase;
    RenameFile words = file;
    words.name = u"hello big WORLD.txt"_s;
    QCOMPARE(applyRenameRules(words, 0, r, {}), u"Hello Big World.txt"_s);
    r.nameCase = NameCase::FirstUpper;
    QCOMPARE(applyRenameRules(words, 0, r, {}), u"Hello big world.txt"_s);
    r.nameCase = NameCase::Keep;
    r.extMask = QString();
    QCOMPARE(applyRenameRules(words, 0, r, {}), u"hello big WORLD"_s);

    // 날짜 기준: 수정한 날짜(샘플은 촬영 + 2일)
    r = RenameRules();
    r.mask = u"[Y][M][D]"_s;
    r.dateSource = DateSource::Modified;
    QCOMPARE(applyRenameRules(file, 0, r, {}), u"20260916.JPG"_s);
}

// ------------------------------------------------------------------------------------ 계획기

void TestDialogs::nameChecks()
{
    QCOMPARE(firstInvalidChar(u"a:b"_s), QChar(u':'));
    QVERIFY(firstInvalidChar(u"보고서 (최종).pdf"_s).isNull());
    QVERIFY(isReservedName(u"con.txt"_s));
    QVERIFY(isReservedName(u"LPT9"_s));
    QVERIFY(!isReservedName(u"console.txt"_s));
    QVERIFY(hasTrailingDotOrSpace(u"name."_s));
    QVERIFY(hasTrailingDotOrSpace(u"name "_s));
    QVERIFY(!hasTrailingDotOrSpace(u"name.txt"_s));
}

void TestDialogs::moveRenamePlans()
{
    const MockProbe probe;
    const QString src = u"D:\\Work\\fm-core\\src\\panel"_s;
    const QString original = u"PanelView.cpp"_s;
    const MoveRenameOptions options;
    auto plan = [&](const QString &input) { return planMoveRename(input, src, original, options, probe); };

    QCOMPARE(plan(original).operation, MoveRenamePlan::NoChange);

    MoveRenamePlan p = plan(u"BandedPanelView.cpp"_s);
    QCOMPARE(p.operation, MoveRenamePlan::Rename);
    QCOMPARE(p.issue, MoveRenamePlan::None);
    QVERIFY(p.canProceed);

    p = plan(u"Panel:View.cpp"_s);
    QCOMPARE(p.issue, MoveRenamePlan::InvalidChar);
    QVERIFY(!p.canProceed);
    QCOMPARE(plan(u"CON.cpp"_s).issue, MoveRenamePlan::Reserved);
    QCOMPARE(plan(u"PanelView.cpp."_s).issue, MoveRenamePlan::TrailingDotSpace);
    QCOMPARE(plan(QString()).issue, MoveRenamePlan::Empty);

    p = plan(u"PanelView.txt"_s);
    QCOMPARE(p.issue, MoveRenamePlan::ExtensionChange);

    // 상대 경로 이동(같은 볼륨) · 다른 볼륨 · 없는 폴더(새로 만듦)
    p = plan(u"..\\platform\\PanelView.cpp"_s);
    QCOMPARE(p.operation, MoveRenamePlan::Move);
    QVERIFY(p.sameVolume);
    QCOMPARE(p.directory, u"D:\\Work\\fm-core\\src\\platform\\"_s);

    p = plan(u"E:\\Backup\\Docs\\PanelView2.cpp"_s);
    QCOMPARE(p.operation, MoveRenamePlan::MoveRename);
    QVERIFY(!p.sameVolume);

    p = plan(u"D:\\Work\\fm-core\\src\\legacy\\PanelView.cpp"_s);
    QCOMPARE(p.issue, MoveRenamePlan::NewFolder);
    QVERIFY(p.canProceed);
    MoveRenameOptions noCreate;
    noCreate.createDirectories = false;
    p = planMoveRename(u"D:\\Work\\fm-core\\src\\legacy\\PanelView.cpp"_s, src, original, noCreate, probe);
    QCOMPARE(p.issue, MoveRenamePlan::MissingFolder);
    QVERIFY(!p.canProceed);
}

void TestDialogs::folderPlans()
{
    const MockProbe probe;
    const QString base = u"D:\\Work\\fm-core"_s;
    FolderPlan p = planFolders(base, u"src\\platform\\win32\\shim"_s, probe);
    QVERIFY(p.canCreate);
    QCOMPARE(p.relativePath, u"src\\platform\\win32\\shim"_s);
    // 현재 위치 + 조각 4개: src · platform 있음, win32 · shim 새로 만듦
    QCOMPARE(p.nodes.size(), 5);
    QCOMPARE(p.nodes.at(1).state, fm::ui::FolderPlanView::Node::Existing);
    QCOMPARE(p.nodes.at(2).state, fm::ui::FolderPlanView::Node::Existing);
    QCOMPARE(p.nodes.at(3).state, fm::ui::FolderPlanView::Node::New);
    QCOMPARE(p.nodes.at(4).state, fm::ui::FolderPlanView::Node::New);

    p = planFolders(base, u"  src / / platform  "_s, probe);  // 공백 · 빈 조각 정리, 모두 있음
    QCOMPARE(p.relativePath, u"src\\platform"_s);
    QVERIFY(!p.canCreate);

    p = planFolders(base, u"a\\..\\b"_s, probe);
    QVERIFY(!p.canCreate);
    QVERIFY(p.helpIsError);
}

// ------------------------------------------------------------------------------------ 시뮬레이터

void TestDialogs::simulator()
{
    const ProgressDialog::Operation op = ProgressDialog::boardCopy();
    ProgressSimulator sim;
    sim.setFiles(op.fileSizes);
    sim.preroll(9.0);
    // 목업 스크립트 그대로 9초면 약 32 %(목업 주석 · 정적 값의 37 %와 다르다 — 아래 prerollTo로 맞춘다)
    QVERIFY2(sim.percent() >= 31 && sim.percent() <= 33, qPrintable(QString::number(sim.percent())));
    QCOMPARE(sim.fileIndex(), 2);
    QCOMPARE(sim.samples().size(), 19);  // 0초 + 0.5초 × 18
    QVERIFY(sim.speed() > 140.0 * 1024 * 1024);

    ProgressSimulator board;
    board.setFiles(op.fileSizes);
    board.prerollTo(qint64(1.50 * 1024 * 1024 * 1024));
    QVERIFY2(board.percent() >= 37 && board.percent() <= 38, qPrintable(QString::number(board.percent())));
    QVERIFY(board.elapsed() >= 10.0 && board.elapsed() <= 11.0);

    // 일시 정지: 경과 시간은 흐르고 바이트는 그대로
    const qint64 before = sim.bytes();
    sim.setPaused(true);
    sim.preroll(2.0);
    QVERIFY(sim.bytes() - before < 64 * 1024 * 1024);  // 평활 속도가 줄어드는 동안만 조금 더
    const qint64 paused = sim.bytes();
    sim.preroll(10.0);
    QCOMPARE(sim.bytes(), paused);
    sim.setPaused(false);

    // 반복 끔: 끝에서 멈추고 finished
    ProgressSimulator small;
    small.setLoop(false);
    small.setFiles({8 * 1024 * 1024});
    QSignalSpy finished(&small, &ProgressSimulator::finished);
    small.preroll(5.0);
    QCOMPARE(finished.size(), 1);
    QVERIFY(small.isFinished());
    QCOMPARE(small.bytes(), small.total());
    QCOMPARE(small.percent(), 100);

    // 반복 켬(기본): 처음부터 다시
    ProgressSimulator loop;
    loop.setFiles({8 * 1024 * 1024});
    QSignalSpy restarted(&loop, &ProgressSimulator::restarted);
    loop.preroll(5.0);
    QVERIFY(restarted.size() >= 1);
    QVERIFY(!loop.isFinished());
}

// ------------------------------------------------------------------------------------ 대화상자

void TestDialogs::allVariants()
{
    const QList<DialogVariant> variants = dialogVariants();
    QVERIFY(variants.size() >= 50);
    // copy.historyOpen은 최근 대상 메뉴를 QMenu::exec()로 연다 — 열린 팝업을 닫아 중첩 루프를 끝낸다.
    QTimer popupCloser;
    popupCloser.setInterval(100);
    connect(&popupCloser, &QTimer::timeout, this, [] {
        if (QWidget *popup = QApplication::activePopupWidget())
            popup->close();
    });
    popupCloser.start();
    for (const DialogVariant &v : variants) {
        std::unique_ptr<QDialog> dialog(createDialog(v.id));
        QVERIFY2(dialog, qPrintable(v.id));
        dialog->show();
        QVERIFY2(QTest::qWaitForWindowExposed(dialog.get()), qPrintable(v.id));
        QVERIFY2(!dialog->windowTitle().isEmpty(), qPrintable(v.id));
        // 고정 크기 대화상자는 목업 클라이언트 크기(간단히 진행 창은 폭만). 제안 변형(목업에 없음 — 예: 128개 삭제는
        // 목록이 5행)은 내용에 맞춰 커질 수 있어 폭만 본다.
        if (v.fromMockup && v.client.height() > 0 && v.dialog != u"multirename" && v.dialog != u"settings")
            QVERIFY2(dialog->size() == v.client, qPrintable(u"%1: %2x%3 (목업 %4x%5)"_s.arg(v.id).arg(dialog->width()).arg(dialog->height()).arg(v.client.width()).arg(v.client.height())));
        else if (v.client.width() > 0 && v.dialog != u"settings")  // 설정 창은 화면의 92 %까지만(작은 화면 · offscreen)
            QCOMPARE(dialog->width(), v.client.width());
        dialog->close();
    }
    QVERIFY(!createDialog(u"nope.none"_s));
    std::unique_ptr<QDialog> group(createDialog(u"multirename"_s));  // 묶음 이름 → 첫 변형
    QVERIFY(qobject_cast<MultiRenameDialog *>(group.get()));
}

void TestDialogs::multiRename()
{
    MultiRenameDialog dialog(MultiRenameDialog::boardContext());
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QCOMPARE(dialog.windowTitle(), u"다중 이름 변경 — 12개 파일 · D:\\Photos\\2026-09 제주"_s);
    QCOMPARE(dialog.summary().text(), u"12개 중 10개 변경 · 충돌/오류 1 · 변경 없음 1"_s);
    auto *info = dialog.findChild<QLabel *>(u"previewInfo"_s);
    auto *renameButton = dialog.findChild<QPushButton *>(u"renameButton"_s);
    auto *undoButton = dialog.findChild<QPushButton *>(u"undoButton"_s);
    QVERIFY(info && renameButton && undoButton);
    // 자동: 긴 이름이 있으므로 2줄
    QVERIFY(dialog.previewView()->isTwoLine());
    QCOMPARE(info->text(), u"12개 · 긴 이름 4개 · 자동으로 2줄 표시 중"_s);
    QVERIFY(renameButton->isEnabled());
    QVERIFY(!renameButton->isDefault());
    QVERIFY(undoButton->isEnabled());

    // Ctrl+1 → 1줄, Ctrl+3 → 자동
    QTest::keyClick(&dialog, Qt::Key_1, Qt::ControlModifier);
    QVERIFY(!dialog.previewView()->isTwoLine());
    QCOMPARE(info->text(), u"12개 · 긴 이름 4개"_s);
    QTest::keyClick(&dialog, Qt::Key_3, Qt::ControlModifier);
    QVERIFY(dialog.previewView()->isTwoLine());

    // 모델: 7열, 상태 역할
    RenamePreviewModel *model = dialog.previewView()->model();
    QCOMPARE(model->columnCount(), int(RenamePreviewModel::ColumnCount));
    QCOMPARE(model->index(6, 0).data(RenamePreviewModel::StateRole).toInt(), int(RenamePreviewRow::Exists));
    QCOMPARE(model->index(6, RenamePreviewModel::StatusColumn).data().toString(), u"충돌 · 이미 있음"_s);
    QCOMPARE(model->index(0, RenamePreviewModel::DateColumn).data().toString(), u"2026-09-14 06:12"_s);

    // 이름 바꾸기 → 정상 행만 바뀌고 되돌리기 기록에 쌓인다
    QSignalSpy renamed(&dialog, &MultiRenameDialog::renamed);
    dialog.applyRename();
    QCOMPARE(renamed.size(), 1);
    QCOMPARE(renamed.first().first().value<QList<RenamePreviewRow>>().size(), 10);
    QCOMPARE(dialog.files().at(0).name, u"2026-09-14_제주_001.mp4"_s);
    QCOMPARE(dialog.summary().text(), u"12개 중 0개 변경 · 충돌/오류 1 · 변경 없음 11"_s);
    QVERIFY(!renameButton->isEnabled());
    dialog.undo();
    QCOMPARE(dialog.summary().text(), u"12개 중 10개 변경 · 충돌/오류 1 · 변경 없음 1"_s);
    // 보드의 지난 기록 — 11번의 원래 이름이 돌아온다
    dialog.undo();
    QCOMPARE(dialog.files().at(10).name, u"IMG_20260915_134502.JPG"_s);
    QCOMPARE(dialog.summary().text(), u"12개 중 11개 변경 · 충돌/오류 1 · 변경 없음 0"_s);
    QVERIFY(!dialog.canUndo());
    QVERIFY(!undoButton->isEnabled());

    // 막기 설정
    MultiRenameDialog blocked(MultiRenameDialog::boardContext());
    blocked.applyVariant(u"multirename.blocked"_s);
    QVERIFY(!blocked.findChild<QPushButton *>(u"renameButton"_s)->isEnabled());

    // 짧은 이름만 → 자동 1줄
    MultiRenameDialog shortNames(MultiRenameDialog::boardContext());
    shortNames.applyVariant(u"multirename.autoOneLine"_s);
    QVERIFY(!shortNames.previewView()->isTwoLine());
    QCOMPARE(shortNames.findChild<QLabel *>(u"previewInfo"_s)->text(), u"8개 · 긴 이름 0개 · 자동으로 1줄 표시 중"_s);

    // 토큰 단추 → 마스크 끝에 넣는다(커서가 끝에 있을 때)
    MultiRenameDialog token(MultiRenameDialog::boardContext());
    token.applyVariant(u"multirename.tokenAppend"_s);
    QCOMPARE(token.rules().mask, u"[Y]-[M]-[D]_제주_[C][t]"_s);
    QCOMPARE(token.summary().changed, 12);
}

void TestDialogs::progress()
{
    // 자세히(목업): 약 37 %에서 계속 진행
    std::unique_ptr<QDialog> running(createDialog(u"progress.copy.detail.running"_s));
    auto *detail = qobject_cast<ProgressDialog *>(running.get());
    QVERIFY(detail);
    QCOMPARE(detail->mode(), ProgressDialog::Detail);
    QVERIFY(detail->windowTitle().endsWith(u"% · 복사 중"_s));
    QCOMPARE(detail->size(), QSize(640, 524));
    detail->setPaused(true);
    QVERIFY(detail->windowTitle().contains(u"일시 정지됨 — 복사"_s));
    QCOMPARE(detail->findChild<QPushButton *>(u"pauseButton"_s)->text(), u"재개(&P)"_s);
    detail->setPaused(false);
    detail->setMode(ProgressDialog::Compact);
    QCOMPARE(detail->width(), 520);
    QVERIFY(detail->height() < 300);

    // 간단히 변형: 이동 일시 정지 62 % · 관리자 권한 복사 72 %
    std::unique_ptr<QDialog> move(createDialog(u"progress.move.compact.paused"_s));
    QCOMPARE(move->windowTitle(), u"62% · 일시 정지됨 — 이동"_s);
    QVERIFY(qobject_cast<ProgressDialog *>(move.get())->isPaused());
    std::unique_ptr<QDialog> elevated(createDialog(u"progress.copy.compact.elevated"_s));
    QCOMPARE(elevated->windowTitle(), u"72% · 복사 중 (관리자)"_s);
    QVERIFY(elevated->findChild<QPushButton *>(u"backgroundButton"_s)->isHidden());
    std::unique_ptr<QDialog> del(createDialog(u"progress.delete.compact.running"_s));
    QCOMPARE(del->windowTitle(), u"31% · 삭제 중"_s);
    QVERIFY(del->findChild<QPushButton *>(u"pauseButton"_s)->isHidden());

    // 실제 작업(반복 끔): 끝나면 "완료되면 창 닫기"에 따라 닫힌다
    ProgressDialog::Operation op;
    op.source = u"C:\\a"_s;
    op.target = u"C:\\b"_s;
    op.fileNames = {u"small.bin"_s};
    op.fileSizes = {2 * 1024 * 1024};
    ProgressDialog closing(op);
    QSignalSpy accepted(&closing, &QDialog::accepted);
    closing.show();
    QVERIFY(accepted.wait(3000));

    ProgressDialog staying(op);
    staying.findChild<QCheckBox *>(u"closeWhenDoneCheck"_s)->setChecked(false);
    staying.show();
    QTRY_VERIFY_WITH_TIMEOUT(staying.isDone(), 3000);
    QVERIFY(staying.isVisible());
    QCOMPARE(staying.windowTitle(), u"100% · 복사 완료"_s);
    QCOMPARE(staying.findChild<QPushButton *>(u"cancelButton"_s)->text(), u"닫기"_s);
}

// ------------------------------------------------------------------------------------ 권한 대화상자

void TestDialogs::elevationDialogs_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("defaultText");
    for (const auto &[id, title, text] : {
             std::tuple{u"preflight"_s, u"삭제 — 권한 확인"_s, u"계속"_s},
             std::tuple{u"preflight.skip"_s, u"삭제 — 권한 확인"_s, u"39개 삭제"_s},
             std::tuple{u"copy"_s, u"복사 — 관리자 권한 필요"_s, u"관리자 권한으로 계속(&C)"_s},
             std::tuple{u"move"_s, u"이동 — 관리자 권한 필요"_s, u"관리자 권한으로 이동"_s},
             std::tuple{u"move.copyOnly"_s, u"이동 — 관리자 권한 필요"_s, u"복사만 하기"_s},
             std::tuple{u"delete"_s, u"삭제 — 관리자 권한 필요"_s, u"관리자 권한으로 삭제(&D)"_s},
             std::tuple{u"rename"_s, u"이름 변경 — 관리자 권한 필요"_s, u"관리자 권한으로 이름 바꾸기(&R)"_s},
             std::tuple{u"create.folder"_s, u"새 폴더 — 관리자 권한 필요"_s, u"관리자 권한으로 만들기(&C)"_s},
             std::tuple{u"create.file"_s, u"새 파일 — 관리자 권한 필요"_s, u"관리자 권한으로 만들기(&C)"_s},
             std::tuple{u"ownership"_s, u"삭제 — 액세스 거부"_s, u"건너뛰기(&S)"_s},
             std::tuple{u"failed"_s, u"복사 — 관리자 권한을 얻지 못함"_s, u"다시 시도(&R)"_s},
         })
        QTest::newRow(qPrintable(id)) << id << title << text;
}

void TestDialogs::elevationDialogs()
{
    QFETCH(QString, id);
    QFETCH(QString, title);
    QFETCH(QString, defaultText);
    const elev::PromptSpec spec = elev::prompts::board(id);
    ElevationDialog dialog(spec);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QCOMPARE(dialog.windowTitle(), title);
    QCOMPARE(dialog.accessibleName(), spec.heading);
    // 클라이언트 폭 558, 높이 ≥ 목업 − 38(03 §1.2)
    QCOMPARE(dialog.width(), 558);
    QVERIFY2(dialog.height() >= spec.designSize.height() - 38, qPrintable(QString::number(dialog.height())));
    // 기본 단추 = Enter · 처음 포커스(소유권 창은 건너뛰기)
    QVERIFY(dialog.defaultButton());
    QCOMPARE(dialog.defaultButton()->text(), defaultText);
    QVERIFY(dialog.defaultButton()->isDefault());
    QTRY_COMPARE(QApplication::focusWidget(), static_cast<QWidget *>(dialog.defaultButton()));
    // 액세스 키가 겹치지 않는다(03 §3.7)
    QStringList keys;
    static const QRegularExpression mnemonic(u"&([^&])"_s);
    for (QAbstractButton *b : dialog.findChildren<QAbstractButton *>()) {
        const auto m = mnemonic.match(b->text());
        if (m.hasMatch())
            keys.append(m.captured(1).toLower());
    }
    const QSet<QString> unique(keys.cbegin(), keys.cend());
    QCOMPARE(unique.size(), keys.size());
}

void TestDialogs::elevationChoices()
{
    using elev::Choice;
    // 사전 확인: 선택지에 따라 기본 단추 글자 · 방패 · 결과가 바뀐다
    {
        ElevationDialog d(elev::prompts::board(u"preflight"_s));
        QCOMPARE(d.selectedOption(), 0);
        QCOMPARE(d.defaultButton()->glyph(), fm::ui::glyph::Shield);
        d.selectOption(1);
        QCOMPARE(d.defaultButton()->text(), u"39개 삭제"_s);
        QCOMPARE(d.defaultButton()->glyph(), fm::ui::glyph::None);
        QCOMPARE(d.button(Choice::SkipNeedingAdmin), d.defaultButton());
        QSignalSpy decided(&d, &ElevationDialog::decided);
        d.defaultButton()->click();
        QCOMPARE(decided.size(), 1);
        QCOMPARE(d.result().choice, Choice::SkipNeedingAdmin);
        QCOMPARE(d.QDialog::result(), int(QDialog::Accepted));
    }
    // 이동: 복사만 하기, 체크 상자 처음 값(목업) 끔
    {
        ElevationDialog d(elev::prompts::board(u"move"_s));
        QVERIFY(!d.checkBox()->isChecked());
        auto *copyOnly = d.findChildren<fm::ui::OptionRadio *>().value(1);
        QVERIFY(copyOnly);
        copyOnly->radio()->click();
        QCOMPARE(d.defaultButton()->text(), u"복사만 하기"_s);
        d.defaultButton()->click();
        QCOMPARE(d.result().choice, Choice::CopyOnly);
    }
    // 복사: 건너뛰기 + '같은 선택 적용'(처음 켬)
    {
        ElevationDialog d(elev::prompts::board(u"copy"_s));
        QVERIFY(d.checkBox()->isChecked());
        QCOMPARE(d.checkBox()->text(), u"남은 2개 항목에도 같은 선택 적용(&A)"_s);
        d.button(Choice::Skip)->click();
        QCOMPARE(d.result().choice, Choice::Skip);
        QVERIFY(d.result().checked);
        const auto rows = d.findChild<fm::ui::KeyValueCard *>()->rows();
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows.at(0).trailing, u"24.4 MB"_s);
        QCOMPARE(rows.at(1).value, u"C:\\Program Files\\FM Tools\\redist\\"_s);
    }
    // Esc = 취소
    {
        ElevationDialog d(elev::prompts::board(u"delete"_s));
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        QTest::keyClick(&d, Qt::Key_Escape);
        QCOMPARE(d.result().choice, Choice::Cancel);
        QVERIFY(d.result().checked);  // 삭제는 처음 켬
    }
    // 새로 만들기: 대안 카드 → 사용자 폴더, 파일 변형
    {
        ElevationDialog d(elev::prompts::board(u"create.file"_s));
        auto *alt = d.findChild<fm::ui::ActionCard *>();
        QVERIFY(alt);
        QCOMPARE(alt->detail(), u"%LOCALAPPDATA%\\FM Tools\\plugins.json"_s);
        QCOMPARE(d.spec().heading, u"이 위치에 파일을 만들려면 관리자 권한이 필요합니다"_s);
        alt->click();
        QCOMPARE(d.result().choice, Choice::UseUserFolder);
    }
    // 소유권: Enter = 건너뛰기, 위험 동작은 왼쪽 보통 단추
    {
        ElevationDialog d(elev::prompts::board(u"ownership"_s));
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        QCOMPARE(d.button(Choice::TakeOwnership)->role(), fm::ui::Button::Normal);
        QCOMPARE(d.button(Choice::TakeOwnership)->glyph(), fm::ui::glyph::Shield);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
        QCOMPARE(d.result().choice, Choice::Skip);
    }
    // 실패: 작업 취소 · '남은 항목 건너뛰기'
    {
        ElevationDialog d(elev::prompts::board(u"failed"_s));
        QCOMPARE(d.button(Choice::Cancel)->text(), u"작업 취소"_s);
        QVERIFY(!d.checkBox()->isChecked());
        d.checkBox()->setChecked(true);
        d.choose(Choice::Retry);
        QCOMPARE(d.result().choice, Choice::Retry);
        QVERIFY(d.result().checked);
    }
}

// 흐름 시뮬레이션 — 열린 창을 찾아 사용자처럼 고른다.
void TestDialogs::elevationFlows()
{
    QWidget window;
    window.resize(800, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto current = [](ElevationFlow &flow) { return flow.currentDialog(); };
    auto elevation = [&](ElevationFlow &flow) -> ElevationDialog * {
        ElevationDialog *d = nullptr;
        [&] { QTRY_VERIFY((d = qobject_cast<ElevationDialog *>(current(flow))) && d->isVisible()); }();
        return d;
    };
    auto uac = [&](ElevationFlow &flow) -> UacSimulationDialog * {
        UacSimulationDialog *d = nullptr;
        [&] { QTRY_VERIFY((d = qobject_cast<UacSimulationDialog *>(current(flow))) && d->isVisible()); }();
        return d;
    };
    auto closeProgress = [&] {
        for (ProgressDialog *p : window.findChildren<ProgressDialog *>())
            p->close();
    };

    // 복사: 거부 → 승인 → UAC 아니요 → 실패 → 다시 시도 → UAC 예 → 관리자 진행 창
    {
        ElevationFlow flow(ElevationFlow::CopyToProtected, &window);
        QSignalSpy finished(&flow, &ElevationFlow::finished);
        flow.start();
        ElevationDialog *d = elevation(flow);
        QCOMPARE(d->windowTitle(), u"복사 — 관리자 권한 필요"_s);
        QVERIFY(d->isModal());
        d->choose(elev::Choice::Elevate);
        uac(flow)->respond(UacSimulationDialog::No);
        d = elevation(flow);
        QCOMPARE(d->windowTitle(), u"복사 — 관리자 권한을 얻지 못함"_s);
        d->choose(elev::Choice::Retry);
        uac(flow)->respond(UacSimulationDialog::Yes);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toString(), u"복사 3개(관리자) · 건너뜀 0개"_s);
        const auto progress = window.findChildren<ProgressDialog *>();
        QCOMPARE(progress.size(), 1);
        QVERIFY(progress.first()->windowTitle().endsWith(u"(관리자)"_s));
        QVERIFY(flow.log().contains(u"UAC: 아니요"_s));
        QVERIFY(flow.log().contains(u"UAC: 예"_s));
        closeProgress();
    }
    // 복사: 항목마다 건너뛰기(적용 끔) → 마지막은 '같은 선택 적용' 없이
    {
        ElevationFlow flow(ElevationFlow::CopyToProtected, &window);
        QSignalSpy finished(&flow, &ElevationFlow::finished);
        flow.start();
        for (int i = 0; i < 3; ++i) {
            ElevationDialog *d = elevation(flow);
            QCOMPARE(d->checkBox() != nullptr, i < 2);  // 남은 항목이 없으면 체크 상자 없음
            if (d->checkBox())
                d->checkBox()->setChecked(false);
            d->choose(elev::Choice::Skip);
        }
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toString(), u"복사 0개 · 건너뜀 3개"_s);
    }
    // 삭제: 사전 확인 → 승인 → UAC 예 → 소유권(Enter = 건너뛰기)
    {
        ElevationFlow flow(ElevationFlow::DeleteWithOwnership, &window);
        QSignalSpy finished(&flow, &ElevationFlow::finished);
        flow.start();
        elevation(flow)->choose(elev::Choice::Elevate);
        uac(flow)->respond(UacSimulationDialog::Yes);
        ElevationDialog *d = elevation(flow);
        QCOMPARE(d->windowTitle(), u"삭제 — 액세스 거부"_s);
        d->defaultButton()->click();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toString(), u"관리자 권한으로 41개 삭제 · 1개 건너뜀"_s);
        closeProgress();
    }
    // UAC 시간 초과 → 실패 창 → 작업 취소
    {
        ElevationFlow flow(ElevationFlow::DeleteWithOwnership, &window);
        flow.setUacTimeout(1);
        QSignalSpy finished(&flow, &ElevationFlow::finished);
        flow.start();
        elevation(flow)->choose(elev::Choice::Elevate);
        uac(flow);
        ElevationDialog *failed = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((failed = qobject_cast<ElevationDialog *>(flow.currentDialog())) && failed->isVisible(), 4000);
        QCOMPARE(failed->windowTitle(), u"삭제 — 관리자 권한을 얻지 못함"_s);
        QVERIFY(flow.log().contains(u"UAC: 시간 초과"_s));
        failed->choose(elev::Choice::Cancel);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toString(), u"작업 취소"_s);
    }
}

void TestDialogs::fileOpSettings()
{
    // 설정 › 파일 작업 · 관리자 권한 · 키보드 → 대화상자 · 진행 창의 처음 값
    using Ops = fm::settings::FileOpsSettings;
    FileOpContext context = BoardContext::copy();
    context.fileOps.onConflict = Ops::Conflict::Skip;
    context.fileOps.verifyHash = true;
    context.fileOps.copyAcl = true;
    context.fileOps.copyAds = false;
    context.fileOps.links = Ops::Links::CopyTarget;
    {
        CopyDialog copy(context);
        const CopyRequest r = copy.request();
        QCOMPARE(r.overwritePolicy, 3);
        QVERIFY(r.verify && r.acl && !r.alternateStreams && !r.symlinksAsLinks && r.keepAttributes);
    }
    {
        FileOpContext remove = BoardContext::remove();
        QVERIFY(!DeleteDialog(remove, false).request().forceReadOnly);  // 목업 기본: 매번 묻기
        remove.fileOps.readOnly = Ops::ReadOnly::Delete;
        QVERIFY(DeleteDialog(remove, false).request().forceReadOnly);
    }

    // 진행 창: 간단히로 시작 · 완료되면 닫기 끔 · 관리자 제목 끔 · 대기열
    ProgressDialog::Operation op;
    op.source = u"C:\\a"_s;
    op.target = u"C:\\b"_s;
    op.fileNames = {u"big.bin"_s};
    op.fileSizes = {qint64(4) << 30};
    op.detailed = false;
    op.closeWhenDone = false;
    {
        ProgressDialog p(op);
        QCOMPARE(p.mode(), ProgressDialog::Compact);
        QVERIFY(!p.findChild<QCheckBox *>(u"closeWhenDoneCheck"_s)->isChecked());
        p.setWaiting(true);
        QCOMPARE(p.windowTitle(), u"대기 중 — 복사"_s);
        QVERIFY(!p.findChild<QPushButton *>(u"pauseButton"_s)->isEnabled());
        const int before = p.simulator()->percent();
        QTest::qWait(300);
        QCOMPARE(p.simulator()->percent(), before);  // 대기 중에는 진행하지 않는다
        p.setWaiting(false);
        QVERIFY(p.windowTitle().endsWith(u"% · 복사 중"_s));
        QVERIFY(p.findChild<QPushButton *>(u"pauseButton"_s)->isEnabled());
    }
    op.elevated = true;
    op.adminTitle = false;
    {
        ProgressDialog p(op);
        QVERIFY(p.windowTitle().endsWith(u"% · 복사 중"_s));  // "(관리자)" 없음
    }
    op.elevated = false;

    // Esc: 바로 취소 · 창 숨기기(백그라운드)
    op.esc = ProgressDialog::Operation::EscAction::CancelImmediately;
    {
        ProgressDialog p(op);
        p.show();
        QVERIFY(QTest::qWaitForWindowExposed(&p));
        QSignalSpy rejected(&p, &QDialog::rejected);
        QTest::keyClick(&p, Qt::Key_Escape);
        QCOMPARE(rejected.count(), 1);
    }
    op.esc = ProgressDialog::Operation::EscAction::HideWindow;
    {
        ProgressDialog p(op);
        p.show();
        QVERIFY(QTest::qWaitForWindowExposed(&p));
        QSignalSpy background(&p, &ProgressDialog::backgroundRequested);
        QSignalSpy rejected(&p, &QDialog::rejected);
        QTest::keyClick(&p, Qt::Key_Escape);
        QCOMPARE(background.count(), 1);
        QCOMPARE(rejected.count(), 0);
        QVERIFY(!p.isVisible());
    }

    // 다중 이름 변경: 1줄로 시작 · 되돌리기 기록 1개 · 기록 안 함
    MultiRenameDialog::Context mc = MultiRenameDialog::boardContext();
    mc.recordMode = 0;
    mc.undoDepth = 1;
    {
        MultiRenameDialog dialog(mc);
        QCOMPARE(dialog.findChild<fm::ui::SegmentedControl *>(u"recordSegment"_s)->currentIndex(), 0);
        QVERIFY(!dialog.previewView()->isTwoLine());
        dialog.applyRename();
        dialog.undo();
        QVERIFY(!dialog.canUndo());  // 기록 1개 — 보드의 이전 기록은 밀려났다
    }
    mc.undoDepth = 0;
    {
        MultiRenameDialog dialog(mc);
        QVERIFY(dialog.canUndo());  // 넘겨받은 이전 기록은 그대로
        dialog.undo();
        dialog.applyRename();
        QVERIFY(!dialog.canUndo());  // 기록하지 않는다
    }
}

void TestDialogs::elevationSettings()
{
    // 설정 › 관리자 권한 → 권한 흐름(사전 확인 끔 · 도우미 실행 중 · 소유권 창 끔 · 소유권 기본 단추 · UAC 계속 기다림)
    QWidget window;
    window.resize(800, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto elevation = [&](ElevationFlow &flow) -> ElevationDialog * {
        ElevationDialog *d = nullptr;
        [&] { QTRY_VERIFY((d = qobject_cast<ElevationDialog *>(flow.currentDialog())) && d->isVisible()); }();
        return d;
    };
    auto closeProgress = [&] {
        for (ProgressDialog *p : window.findChildren<ProgressDialog *>())
            p->close();
    };

    // 사전 확인 끔 → 처리 중 거부 창 · 도우미가 살아 있으면 UAC 없이 · 소유권 창 끔 → 조용히 건너뜀
    {
        ElevationFlow flow(ElevationFlow::DeleteWithOwnership, &window);
        ElevationFlow::Options o;
        o.preflight = false;
        o.helperRunning = true;
        o.askOwnership = false;
        flow.setOptions(o);
        QSignalSpy finished(&flow, &ElevationFlow::finished);
        QSignalSpy approved(&flow, &ElevationFlow::helperApproved);
        flow.start();
        ElevationDialog *d = elevation(flow);
        QCOMPARE(d->windowTitle(), u"삭제 — 관리자 권한 필요"_s);
        d->choose(elev::Choice::Elevate);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toString(), u"관리자 권한으로 41개 삭제 · 1개 건너뜀"_s);
        QVERIFY(flow.log().contains(u"권한 상승 도우미 실행 중 — UAC 확인 생략"_s));
        QVERIFY(flow.log().contains(u"소유권 창 끔 — 조용히 건너뜀"_s));
        QCOMPARE(approved.count(), 0);
        closeProgress();
    }
    // 소유권 창의 Enter = 소유권 가져오기, UAC 승인은 helperApproved로 알린다
    {
        ElevationFlow flow(ElevationFlow::DeleteWithOwnership, &window);
        ElevationFlow::Options o;
        o.ownershipDefault = elev::Choice::TakeOwnership;
        o.uacTimeout = 0;
        flow.setOptions(o);
        QSignalSpy finished(&flow, &ElevationFlow::finished);
        QSignalSpy approved(&flow, &ElevationFlow::helperApproved);
        flow.start();
        elevation(flow)->choose(elev::Choice::Elevate);
        UacSimulationDialog *uac = nullptr;
        QTRY_VERIFY((uac = qobject_cast<UacSimulationDialog *>(flow.currentDialog())) && uac->isVisible());
        QVERIFY(uac->findChild<QLabel *>(u"countdown"_s)->text().contains(u"계속 기다림"_s));
        uac->respond(UacSimulationDialog::Yes);
        QCOMPARE(approved.count(), 1);
        ElevationDialog *d = elevation(flow);
        QCOMPARE(d->windowTitle(), u"삭제 — 액세스 거부"_s);
        d->defaultButton()->click();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toString(), u"관리자 권한으로 42개 삭제 · 소유권 1개 변경"_s);
        closeProgress();
    }
    // 소유권 창의 Enter = 취소
    {
        const elev::ItemInfo item{u"x"_s, true, -1, fm::style::Token::KDoc};
        const elev::PromptSpec spec = elev::prompts::ownershipDenied(elev::Operation::Delete, item, u"C:\\"_s, u"SYSTEM"_s, false,
                                                                     elev::Choice::Cancel);
        int defaults = 0;
        for (const elev::ButtonSpec &b : spec.buttons) {
            defaults += b.isDefault ? 1 : 0;
            if (b.isDefault)
                QCOMPARE(b.choice, elev::Choice::Cancel);
        }
        QCOMPARE(defaults, 1);
    }
}

void TestDialogs::settingsKeys()
{
    namespace st = fm::settings;
    SettingsDialog dialog;
    dialog.setCurrentPage(u"keys"_s);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *view = dialog.findChild<QTreeView *>(u"keyTable"_s);
    QVERIFY(view);
    constexpr int kCommandRole = Qt::UserRole + 2;
    auto rowOf = [&](const QString &id) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        for (int r = 0; r < view->model()->rowCount(); ++r) {
            if (view->model()->index(r, 0).data(kCommandRole).toString() == id)
                return view->model()->index(r, 0);
        }
        return QModelIndex();
    };
    auto captureOf = [&](const QString &id) { return qobject_cast<fm::ui::KeyCaptureEdit *>(view->indexWidget(rowOf(id).siblingAtColumn(1))); };
    auto keysOf = [&](const QString &id) { return st::effectiveKeys(dialog.session()->pending().keys, id); };
    auto banner = [&] {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        return view->findChild<QWidget *>(u"conflictBanner"_s);
    };

    // 새 폴더(F7)에 F5 → 복사(F5, 같은 파일 목록 범위)와 겹침 → 배너, 설정은 아직 그대로
    Q_EMIT view->doubleClicked(rowOf(u"newFolder"_s));
    fm::ui::KeyCaptureEdit *capture = captureOf(u"newFolder"_s);
    QVERIFY(capture && capture->isCapturing());
    QTest::keyClick(capture, Qt::Key_F5);
    QWidget *alert = banner();
    QVERIFY(alert);
    QVERIFY(alert->findChild<QLabel *>()->text().contains(u"‘복사’"_s));
    QCOMPARE(keysOf(u"newFolder"_s), QList<QKeySequence>{QKeySequence(Qt::Key_F7)});
    if (const QString dir = qEnvironmentVariable("FM_TEST_SHOTS"); !dir.isEmpty())
        dialog.grab().save(dir + u"/settings-keys-conflict.png"_s);

    // 다른 키 누르기 → 다시 입력 상태 · 배너 닫힘
    auto buttons = alert->findChildren<QPushButton *>();
    QCOMPARE(buttons.size(), 2);
    QTest::mouseClick(buttons.at(1), Qt::LeftButton);
    QVERIFY(!banner());
    capture = captureOf(u"newFolder"_s);
    QVERIFY(capture && capture->isCapturing());

    // 다시 F5 → 그래도 바꾸기 → 새 폴더 = F5, 복사 = 없음
    QTest::keyClick(capture, Qt::Key_F5);
    alert = banner();
    QVERIFY(alert);
    QTest::mouseClick(alert->findChildren<QPushButton *>().at(0), Qt::LeftButton);
    QVERIFY(!banner());
    QCOMPARE(keysOf(u"newFolder"_s), QList<QKeySequence>{QKeySequence(Qt::Key_F5)});
    QVERIFY(keysOf(u"copy"_s).isEmpty());
    QVERIFY(dialog.session()->pending().keys.overrides.contains(u"copy"_s));

    // 금지 키는 받지 않고 입력을 이어 간다 · Esc = 취소
    Q_EMIT view->doubleClicked(rowOf(u"rename"_s));
    capture = captureOf(u"rename"_s);
    QVERIFY(capture);
    QTest::keyClick(capture, Qt::Key_F4, Qt::AltModifier);
    QVERIFY(capture->isCapturing());
    QTest::keyClick(capture, Qt::Key_Escape);
    QVERIFY(!captureOf(u"rename"_s));
    QCOMPARE(keysOf(u"rename"_s), QList<QKeySequence>{QKeySequence(Qt::Key_F2)});

    // 대기열에 추가(F2, 복사 · 이동 대화상자)는 파일 목록의 F2와 겹치지 않는다 → 바로 반영
    Q_EMIT view->doubleClicked(rowOf(u"queue"_s));
    capture = captureOf(u"queue"_s);
    QVERIFY(capture);
    QTest::keyClick(capture, Qt::Key_F2);
    QVERIFY(!banner());
    QCOMPARE(keysOf(u"queue"_s), QList<QKeySequence>{QKeySequence(Qt::Key_F2)});
    QVERIFY(!dialog.session()->pending().keys.overrides.contains(u"queue"_s));  // 기본값과 같으면 사용자 지정 없음

    // Backspace = 키 지우기
    Q_EMIT view->doubleClicked(rowOf(u"rename"_s));
    QTest::keyClick(captureOf(u"rename"_s), Qt::Key_Backspace);
    QVERIFY(keysOf(u"rename"_s).isEmpty());

    // 범위 세그먼트 · 검색(키 글자)
    auto *scope = dialog.findChild<fm::ui::SegmentedControl *>(u"scopeSegment"_s);
    QVERIFY(scope);
    scope->setCurrentIndex(2);  // 패널
    QVERIFY(rowOf(u"showHidden"_s).isValid());
    QVERIFY(!rowOf(u"newFolder"_s).isValid());
    scope->setCurrentIndex(0);
    auto *search = dialog.findChild<fm::ui::SearchField *>(u"searchEdit"_s);
    QVERIFY(search);
    search->setText(u"ctrl+h"_s);
    QVERIFY(rowOf(u"showHidden"_s).isValid());
    QVERIFY(!rowOf(u"copy"_s).isValid());
    search->clear();

    // 기본값으로 되돌리기
    QPushButton *reset = nullptr;
    for (QPushButton *b : dialog.findChildren<QPushButton *>()) {
        if (b->isVisible() && b->text() == u"기본값으로 되돌리기(&R)"_s)
            reset = b;
    }
    QVERIFY(reset);
    QTest::mouseClick(reset, Qt::LeftButton);
    QVERIFY(dialog.session()->pending().keys.overrides.isEmpty());
    QCOMPARE(keysOf(u"copy"_s), QList<QKeySequence>{QKeySequence(Qt::Key_F5)});
}

void TestDialogs::settingsPages()
{
    namespace st = fm::settings;
    namespace fl = fm::filelist;
    const QString shots = qEnvironmentVariable("FM_TEST_SHOTS");
    SettingsDialog dialog;
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    const AppSettings defaults;
    auto pending = [&]() -> const AppSettings & { return dialog.session()->pending(); };
    auto button = [&](const QString &text) -> QPushButton * {
        for (QPushButton *b : dialog.findChildren<QPushButton *>()) {
            if (b->isVisible() && b->text() == text)
                return b;
        }
        return nullptr;
    };

    // 9개 페이지 모두 실제 페이지(임시 페이지 없음)
    QCOMPARE(dialog.pageIds().size(), 9);
    for (const QString &id : dialog.pageIds()) {
        dialog.setCurrentPage(id);
        QCOMPARE(dialog.currentPageId(), id);
        QVERIFY(!dialog.findChild<QLabel *>(u"(준비 중)"_s));
        if (!shots.isEmpty())
            dialog.grab().save(shots + u"/settings-"_s + id + u".png"_s);
    }

    // 일반 · 모양: 다크 카드 → 테마 다크, 강조색 견본 → 테마 구성표의 기준 색(두 페이지가 같은 보류 값)
    dialog.setCurrentPage(u"appearance"_s);
    auto *darkCard = dialog.findChild<fm::ui::ThemeModeCard *>(u"darkCard"_s);
    QVERIFY(darkCard);
    QTest::mouseClick(darkCard, Qt::LeftButton);
    QCOMPARE(pending().appearance.scheme, st::Scheme::Dark);
    auto *picker = dialog.findChild<fm::ui::AccentPicker *>(u"accentPicker"_s);
    QVERIFY(picker);
    Q_EMIT picker->accentChosen(QColor(0x0F7A6E));
    QCOMPARE(pending().theme.scheme.seeds.accent, std::optional<QColor>(QColor(0x0F7A6E)));
    QVERIFY(dialog.session()->isDirty());
    QPushButton *reset = button(u"기본값으로 되돌리기(&R)"_s);
    QVERIFY(reset);
    QTest::mouseClick(reset, Qt::LeftButton);
    QCOMPARE(pending().appearance.scheme, defaults.appearance.scheme);
    QVERIFY(!pending().theme.scheme.seeds.accent);

    // 파일 패널: 섬네일 모드면 미리보기가 섬네일로 · 링크가 보인다
    dialog.setCurrentPage(u"panel"_s);
    auto *mode = dialog.findChild<fm::ui::SegmentedControl *>(u"viewModeSegment"_s);
    QVERIFY(mode);
    mode->setCurrentIndex(int(fl::ViewMode::Thumbnails));
    QCOMPARE(pending().panel.defaultViewMode, fl::ViewMode::Thumbnails);
    QVERIFY(dialog.findChild<QPushButton *>(u"thumbsLink"_s)->isVisible());
    mode->setCurrentIndex(int(fl::ViewMode::Auto));

    // 파일 그룹: 새 그룹 → 선택 그룹 뒤에 추가, 이름 편집, 삭제(기본 제공 그룹은 삭제 불가)
    dialog.setCurrentPage(u"groups"_s);
    const int groups = int(pending().groups.groups.size());
    QTest::mouseClick(button(u"새 그룹(&W)"_s), Qt::LeftButton);
    QCOMPARE(int(pending().groups.groups.size()), groups + 1);
    auto *name = dialog.findChild<QLineEdit *>(u"nameEdit"_s);
    QVERIFY(name && name->isEnabled());
    name->selectAll();
    QTest::keyClicks(name, u"Test group"_s);  // QTest는 ASCII 키만 보낸다
    QCOMPARE(pending().groups.groups.at(1).name, u"Test group"_s);
    auto *remove = dialog.findChild<QToolButton *>(u"deleteButton"_s);
    QVERIFY(remove && remove->isEnabled());
    QTest::mouseClick(remove, Qt::LeftButton);
    QCOMPARE(int(pending().groups.groups.size()), groups);

    // 열: 사용자 정의 열 → 선택 열 뒤에 식 열, 제거
    dialog.setCurrentPage(u"columns"_s);
    const int columns = int(pending().columns.sets.first().columns.size());
    QTest::mouseClick(button(u"사용자 정의 열 만들기(&U)"_s), Qt::LeftButton);
    QCOMPARE(int(pending().columns.sets.first().columns.size()), columns + 1);
    QVERIFY(pending().columns.sets.first().columns.at(1).custom);
    QTest::mouseClick(dialog.findChild<QToolButton *>(u"columnRemoveButton"_s), Qt::LeftButton);
    QCOMPARE(int(pending().columns.sets.first().columns.size()), columns);

    // 테마 색상: 모든 토큰 보기, 편집 변형 다크, 직접 지정 → 칩 · 되돌리기
    dialog.setCurrentPage(u"theme"_s);
    dialog.session()->edit(Section::Dialog, [](AppSettings &p) { p.dialog.themeView = u"adv"_s; });
    auto *tree = dialog.findChild<QTreeView *>(u"tokenTree"_s);
    QVERIFY(tree && tree->isVisible());
    QPushButton *override = button(u"직접 지정(&D)"_s);
    QVERIFY(override);
    QTest::mouseClick(override, Qt::LeftButton);
    const auto d = std::size_t(pending().appearance.design);
    const bool dark = fm::style::isDarkVariant(fm::style::ThemeManager::instance().effectiveVariant());
    const auto &ov = pending().theme.scheme.overrides[d][dark ? 1 : 0];
    QVERIFY(ov[std::size_t(fm::style::Token::AccentFg)].has_value());
    if (!shots.isEmpty())
        dialog.grab().save(shots + u"/settings-theme-adv.png"_s);
    QTest::mouseClick(button(u"자동으로 되돌리기(&U)"_s), Qt::LeftButton);
    QVERIFY(pending().theme.scheme.isPristine());

    // 적용 → 보관소(메모리 — 경로 없음)에 반영
    QVERIFY(st::SettingsStore::instance().filePath().isEmpty());
    dialog.session()->edit(Section::Panel, [](AppSettings &p) { p.panel.showHidden = true; });
    dialog.applyPending();
    QVERIFY(st::SettingsStore::instance().settings().panel.showHidden);
    QVERIFY(!dialog.session()->isDirty());
    dialog.session()->edit(Section::Panel, [](AppSettings &p) { p.panel.showHidden = false; });
    dialog.applyPending();
}

QTEST_MAIN(TestDialogs)
#include "tst_dialogs.moc"
