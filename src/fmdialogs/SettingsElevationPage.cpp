// 설정 › 관리자 권한(docs/specs/05 §2.4) — 왼쪽 행은 .ui, 오른쪽 보호된 위치 · 최근 권한 요청은 코드.

#include "SettingsPages_p.h"

#include "ui_SettingsElevationPage.h"

#include <fmsettings/AppSettings.h>
#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/SettingsWidgets.h>

#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QPainter>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeView>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

namespace fs = fm::style;
using T = fs::Token;
using ES = fm::settings::ElevationSettings;

constexpr int kResultRole = Qt::UserRole + 1;  // 0 ok · 1 warn · 2 mute

/// 보호된 위치 한 행(05 §2.4) — 높이 34: 자물쇠 14 · 경로(고정폭 12.5) · 태그 · × 28.
class PathRow final : public QWidget
{
public:
    PathRow(const QString &path, bool builtin, QWidget *parent)
        : QWidget(parent)
        , m_path(path)
        , m_builtin(builtin)
    {
        setFixedHeight(34);
        m_remove = new QToolButton(this);
        m_remove->setAutoRaise(true);
        m_remove->setFixedSize(28, 28);
        m_remove->setEnabled(!builtin);
        m_remove->setAccessibleName(tr("%1 제거").arg(path));
        m_remove->setToolTip(builtin ? tr("기본 항목은 지울 수 없습니다") : tr("제거"));
        refreshIcon();
        connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, this, [this] {
            refreshIcon();
            update();
        });
    }
    QToolButton *removeButton() const { return m_remove; }
    QString path() const { return m_path; }

protected:
    void resizeEvent(QResizeEvent *) override { m_remove->move(width() - 6 - 28, 3); }
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        if (!m_first)
            p.fillRect(QRect(0, 0, width(), 1), tc[T::Grid]);
        fs::paintGlyph(&p, fs::Glyph::Lock, QRectF(12, (height() - 14) / 2.0, 14, 14), tc[T::Fg3]);
        const QString tag = m_builtin ? tr("기본") : tr("추가함");
        const QSize chip = fm::ui::chipSize(tag, 10.5, 5, 16);
        const qreal chipLeft = width() - 6 - 28 - 8 - chip.width();
        fm::ui::paintChip(&p, QRectF(chipLeft, (height() - chip.height()) / 2.0, chip.width(), chip.height()), tag,
                          m_builtin ? fm::ui::ChipKind::Mute : fm::ui::ChipKind::Accent, tc, 3, 10.5);
        const QFont mono = fs::monoFont(12.5);
        p.setFont(mono);
        p.setPen(tc[T::Fg]);
        const qreal left = 12 + 14 + 8;
        const int width = int(chipLeft - 8 - left);
        p.drawText(QRectF(left, 0, width, height()), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(mono).elidedText(m_path, Qt::ElideMiddle, width));
    }

public:
    bool m_first = false;

private:
    void refreshIcon()
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        fs::GlyphStateColors colors;
        colors.normal = tc[T::Fg2];
        QColor disabled = tc[T::Fg3];
        disabled.setAlphaF(0.5);
        colors.disabled = disabled;
        m_remove->setIcon(fs::glyphIcon(fs::Glyph::Close, colors, 10));
    }

    QString m_path;
    bool m_builtin;
    QToolButton *m_remove = nullptr;
};

/// 결과 알약(05 §2.4) — 높이 20, 모서리 10, 좌우 7, 11 px 600.
class ResultDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(option.widget);
        QStyleOptionViewItem o = option;
        initStyleOption(&o, index);
        o.text.clear();
        option.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &o, p, option.widget);
        const QString text = index.data().toString();
        const int kind = index.data(kResultRole).toInt();
        const QSize size = fm::ui::chipSize(text, 11, 7, 20);
        const QRectF r(option.rect.left() + 10, option.rect.center().y() - 9.5, size.width(), size.height());
        fm::ui::paintChip(p, r, text, kind == 0 ? fm::ui::ChipKind::Ok : kind == 1 ? fm::ui::ChipKind::Warn : fm::ui::ChipKind::Mute, tc,
                          10, 11);
    }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 30);
    }
};

class ElevationPage final : public SettingsPage
{
public:
    ElevationPage(SettingsSession *session, QWidget *parent)
        : SettingsPage(session, parent)
        , ui(std::make_unique<Ui::SettingsElevationPage>())
    {
        ui->setupUi(this);
        const Section s = Section::Elevation;
        bindCheck(ui->preflightSwitch, s, [](const AppSettings &p) { return p.elevation.preflight; },
                  [](AppSettings &p, bool on) { p.elevation.preflight = on; });
        bindCheck(ui->applySwitch, s, [](const AppSettings &p) { return p.elevation.applyToRemainingDefault; },
                  [](AppSettings &p, bool on) { p.elevation.applyToRemainingDefault = on; });
        bindSegment(ui->helperSegment, s, [](const AppSettings &p) { return int(p.elevation.helperLifetime); },
                    [](AppSettings &p, int i) { p.elevation.helperLifetime = ES::HelperLifetime(i); });
        bindCombo(ui->uacCombo, s,
                  [](const AppSettings &p) {
                      const int t = p.elevation.uacTimeoutSec;
                      return t <= 0 ? 3 : t <= 60 ? 0 : t <= 120 ? 1 : 2;
                  },
                  [](AppSettings &p, int i) { p.elevation.uacTimeoutSec = std::array{60, 120, 300, 0}[std::size_t(i)]; });
        bindCombo(ui->takeCombo, s, [](const AppSettings &p) { return int(p.elevation.takeOwnership); },
                  [](AppSettings &p, int i) { p.elevation.takeOwnership = ES::Ownership(i); });
        bindCombo(ui->defaultButtonCombo, s, [](const AppSettings &p) { return int(p.elevation.ownershipDefault); },
                  [](AppSettings &p, int i) { p.elevation.ownershipDefault = ES::OwnershipButton(i); });
        bindCheck(ui->backupSwitch, s, [](const AppSettings &p) { return p.elevation.backupAclBeforeChange; },
                  [](AppSettings &p, bool on) { p.elevation.backupAclBeforeChange = on; });
        bindCheck(ui->warningSwitch, s, [](const AppSettings &p) { return p.elevation.adminTitleWarning; },
                  [](AppSettings &p, bool on) { p.elevation.adminTitleWarning = on; });

        buildProtected();
        buildLog();
    }

    QString pageId() const override { return u"elevation"_s; }
    QString title() const override { return tr("관리자 권한"); }
    QString description() const override
    {
        return tr("파일 작업에 관리자 권한이 필요할 때 언제, 어떻게 물을지 정합니다. 권한이 필요한 작업만 별도의 권한 상승 도우미가 처리합니다.");
    }
    fm::settings::Sections sections() const override { return Section::Elevation; }

    void syncFromPending() override
    {
        SettingsPage::syncFromPending();
        if (m_shownPaths != pending().elevation.protectedPathsUser)
            rebuildPaths();
        const int days = pending().elevation.logRetentionDays;
        const QSignalBlocker block(m_retention);
        m_retention->setCurrentIndex(days <= 0 ? 3 : days <= 7 ? 0 : days <= 30 ? 1 : 2);
    }

private:
    void buildProtected()
    {
        auto *toolbar = new QWidget(ui->protectedCard);
        toolbar->setFixedHeight(40);
        auto *tl = new QHBoxLayout(toolbar);
        tl->setContentsMargins(8, 0, 8, 0);
        auto *add = new fm::ui::Button(tr("위치 추가(&L)"), toolbar);
        add->setCompact(true);
        add->setGlyph(fm::ui::glyph::PlusSmall);
        add->setAutoDefault(false);
        tl->addWidget(add);
        tl->addStretch(1);
        ui->protectedLayout->addWidget(toolbar);
        auto *line = new QFrame(ui->protectedCard);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Plain);
        line->setFixedHeight(1);
        ui->protectedLayout->addWidget(line);
        m_paths = new QWidget(ui->protectedCard);
        m_pathsLayout = new QVBoxLayout(m_paths);
        m_pathsLayout->setContentsMargins(0, 0, 0, 0);
        m_pathsLayout->setSpacing(0);
        ui->protectedLayout->addWidget(m_paths);
        connect(add, &QPushButton::clicked, this, [this] {
            const QString dir = QFileDialog::getExistingDirectory(this, tr("보호할 위치"));
            if (dir.isEmpty())
                return;
            QString path = QDir::toNativeSeparators(dir);
            while (path.size() > 3 && path.endsWith(u'\\'))
                path.chop(1);
            QStringList all = fm::settings::builtinProtectedPaths() + pending().elevation.protectedPathsUser;
            for (const QString &existing : all) {
                if (existing.compare(path, Qt::CaseInsensitive) == 0)
                    return;  // 중복(대소문자 무시)
            }
            session()->edit(Section::Elevation, [&](AppSettings &p) { p.elevation.protectedPathsUser.append(path); });
        });
        rebuildPaths();
    }

    void rebuildPaths()
    {
        qDeleteAll(m_rows);
        m_rows.clear();
        m_shownPaths = pending().elevation.protectedPathsUser;
        bool first = true;
        auto add = [&](const QString &path, bool builtin) {
            auto *row = new PathRow(path, builtin, m_paths);
            m_rows.append(row);
            row->m_first = first;
            first = false;
            m_pathsLayout->addWidget(row);
            connect(row->removeButton(), &QToolButton::clicked, this, [this, path] {
                session()->edit(Section::Elevation, [&](AppSettings &p) { p.elevation.protectedPathsUser.removeAll(path); });
            });
        };
        for (const QString &path : fm::settings::builtinProtectedPaths())
            add(path, true);
        for (const QString &path : std::as_const(m_shownPaths))
            add(path, false);
    }

    void buildLog()
    {
        auto *view = new QTreeView(ui->logCard);
        view->setObjectName(u"elevationLog"_s);
        view->setRootIsDecorated(false);
        view->setFrameShape(QFrame::NoFrame);
        view->setSelectionMode(QAbstractItemView::NoSelection);
        view->setFocusPolicy(Qt::NoFocus);
        view->setUniformRowHeights(true);
        view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        fs::setFlatHeader(view);
        auto *model = new QStandardItemModel(0, 4, view);
        model->setHorizontalHeaderLabels({tr("시각"), tr("작업"), tr("대상"), tr("결과")});
        // 기록은 설정이 아니라 데이터다(05 §4.6) — 데모는 목업의 5줄
        const struct { const char *time, *op, *target, *result; int kind; } rows[] = {
            {"09-28 09:41", "복사", "C:\\Program Files\\FM Tools\\redist\\", "승인", 0},
            {"09-28 09:38", "삭제", "D:\\Work\\fm-core\\build\\msi-staging (3개)", "승인", 0},
            {"09-27 22:15", "이름 변경", "C:\\Program Files\\FM Tools\\settings.ini", "UAC 거부", 1},
            {"09-27 18:02", "삭제", "C:\\Program Files\\WindowsApps\\Fabrikam.PhotoTools…", "건너뜀 · 소유권", 2},
            {"09-26 11:07", "새 폴더", "C:\\Program Files\\FM Tools\\plugins", "승인", 0},
        };
        for (const auto &r : rows) {
            auto *time = new QStandardItem(QString::fromUtf8(r.time));
            time->setFont(fs::withTabularNumbers(fs::pixelFont(font(), 12)));
            auto *target = new QStandardItem(QString::fromUtf8(r.target));
            target->setFont(fs::monoFont(12));
            auto *result = new QStandardItem(QString::fromUtf8(r.result));
            result->setData(r.kind, kResultRole);
            model->appendRow({time, new QStandardItem(QString::fromUtf8(r.op)), target, result});
        }
        view->setModel(model);
        view->setItemDelegateForColumn(3, new ResultDelegate(view));
        view->header()->setStretchLastSection(false);
        view->header()->setSectionResizeMode(QHeaderView::Fixed);
        view->header()->setSectionResizeMode(2, QHeaderView::Stretch);
        view->header()->resizeSection(0, 88);
        view->header()->resizeSection(1, 64);
        view->header()->resizeSection(3, 132);
        view->setFixedHeight(28 + 30 * 5 + 2);
        ui->logLayout->addWidget(view);

        auto *footer = new QWidget(ui->logCard);
        auto *fl = new QHBoxLayout(footer);
        fl->setContentsMargins(10, 8, 10, 8);
        fl->setSpacing(8);
        auto *label = new fm::ui::Label(tr("기록 보관"), fm::ui::Label::Meta, footer);
        m_retention = new QComboBox(footer);
        m_retention->addItems({tr("7일"), tr("30일"), tr("90일"), tr("보관 안 함")});
        m_retention->setFixedWidth(96);
        m_retention->setAccessibleName(tr("기록 보관"));
        auto *clear = new fm::ui::Button(tr("기록 지우기"), footer);
        clear->setCompact(true);
        clear->setAutoDefault(false);
        fl->addWidget(label);
        fl->addWidget(m_retention);
        fl->addStretch(1);
        fl->addWidget(clear);
        auto *line = new QFrame(ui->logCard);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Plain);
        line->setFixedHeight(1);
        ui->logLayout->addWidget(line);
        ui->logLayout->addWidget(footer);
        connect(m_retention, &QComboBox::currentIndexChanged, this, [this](int i) {
            session()->edit(Section::Elevation,
                            [&](AppSettings &p) { p.elevation.logRetentionDays = std::array{7, 30, 90, 0}[std::size_t(i)]; });
        });
        connect(clear, &QPushButton::clicked, model, [model] { model->removeRows(0, model->rowCount()); });
        addCustomItem(m_retention, [](const AppSettings &a, const AppSettings &b) { return a.elevation.logRetentionDays != b.elevation.logRetentionDays; },
                      [](AppSettings &p, const AppSettings &d) { p.elevation.logRetentionDays = d.elevation.logRetentionDays; },
                      Section::Elevation);
    }

    std::unique_ptr<Ui::SettingsElevationPage> ui;
    QWidget *m_paths = nullptr;
    QVBoxLayout *m_pathsLayout = nullptr;
    QStringList m_shownPaths;
    QList<PathRow *> m_rows;
    QComboBox *m_retention = nullptr;
};

} // namespace

SettingsPage *createElevationPage(SettingsSession *session, QWidget *parent)
{
    return new ElevationPage(session, parent);
}

} // namespace fm::dialogs
