#include "CatalogWindow.h"

#include <fmdialogs/DialogCatalog.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/Card.h>
#include <fmwidgets/Label.h>
#include <fmwidgets/SegmentedControl.h>
#include <fmwidgets/SettingsWidgets.h>
#include <fmwidgets/Switch.h>

#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace fm::app {

namespace {

namespace fs = fm::style;

constexpr int kIdRole = Qt::UserRole + 1;

} // namespace

CatalogWindow::CatalogWindow(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(u"대화상자 카탈로그"_s);
    setObjectName(u"CatalogWindow"_s);
    fs::setDensity(this, fs::Density::Dialog);
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    // 왼쪽 — 찾기 + 트리
    auto *left = new QVBoxLayout();
    left->setSpacing(8);
    m_search = new fm::ui::SearchField(this);
    m_search->setPlaceholderText(u"변형 찾기 — 이름 또는 ID"_s);
    m_search->setAccessibleName(u"변형 찾기"_s);
    m_tree = new QTreeView(this);
    m_tree->setObjectName(u"catalogTree"_s);
    m_tree->setUniformRowHeights(true);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setAlternatingRowColors(false);
    fs::setFlatHeader(m_tree);
    left->addWidget(m_search);
    left->addWidget(m_tree, 1);
    root->addLayout(left, 3);

    // 오른쪽 — 고른 변형 · 열기 · 디자인 스위치
    auto *right = new QVBoxLayout();
    right->setSpacing(12);
    auto *details = new fm::ui::Card(this);
    auto *dl = new QVBoxLayout(details);
    dl->setContentsMargins(16, 14, 16, 14);
    dl->setSpacing(6);
    m_title = new fm::ui::Label(u"변형을 고르세요"_s, fm::ui::Label::Heading, details);
    m_id = new fm::ui::Label(QString(), fm::ui::Label::PathMeta, details);
    m_meta = new fm::ui::Label(QString(), fm::ui::Label::Help, details);
    dl->addWidget(m_title);
    dl->addWidget(m_id);
    dl->addWidget(m_meta);
    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    m_open = new fm::ui::Button(u"열기(&O)"_s, details);
    m_open->setRole(fm::ui::Button::Primary);
    m_open->setDefault(true);
    m_closeAll = new fm::ui::Button(u"모두 닫기"_s, details);
    m_openCount = new fm::ui::Label(QString(), fm::ui::Label::Summary, details);
    buttons->addWidget(m_open);
    buttons->addWidget(m_closeAll);
    buttons->addWidget(m_openCount);
    buttons->addStretch(1);
    dl->addSpacing(6);
    dl->addLayout(buttons);
    right->addWidget(details);

    right->addWidget(new fm::ui::Label(u"디자인 · 색 구성표"_s, fm::ui::Label::SectionTitle, this));
    auto *theme = new fm::ui::Card(this);
    auto *tl = new QVBoxLayout(theme);
    tl->setContentsMargins(1, 1, 1, 1);
    tl->setSpacing(0);
    m_watercolor = new fm::ui::Switch(theme);
    m_watercolor->setOnText(u"시안2 · 워터컬러"_s);
    m_watercolor->setOffText(u"시안1 · 기본"_s);
    auto *designRow = new fm::ui::SettingRow(u"디자인"_s, u"열린 대화상자도 바로 바뀐다"_s, theme);
    designRow->addControl(m_watercolor);
    m_scheme = new fm::ui::SegmentedControl(theme);
    m_scheme->setItems({u"시스템"_s, u"라이트"_s, u"다크"_s});
    auto *schemeRow = new fm::ui::SettingRow(u"색 구성표"_s, QString(), theme);
    schemeRow->addControl(m_scheme);
    m_tone = new fm::ui::SegmentedControl(theme);
    m_tone->setItems({u"회색"_s, u"남색"_s});
    auto *toneRow = new fm::ui::SettingRow(u"다크 색조"_s, u"남색은 시안2 전용"_s, theme);
    toneRow->addControl(m_tone);
    for (fm::ui::SettingRow *row : {designRow, schemeRow, toneRow})
        tl->addWidget(row);
    right->addWidget(theme);
    right->addWidget(new fm::ui::Label(u"목업 보드의 상태로 채운 변형을 연다. \"제안\"은 목업에 없는 상태를 보이려고 더한 변형이다. "
                                       "여러 개를 동시에 열어 나란히 둘 수 있다."_s,
                                       fm::ui::Label::Help, this));
    right->addStretch(1);
    root->addLayout(right, 2);

    buildModel();

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_filter->setFilterFixedString(text.trimmed());
        if (!text.trimmed().isEmpty())
            m_tree->expandAll();
    });
    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &index) { showDetails(index); });
    connect(m_tree, &QTreeView::activated, this, [this](const QModelIndex &index) {
        if (const QString id = index.siblingAtColumn(0).data(kIdRole).toString(); !id.isEmpty())
            openVariant(id);
    });
    connect(m_open, &QPushButton::clicked, this, [this] {
        if (const QString id = currentId(); !id.isEmpty())
            openVariant(id);
    });
    connect(m_closeAll, &QPushButton::clicked, this, &CatalogWindow::closeAll);

    auto &tm = fs::ThemeManager::instance();
    // 스타일 교체는 이벤트 처리 중이 아니라 다음 차례에(메인 창 도구 모음과 같음)
    connect(m_watercolor, &QCheckBox::toggled, this, [this, &tm](bool on) {
        if (!m_syncing)
            QMetaObject::invokeMethod(this, [&tm, on] { tm.setDesign(on ? fs::Design::Watercolor : fs::Design::Standard); }, Qt::QueuedConnection);
    });
    connect(m_scheme, &fm::ui::SegmentedControl::currentIndexChanged, this, [this, &tm](int i) {
        if (m_syncing)
            return;
        const auto scheme = i == 1 ? fs::ThemeManager::Scheme::Light : i == 2 ? fs::ThemeManager::Scheme::Dark : fs::ThemeManager::Scheme::System;
        QMetaObject::invokeMethod(this, [&tm, scheme] { tm.setScheme(scheme); }, Qt::QueuedConnection);
    });
    connect(m_tone, &fm::ui::SegmentedControl::currentIndexChanged, this, [this, &tm](int i) {
        if (m_syncing)
            return;
        const auto tone = i == 1 ? fs::ThemeManager::DarkTone::Navy : fs::ThemeManager::DarkTone::Gray;
        QMetaObject::invokeMethod(this, [&tm, tone] { tm.setDarkTone(tone); }, Qt::QueuedConnection);
    });
    connect(&tm, &fs::ThemeManager::changed, this, &CatalogWindow::syncThemeControls);
    syncThemeControls();

    auto *find = new QShortcut(QKeySequence::Find, this);
    connect(find, &QShortcut::activated, this, [this] {
        m_search->setFocus();
        m_search->selectAll();
    });
    resize(960, 680);
    showDetails(QModelIndex());
}

CatalogWindow::~CatalogWindow()
{
    // 열린 대화상자는 자식이라 ~QWidget에서 지워지는데, 그때는 m_dialogs · m_openCount가 이미 없다 — 먼저 끊고 지운다
    const auto dialogs = m_dialogs;
    for (const QPointer<QDialog> &d : dialogs) {
        if (d) {
            disconnect(d, nullptr, this, nullptr);
            delete d.data();
        }
    }
}

void CatalogWindow::buildModel()
{
    m_model = new QStandardItemModel(0, 3, this);
    m_model->setHorizontalHeaderLabels({u"변형"_s, u"ID"_s, u"출처"_s});
    QHash<QString, QStandardItem *> groups;
    const QList<fm::dialogs::DialogVariant> variants = fm::dialogs::dialogVariants();
    for (const fm::dialogs::DialogVariant &v : variants) {
        QStandardItem *group = groups.value(v.dialog);
        if (!group) {
            group = new QStandardItem(fm::dialogs::dialogGroupLabel(v.dialog));
            QFont f = group->font();
            f.setBold(true);
            group->setFont(f);
            m_model->appendRow({group, new QStandardItem(), new QStandardItem()});
            groups.insert(v.dialog, group);
        }
        auto *label = new QStandardItem(v.label);
        label->setData(v.id, kIdRole);
        label->setToolTip(v.id);
        auto *id = new QStandardItem(v.id);
        auto *origin = new QStandardItem(v.fromMockup ? u"목업"_s : u"제안"_s);
        group->appendRow({label, id, origin});
    }
    for (QStandardItem *g : std::as_const(groups))
        g->setText(u"%1  (%2)"_s.arg(g->text()).arg(g->rowCount()));
    m_filter = new QSortFilterProxyModel(this);
    m_filter->setSourceModel(m_model);
    m_filter->setRecursiveFilteringEnabled(true);
    m_filter->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_filter->setFilterKeyColumn(-1);
    m_tree->setModel(m_filter);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->resizeSection(1, 180);
    m_tree->header()->resizeSection(2, 56);
    m_tree->expandAll();
}

QString CatalogWindow::currentId() const
{
    return m_tree->currentIndex().siblingAtColumn(0).data(kIdRole).toString();
}

void CatalogWindow::showDetails(const QModelIndex &index)
{
    const QString id = index.siblingAtColumn(0).data(kIdRole).toString();
    m_open->setEnabled(!id.isEmpty());
    if (id.isEmpty()) {
        m_title->setText(u"변형을 고르세요"_s);
        m_id->clear();
        m_meta->setText(u"왼쪽 트리에서 변형을 두 번 누르거나 Enter로 엽니다. Ctrl+F = 찾기."_s);
        return;
    }
    const QList<fm::dialogs::DialogVariant> variants = fm::dialogs::dialogVariants();
    for (const fm::dialogs::DialogVariant &v : variants) {
        if (v.id != id)
            continue;
        m_title->setText(u"%1 — %2"_s.arg(fm::dialogs::dialogGroupLabel(v.dialog), v.label));
        m_id->setText(v.id);
        QString meta = v.fromMockup ? u"목업 보드에 있는 상태"_s : u"제안 — 목업에 없는 상태"_s;
        if (v.client.width() > 0)
            meta += v.client.height() > 0 ? u" · 목업 클라이언트 %1 × %2"_s.arg(v.client.width()).arg(v.client.height())
                                          : u" · 목업 폭 %1"_s.arg(v.client.width());
        m_meta->setText(meta + u" · fmdemo --open "_s + v.id);
        return;
    }
}

QDialog *CatalogWindow::openVariant(const QString &id)
{
    QDialog *dialog = fm::dialogs::createDialog(id, this);
    if (!dialog)
        return nullptr;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_dialogs.removeAll(nullptr);
    m_dialogs.append(dialog);
    connect(dialog, &QObject::destroyed, this, [this] {
        m_dialogs.removeAll(nullptr);
        m_openCount->setText(openCount() > 0 ? u"열린 대화상자 %1개"_s.arg(openCount()) : QString());
    });
    dialog->show();
    m_openCount->setText(u"열린 대화상자 %1개"_s.arg(openCount()));
    return dialog;
}

void CatalogWindow::closeAll()
{
    const auto dialogs = m_dialogs;
    for (const QPointer<QDialog> &d : dialogs) {
        if (d)
            d->close();
    }
}

int CatalogWindow::openCount() const
{
    int n = 0;
    for (const QPointer<QDialog> &d : m_dialogs)
        n += d ? 1 : 0;
    return n;
}

void CatalogWindow::syncThemeControls()
{
    const auto &tm = fs::ThemeManager::instance();
    m_syncing = true;
    m_watercolor->setChecked(tm.design() == fs::Design::Watercolor);
    m_scheme->setCurrentIndex(tm.scheme() == fs::ThemeManager::Scheme::Light ? 1 : tm.scheme() == fs::ThemeManager::Scheme::Dark ? 2 : 0);
    m_tone->setCurrentIndex(tm.darkTone() == fs::ThemeManager::DarkTone::Navy ? 1 : 0);
    m_tone->setEnabled(tm.design() == fs::Design::Watercolor);
    m_syncing = false;
}

} // namespace fm::app
