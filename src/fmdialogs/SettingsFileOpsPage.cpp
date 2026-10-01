// 설정 › 파일 작업(docs/specs/05 §2.3) — .ui 행 18개. 대화상자 · 진행 창 · 다중 이름 변경의 처음 값.

#include "SettingsPages_p.h"

#include "ui_SettingsFileOpsPage.h"

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

using FO = fm::settings::FileOpsSettings;

class FileOpsPage final : public SettingsPage
{
public:
    FileOpsPage(SettingsSession *session, QWidget *parent)
        : SettingsPage(session, parent)
        , ui(std::make_unique<Ui::SettingsFileOpsPage>())
    {
        ui->setupUi(this);
        const Section s = Section::FileOps;
        bindCombo(ui->conflictCombo, s, [](const AppSettings &p) { return int(p.fileOps.onConflict); },
                  [](AppSettings &p, int i) { p.fileOps.onConflict = FO::Conflict(i); });
        bindCheck(ui->verifySwitch, s, [](const AppSettings &p) { return p.fileOps.verifyHash; },
                  [](AppSettings &p, bool on) { p.fileOps.verifyHash = on; });
        bindCheck(ui->attributesSwitch, s, [](const AppSettings &p) { return p.fileOps.keepAttributes; },
                  [](AppSettings &p, bool on) { p.fileOps.keepAttributes = on; });
        bindCheck(ui->aclSwitch, s, [](const AppSettings &p) { return p.fileOps.copyAcl; },
                  [](AppSettings &p, bool on) { p.fileOps.copyAcl = on; });
        bindCheck(ui->adsSwitch, s, [](const AppSettings &p) { return p.fileOps.copyAds; },
                  [](AppSettings &p, bool on) { p.fileOps.copyAds = on; });
        bindCombo(ui->linksCombo, s, [](const AppSettings &p) { return int(p.fileOps.links); },
                  [](AppSettings &p, int i) { p.fileOps.links = FO::Links(i); });
        bindSegment(ui->deleteModeSegment, s, [](const AppSettings &p) { return int(p.fileOps.deleteMode); },
                    [](AppSettings &p, int i) { p.fileOps.deleteMode = FO::DeleteMode(i); });
        bindCheck(ui->confirmSwitch, s, [](const AppSettings &p) { return p.fileOps.confirmDelete; },
                  [](AppSettings &p, bool on) { p.fileOps.confirmDelete = on; });
        bindCombo(ui->readOnlyCombo, s, [](const AppSettings &p) { return int(p.fileOps.readOnly); },
                  [](AppSettings &p, int i) { p.fileOps.readOnly = FO::ReadOnly(i); });

        // 진행 창 표시: 바로(0) · 1초 · 3초 · 표시 안 함(-1)
        bindCombo(ui->progressShowCombo, s,
                  [](const AppSettings &p) {
                      const int ms = p.fileOps.progressDelayMs;
                      return ms < 0 ? 3 : ms == 0 ? 0 : ms <= 1000 ? 1 : 2;
                  },
                  [](AppSettings &p, int i) { p.fileOps.progressDelayMs = std::array{0, 1000, 3000, -1}[std::size_t(i)]; });
        bindSegment(ui->progressOpenSegment, s, [](const AppSettings &p) { return p.fileOps.progressDetailed ? 1 : 0; },
                    [](AppSettings &p, int i) { p.fileOps.progressDetailed = i == 1; });
        bindCheck(ui->closeSwitch, s, [](const AppSettings &p) { return p.fileOps.closeWhenDone; },
                  [](AppSettings &p, bool on) { p.fileOps.closeWhenDone = on; });
        bindCheck(ui->notifySwitch, s, [](const AppSettings &p) { return p.fileOps.notifyWhenDone; },
                  [](AppSettings &p, bool on) { p.fileOps.notifyWhenDone = on; });
        bindCombo(ui->jobsCombo, s,
                  [](const AppSettings &p) { return p.fileOps.maxConcurrentJobs <= 0 ? 4 : std::clamp(p.fileOps.maxConcurrentJobs, 1, 4) - 1; },
                  [](AppSettings &p, int i) { p.fileOps.maxConcurrentJobs = i == 4 ? 0 : i + 1; });

        bindSegment(ui->recordSegment, s, [](const AppSettings &p) { return int(p.fileOps.renamePreview); },
                    [](AppSettings &p, int i) { p.fileOps.renamePreview = FO::RecordMode(i); });
        bindCombo(ui->presetCombo, s,
                  [this](const AppSettings &p) {
                      const QString &preset = p.fileOps.renameDefaultPreset;
                      if (preset == u"@last")
                          return 3;
                      if (preset.isEmpty())
                          return 4;
                      const int i = ui->presetCombo->findText(preset);
                      return i >= 0 && i < 3 ? i : 0;
                  },
                  [this](AppSettings &p, int i) {
                      p.fileOps.renameDefaultPreset = i == 3 ? u"@last"_s : i == 4 ? QString() : ui->presetCombo->itemText(i);
                  });
        bindCombo(ui->undoCombo, s,
                  [](const AppSettings &p) {
                      switch (p.fileOps.renameUndoDepth) {
                      case 0:   return 4;
                      case 10:  return 0;
                      case 50:  return 2;
                      case 100: return 3;
                      default:  return 1;
                      }
                  },
                  [](AppSettings &p, int i) { p.fileOps.renameUndoDepth = std::array{10, 20, 50, 100, 0}[std::size_t(i)]; });
        bindCombo(ui->problemCombo, s, [](const AppSettings &p) { return int(p.fileOps.renameOnProblem); },
                  [](AppSettings &p, int i) { p.fileOps.renameOnProblem = FO::RenameProblem(i); });
    }

    QString pageId() const override { return u"fileops"_s; }
    QString title() const override { return tr("파일 작업"); }
    QString description() const override
    {
        return tr("복사 · 이동 · 삭제 대화상자와 진행 창, 다중 이름 변경 도구의 기본값입니다. 대화상자에서 바꾼 값은 그 작업에만 적용됩니다.");
    }
    fm::settings::Sections sections() const override { return Section::FileOps; }

private:
    std::unique_ptr<Ui::SettingsFileOpsPage> ui;
};

} // namespace

SettingsPage *createFileOpsPage(SettingsSession *session, QWidget *parent)
{
    return new FileOpsPage(session, parent);
}

} // namespace fm::dialogs
