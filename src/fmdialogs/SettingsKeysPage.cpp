// 설정 › 키보드(docs/specs/05 §2.5) — 도구 줄 · 대화상자 구역은 .ui, 단축키 표(범주 = 묶음 머리, 명령 = 행) · 키 입력 ·
// 충돌 배너는 코드. 충돌은 범위를 보고 판정한다(창 ⊃ 패널 ⊃ 파일 목록, 대화상자는 독립).

#include "SettingsPages_p.h"

#include "ui_SettingsKeysPage.h"

#include <fmsettings/Commands.h>
#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/SettingsWidgets.h>

#include <QAccessible>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QShortcut>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTreeView>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

namespace fs = fm::style;
namespace st = fm::settings;
using T = fs::Token;

enum Column { NameColumn, KeysColumn, DefaultColumn, ScopeColumn, EditColumn, ColumnCount };
enum Role { KindRole = Qt::UserRole + 1, CommandRole, KeysRole, DefaultKeysRole, ModifiedRole, EditingRole };
enum Kind { CategoryRow, CommandRow, BannerRow };

class KeyTableDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const int kind = index.siblingAtColumn(0).data(KindRole).toInt();
        const int h = kind == CategoryRow ? 26 : kind == BannerRow ? 40 : 28;
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), h);
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(option.widget);
        const QModelIndex first = index.siblingAtColumn(0);
        const int kind = first.data(KindRole).toInt();
        const QRect r = option.rect;
        p->save();
        if (kind == CategoryRow) {
            // 묶음 머리: 26 px, --win 바탕, 11.5 px 600 --fg2, 아래 --grid
            p->fillRect(r, tc[T::Win]);
            p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), tc[T::Grid]);
            p->setFont(fs::pixelFont(option.font, 11.5, QFont::DemiBold));
            p->setPen(tc[T::Fg2]);
            p->drawText(r.adjusted(12, 0, -12, 0), Qt::AlignLeft | Qt::AlignVCenter, first.data().toString());
            p->restore();
            return;
        }
        if (kind == BannerRow) {
            p->restore();
            return;  // 배너 위젯이 그린다
        }
        const bool editing = first.data(EditingRole).toBool();
        p->fillRect(r, editing ? tc[T::Sel] : tc[T::Surface]);
        p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), tc[T::Grid]);
        switch (index.column()) {
        case NameColumn: {
            qreal left = r.left() + 12;
            if (first.data(ModifiedRole).toBool()) {
                p->setRenderHint(QPainter::Antialiasing);
                p->setPen(Qt::NoPen);
                p->setBrush(tc[T::Accent]);
                p->drawEllipse(QRectF(left, r.center().y() - 2.5, 6, 6));
                left += 12;
            }
            p->setFont(fs::pixelFont(option.font, 13, editing ? QFont::DemiBold : QFont::Normal));
            p->setPen(tc[T::Fg]);
            p->drawText(QRectF(left, r.top(), r.right() - left - 8, r.height()), Qt::AlignLeft | Qt::AlignVCenter, first.data().toString());
            break;
        }
        case KeysColumn:
        case DefaultColumn: {
            if (editing && index.column() == KeysColumn)
                break;  // 입력 칸 위젯이 그린다
            const bool dim = index.column() == DefaultColumn;
            const QStringList keys = first.data(dim ? DefaultKeysRole : KeysRole).toStringList();
            qreal x = r.left() + 12;
            if (keys.isEmpty()) {
                p->setFont(fs::pixelFont(option.font, 12));
                p->setPen(tc[T::Fg3]);
                p->drawText(QRectF(x, r.top(), r.width() - 24, r.height()), Qt::AlignLeft | Qt::AlignVCenter, tr("없음"));
                break;
            }
            for (const QString &k : keys) {
                const QSize s = fm::ui::keyCapSize(k);
                if (x + s.width() > r.right() - 8)
                    break;
                fm::ui::paintKeyCap(p, QRectF(x, r.center().y() - 10, s.width(), s.height()), k, tc, dim, false);
                x += s.width() + 4;
            }
            break;
        }
        case ScopeColumn:
            p->setFont(fs::pixelFont(option.font, 12));
            p->setPen(tc[T::Fg3]);
            p->drawText(r.adjusted(12, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter, index.data().toString());
            break;
        case EditColumn: {
            // 연필 단추(28 × 26)
            const QRectF icon(r.center().x() - 7, r.center().y() - 7, 14, 14);
            if (option.state & QStyle::State_MouseOver) {
                p->setRenderHint(QPainter::Antialiasing);
                p->setPen(Qt::NoPen);
                QColor hover = tc[T::Fg];
                hover.setAlphaF(0.06f);
                p->setBrush(hover);
                p->drawRoundedRect(QRectF(r.center().x() - 14, r.center().y() - 13, 28, 26), 4, 4);
            }
            fs::paintGlyph(p, fs::Glyph::Rename, icon, tc[T::Fg2]);
            break;
        }
        default:
            break;
        }
        p->restore();
    }
};

/// 충돌 배너(05 §2.5.2) — 여백 6 12 6 14, 간격 10, --warn-bg, 아래 --warn-line, 경고 16 · 글 12.5 · 작은 버튼 둘.
class ConflictBanner final : public QWidget
{
public:
    ConflictBanner(const QString &key, const QString &command, QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(u"conflictBanner"_s);
        setAccessibleName(tr("단축키 겹침"));
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(14 + 16 + 10, 6, 12, 7);
        layout->setSpacing(10);
        m_text = new QLabel(tr("%1은 ‘%2’에 이미 쓰입니다. 바꾸면 그 명령의 단축키가 비워집니다.").arg(key, command), this);
        m_text->setFont(fs::pixelFont(font(), 12.5));
        m_text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);  // 좁으면 단추보다 글이 먼저 잘린다
        m_text->setToolTip(m_text->text());
        force = new fm::ui::Button(tr("그래도 바꾸기(&O)"), this);
        force->setCompact(true);
        force->setAutoDefault(false);
        again = new fm::ui::Button(tr("다른 키 누르기"), this);
        again->setCompact(true);
        again->setAutoDefault(false);
        layout->addWidget(m_text, 1);
        layout->addWidget(force);
        layout->addWidget(again);
    }
    QString text() const { return m_text->text(); }

    fm::ui::Button *force = nullptr;
    fm::ui::Button *again = nullptr;

protected:
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        p.fillRect(rect(), tc[T::WarnBg]);
        p.fillRect(QRect(0, height() - 1, width(), 1), tc[T::WarnLine]);
        fs::paintGlyph(&p, fs::Glyph::Warning, QRectF(14, (height() - 1 - 16) / 2.0, 16, 16), tc[T::Warn]);
    }

private:
    QLabel *m_text = nullptr;
};

class KeysPage final : public SettingsPage
{
public:
    KeysPage(SettingsSession *session, QWidget *parent)
        : SettingsPage(session, parent)
        , ui(std::make_unique<Ui::SettingsKeysPage>())
    {
        ui->setupUi(this);
        ui->scopeSegment->setSegmentSize(fm::ui::SegmentedControl::Normal);
        ui->scopeSegment->setCurrentIndex(0);
        for (const QString &layout : st::keyLayouts())
            ui->layoutCombo->addItem(st::keyLayoutLabel(layout), layout);

        const Section s = Section::Keys;
        bindCheck(ui->mnemonicSwitch, s, [](const AppSettings &p) { return p.keys.alwaysShowMnemonics; },
                  [](AppSettings &p, bool on) { p.keys.alwaysShowMnemonics = on; });
        bindCombo(ui->escCombo, s, [](const AppSettings &p) { return int(p.keys.progressEsc); },
                  [](AppSettings &p, int i) { p.keys.progressEsc = st::KeyBindingSettings::ProgressEsc(i); });
        bindCombo(ui->layoutCombo, s,
                  [](const AppSettings &p) { return std::max<int>(0, int(st::keyLayouts().indexOf(p.keys.layout))); },
                  [](AppSettings &p, int i) { p.keys.layout = st::keyLayouts().value(i, u"default"_s); });
        addCustomItem(ui->tableCard, [](const AppSettings &a, const AppSettings &b) { return a.keys.overrides != b.keys.overrides; },
                      [](AppSettings &p, const AppSettings &d) { p.keys.overrides = d.keys.overrides; }, s);

        m_model = new QStandardItemModel(0, ColumnCount, this);
        m_model->setHorizontalHeaderLabels({tr("명령"), tr("단축키"), tr("기본값"), tr("범위"), QString()});
        m_model->setHeaderData(EditColumn, Qt::Horizontal, tr("편집"), Qt::AccessibleTextRole);
        m_view = new QTreeView(ui->tableCard);
        m_view->setObjectName(u"keyTable"_s);
        m_view->setModel(m_model);
        m_view->setItemDelegate(new KeyTableDelegate(m_view));
        m_view->setRootIsDecorated(false);
        m_view->setItemsExpandable(false);
        m_view->setIndentation(0);
        m_view->setFrameShape(QFrame::NoFrame);
        m_view->setSelectionMode(QAbstractItemView::NoSelection);
        m_view->setMouseTracking(true);
        m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_view->setContextMenuPolicy(Qt::CustomContextMenu);
        m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        fs::setFlatHeader(m_view);
        QHeaderView *header = m_view->header();
        header->setStretchLastSection(false);
        header->setSectionResizeMode(QHeaderView::Fixed);
        header->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
        header->resizeSection(KeysColumn, 220);
        header->resizeSection(DefaultColumn, 170);
        header->resizeSection(ScopeColumn, 104);
        header->resizeSection(EditColumn, 40);
        m_view->installEventFilter(this);
        ui->tableLayout->addWidget(m_view);

        connect(m_view, &QTreeView::clicked, this, [this](const QModelIndex &index) {
            const QString id = index.siblingAtColumn(0).data(CommandRole).toString();
            if (!id.isEmpty() && index.column() == EditColumn)
                startEditing(id);
        });
        connect(m_view, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
            const QString id = index.siblingAtColumn(0).data(CommandRole).toString();
            if (!id.isEmpty())
                startEditing(id);
        });
        connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
            const QModelIndex index = m_view->indexAt(pos);
            const QString id = index.siblingAtColumn(0).data(CommandRole).toString();
            if (id.isEmpty() || !index.siblingAtColumn(0).data(ModifiedRole).toBool())
                return;
            QMenu menu(this);
            connect(menu.addAction(tr("기본값으로")), &QAction::triggered, this, [this, id] {
                this->session()->edit(Section::Keys, [&](AppSettings &p) { p.keys.overrides.remove(id); });
            });
            menu.exec(m_view->viewport()->mapToGlobal(pos));
        });
        connect(ui->searchEdit, &QLineEdit::textChanged, this, [this] { rebuild(); });
        connect(ui->scopeSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, [this] { rebuild(); });
        rebuild();
    }

    QString pageId() const override { return u"keys"_s; }
    QString title() const override { return tr("키보드"); }
    QString description() const override
    {
        return tr("명령마다 단축키를 바꿉니다. 이미 쓰는 키를 누르면 겹치는 명령을 바로 알려 줍니다.");
    }
    fm::settings::Sections sections() const override { return Section::Keys; }
    QList<QAbstractButton *> footerButtons() override
    {
        if (!m_import)
            m_import = makeImportExportButton();
        QList<QAbstractButton *> buttons = SettingsPage::footerButtons();
        buttons.append(m_import);
        return buttons;
    }

    void syncFromPending() override
    {
        SettingsPage::syncFromPending();
        // 사용자가 하나라도 바꾸면 배열 이름 뒤에 "· 수정됨"
        for (int i = 0; i < ui->layoutCombo->count(); ++i) {
            const QString layout = st::keyLayouts().value(i);
            const bool modified = layout == pending().keys.layout && !pending().keys.overrides.isEmpty();
            ui->layoutCombo->setItemText(i, st::keyLayoutLabel(layout) + (modified ? tr(" · 수정됨") : QString()));
        }
        if (!m_capture || !m_capture->isCapturing())
            rebuild();
    }

    QStringList searchKeywords() const override
    {
        QStringList words = SettingsPage::searchKeywords();
        for (const st::CommandDef &c : st::commands())
            words.append(c.name);
        return words;
    }

    /// 목업: 열 세트 바꾸기 입력 중 Ctrl+M → 다중 이름 변경과 겹침 배너.
    void showBoardState() override
    {
        m_editing = u"columnSet"_s;
        m_pendingKey = QKeySequence(Qt::CTRL | Qt::Key_M);
        m_conflict = st::findConflict(pending().keys, m_editing, m_pendingKey).value_or(QString());
        rebuild();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_view && event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                const QString id = m_view->currentIndex().siblingAtColumn(0).data(CommandRole).toString();
                if (!id.isEmpty()) {
                    startEditing(id);
                    return true;
                }
            }
        }
        return SettingsPage::eventFilter(watched, event);
    }

private:
    void rebuild()
    {
        const QString query = ui->searchEdit->text().trimmed().toLower();
        const int scope = ui->scopeSegment->currentIndex();  // 0 전체, 1 파일 작업, 2 패널, 3 탐색, 4 대화상자
        const st::KeyBindingSettings &keys = pending().keys;
        if (m_capture)
            m_capture->stopCapture();
        m_capture = nullptr;
        m_model->removeRows(0, m_model->rowCount());
        for (const st::CommandCategory category : {st::CommandCategory::FileOps, st::CommandCategory::Panel,
                                                   st::CommandCategory::Navigation, st::CommandCategory::Dialog}) {
            if (scope > 0 && int(category) != scope - 1)
                continue;
            QList<QList<QStandardItem *>> rows;
            for (const st::CommandDef &c : st::commands()) {
                if (c.category != category)
                    continue;
                const QList<QKeySequence> current = st::effectiveKeys(keys, c.id);
                QStringList currentText, defaultText;
                for (const QKeySequence &k : current)
                    currentText.append(k.toString(QKeySequence::NativeText));
                for (const QKeySequence &k : st::layoutKeys(c.id, keys.layout))
                    defaultText.append(k.toString(QKeySequence::NativeText));
                if (!query.isEmpty() && !c.name.toLower().contains(query)
                    && !currentText.join(u' ').toLower().contains(query))
                    continue;
                auto *name = new QStandardItem(c.name);
                name->setData(CommandRow, KindRole);
                name->setData(c.id, CommandRole);
                name->setData(currentText, KeysRole);
                name->setData(defaultText, DefaultKeysRole);
                name->setData(keys.overrides.contains(c.id), ModifiedRole);
                name->setData(c.id == m_editing, EditingRole);
                name->setData(u"%1: %2"_s.arg(c.name, currentText.isEmpty() ? tr("없음") : currentText.join(u", "_s)),
                              Qt::AccessibleTextRole);
                auto *keysItem = new QStandardItem();
                auto *defaultItem = new QStandardItem();
                auto *scopeItem = new QStandardItem(st::scopeLabel(c.scope));
                auto *edit = new QStandardItem();
                edit->setData(tr("%1 단축키 바꾸기").arg(c.name), Qt::AccessibleTextRole);
                rows.append({name, keysItem, defaultItem, scopeItem, edit});
            }
            if (rows.isEmpty())
                continue;
            auto *head = new QStandardItem(st::categoryLabel(category));
            head->setData(CategoryRow, KindRole);
            QList<QStandardItem *> headRow{head};
            for (int c = 1; c < ColumnCount; ++c)
                headRow.append(new QStandardItem());
            m_model->appendRow(headRow);
            m_view->setFirstColumnSpanned(m_model->rowCount() - 1, QModelIndex(), true);
            for (const auto &row : std::as_const(rows)) {
                m_model->appendRow(row);
                if (row.first()->data(CommandRole).toString() == m_editing)
                    installEditor(m_model->rowCount() - 1);
            }
        }
        fitHeight();
    }

    /// 페이지가 스크롤되므로 표는 안쪽 스크롤 없이 모든 행을 보인다.
    void fitHeight()
    {
        int h = m_view->header()->sizeHint().height();
        for (int row = 0; row < m_model->rowCount(); ++row) {
            const int kind = m_model->index(row, 0).data(KindRole).toInt();
            h += kind == CategoryRow ? 26 : kind == BannerRow ? 40 : 28;
        }
        m_view->setFixedHeight(h + 2);
    }

    void installEditor(int row)
    {
        m_capture = new fm::ui::KeyCaptureEdit(m_view);
        m_view->setIndexWidget(m_model->index(row, KeysColumn), m_capture);
        connect(m_capture, &fm::ui::KeyCaptureEdit::captured, this, &KeysPage::onCaptured);
        connect(m_capture, &fm::ui::KeyCaptureEdit::cancelled, this, [this] { stopEditing(); });
        connect(m_capture, &fm::ui::KeyCaptureEdit::cleared, this, [this] {
            const QString id = m_editing;
            stopEditing();
            session()->edit(Section::Keys, [&](AppSettings &p) { setKeys(p.keys, id, {}); });
        });
        if (!m_conflict.isEmpty()) {
            showBanner(row);
            return;
        }
        m_capture->startCapture();
    }

    void startEditing(const QString &id)
    {
        m_editing = id;
        m_conflict.clear();
        m_pendingKey = QKeySequence();
        rebuild();
    }

    void stopEditing()
    {
        if (m_capture)
            m_capture->stopCapture();
        m_editing.clear();
        m_conflict.clear();
        rebuild();
        m_view->setFocus();
    }

    void onCaptured(const QKeySequence &key)
    {
        if (st::isForbiddenKey(key)) {
            m_capture->setCaption(tr("이 키는 쓸 수 없습니다"));
            return;
        }
        m_pendingKey = key;
        if (const auto other = st::findConflict(pending().keys, m_editing, key)) {
            m_conflict = *other;
            m_capture->stopCapture();
            rebuild();
            return;
        }
        commit(key);
    }

    void commit(const QKeySequence &key)
    {
        const QString id = m_editing;
        const QString conflict = m_conflict;
        m_editing.clear();
        m_conflict.clear();
        if (m_capture)
            m_capture->stopCapture();
        session()->edit(Section::Keys, [&](AppSettings &p) {
            // 누른 키가 첫 키를 바꾸고 나머지 키는 둔다
            QList<QKeySequence> keys = st::effectiveKeys(p.keys, id);
            if (keys.isEmpty())
                keys.append(key);
            else
                keys[0] = key;
            for (qsizetype i = keys.size() - 1; i > 0; --i) {
                if (keys.indexOf(keys.at(i)) < i)
                    keys.removeAt(i);
            }
            setKeys(p.keys, id, keys);
            if (!conflict.isEmpty()) {
                QList<QKeySequence> other = st::effectiveKeys(p.keys, conflict);
                other.removeAll(key);
                setKeys(p.keys, conflict, other);  // 비면 "없음"
            }
        });
        rebuild();
        m_view->setFocus();
    }

    /// 배열 기본값과 같아지면 사용자 지정을 지운다(기본값과 다름 점이 남지 않게).
    static void setKeys(st::KeyBindingSettings &keys, const QString &id, const QList<QKeySequence> &value)
    {
        if (value == st::layoutKeys(id, keys.layout))
            keys.overrides.remove(id);
        else
            keys.overrides.insert(id, value);
    }

    void showBanner(int editorRow)
    {
        // 편집 행 바로 아래에 배너 행(role=alert)
        QList<QStandardItem *> row{new QStandardItem()};
        row.first()->setData(BannerRow, KindRole);
        for (int c = 1; c < ColumnCount; ++c)
            row.append(new QStandardItem());
        m_model->insertRow(editorRow + 1, row);
        m_view->setFirstColumnSpanned(editorRow + 1, QModelIndex(), true);
        fitHeight();
        m_capture->setPending(m_pendingKey);
        const st::CommandDef *other = st::findCommand(m_conflict);
        auto *banner = new ConflictBanner(m_pendingKey.toString(QKeySequence::NativeText), other ? other->name : m_conflict, m_view);
        m_view->setIndexWidget(m_model->index(editorRow + 1, 0), banner);
        connect(banner->force, &QPushButton::clicked, this, [this] { commit(m_pendingKey); });
        connect(banner->again, &QPushButton::clicked, this, [this] {
            m_conflict.clear();
            rebuild();  // 입력 상태로(hot 칩 비움)
        });
        // Esc = 입력 취소(원래 키 그대로) — 대화상자 거부로 새지 않게
        auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), banner, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
        connect(esc, &QShortcut::activated, this, [this] { stopEditing(); });
        QAccessibleEvent alert(banner, QAccessible::Alert);
        QAccessible::updateAccessibility(&alert);
        banner->force->setFocus();
    }

    std::unique_ptr<Ui::SettingsKeysPage> ui;
    QStandardItemModel *m_model = nullptr;
    QTreeView *m_view = nullptr;
    fm::ui::KeyCaptureEdit *m_capture = nullptr;
    QAbstractButton *m_import = nullptr;
    QString m_editing;
    QString m_conflict;
    QKeySequence m_pendingKey;
};

} // namespace

SettingsPage *createKeysPage(SettingsSession *session, QWidget *parent)
{
    return new KeysPage(session, parent);
}

} // namespace fm::dialogs
