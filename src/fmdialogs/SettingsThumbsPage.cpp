// 설정 › 섬네일 보기(docs/specs/04 §2.4) — 행은 .ui, 오른쪽 미리보기는 메인 창과 같은 ThumbnailView를
// 목업의 9개 항목(MockFileSource::thumbnailPreview)으로 돌린다. 역상 체크 상자는 미리보기 전용(저장 안 함).

#include "SettingsPages_p.h"

#include "ui_SettingsThumbsPage.h"

#include <fmfilelist/FileListModel.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmfilelist/MockFileSource.h>
#include <fmfilelist/ThumbnailView.h>
#include <fmstyle/StylePaint.h>
#include <fmwidgets/SegmentedControl.h>

#include <QMessageBox>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

namespace fl = fm::filelist;
namespace fs = fm::style;
namespace st = fm::settings;
using TS = st::ThumbSettings;

class ThumbsPage final : public SettingsPage
{
public:
    ThumbsPage(SettingsSession *session, QWidget *parent)
        : SettingsPage(session, parent)
        , ui(std::make_unique<Ui::SettingsThumbsPage>())
    {
        ui->setupUi(this);
        ui->targetsCombo->setCheckItems({tr("이미지"), tr("동영상"), tr("PDF"), tr("글꼴"), tr("문서")});
        for (QSpinBox *spin : {ui->concurrencySpin, ui->autoPercentSpin})
            spin->setFont(fs::withTabularNumbers(spin->font()));
        bindRows();
        buildPreview();
    }

    QString pageId() const override { return u"thumbs"_s; }
    QString title() const override { return tr("섬네일 보기"); }
    QString description() const override
    {
        return tr("패널을 섬네일로 볼 때의 크기, 이름 표시, 섬네일을 만드는 방법을 정합니다. 표시 방식은 패널의 보기 버튼이나 Ctrl+Shift+4로 바꿉니다.");
    }
    fm::settings::Sections sections() const override { return Section::Thumbs; }

    void syncFromPending() override
    {
        SettingsPage::syncFromPending();
        const TS &t = pending().thumbs;
        {
            const QSignalBlocker block(ui->targetsCombo);
            ui->targetsCombo->setCheckedMask(t.targets);
        }
        ui->autoPercentSpin->setEnabled(t.autoThumbnailFolders);
        ui->autoPercentUnit->setEnabled(t.autoThumbnailFolders);
        // 역상 체크 상자의 처음 값 = 파일 패널 페이지의 보류 값(바뀌면 다시 따라간다)
        const auto panelInv = std::pair{pending().panel.inverseCursor, pending().panel.inverseSelection};
        if (panelInv != m_panelInv) {
            m_panelInv = panelInv;
            const QSignalBlocker b1(ui->invCursorCheck);
            const QSignalBlocker b2(ui->invSelCheck);
            ui->invCursorCheck->setChecked(panelInv.first);
            ui->invSelCheck->setChecked(panelInv.second);
        }
        refreshPreview();
    }

    /// 목업 보드: 미리보기 역상 커서 · 역상 선택 둘 다 켬.
    void showBoardState() override
    {
        ui->invCursorCheck->setChecked(true);
        ui->invSelCheck->setChecked(true);
    }

private:
    void bindRows()
    {
        const Section s = Section::Thumbs;
        bindSegment(ui->sizeSegment, s,
                    [](const AppSettings &p) {
                        const auto *it = std::find(std::begin(fl::kThumbnailSizes), std::end(fl::kThumbnailSizes), p.thumbs.sizePx);
                        return it == std::end(fl::kThumbnailSizes) ? 1 : int(it - std::begin(fl::kThumbnailSizes));
                    },
                    [](AppSettings &p, int i) { p.thumbs.sizePx = fl::kThumbnailSizes[i]; });
        bindSegment(ui->nameLinesSegment, s, [](const AppSettings &p) { return p.thumbs.nameLines == 0 ? 2 : p.thumbs.nameLines - 1; },
                    [](AppSettings &p, int i) { p.thumbs.nameLines = i == 2 ? 0 : i + 1; });
        bindSegment(ui->infoSegment, s, [](const AppSettings &p) { return int(p.thumbs.info); },
                    [](AppSettings &p, int i) { p.thumbs.info = fl::ThumbnailAppearance::Info(i); });
        bindSegment(ui->fitSegment, s, [](const AppSettings &p) { return p.thumbs.fill ? 1 : 0; },
                    [](AppSettings &p, int i) { p.thumbs.fill = i == 1; });
        bindCheck(ui->badgeSwitch, s, [](const AppSettings &p) { return p.thumbs.typeBadge; },
                  [](AppSettings &p, bool on) { p.thumbs.typeBadge = on; });

        bindCombo(ui->providerCombo, s, [](const AppSettings &p) { return int(p.thumbs.provider); },
                  [](AppSettings &p, int i) { p.thumbs.provider = TS::Provider(i); });
        connect(ui->targetsCombo, &fm::ui::CheckListCombo::checkedMaskChanged, this, [this](int mask) {
            session()->edit(Section::Thumbs, [&](AppSettings &p) { p.thumbs.targets = mask; });
        });
        addCustomItem(ui->targetsCombo, [](const AppSettings &a, const AppSettings &b) { return a.thumbs.targets != b.thumbs.targets; },
                      [](AppSettings &p, const AppSettings &d) { p.thumbs.targets = d.thumbs.targets; }, s);
        bindCheck(ui->networkSwitch, s, [](const AppSettings &p) { return p.thumbs.iconsOnlyOnNetworkRemovable; },
                  [](AppSettings &p, bool on) { p.thumbs.iconsOnlyOnNetworkRemovable = on; });
        bindSpin(ui->concurrencySpin, s, [](const AppSettings &p) { return p.thumbs.concurrency; },
                 [](AppSettings &p, int v) { p.thumbs.concurrency = v; });
        connect(ui->clearCacheButton, &QPushButton::clicked, this, [this] {
            // 앱 자체 캐시만 지운다(Windows 섬네일 캐시는 그대로) — 데모에는 캐시가 없어 안내만 바꾼다
            if (QMessageBox::question(this, tr("캐시 비우기"), tr("이 앱이 만든 섬네일 캐시를 지울까요?\nWindows 섬네일 캐시는 그대로 둡니다."))
                == QMessageBox::Yes)
                ui->concurrencyRow->setDescription(tr("캐시를 비웠습니다 · 보이는 항목부터 다시 만듭니다"));
        });

        bindSpin(ui->autoPercentSpin, s, [](const AppSettings &p) { return p.thumbs.autoThumbnailPct; },
                 [](AppSettings &p, int v) { p.thumbs.autoThumbnailPct = v; });
        bindCheck(ui->autoSwitch, s, [](const AppSettings &p) { return p.thumbs.autoThumbnailFolders; },
                  [](AppSettings &p, bool on) { p.thumbs.autoThumbnailFolders = on; });

        connect(ui->invCursorCheck, &QCheckBox::toggled, this, [this] { refreshPreview(); });
        connect(ui->invSelCheck, &QCheckBox::toggled, this, [this] { refreshPreview(); });
    }

    void buildPreview()
    {
        const fl::MockFolder folder = fl::MockFileSource::thumbnailPreview();
        m_model = new fl::FileListModel(folder.entries, this);
        m_proxy = new fl::FileSortProxy(this);
        m_proxy->setSourceModel(m_model);
        m_proxy->sort(-1);  // 목업 순서 그대로
        m_view = new fl::ThumbnailView(ui->previewCard);
        m_view->setObjectName(u"thumbsPreview"_s);
        m_view->setPreviewMode(true);
        m_view->setSortText(tr("이름 ↑"));
        m_view->setModel(m_proxy);
        m_view->setPaneActive(true);
        m_view->setCursorRow(folder.cursor);
        m_view->setMinimumHeight(320);
        ui->previewLayout->addWidget(m_view);
        // 미리보기 전용 — 두 섬네일 구현 비교(PLAN §7.4, 저장하지 않음)
        auto *backend = new fm::ui::SegmentedControl(this);
        backend->setObjectName(u"backendSegment"_s);
        backend->setItems({tr("Qt 목록"), tr("Qtitan 카드")});
        backend->setSegmentSize(fm::ui::SegmentedControl::Small);
        backend->setAccessibleName(tr("섬네일 구현"));
        backend->setToolTip(tr("미리보기에서 두 구현을 비교합니다(저장하지 않음)"));
        ui->previewHeader->insertWidget(1, backend);
        connect(backend, &fm::ui::SegmentedControl::currentIndexChanged, this, [this](int i) {
            m_view->setBackend(i == 1 ? fl::ThumbnailView::QtitanCards : fl::ThumbnailView::QtList);
        });
    }

    void refreshPreview()
    {
        if (!m_view)
            return;
        fl::ThumbnailAppearance a = pending().thumbs.toThumbnailAppearance(pending().panel);
        a.invertCursor = ui->invCursorCheck->isChecked();
        a.invertSelection = ui->invSelCheck->isChecked();
        a.boldSelection = true;  // 목업 미리보기는 선택 이름 굵게(bsel)
        m_view->setAppearance(a);
    }

    std::unique_ptr<Ui::SettingsThumbsPage> ui;
    fl::FileListModel *m_model = nullptr;
    fl::FileSortProxy *m_proxy = nullptr;
    fl::ThumbnailView *m_view = nullptr;
    std::pair<bool, bool> m_panelInv{false, false};
};

} // namespace

SettingsPage *createThumbsPage(SettingsSession *session, QWidget *parent)
{
    return new ThumbsPage(session, parent);
}

} // namespace fm::dialogs
