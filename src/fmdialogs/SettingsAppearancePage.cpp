// 설정 › 일반 · 모양(docs/specs/04 §2.1) — 행은 모두 .ui, 테마 카드 · 강조색 · 색 구성표 · 글꼴은 코드로 잇는다.
// 강조색 · Windows 강조색 · 색 구성표는 테마 색상 페이지와 같은 보류 값(ThemeSettings)을 공유한다.

#include "SettingsPages_p.h"

#include "ui_SettingsAppearancePage.h"

#include <fmsettings/SettingsStore.h>
#include <fmstyle/ColorScheme.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>

#include <QButtonGroup>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

namespace fs = fm::style;
namespace st = fm::settings;
using T = fs::Token;
using AS = st::AppearanceSettings;
using GS = st::GeneralSettings;
using TS = st::TabSettings;

constexpr std::array kScales{0, 100, 125, 150, 175, 200};
const QStringList kLanguages{QString(), u"ko"_s, u"en"_s};

class AppearancePage final : public SettingsPage
{
public:
    AppearancePage(SettingsSession *session, QWidget *parent)
        : SettingsPage(session, parent)
        , ui(std::make_unique<Ui::SettingsAppearancePage>())
    {
        ui->setupUi(this);
        ui->accentPicker->setAddButtonVisible(false);
        ui->themeGrid->installEventFilter(this);
        for (int px = 9; px <= 20; ++px)
            ui->listSizeCombo->addItem(QString::number(px));
        for (int px = 9; px <= 18; ++px)
            ui->monoSizeCombo->addItem(QString::number(px));
        for (QComboBox *size : {ui->listSizeCombo, ui->monoSizeCombo})
            size->setFont(fs::withTabularNumbers(size->font()));
        const int system = QGuiApplication::primaryScreen()
                               ? qRound(QGuiApplication::primaryScreen()->devicePixelRatio() * 100)
                               : 100;
        ui->scaleCombo->addItem(tr("시스템 따름 (%1%)").arg(system));
        for (std::size_t i = 1; i < kScales.size(); ++i)
            ui->scaleCombo->addItem(u"%1%"_s.arg(kScales[i]));

        m_saved = st::SettingsStore::instance().savedSchemes();
        bindTheme();
        bindFonts();
        bindGeneral();
    }

    QString pageId() const override { return u"appearance"_s; }
    QString title() const override { return tr("일반 · 모양"); }
    QString description() const override { return tr("테마, 강조색, 글꼴과 행 밀도, 시작 동작을 정합니다."); }
    fm::settings::Sections sections() const override { return Section::Appearance | Section::General | Section::Tabs; }

    void syncFromPending() override
    {
        SettingsPage::syncFromPending();
        const AppSettings &p = pending();
        const fs::ThemeSeeds &seeds = p.theme.scheme.seeds;

        // 테마 카드 — 그림 첫 줄은 보류 강조색
        const QColor accent = effectiveAccent(p);
        for (fm::ui::ThemeModeCard *card : {ui->systemCard, ui->lightCard, ui->darkCard}) {
            card->setAccent(accent);
            const QSignalBlocker block(card);
            card->setChecked(int(card->mode()) == int(p.appearance.scheme));
        }

        // 강조색 — Windows 강조색을 쓰면 견본은 사용 안 함
        {
            const QSignalBlocker block(ui->accentPicker);
            ui->accentPicker->setCurrent(seeds.accent);
        }
        ui->accentPicker->setEnabled(!seeds.useSystemAccent);
        ui->accentRow->setDescription(tr("%1 · 버튼, 진행 막대, 선택 표시에 쓰임")
                                          .arg(seeds.useSystemAccent ? tr("Windows 강조색") : ui->accentPicker->currentName()));

        syncSchemeCombo();
        for (const auto &[combo, family] : {std::pair{ui->listFontCombo, p.appearance.listFontFamily},
                                            std::pair{ui->monoFontCombo, p.appearance.monoFontFamily}}) {
            const QSignalBlocker block(combo);
            combo->setCurrentFont(QFont(family));
        }

        // 행 밀도 설명(목업의 유일한 상호작용)
        static const char *densityText[] = {"1줄 22 px · 2줄 40 px", "1줄 24 px · 2줄 44 px", "1줄 28 px · 2줄 52 px"};
        ui->densityRow->setDescription(QString::fromUtf8(densityText[int(p.appearance.density)]));
        // 다시 시작해야 하는 값은 적용된 값과 다르면 알린다
        const AppSettings &applied = session()->applied();
        ui->scaleRow->setDescription(p.appearance.displayScalePercent != applied.appearance.displayScalePercent
                                         ? tr("다시 시작하면 적용")
                                         : QString());
        ui->languageRow->setDescription(p.general.language != applied.general.language ? tr("다시 시작하면 적용") : QString());
        ui->startupRow->setDescription(p.general.startup == GS::Startup::SpecificFolders
                                           ? (p.general.startupFolders.isEmpty() ? tr("지정한 폴더 없음 — 홈 폴더를 엽니다")
                                                                                 : p.general.startupFolders.join(u" · "_s))
                                           : QString());
        const bool watercolor = p.appearance.design == fs::Design::Watercolor;
        ui->darkToneRow->setEnabled(watercolor);
        ui->coloredTitleRow->setEnabled(watercolor);
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        // 테마 색상 페이지에서 저장한 구성표가 목록에 보이게
        m_saved = st::SettingsStore::instance().savedSchemes();
        syncSchemeCombo();
        SettingsPage::showEvent(event);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        // 테마 격자는 카드의 첫 "행" — 아래 행들과 1 px --grid로 구분(04 §2.1.1)
        if (watched == ui->themeGrid && event->type() == QEvent::Paint) {
            QPainter p(ui->themeGrid);
            p.fillRect(QRect(0, ui->themeGrid->height() - 1, ui->themeGrid->width(), 1), fs::themeColorsFor(ui->themeGrid)[T::Grid]);
        }
        return SettingsPage::eventFilter(watched, event);
    }

private:
    static QColor effectiveAccent(const AppSettings &p)
    {
        const fs::ThemeSeeds &seeds = p.theme.scheme.seeds;
        if (seeds.useSystemAccent) {
            if (const auto system = fs::ThemeManager::systemAccent())
                return *system;
        }
        if (seeds.accent)
            return *seeds.accent;
        return QColor::fromRgba(fs::builtinColor(T::Accent, fs::Variant::Light, p.appearance.design));
    }

    void bindTheme()
    {
        // 테마 카드 3장 = 라디오 그룹(Y · L · K)
        auto *group = new QButtonGroup(this);
        group->addButton(ui->systemCard, int(st::Scheme::System));
        group->addButton(ui->lightCard, int(st::Scheme::Light));
        group->addButton(ui->darkCard, int(st::Scheme::Dark));
        connect(group, &QButtonGroup::idClicked, this, [this](int id) {
            session()->edit(Section::Appearance, [&](AppSettings &p) { p.appearance.scheme = st::Scheme(id); });
        });
        addCustomItem(ui->themeGrid, [](const AppSettings &a, const AppSettings &b) { return a.appearance.scheme != b.appearance.scheme; },
                      [](AppSettings &p, const AppSettings &d) { p.appearance.scheme = d.appearance.scheme; }, Section::Appearance);

        connect(ui->accentPicker, &fm::ui::AccentPicker::accentChosen, this, [this](const std::optional<QColor> &accent) {
            session()->edit(Section::Theme, [&](AppSettings &p) { p.theme.scheme.seeds.accent = accent; });
        });
        addCustomItem(ui->accentRow, [](const AppSettings &a, const AppSettings &b) { return a.theme.scheme.seeds.accent != b.theme.scheme.seeds.accent; },
                      [](AppSettings &p, const AppSettings &d) { p.theme.scheme.seeds.accent = d.theme.scheme.seeds.accent; }, Section::Theme);
        bindCheck(ui->systemAccentSwitch, Section::Theme, [](const AppSettings &p) { return p.theme.scheme.seeds.useSystemAccent; },
                  [](AppSettings &p, bool on) { p.theme.scheme.seeds.useSystemAccent = on; });
        bindCheck(ui->darkTitleSwitch, Section::Appearance, [](const AppSettings &p) { return p.appearance.darkTitleBar; },
                  [](AppSettings &p, bool on) { p.appearance.darkTitleBar = on; });

        // 색 구성표: 기본 (내장) + themes 폴더. 고르면 그 구성표의 색으로 바뀐다(수정 내용은 버림)
        connect(ui->schemeCombo, &QComboBox::activated, this, [this](int index) {
            const QString id = ui->schemeCombo->itemData(index).toString();
            if (id == pending().theme.schemeId && !isSchemeModified())
                return;  // 같은 구성표를 다시 고르면 수정 내용만 버린다
            fs::ColorScheme chosen;
            for (const fs::ColorScheme &s : std::as_const(m_saved)) {
                if (s.id == id)
                    chosen = s;
            }
            session()->edit(Section::Theme, [&](AppSettings &p) {
                p.theme.schemeId = id;
                p.theme.scheme = chosen;
            });
        });
        addCustomItem(ui->schemeRow,
                      [](const AppSettings &a, const AppSettings &b) {
                          return a.theme.schemeId != b.theme.schemeId || !a.theme.scheme.sameColors(b.theme.scheme);
                      },
                      [](AppSettings &p, const AppSettings &d) {
                          p.theme.schemeId = d.theme.schemeId;
                          p.theme.scheme = d.theme.scheme;
                      },
                      Section::Theme);
        connect(ui->editColorsButton, &QPushButton::clicked, this, [this] { Q_EMIT navigateRequested(u"theme"_s); });

        // 디자인 · 다크 색조 · 제목 표시줄 색(제안)
        bindSegment(ui->designSegment, Section::Appearance, [](const AppSettings &p) { return int(p.appearance.design); },
                    [](AppSettings &p, int i) { p.appearance.design = fs::Design(i); });
        bindSegment(ui->darkToneSegment, Section::Appearance, [](const AppSettings &p) { return int(p.appearance.darkTone); },
                    [](AppSettings &p, int i) { p.appearance.darkTone = st::DarkTone(i); });
        bindCheck(ui->coloredTitleSwitch, Section::Appearance, [](const AppSettings &p) { return p.appearance.coloredTitleBar; },
                  [](AppSettings &p, bool on) { p.appearance.coloredTitleBar = on; });
    }

    void bindFonts()
    {
        const Section s = Section::Appearance;
        auto bindFont = [&](QFontComboBox *combo, QString AS::*family) {
            connect(combo, &QFontComboBox::currentFontChanged, this, [this, family](const QFont &font) {
                session()->edit(Section::Appearance, [&](AppSettings &p) { p.appearance.*family = font.family(); });
            });
            addCustomItem(combo, [family](const AppSettings &a, const AppSettings &b) { return a.appearance.*family != b.appearance.*family; },
                          [family](AppSettings &p, const AppSettings &d) { p.appearance.*family = d.appearance.*family; }, Section::Appearance);
        };
        bindFont(ui->listFontCombo, &AS::listFontFamily);
        bindFont(ui->monoFontCombo, &AS::monoFontFamily);
        bindCombo(ui->listSizeCombo, s, [](const AppSettings &p) { return std::clamp(p.appearance.listFontPx, 9, 20) - 9; },
                  [](AppSettings &p, int i) { p.appearance.listFontPx = 9 + i; });
        bindCombo(ui->monoSizeCombo, s, [](const AppSettings &p) { return std::clamp(p.appearance.monoFontPx, 9, 18) - 9; },
                  [](AppSettings &p, int i) { p.appearance.monoFontPx = 9 + i; });
        bindSegment(ui->densitySegment, s, [](const AppSettings &p) { return int(p.appearance.density); },
                    [](AppSettings &p, int i) { p.appearance.density = AS::Density(i); });
        bindCombo(ui->scaleCombo, s,
                  [](const AppSettings &p) {
                      const auto it = std::find(kScales.begin(), kScales.end(), p.appearance.displayScalePercent);
                      return it == kScales.end() ? 0 : int(it - kScales.begin());
                  },
                  [](AppSettings &p, int i) { p.appearance.displayScalePercent = kScales[std::size_t(i)]; });
    }

    void bindGeneral()
    {
        bindCombo(ui->languageCombo, Section::General, [](const AppSettings &p) { return std::max<int>(0, int(kLanguages.indexOf(p.general.language))); },
                  [](AppSettings &p, int i) { p.general.language = kLanguages.value(i); });
        bindCombo(ui->startupCombo, Section::General, [](const AppSettings &p) { return int(p.general.startup); },
                  [](AppSettings &p, int i) { p.general.startup = GS::Startup(i); });
        // "지정한 폴더 열기…" — 사용자가 고를 때만 폴더를 묻는다
        connect(ui->startupCombo, &QComboBox::activated, this, [this](int index) {
            if (GS::Startup(index) != GS::Startup::SpecificFolders || !pending().general.startupFolders.isEmpty())
                return;
            const QString dir = QFileDialog::getExistingDirectory(this, tr("시작할 때 열 폴더"));
            if (!dir.isEmpty())
                session()->edit(Section::General, [&](AppSettings &p) { p.general.startupFolders = {QDir::toNativeSeparators(dir)}; });
        });
        bindCheck(ui->singleInstanceSwitch, Section::General, [](const AppSettings &p) { return p.general.singleInstance; },
                  [](AppSettings &p, bool on) { p.general.singleInstance = on; });
        bindCheck(ui->commandLineSwitch, Section::General, [](const AppSettings &p) { return p.general.showCommandLine; },
                  [](AppSettings &p, bool on) { p.general.showCommandLine = on; });
        bindCheck(ui->functionKeysSwitch, Section::General, [](const AppSettings &p) { return p.general.showFunctionKeyBar; },
                  [](AppSettings &p, bool on) { p.general.showFunctionKeyBar = on; });
        bindCombo(ui->trayCombo, Section::General, [](const AppSettings &p) { return int(p.general.trayIcon); },
                  [](AppSettings &p, int i) { p.general.trayIcon = GS::TrayIcon(i); });

        bindCombo(ui->newTabCombo, Section::Tabs, [](const AppSettings &p) { return int(p.tabs.newTabPosition); },
                  [](AppSettings &p, int i) { p.tabs.newTabPosition = TS::NewTabPosition(i); });
        bindCombo(ui->tabTitleCombo, Section::Tabs, [](const AppSettings &p) { return int(p.tabs.title); },
                  [](AppSettings &p, int i) { p.tabs.title = TS::TabTitle(i); });
        bindCheck(ui->rememberViewSwitch, Section::Tabs, [](const AppSettings &p) { return p.tabs.rememberViewPerTab; },
                  [](AppSettings &p, bool on) { p.tabs.rememberViewPerTab = on; });
    }

    bool isSchemeModified() const
    {
        const st::ThemeSettings &t = pending().theme;
        if (t.schemeId == u"builtin")
            return !t.scheme.isPristine();
        for (const fs::ColorScheme &s : m_saved) {
            if (s.id == t.schemeId)
                return !t.scheme.sameColors(s);
        }
        return false;
    }

    void syncSchemeCombo()
    {
        const st::ThemeSettings &t = pending().theme;
        const bool modified = isSchemeModified();
        const QSignalBlocker block(ui->schemeCombo);
        ui->schemeCombo->clear();
        auto add = [&](const QString &name, const QString &id) {
            ui->schemeCombo->addItem(id == t.schemeId && modified ? tr("%1 — 수정됨").arg(name) : name, id);
            ui->schemeCombo->setItemData(ui->schemeCombo->count() - 1, name, Qt::ToolTipRole);
        };
        add(tr("기본 (내장)"), u"builtin"_s);
        bool found = t.schemeId == u"builtin";
        for (const fs::ColorScheme &s : std::as_const(m_saved)) {
            add(s.name.isEmpty() ? s.id : s.name, s.id);
            found = found || s.id == t.schemeId;
        }
        if (!found)
            add(t.scheme.name.isEmpty() ? t.schemeId : t.scheme.name, t.schemeId);  // 파일이 사라진 구성표
        ui->schemeCombo->setCurrentIndex(std::max(0, ui->schemeCombo->findData(t.schemeId)));
    }

    std::unique_ptr<Ui::SettingsAppearancePage> ui;
    QList<fs::ColorScheme> m_saved;
};

} // namespace

SettingsPage *createAppearancePage(SettingsSession *session, QWidget *parent)
{
    return new AppearancePage(session, parent);
}

} // namespace fm::dialogs
