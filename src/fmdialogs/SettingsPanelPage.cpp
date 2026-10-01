// 설정 › 파일 패널(docs/specs/04 §2.3) — 행은 .ui, 미리보기는 메인 창과 같은 FileListView · ThumbnailView를
// 목업의 5개 항목(MockFileSource::settingsPanelPreview)으로 돌린다. 미리보기는 늘 보류 값을 그린다(04 §1.8).

#include "SettingsPages_p.h"

#include "ui_SettingsPanelPage.h"

#include <fmfilelist/FileListModel.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmfilelist/MockFileSource.h>
#include <fmfilelist/ThumbnailView.h>
#include <fmstyle/StylePaint.h>

#include <QLineEdit>
#include <QStackedWidget>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

namespace fl = fm::filelist;
namespace fs = fm::style;
namespace st = fm::settings;
using PS = st::PanelSettings;

const char *const kSeparatorNames[] = {"없음", "교차 배경", "구분선", "여백", "틴트"};

QString separatorName(fl::RecordSeparator s)
{
    return QString::fromUtf8(kSeparatorNames[int(s)]);
}

/// 날짜 형식 선택지 — 표시 글자와 저장 값.
struct DateFormatChoice
{
    const char *label;
    const char *value;
};
constexpr DateFormatChoice kDateFormats[] = {
    {"yyyy-MM-dd HH:mm", "yyyy-MM-dd HH:mm"},
    {"yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd HH:mm:ss"},
    {"시스템 짧은 형식", "system-short"},
    {"시스템 긴 형식", "system-long"},
};

/// 미리보기 열 — 1줄 `1fr | 72 | 118`(이름 · 크기 · 수정한 날짜), 2줄 `36 | 100 | 72 | 1fr`(종류 · 크기 · 날짜).
fl::ListColumnLayout previewLayout()
{
    using R = fl::ListColumn::Role;
    using L = fl::ListColumn::TwoLine;
    fl::ListColumnLayout layout;
    layout.columns = {
        {fl::IconColumn, R::Icon, QString(), 16, 36, Qt::AlignCenter, false, L::Row0Full},
        {fl::NameColumn, R::Name, QString(), 0, 0, Qt::AlignLeft, true, L::Row0Full},
        {fl::TypeColumn, R::Meta, QString(), 100, -1, Qt::AlignLeft, false, L::Row1},
        {fl::SizeColumn, R::Meta, QString(), 72, -1, Qt::AlignRight, true, L::Row1, false, true},
        {fl::ModifiedColumn, R::Meta, QString(), 118, 118, Qt::AlignLeft, true, L::Row1, false, true},
        {fl::FillerColumn, R::Filler, QString(), 0, 0, Qt::AlignLeft, false, L::Row1},
    };
    return layout;
}

class PanelPage final : public SettingsPage
{
public:
    PanelPage(SettingsSession *session, QWidget *parent)
        : SettingsPage(session, parent)
        , ui(std::make_unique<Ui::SettingsPanelPage>())
    {
        ui->setupUi(this);
        ui->sep1Segment->setSegmentSize(fm::ui::SegmentedControl::Compact);
        ui->sep2Segment->setSegmentSize(fm::ui::SegmentedControl::Compact);
        for (QSpinBox *spin : {ui->autoWidthSpin, ui->autoPercentSpin})
            spin->setFont(fs::withTabularNumbers(spin->font()));
        for (const DateFormatChoice &c : kDateFormats)
            ui->dateFormatCombo->addItem(QString::fromUtf8(c.label), QString::fromUtf8(c.value));
        ui->dateFormatCombo->setFont(fs::monoFont(12.5));

        bindRows();
        buildPreview();
        connect(ui->thumbsLink, &QPushButton::clicked, this, [this] { Q_EMIT navigateRequested(u"thumbs"_s); });
    }

    QString pageId() const override { return u"panel"_s; }
    QString title() const override { return tr("파일 패널"); }
    QString description() const override
    {
        return tr("레코드 표시 방식, 커서와 선택, 목록에 보일 항목을 정합니다. 탭에서 따로 바꾼 값이 있으면 그 탭은 탭 설정을 따릅니다.");
    }
    fm::settings::Sections sections() const override { return Section::Panel; }

    void syncFromPending() override
    {
        SettingsPage::syncFromPending();
        const PS &p = pending().panel;
        ui->autoRow->setEnabled(p.defaultViewMode == fl::ViewMode::Auto);
        {
            const QSignalBlocker block(ui->dateFormatCombo);
            const int index = ui->dateFormatCombo->findData(p.dateFormat);
            if (index >= 0)
                ui->dateFormatCombo->setCurrentIndex(index);
            else
                ui->dateFormatCombo->setEditText(p.dateFormat);
        }
        refreshPreview();
    }

    /// 목업 보드: 2줄 구분 = 구분선, 숨김 파일 표시 켬 → "기본값과 다른 설정 2개".
    void showBoardState() override
    {
        session()->edit(Section::Panel, [](AppSettings &p) {
            p.panel.separator2 = fl::RecordSeparator::Line;
            p.panel.showHidden = true;
        });
    }

private:
    void bindRows()
    {
        const Section s = Section::Panel;
        bindSegment(ui->viewModeSegment, s, [](const AppSettings &p) { return int(p.panel.defaultViewMode); },
                    [](AppSettings &p, int i) { p.panel.defaultViewMode = fl::ViewMode(i); });
        bindSpin(ui->autoWidthSpin, s, [](const AppSettings &p) { return p.panel.autoSwitchWidthPx; },
                 [](AppSettings &p, int v) { p.panel.autoSwitchWidthPx = v; });
        bindSpin(ui->autoPercentSpin, s, [](const AppSettings &p) { return p.panel.autoSwitchTruncatedPct; },
                 [](AppSettings &p, int v) { p.panel.autoSwitchTruncatedPct = v; });
        bindSegment(ui->sep1Segment, s, [](const AppSettings &p) { return int(p.panel.separator1); },
                    [](AppSettings &p, int i) { p.panel.separator1 = fl::RecordSeparator(i); });
        bindSegment(ui->sep2Segment, s, [](const AppSettings &p) { return int(p.panel.separator2); },
                    [](AppSettings &p, int i) { p.panel.separator2 = fl::RecordSeparator(i); });
        bindSegment(ui->nameBelowSegment, s, [](const AppSettings &p) { return p.panel.nameBelow ? 1 : 0; },
                    [](AppSettings &p, int i) { p.panel.nameBelow = i == 1; });
        bindCombo(ui->overflowCombo, s, [](const AppSettings &p) { return int(p.panel.nameOverflow); },
                  [](AppSettings &p, int i) { p.panel.nameOverflow = fl::NameElide(i); });

        bindCheck(ui->hiddenSwitch, s, [](const AppSettings &p) { return p.panel.showHidden; },
                  [](AppSettings &p, bool on) { p.panel.showHidden = on; });
        bindCheck(ui->protectedSwitch, s, [](const AppSettings &p) { return p.panel.showProtectedOs; },
                  [](AppSettings &p, bool on) { p.panel.showProtectedOs = on; });
        bindCheck(ui->foldersFirstSwitch, s, [](const AppSettings &p) { return p.panel.foldersFirst; },
                  [](AppSettings &p, bool on) { p.panel.foldersFirst = on; });
        bindCombo(ui->sizeUnitCombo, s, [](const AppSettings &p) { return int(p.panel.sizeUnit); },
                  [](AppSettings &p, int i) { p.panel.sizeUnit = PS::SizeUnit(i); });
        // 날짜 형식: 선택지면 그 값, 아니면 직접 쓴 Qt 형식 문자열
        connect(ui->dateFormatCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
            QString value = text.trimmed();
            for (const DateFormatChoice &c : kDateFormats) {
                if (value == QString::fromUtf8(c.label))
                    value = QString::fromUtf8(c.value);
            }
            if (value.isEmpty() || value == pending().panel.dateFormat)
                return;
            session()->edit(Section::Panel, [&](AppSettings &p) { p.panel.dateFormat = value; });
        });
        addCustomItem(ui->dateFormatCombo, [](const AppSettings &a, const AppSettings &b) { return a.panel.dateFormat != b.panel.dateFormat; },
                      [](AppSettings &p, const AppSettings &d) { p.panel.dateFormat = d.panel.dateFormat; }, s);

        bindCheck(ui->invCursorSwitch, s, [](const AppSettings &p) { return p.panel.inverseCursor; },
                  [](AppSettings &p, bool on) { p.panel.inverseCursor = on; });
        bindCheck(ui->invSelSwitch, s, [](const AppSettings &p) { return p.panel.inverseSelection; },
                  [](AppSettings &p, bool on) { p.panel.inverseSelection = on; });
        bindCheck(ui->boldSwitch, s, [](const AppSettings &p) { return p.panel.boldSelection; },
                  [](AppSettings &p, bool on) { p.panel.boldSelection = on; });
        bindCombo(ui->inactiveCombo, s, [](const AppSettings &p) { return int(p.panel.inactiveCursor); },
                  [](AppSettings &p, int i) { p.panel.inactiveCursor = fl::InactiveCursor(i); });

        // 지금 만지는 설정이 보이도록 미리보기 배치를 고정한다(저장하지 않는 UI 상태, 04 §2.3.2)
        auto force = [this](std::optional<fl::ViewMode> mode) {
            m_forced = mode;
            refreshPreview();
        };
        connect(ui->viewModeSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, [force] { force(std::nullopt); });
        connect(ui->sep1Segment, &fm::ui::SegmentedControl::currentIndexChanged, this, [force] { force(fl::ViewMode::OneLine); });
        connect(ui->sep2Segment, &fm::ui::SegmentedControl::currentIndexChanged, this, [force] { force(fl::ViewMode::TwoLine); });
        connect(ui->nameBelowSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, [force] { force(fl::ViewMode::TwoLine); });
    }

    void buildPreview()
    {
        const fl::MockFolder folder = fl::MockFileSource::settingsPanelPreview();
        m_model = new fl::FileListModel(folder.entries, this);
        m_proxy = new fl::FileSortProxy(this);
        m_proxy->setSourceModel(m_model);
        m_proxy->setShowSystem(true);  // 미리보기는 숨김 스위치만 본다(5번 desktop.ini)
        m_proxy->sort(-1);             // 목업 순서 그대로

        m_stack = new QStackedWidget(ui->previewCard);
        m_list = new fl::FileListView(m_stack);
        m_list->setObjectName(u"panelPreviewList"_s);
        m_list->setPreviewMode(true);
        m_list->setColumnLayout(previewLayout());
        m_list->setModel(m_proxy);
        m_list->setPaneActive(true);
        m_list->setSortIndicator(fl::NameColumn, Qt::AscendingOrder, false);
        m_list->setCursorRow(folder.cursor);
        m_thumbs = new fl::ThumbnailView(m_stack);
        m_thumbs->setObjectName(u"panelPreviewThumbs"_s);
        m_thumbs->setPreviewMode(true);
        m_thumbs->setInfoBarVisible(false);
        m_thumbs->setModel(m_proxy);
        m_thumbs->setPaneActive(true);
        m_thumbs->setCursorRow(folder.cursor);
        m_stack->addWidget(m_list);
        m_stack->addWidget(m_thumbs);
        ui->previewLayout->addWidget(m_stack);
        m_stack->setAttribute(Qt::WA_TransparentForMouseEvents);  // 파일 패널 미리보기는 클릭 무반응(04 §1.8)
        m_stack->setFocusPolicy(Qt::NoFocus);
        connect(m_list, &fl::FileListView::twoLineChanged, this, [this] { refreshNote(); });
    }

    fl::ViewMode previewMode() const { return m_forced.value_or(pending().panel.defaultViewMode); }

    void refreshPreview()
    {
        if (!m_list)
            return;
        const PS &p = pending().panel;
        m_proxy->setShowHidden(p.showHidden);
        const fl::ViewMode mode = previewMode();
        if (mode == fl::ViewMode::Thumbnails) {
            fl::ThumbnailAppearance a = pending().thumbs.toThumbnailAppearance(p);
            a.size = 96;  // 섬네일 · 보통(Main과 같음)
            a.nameLines = 2;
            a.info = fl::ThumbnailAppearance::Info::Size;
            m_thumbs->setAppearance(a);
            m_stack->setCurrentWidget(m_thumbs);
            m_stack->setFixedHeight(2 * (96 + 60) + 16);
        } else {
            m_list->setAppearance(p.toListAppearance());
            m_list->setViewMode(mode);
            m_stack->setCurrentWidget(m_list);
            m_stack->setFixedHeight(m_list->preferredHeight(m_proxy->rowCount()));
        }
        ui->thumbsLink->setVisible(mode == fl::ViewMode::Thumbnails);
        refreshNote();
    }

    void refreshNote()
    {
        const PS &p = pending().panel;
        const fl::ViewMode mode = previewMode();
        QString note;
        if (mode == fl::ViewMode::Thumbnails)
            note = tr("— 섬네일 · 보통");
        else if (mode == fl::ViewMode::Auto && !m_forced)
            note = m_list->isTwoLine() ? tr("— 자동: 긴 이름이 많아 2줄 · %1").arg(separatorName(p.separator2))
                                       : tr("— 자동: 1줄 · %1").arg(separatorName(p.separator1));
        else if (mode == fl::ViewMode::OneLine)
            note = tr("— 1줄 · %1").arg(separatorName(p.separator1));
        else
            note = tr("— 2줄 · %1").arg(separatorName(p.separator2));
        ui->previewNote->setText(note);
        if (mode != fl::ViewMode::Thumbnails)
            m_stack->setFixedHeight(m_list->preferredHeight(m_proxy->rowCount()));
    }

    std::unique_ptr<Ui::SettingsPanelPage> ui;
    fl::FileListModel *m_model = nullptr;
    fl::FileSortProxy *m_proxy = nullptr;
    QStackedWidget *m_stack = nullptr;
    fl::FileListView *m_list = nullptr;
    fl::ThumbnailView *m_thumbs = nullptr;
    std::optional<fl::ViewMode> m_forced;
};

} // namespace

SettingsPage *createPanelPage(SettingsSession *session, QWidget *parent)
{
    return new PanelPage(session, parent);
}

} // namespace fm::dialogs
