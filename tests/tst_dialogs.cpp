// 파일 작업 대화상자(P5) 테스트 — 다중 이름 변경 엔진(02 §9.5 표의 값), 이름 검사 · 이동 해석 · 새 폴더 계획,
// 진행 시뮬레이터, 대화상자 변형 전체 생성, 다중 이름 변경 · 진행 창 동작.

#include <fmdialogs/DialogCatalog.h>
#include <fmdialogs/FileOpContext.h>
#include <fmdialogs/MultiRenameDialog.h>
#include <fmdialogs/Planners.h>
#include <fmdialogs/ProgressDialog.h>
#include <fmdialogs/ProgressSimulator.h>
#include <fmdialogs/RenameEngine.h>
#include <fmdialogs/RenamePreview.h>

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

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
        // 고정 크기 대화상자는 목업 클라이언트 크기(간단히 진행 창은 폭만)
        if (v.client.height() > 0 && v.dialog != u"multirename")
            QCOMPARE(dialog->size(), v.client);
        else if (v.client.width() > 0)
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

QTEST_MAIN(TestDialogs)
#include "tst_dialogs.moc"
