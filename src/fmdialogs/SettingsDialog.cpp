#include "fmdialogs/SettingsDialog.h"

#include "SettingsPages_p.h"

#include <fmsettings/SettingsStore.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/DialogChrome.h>
#include <fmwidgets/DialogFooter.h>
#include <fmwidgets/Label.h>
#include <fmwidgets/SegmentedControl.h>
#include <fmwidgets/SettingsWidgets.h>

#include <QAbstractButton>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QScreen>
#include <QScrollArea>
#include <QShortcut>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace fs = fm::style;
namespace st = fm::settings;
using T = fs::Token;

namespace {

constexpr int kPageIdRole = Qt::UserRole + 1;
constexpr int kHitsRole = Qt::UserRole + 2;

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

QString normalizedQuery(const QString &text)
{
    QString t = text.toLower();
    t.remove(u' ');
    t.remove(u'&');
    return t;
}

/// 탐색 항목(04 §1.3) — 36 px, 모서리 6, 선택 = --accent-soft + 600 --accent-fg + 왼쪽 3 × 16 막대.
class NavDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return QSize(212, 38); }

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(option.widget);
        const bool square = tc.isWatercolor();
        const bool selected = option.state & QStyle::State_Selected;
        const bool hover = option.state & QStyle::State_MouseOver;
        const QVariant hits = index.data(kHitsRole);
        const bool dimmed = hits.isValid() && hits.toInt() == 0;
        const QRectF r = QRectF(option.rect).adjusted(0, 1, 0, -1);  // 항목 간격 2
        p->save();
        p->setRenderHint(QPainter::Antialiasing, !square);
        if (selected || hover) {
            p->setPen(Qt::NoPen);
            p->setBrush(selected ? tc[T::AccentSoft] : mix(tc[T::Win], tc[T::Fg], 0.05));
            p->drawRoundedRect(r, square ? 0 : 6, square ? 0 : 6);
        }
        if (selected) {
            p->setBrush(tc[T::Accent]);
            p->drawRoundedRect(QRectF(r.left(), r.top() + 10, 3, 16), square ? 0 : 2, square ? 0 : 2);
        }
        const QColor iconColor = selected ? tc[T::AccentFg] : dimmed ? tc[T::Fg3] : tc[T::Fg2];
        p->setOpacity(dimmed ? 0.5 : 1.0);
        paintSettingsNavIcon(p, index.data(kPageIdRole).toString(), QRectF(r.left() + 10, r.center().y() - 8, 16, 16), iconColor);
        p->setOpacity(1.0);
        QFont f = fs::pixelFont(option.font, 13, selected ? QFont::DemiBold : QFont::Normal);
        p->setFont(f);
        p->setPen(selected ? tc[T::AccentFg] : dimmed ? tc[T::Fg3] : tc[T::Fg]);
        qreal right = r.right() - 10;
        if (hits.isValid() && hits.toInt() > 0) {
            const QString count = QString::number(hits.toInt());
            p->save();
            p->setFont(fs::withTabularNumbers(fs::pixelFont(option.font, 11.5)));
            p->setPen(tc[T::Fg3]);
            const int w = QFontMetrics(p->font()).horizontalAdvance(count);
            p->drawText(QRectF(right - w, r.top(), w, r.height()), Qt::AlignRight | Qt::AlignVCenter, count);
            p->restore();
            right -= w + 8;
        }
        const QRectF text(r.left() + 36, r.top(), right - r.left() - 36, r.height());
        p->drawText(text, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(f).elidedText(index.data().toString(), Qt::ElideRight, int(text.width())));
        if ((option.state & QStyle::State_HasFocus) && (option.state & QStyle::State_KeyboardFocusChange)) {
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(tc[T::Focus], 2.0));
            p->drawRoundedRect(r.adjusted(1, 1, -1, -1), square ? 0 : 6, square ? 0 : 6);
        }
        p->restore();
    }
};

} // namespace

// ---------------------------------------------------------------------------------------------
// 탐색 아이콘

void paintSettingsNavIcon(QPainter *painter, const QString &pageId, const QRectF &rect, const QColor &color)
{
    static const QHash<QString, QString> paths = {
        {u"appearance"_s, u"M8 1.5a6.5 6.5 0 1 0 0 13c.9 0 1.4-.6 1.4-1.3 0-.9-.8-1.2-.8-2 0-.7.6-1.2 1.3-1.2H12a2.5 2.5 0 0 0 2.5-2.5C14.5 4.2 11.6 1.5 8 1.5zM4.8 7.2h.01M6.5 4.6h.01M9.6 4.6h.01"_s},
        {u"theme"_s, u"M2.5 2.5h4v9a2 2 0 0 1-4 0zM6.5 5.5l2.9-2.9 2.8 2.8-5.7 5.7M9 13.5h4.5V9.5h-3.3M4.5 11.5h.01"_s},
        {u"panel"_s, u"M1.5 3h13v10h-13zM8 3v10"_s},
        {u"thumbs"_s, u"M2 2h5v5H2zM9 2h5v5H9zM2 9h5v5H2zM9 9h5v5H9z"_s},
        {u"groups"_s, u"M2 2.5h5.3l6.2 6.2-4.8 4.8L2.5 7.3V2.5zM5 5h.01"_s},
        {u"columns"_s, u"M1.5 3h13v10h-13zM6 3v10M10.5 3v10"_s},
        {u"fileops"_s, u"M5 5h8.5v8.5H5zM11 5V2.5H2.5V11H5"_s},
        {u"elevation"_s, u"M8 1.5l5.5 2v4.2c0 3.3-2.3 5.8-5.5 6.8-3.2-1-5.5-3.5-5.5-6.8V3.5z"_s},
        {u"keys"_s, u"M1.5 4h13v8h-13zM4 6.7h.01M6.5 6.7h.01M9 6.7h.01M11.5 6.7h.01M4.5 9.5h7"_s},
    };
    const QString path = paths.value(pageId);
    if (path.isEmpty())
        return;
    const QString svg = u"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><path d='%1' fill='none' stroke='%2' "
                        u"stroke-width='1.3' stroke-linecap='round' stroke-linejoin='round'/></svg>"_s.arg(path, color.name());
    QSvgRenderer renderer(svg.toUtf8());
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    renderer.render(painter, rect);
    painter->restore();
}

// ---------------------------------------------------------------------------------------------
// SettingsSession

SettingsSession::SettingsSession(const AppSettings &applied, QObject *parent)
    : QObject(parent)
    , m_applied(applied)
    , m_pending(applied)
{
}

void SettingsSession::edit(Section section, const std::function<void(AppSettings &)> &mutate)
{
    const AppSettings before = m_pending;
    mutate(m_pending);
    if (!(before == m_pending))
        Q_EMIT pendingChanged(section);
}

bool SettingsSession::isDirty() const
{
    return bool(st::differingSections(m_applied, m_pending) & ~st::Sections(Section::Dialog));
}

void SettingsSession::apply()
{
    m_applied = m_pending;
    st::SettingsStore::instance().setSettings(m_applied);
    Q_EMIT appliedChanged();
}

void SettingsSession::revert()
{
    m_pending = m_applied;
}

// ---------------------------------------------------------------------------------------------
// SettingsPage

SettingsPage::SettingsPage(SettingsSession *session, QWidget *parent)
    : QWidget(parent)
    , m_session(session)
{
}

const AppSettings &SettingsPage::defaults()
{
    static const AppSettings d;
    return d;
}

QString SettingsPage::footerPath() const
{
    return st::SettingsStore::displayPath();
}

QList<QAbstractButton *> SettingsPage::footerButtons()
{
    static const char *key = "fmResetButton";
    auto *existing = findChild<fm::ui::Button *>(QString::fromLatin1(key), Qt::FindDirectChildrenOnly);
    if (!existing) {
        existing = makeResetButton();
        existing->setObjectName(QString::fromLatin1(key));
    }
    return {existing};
}

fm::ui::Button *SettingsPage::makeResetButton()
{
    auto *b = new fm::ui::Button(tr("기본값으로 되돌리기(&R)"), this);
    b->setAutoDefault(false);
    b->hide();
    connect(b, &QPushButton::clicked, this, &SettingsPage::resetToDefaults);
    return b;
}

fm::ui::Button *SettingsPage::makeImportExportButton()
{
    auto *b = new fm::ui::Button(tr("가져오기 · 내보내기(&E)"), this);
    b->setAutoDefault(false);
    b->hide();
    auto *menu = new QMenu(b);
    connect(menu->addAction(tr("가져오기…")), &QAction::triggered, this, &SettingsPage::importSections);
    connect(menu->addAction(tr("내보내기…")), &QAction::triggered, this, &SettingsPage::exportSections);
    b->setMenu(menu);
    return b;
}

namespace {

QStringList sectionKeys(st::Sections sections)
{
    QStringList keys;
    const std::pair<Section, const char *> table[] = {
        {Section::Appearance, "appearance"}, {Section::General, "general"}, {Section::Tabs, "tabs"}, {Section::Theme, "theme"},
        {Section::Panel, "panel"}, {Section::Thumbs, "thumbs"}, {Section::Groups, "groups"}, {Section::Columns, "columns"},
        {Section::FileOps, "fileOps"}, {Section::Elevation, "elevation"}, {Section::Keys, "keys"}};
    for (const auto &[section, key] : table) {
        if (sections & section)
            keys.append(QString::fromLatin1(key));
    }
    return keys;
}

} // namespace

void SettingsPage::exportSections()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("내보내기"), title() + u".json"_s, tr("설정 (*.json)"));
    if (path.isEmpty())
        return;
    const QJsonObject all = pending().toJson();
    QJsonObject payload;
    for (const QString &key : sectionKeys(sections()))
        payload[key] = all.value(key);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("내보내기"), file.errorString());
        return;
    }
    file.write(QJsonDocument(QJsonObject{{u"format"_s, u"fm-settings-section"_s}, {u"data"_s, payload}}).toJson());
}

void SettingsPage::importSections()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("가져오기"), QString(), tr("설정 (*.json)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("가져오기"), file.errorString());
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value(u"format"_s).toString() != u"fm-settings-section") {
        QMessageBox::warning(this, tr("가져오기"), tr("설정 파일이 아닙니다."));
        return;
    }
    const QJsonObject payload = root.value(u"data"_s).toObject();
    QJsonObject merged = pending().toJson();
    for (const QString &key : sectionKeys(sections())) {
        if (payload.contains(key))
            merged[key] = payload.value(key);
    }
    const AppSettings imported = AppSettings::fromJson(merged);
    const st::Sections scope = sections();
    for (const Section s : {Section::Appearance, Section::General, Section::Tabs, Section::Theme, Section::Panel, Section::Thumbs,
                            Section::Groups, Section::Columns, Section::FileOps, Section::Elevation, Section::Keys}) {
        if (scope & s)
            session()->edit(s, [&](AppSettings &p) {
                const QString key = sectionKeys(s).value(0);
                QJsonObject json = p.toJson();
                json[key] = imported.toJson().value(key);
                p = AppSettings::fromJson(json);
            });
    }
}

void SettingsPage::addBinding(Binding binding)
{
    m_bindings.append(std::move(binding));
}

void SettingsPage::bindCheck(QAbstractButton *widget, Section section, std::function<bool(const AppSettings &)> get,
                             std::function<void(AppSettings &, bool)> set)
{
    connect(widget, &QAbstractButton::toggled, this, [this, section, set](bool on) {
        session()->edit(section, [&](AppSettings &p) { set(p, on); });
    });
    addBinding({widget, section, [widget, get](const AppSettings &p) {
                    const QSignalBlocker block(widget);
                    widget->setChecked(get(p));
                },
                [get](const AppSettings &a, const AppSettings &b) { return get(a) != get(b); },
                [get, set](AppSettings &p, const AppSettings &d) { set(p, get(d)); }});
}

void SettingsPage::bindCombo(QComboBox *widget, Section section, std::function<int(const AppSettings &)> get,
                             std::function<void(AppSettings &, int)> set)
{
    connect(widget, &QComboBox::currentIndexChanged, this, [this, section, set](int index) {
        if (index >= 0)
            session()->edit(section, [&](AppSettings &p) { set(p, index); });
    });
    addBinding({widget, section, [widget, get](const AppSettings &p) {
                    const QSignalBlocker block(widget);
                    widget->setCurrentIndex(get(p));
                },
                [get](const AppSettings &a, const AppSettings &b) { return get(a) != get(b); },
                [get, set](AppSettings &p, const AppSettings &d) { set(p, get(d)); }});
}

void SettingsPage::bindSegment(fm::ui::SegmentedControl *widget, Section section, std::function<int(const AppSettings &)> get,
                               std::function<void(AppSettings &, int)> set)
{
    connect(widget, &fm::ui::SegmentedControl::currentIndexChanged, this, [this, section, set](int index) {
        if (index >= 0)
            session()->edit(section, [&](AppSettings &p) { set(p, index); });
    });
    addBinding({widget, section, [widget, get](const AppSettings &p) {
                    const QSignalBlocker block(widget);
                    widget->setCurrentIndex(get(p));
                },
                [get](const AppSettings &a, const AppSettings &b) { return get(a) != get(b); },
                [get, set](AppSettings &p, const AppSettings &d) { set(p, get(d)); }});
}

void SettingsPage::bindSpin(QSpinBox *widget, Section section, std::function<int(const AppSettings &)> get,
                            std::function<void(AppSettings &, int)> set)
{
    connect(widget, &QSpinBox::valueChanged, this, [this, section, set](int value) {
        session()->edit(section, [&](AppSettings &p) { set(p, value); });
    });
    addBinding({widget, section, [widget, get](const AppSettings &p) {
                    const QSignalBlocker block(widget);
                    widget->setValue(get(p));
                },
                [get](const AppSettings &a, const AppSettings &b) { return get(a) != get(b); },
                [get, set](AppSettings &p, const AppSettings &d) { set(p, get(d)); }});
}

void SettingsPage::bindText(QLineEdit *widget, Section section, std::function<QString(const AppSettings &)> get,
                            std::function<void(AppSettings &, const QString &)> set)
{
    connect(widget, &QLineEdit::textEdited, this, [this, section, set](const QString &text) {
        session()->edit(section, [&](AppSettings &p) { set(p, text); });
    });
    addBinding({widget, section, [widget, get](const AppSettings &p) {
                    const QSignalBlocker block(widget);
                    if (widget->text() != get(p))
                        widget->setText(get(p));
                },
                [get](const AppSettings &a, const AppSettings &b) { return get(a) != get(b); },
                [get, set](AppSettings &p, const AppSettings &d) { set(p, get(d)); }});
}

void SettingsPage::addCustomItem(QWidget *rowWidget, std::function<bool(const AppSettings &, const AppSettings &)> differs,
                                 std::function<void(AppSettings &, const AppSettings &)> reset, Section section)
{
    addBinding({rowWidget, section, nullptr, std::move(differs), std::move(reset)});
}

void SettingsPage::syncFromPending()
{
    for (const Binding &b : std::as_const(m_bindings)) {
        if (b.sync)
            b.sync(pending());
    }
    refreshModifiedMarks();
}

void SettingsPage::refreshModifiedMarks()
{
    QHash<fm::ui::SettingRow *, bool> rows;
    for (const Binding &b : std::as_const(m_bindings)) {
        fm::ui::SettingRow *row = nullptr;
        for (QWidget *w = b.widget; w && w != this; w = w->parentWidget()) {
            if ((row = qobject_cast<fm::ui::SettingRow *>(w)))
                break;
        }
        if (row)
            rows[row] = rows.value(row) || b.differs(pending(), defaults());
    }
    for (auto it = rows.cbegin(); it != rows.cend(); ++it)
        it.key()->setModified(it.value());
    Q_EMIT modifiedCountChanged();
}

void SettingsPage::resetToDefaults()
{
    QHash<int, bool> touched;
    for (const Binding &b : std::as_const(m_bindings))
        touched[int(b.section)] = true;
    for (auto it = touched.cbegin(); it != touched.cend(); ++it) {
        const auto section = Section(it.key());
        session()->edit(section, [&](AppSettings &p) {
            for (const Binding &b : std::as_const(m_bindings)) {
                if (b.section == section && b.reset)
                    b.reset(p, defaults());
            }
        });
    }
}

int SettingsPage::modifiedCount() const
{
    QSet<QWidget *> seen;
    int count = 0;
    for (const Binding &b : m_bindings) {
        if (b.differs && b.differs(pending(), defaults()) && !seen.contains(b.widget)) {
            seen.insert(b.widget);
            ++count;
        }
    }
    return count;
}

QStringList SettingsPage::searchKeywords() const
{
    QStringList words;
    for (const fm::ui::SettingRow *row : findChildren<fm::ui::SettingRow *>()) {
        words.append(row->title().remove(u'&'));
        if (!row->description().isEmpty())
            words.append(row->description());
    }
    for (const fm::ui::Label *label : findChildren<fm::ui::Label *>()) {
        if (label->textRole() == fm::ui::Label::SectionTitle)
            words.append(label->text().remove(u'&'));
    }
    for (const QComboBox *combo : findChildren<QComboBox *>()) {
        for (int i = 0; i < combo->count(); ++i)
            words.append(combo->itemText(i));
    }
    for (const QAbstractButton *b : findChildren<QAbstractButton *>()) {
        if (!b->text().isEmpty())
            words.append(QString(b->text()).remove(u'&'));
    }
    return words;
}

QWidget *SettingsPage::searchTarget(const QString &query) const
{
    const QString q = normalizedQuery(query);
    if (q.isEmpty())
        return nullptr;
    for (fm::ui::SettingRow *row : findChildren<fm::ui::SettingRow *>()) {
        if (normalizedQuery(row->title()).contains(q) || normalizedQuery(row->description()).contains(q))
            return row;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------------------------
// SettingsDialog

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    AppSettings applied = st::SettingsStore::instance().settings();
    st::captureTheme(applied);  // 도구 모음에서 바꾼 테마도 보이게
    m_session = new SettingsSession(applied, this);

    fm::ui::DialogChromeOptions chrome;
    chrome.fixedSize = false;
    chrome.maximizeButton = true;
    fm::ui::setupDialogChrome(this, chrome);
    fs::setDensity(this, fs::Density::Settings);  // 입력 · 콤보 30, 세그먼트 30
    setMinimumSize(960, 640);
    buildShell();

    for (auto *create : {createAppearancePage, createThemePage, createPanelPage, createThumbsPage, createGroupsPage,
                         createColumnsPage, createFileOpsPage, createElevationPage, createKeysPage})
        addPage(create(m_session, this));
    for (SettingsPage *page : std::as_const(m_pages))
        page->syncFromPending();

    connect(m_session, &SettingsSession::pendingChanged, this, [this](Section) {
        for (SettingsPage *page : std::as_const(m_pages))
            page->syncFromPending();
        refreshFooter();
        if (!m_search->text().isEmpty())
            runSearch();
    });
    setCurrentPage(applied.dialog.lastPage);
    if (m_nav->currentIndex().row() < 0)
        setCurrentPage(u"appearance"_s);
    refreshFooter();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::buildShell()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *topLine = new QFrame(this);
    topLine->setFrameShape(QFrame::HLine);
    topLine->setFrameShadow(QFrame::Plain);
    topLine->setFixedHeight(1);
    root->addWidget(topLine);

    auto *body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    root->addLayout(body, 1);

    // 왼쪽 탐색 232
    auto *nav = new QWidget(this);
    nav->setObjectName(u"settingsNav"_s);
    nav->setFixedWidth(232);
    auto *navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(10, 12, 10, 14);
    navLayout->setSpacing(0);
    m_search = new fm::ui::SearchField(nav);
    m_search->setPlaceholderText(tr("설정 찾기   Ctrl+F"));
    m_search->setAccessibleName(tr("설정 찾기"));
    m_search->setMinimumHeight(32);
    navLayout->addWidget(m_search);
    navLayout->addSpacing(10);
    m_nav = new QListView(nav);
    m_nav->setObjectName(u"settingsNavList"_s);
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setUniformItemSizes(true);
    m_nav->setMouseTracking(true);
    m_nav->setAttribute(Qt::WA_Hover);
    m_nav->viewport()->setAutoFillBackground(false);
    m_nav->setAutoFillBackground(false);
    m_nav->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_nav->setItemDelegate(new NavDelegate(m_nav));
    m_navModel = new QStandardItemModel(this);
    m_nav->setModel(m_navModel);
    m_nav->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navLayout->addWidget(m_nav, 1);
    m_navPath = new QLabel(nav);
    m_navPath->setFont(fs::monoFont(11.5));
    m_navPath->setWordWrap(true);
    m_navPath->setContentsMargins(10, 0, 10, 0);
    navLayout->addWidget(m_navPath);
    body->addWidget(nav);

    auto *navLine = new QFrame(this);
    navLine->setFrameShape(QFrame::VLine);
    navLine->setFrameShadow(QFrame::Plain);
    navLine->setFixedWidth(1);
    body->addWidget(navLine);

    // 내용: 여백 20 24, 머리 + 페이지
    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 20, 24, 20);
    contentLayout->setSpacing(16);
    auto *header = new QHBoxLayout();
    header->setSpacing(16);
    auto *titles = new QVBoxLayout();
    titles->setSpacing(4);
    m_pageTitle = new fm::ui::Label(QString(), fm::ui::Label::PageTitle, content);
    m_pageDescription = new fm::ui::Label(QString(), fm::ui::Label::Description, content);
    titles->addWidget(m_pageTitle);
    titles->addWidget(m_pageDescription);
    header->addLayout(titles, 1);
    m_headerTrailing = new QHBoxLayout();
    m_headerTrailing->setSpacing(8);
    header->addLayout(m_headerTrailing);
    header->setAlignment(m_headerTrailing, Qt::AlignBottom);
    contentLayout->addLayout(header);
    m_stack = new QStackedWidget(content);
    contentLayout->addWidget(m_stack, 1);
    body->addWidget(content, 1);

    // 바닥
    auto *footer = new fm::ui::DialogFooter(this);
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(24, 16, 24, 16);
    footerLayout->setSpacing(8);
    m_footerLeft = new QHBoxLayout();
    m_footerLeft->setSpacing(8);
    footerLayout->addLayout(m_footerLeft);
    m_status = new fm::ui::Label(QString(), fm::ui::Label::Minor, footer);
    m_status->setContentsMargins(8, 0, 0, 0);
    footerLayout->addWidget(m_status);
    footerLayout->addStretch(1);
    m_ok = new fm::ui::Button(tr("확인"), footer);
    m_ok->setRole(fm::ui::Button::Primary);
    m_ok->setDefault(true);
    m_ok->setToolTip(u"Enter"_s);
    m_cancel = new fm::ui::Button(tr("취소"), footer);
    m_cancel->setAutoDefault(false);
    m_cancel->setToolTip(u"Esc"_s);
    m_apply = new fm::ui::Button(tr("적용(&A)"), footer);
    m_apply->setAutoDefault(false);
    footerLayout->addWidget(m_ok);
    footerLayout->addWidget(m_cancel);
    footerLayout->addWidget(m_apply);
    root->addWidget(footer);

    connect(m_ok, &QPushButton::clicked, this, &SettingsDialog::accept);
    connect(m_cancel, &QPushButton::clicked, this, &SettingsDialog::reject);
    connect(m_apply, &QPushButton::clicked, this, &SettingsDialog::applyPending);
    connect(m_nav->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current) { onPageChanged(current.row()); });
    auto *searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(150);
    connect(searchTimer, &QTimer::timeout, this, &SettingsDialog::runSearch);
    connect(m_search, &QLineEdit::textChanged, searchTimer, qOverload<>(&QTimer::start));
    connect(m_search, &QLineEdit::returnPressed, this, &SettingsDialog::jumpToFirstHit);

    auto shortcut = [this](const QKeySequence &key, auto slot) {
        auto *s = new QShortcut(key, this);
        connect(s, &QShortcut::activated, this, slot);
    };
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_F), [this] {
        m_search->setFocus(Qt::ShortcutFocusReason);
        m_search->selectAll();
    });
    auto step = [this](int delta) {
        const int count = m_navModel->rowCount();
        setCurrentPage(m_pages.at((m_stack->currentIndex() + delta + count) % count)->pageId());
    };
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab), [step] { step(1); });
    shortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab), [step] { step(-1); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown), [step] { step(1); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp), [step] { step(-1); });
}

void SettingsDialog::addPage(SettingsPage *page)
{
    m_pages.append(page);
    auto *item = new QStandardItem(page->title());
    item->setData(page->pageId(), kPageIdRole);
    item->setEditable(false);
    m_navModel->appendRow(item);
    QWidget *holder = page;
    if (page->wantsScrollArea()) {
        auto *scroll = new QScrollArea(m_stack);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->viewport()->setAutoFillBackground(false);
        page->setAutoFillBackground(false);
        scroll->setWidget(page);
        holder = scroll;
    }
    m_stack->addWidget(holder);
    connect(page, &SettingsPage::navigateRequested, this, &SettingsDialog::setCurrentPage);
    connect(page, &SettingsPage::modifiedCountChanged, this, &SettingsDialog::refreshFooter);
}

QStringList SettingsDialog::pageIds() const
{
    QStringList ids;
    for (const SettingsPage *p : m_pages)
        ids.append(p->pageId());
    return ids;
}

QString SettingsDialog::currentPageId() const
{
    const int i = m_stack->currentIndex();
    return i >= 0 && i < m_pages.size() ? m_pages.at(i)->pageId() : QString();
}

SettingsPage *SettingsDialog::page(const QString &pageId) const
{
    for (SettingsPage *p : m_pages) {
        if (p->pageId() == pageId)
            return p;
    }
    return nullptr;
}

void SettingsDialog::showBoardState()
{
    if (SettingsPage *p = page(currentPageId()))
        p->showBoardState();
}

void SettingsDialog::setCurrentPage(const QString &pageId)
{
    for (int i = 0; i < m_pages.size(); ++i) {
        if (m_pages.at(i)->pageId() == pageId) {
            m_nav->setCurrentIndex(m_navModel->index(i, 0));
            onPageChanged(i);
            return;
        }
    }
}

void SettingsDialog::onPageChanged(int index)
{
    if (index < 0 || index >= m_pages.size())
        return;
    if (m_stack->currentIndex() != index)
        m_stack->setCurrentIndex(index);
    SettingsPage *page = m_pages.at(index);
    setWindowTitle(tr("설정 — %1").arg(page->title()));
    m_pageTitle->setText(page->title());
    m_pageDescription->setText(page->description());
    m_navPath->setText(page->footerPath());
    m_navPath->setToolTip(page->footerPath());

    // 머리 오른쪽 · 바닥 왼쪽은 페이지마다 바뀐다
    if (m_shownTrailing) {
        m_headerTrailing->removeWidget(m_shownTrailing);
        m_shownTrailing->hide();
    }
    m_shownTrailing = page->headerTrailing();
    if (m_shownTrailing) {
        m_headerTrailing->addWidget(m_shownTrailing);
        m_shownTrailing->show();
    }
    for (QAbstractButton *b : std::as_const(m_shownFooterButtons)) {
        m_footerLeft->removeWidget(b);
        b->hide();
    }
    m_shownFooterButtons = page->footerButtons();
    for (QAbstractButton *b : std::as_const(m_shownFooterButtons)) {
        m_footerLeft->addWidget(b);
        b->show();
    }
    m_session->edit(Section::Dialog, [&](AppSettings &p) { p.dialog.lastPage = page->pageId(); });
    refreshFooter();
}

void SettingsDialog::refreshFooter()
{
    m_apply->setEnabled(m_session->isDirty());
    const int i = m_stack->currentIndex();
    if (i < 0 || i >= m_pages.size())
        return;
    const int modified = m_pages.at(i)->modifiedCount();
    m_status->setText(modified < 0 ? QString() : modified == 0 ? tr("모두 기본값") : tr("기본값과 다른 설정 %1개").arg(modified));
}

void SettingsDialog::setSearchText(const QString &text)
{
    m_search->setText(text);
    runSearch();
}

int SettingsDialog::searchHits(const QString &pageId) const
{
    for (int i = 0; i < m_pages.size(); ++i) {
        if (m_pages.at(i)->pageId() == pageId)
            return i < m_hits.size() ? m_hits.at(i) : 0;
    }
    return 0;
}

void SettingsDialog::runSearch()
{
    const QString q = normalizedQuery(m_search->text());
    m_hits.clear();
    for (int i = 0; i < m_pages.size(); ++i) {
        int hits = 0;
        if (!q.isEmpty()) {
            for (const QString &word : m_pages.at(i)->searchKeywords()) {
                if (normalizedQuery(word).contains(q))
                    ++hits;
            }
        }
        m_hits.append(hits);
        m_navModel->item(i)->setData(q.isEmpty() ? QVariant() : QVariant(hits), kHitsRole);
    }
}

void SettingsDialog::jumpToFirstHit()
{
    runSearch();
    for (int i = 0; i < m_hits.size(); ++i) {
        if (m_hits.at(i) <= 0)
            continue;
        SettingsPage *page = m_pages.at(i);
        setCurrentPage(page->pageId());
        if (QWidget *target = page->searchTarget(m_search->text())) {
            if (auto *scroll = qobject_cast<QScrollArea *>(m_stack->currentWidget()))
                scroll->ensureWidgetVisible(target);
            if (auto *row = qobject_cast<fm::ui::SettingRow *>(target)) {
                // 찾은 행을 1.2초 동안 --accent-soft로
                row->setAutoFillBackground(true);
                QPalette pal = row->palette();
                pal.setColor(QPalette::Window, fs::themeColorsFor(row)[T::AccentSoft]);
                row->setPalette(pal);
                QTimer::singleShot(1200, row, [row] {
                    row->setAutoFillBackground(false);
                    row->setPalette(QPalette());
                });
            }
        }
        return;
    }
}

void SettingsDialog::applyPending()
{
    m_session->apply();
    refreshFooter();
}

void SettingsDialog::accept()
{
    m_session->edit(Section::Dialog, [&](AppSettings &p) { p.dialog.geometry = saveGeometry(); });
    m_session->apply();
    QDialog::accept();
}

void SettingsDialog::reject()
{
    // 마지막 적용 이후의 변경은 버린다(이미 적용한 것은 그대로 — Windows 관례)
    m_session->revert();
    QDialog::reject();
}

void SettingsDialog::fitInitialSize()
{
    // 1180 × 864(목업 1180 × 900 − 제목 표시줄) — 화면이 작으면 사용 가능 영역의 92 %로 줄인다(04 §1.1)
    QSize size(1180, 864);
    if (const QScreen *s = screen()) {
        const QSize avail = s->availableGeometry().size() * 0.92;
        size = size.boundedTo(avail).expandedTo(minimumSize());
    }
    if (!m_session->applied().dialog.geometry.isEmpty() && restoreGeometry(m_session->applied().dialog.geometry))
        return;
    resize(size);
}

void SettingsDialog::showEvent(QShowEvent *event)
{
    if (!m_sizedOnce) {
        m_sizedOnce = true;
        fitInitialSize();
    }
    QDialog::showEvent(event);
}

} // namespace fm::dialogs
