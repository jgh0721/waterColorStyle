#include "fmdialogs/FileOpDialogs.h"

#include "ui_CopyDialog.h"
#include "ui_DeleteDialog.h"
#include "ui_MoveRenameDialog.h"
#include "ui_NewFileDialog.h"
#include "ui_NewFolderDialog.h"

#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/DialogChrome.h>

#include <QButtonGroup>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QShortcut>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

using fm::style::Token;

namespace {

QString normalizedDir(QString path)
{
    path.replace(u'/', u'\\');
    while (path.endsWith(u'\\'))
        path.chop(1);
    return path.toLower();
}

QString joinPath(const QString &dir, const QString &name)
{
    return dir.endsWith(u'\\') ? dir + name : dir + u'\\' + name;
}

const FileSystemProbe &probeOf(const FileOpContext &context)
{
    static const LocalProbe local;
    return context.probe ? *context.probe : static_cast<const FileSystemProbe &>(local);
}

} // namespace

// ================================================================ CopyDialog

CopyDialog::CopyDialog(const FileOpContext &context, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::CopyDialog>())
    , m_context(context)
{
    ui->setupUi(this);
    fm::ui::setupDialogChrome(this);
    ui->filterEdit->setFont(fm::style::monoFont(12));

    const int count = int(m_context.items.size());
    ui->header->setTitle(tr("%1개 항목 복사").arg(count));
    // 부제: {첫 이름 · 가운데 줄임} 외 {n−1}개 · {합계} · {원본}
    const QString first = QFontMetrics(fm::style::pixelFont(font(), 12)).elidedText(m_context.items.value(0).name, Qt::ElideMiddle, 210);
    const QString size = formatBytes(m_context.totalSize());
    ui->header->setSubtitle(count > 1 ? tr("%1 외 %2개 · %3 · %4").arg(first).arg(count - 1).arg(size, m_context.sourceDir)
                                      : tr("%1 · %2 · %3").arg(first, size, m_context.sourceDir));
    ui->header->setToolTip(m_context.countLabel());

    ui->destEdit->setPath(m_context.targetDir);
    ui->destEdit->setHistory(m_context.recentTargets);
    connect(ui->destEdit, &fm::ui::PathEdit::pathChanged, this, &CopyDialog::updateDestination);
    connect(ui->copyButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->queueButton, &QPushButton::clicked, this, [this] { done(Queued); });
    auto *f2 = new QShortcut(QKeySequence(Qt::Key_F2), this);
    connect(f2, &QShortcut::activated, this, [this] {
        if (ui->queueButton->isEnabled())
            done(Queued);
    });
    updateDestination();
    ui->destEdit->lineEdit()->selectAll();
    ui->destEdit->setFocus();
}

CopyDialog::~CopyDialog() = default;

void CopyDialog::updateDestination()
{
    const QString dest = ui->destEdit->path().trimmed();
    const FileSystemProbe &probe = probeOf(m_context);
    const VolumeInfo volume = probe.volume(dest);
    bool ok = volume.valid && !dest.isEmpty();
    fm::ui::Label::Tone tone = fm::ui::Label::Default;
    QString help;
    if (!ok) {
        help = tr("대상 경로를 찾을 수 없습니다");
        tone = fm::ui::Label::Danger;
    } else if (normalizedDir(dest) == normalizedDir(m_context.sourceDir)) {
        help = tr("원본과 같은 폴더입니다 — 이름을 바꿔 복사합니다");
        tone = fm::ui::Label::Warn;
    } else if (volume.available < m_context.totalSize()) {
        help = tr("여유 공간이 부족합니다 (필요 %1 · 여유 %2)").arg(formatBytes(m_context.totalSize()), formatBytes(volume.available));
        tone = fm::ui::Label::Warn;
    } else {
        help = tr("%1 %2 · %3 · 여유 공간 %4 / %5")
                   .arg(volume.drive, volume.label, volume.fileSystem, formatBytes(volume.available), formatBytes(volume.total));
    }
    ui->destHelp->setText(help);
    ui->destHelp->setTone(tone);
    ui->copyButton->setEnabled(ok);
    ui->queueButton->setEnabled(ok);
}

CopyRequest CopyDialog::request() const
{
    CopyRequest r;
    r.destination = ui->destEdit->path();
    r.overwritePolicy = ui->overwriteCombo->currentIndex();
    r.filters = ui->filterEdit->text().split(u';', Qt::SkipEmptyParts);
    r.keepAttributes = ui->keepAttrCheck->isChecked();
    r.verify = ui->verifyCheck->isChecked();
    r.acl = ui->aclCheck->isChecked();
    r.alternateStreams = ui->adsCheck->isChecked();
    r.symlinksAsLinks = ui->symlinkCheck->isChecked();
    r.emptyFolders = ui->emptyDirsCheck->isChecked();
    r.queued = result() == Queued;
    return r;
}

QStringList CopyDialog::variants()
{
    return {u"copy.default"_s, u"copy.historyOpen"_s, u"copy.single"_s, u"copy.destInvalid"_s, u"copy.lowSpace"_s,
            u"copy.sameFolder"_s};
}

void CopyDialog::applyVariant(const QString &id)
{
    if (id == u"copy.single") {
        m_context.items = m_context.items.mid(2, 1);
        const QString size = formatBytes(m_context.totalSize());
        ui->header->setTitle(tr("%1개 항목 복사").arg(m_context.items.size()));
        ui->header->setSubtitle(tr("%1 · %2 · %3").arg(m_context.items.value(0).name, size, m_context.sourceDir));
        updateDestination();
    } else if (id == u"copy.destInvalid") {
        ui->destEdit->setPath(u"Z:\\Backup\\"_s);
    } else if (id == u"copy.lowSpace") {
        ui->destEdit->setPath(u"F:\\Installers\\"_s);
    } else if (id == u"copy.sameFolder") {
        ui->destEdit->setPath(m_context.sourceDir + u'\\');
    } else if (id == u"copy.historyOpen") {
        QTimer::singleShot(0, this, [this] { ui->destEdit->showHistory(); });
    }
}

// ================================================================ MoveRenameDialog

MoveRenameDialog::MoveRenameDialog(const FileOpContext &context, bool moveMode, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::MoveRenameDialog>())
    , m_context(context)
{
    ui->setupUi(this);
    fm::ui::setupDialogChrome(this);
    const QString original = m_context.items.value(0).name;
    const QString full = joinPath(m_context.sourceDir, original);
    ui->header->setSubtitle(tr("원본 · %1").arg(full));
    ui->header->setToolTip(full);
    ui->previewName->setFont(fm::style::pixelFont(font(), 13, QFont::DemiBold));
    ui->previewDir->setElideMode(Qt::ElideMiddle);
    ui->recentBar->setTargets(m_context.recentTargets);
    connect(ui->recentBar, &fm::ui::RecentTargetsBar::targetActivated, this, [this](int, const QString &path) {
        // 칩 경로\지금 이름으로 바꾸고 이름 부분 선택
        const QString name = m_plan.name.isEmpty() ? m_context.items.value(0).name : m_plan.name;
        setTargetText(joinPath(path, name));
    });
    connect(ui->targetEdit, &QLineEdit::textChanged, this, &MoveRenameDialog::updatePlan);
    connect(ui->createDirCheck, &QCheckBox::toggled, this, &MoveRenameDialog::updatePlan);
    connect(ui->allowExtCheck, &QCheckBox::toggled, this, &MoveRenameDialog::updatePlan);
    connect(ui->okButton, &QPushButton::clicked, this, [this] {
        if (m_plan.issue == MoveRenamePlan::ExtensionChange) {
            const auto answer = QMessageBox::question(this, windowTitle(),
                                                      tr("확장자가 바뀝니다 (%1). 계속할까요?").arg(m_plan.detail));
            if (answer != QMessageBox::Yes)
                return;
        }
        accept();
    });
    connect(&fm::style::ThemeManager::instance(), &fm::style::ThemeManager::changed, this, &MoveRenameDialog::refreshIcons);
    refreshIcons();
    setTargetText(moveMode ? joinPath(m_context.targetDir, original) : original);
    ui->targetEdit->setFocus();
}

MoveRenameDialog::~MoveRenameDialog() = default;

void MoveRenameDialog::refreshIcons()
{
    const fm::style::ThemeColors &tc = fm::style::themeColorsFor(this);
    ui->arrowIcon->setPixmap(fm::style::glyphIcon(fm::style::Glyph::ArrowRight, tc[Token::AccentFg], 16).pixmap(16, 16));
}

void MoveRenameDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    updatePlan();  // 새 이름 줄임 폭은 창이 뜬 뒤에 정해진다
}

void MoveRenameDialog::setTargetText(const QString &text)
{
    ui->targetEdit->setText(text);
    // 이름 부분(확장자 제외)만 선택 — 이름 변경 관례
    const qsizetype slash = text.lastIndexOf(u'\\');
    const qsizetype start = slash + 1;
    const QString name = text.mid(start);
    const qsizetype dot = name.lastIndexOf(u'.');
    ui->targetEdit->setSelection(int(start), int(dot > 0 ? dot : name.size()));
}

void MoveRenameDialog::updatePlan()
{
    MoveRenameOptions options;
    options.createDirectories = ui->createDirCheck->isChecked();
    options.allowExtensionChange = ui->allowExtCheck->isChecked();
    m_plan = planMoveRename(ui->targetEdit->text(), m_context.sourceDir, m_context.items.value(0).name, options,
                            probeOf(m_context));
    ui->previewDir->setText(m_plan.directory);
    const QFontMetrics fm(ui->previewName->font());
    const int width = std::max(80, ui->previewName->width());
    ui->previewName->setText(fm.elidedText(m_plan.name, Qt::ElideMiddle, width));
    ui->previewName->setToolTip(m_plan.name);

    ui->opTag->setText(m_plan.operationText());
    ui->opTag->setTone(m_plan.operation == MoveRenamePlan::NoChange ? fm::ui::Tag::Mute : fm::ui::Tag::Info);
    const bool moving = m_plan.operation == MoveRenamePlan::Move || m_plan.operation == MoveRenamePlan::MoveRename;
    ui->volumeTag->setVisible(moving);
    ui->volumeTag->setText(m_plan.volumeText());
    ui->statusTag->setText(m_plan.statusText());
    switch (m_plan.issue) {
    case MoveRenamePlan::None:
        ui->statusTag->setTone(fm::ui::Tag::Ok);
        ui->statusTag->setGlyph(fm::ui::glyph::Check);
        break;
    case MoveRenamePlan::NewFolder:
        ui->statusTag->setTone(fm::ui::Tag::Mute);
        ui->statusTag->setGlyph(fm::ui::glyph::FolderOutline);
        break;
    case MoveRenamePlan::ExtensionChange:
        ui->statusTag->setTone(fm::ui::Tag::Warn);
        ui->statusTag->setGlyph(fm::ui::glyph::Warning);
        break;
    case MoveRenamePlan::Empty:
    case MoveRenamePlan::InvalidChar:
    case MoveRenamePlan::Reserved:
    case MoveRenamePlan::TrailingDotSpace:
    case MoveRenamePlan::Exists:
    case MoveRenamePlan::MissingFolder:
        ui->statusTag->setTone(fm::ui::Tag::Danger);
        ui->statusTag->setGlyph(fm::ui::glyph::Warning);
        break;
    }
    ui->statusTag->setVisible(!(m_plan.operation == MoveRenamePlan::NoChange && m_plan.issue == MoveRenamePlan::None));
    ui->okButton->setEnabled(m_plan.canProceed);
}

MoveRenameRequest MoveRenameDialog::request() const
{
    return {m_plan.directory + m_plan.name, ui->createDirCheck->isChecked()};
}

QStringList MoveRenameDialog::variants()
{
    return {u"move.sameVolume"_s, u"move.renameOnly"_s, u"move.crossVolume"_s, u"move.conflict"_s, u"move.invalid"_s,
            u"move.extChange"_s, u"move.newFolder"_s};
}

void MoveRenameDialog::applyVariant(const QString &id)
{
    const QString original = m_context.items.value(0).name;
    const QString shortName = u"2026년 3분기 보안 감사 보고서"_s;
    if (id == u"move.sameVolume")
        setTargetText(u"D:\\Archive\\2026-Q3\\"_s + shortName + u".pdf"_s);
    else if (id == u"move.renameOnly")
        setTargetText(original);
    else if (id == u"move.crossVolume")
        setTargetText(u"E:\\Backup\\Docs\\"_s + shortName + u".pdf"_s);
    else if (id == u"move.conflict")
        setTargetText(u"D:\\Archive\\2026-Q3\\"_s + shortName + u" (최종).pdf"_s);
    else if (id == u"move.invalid")
        setTargetText(shortName + u"?.pdf"_s);
    else if (id == u"move.extChange")
        setTargetText(u"D:\\Archive\\2026-Q3\\"_s + shortName + u".txt"_s);
    else if (id == u"move.newFolder")
        setTargetText(u"D:\\Archive\\2026-Q4\\"_s + shortName + u".pdf"_s);
}

// ================================================================ DeleteDialog

DeleteDialog::DeleteDialog(const FileOpContext &context, bool permanent, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::DeleteDialog>())
    , m_context(context)
{
    ui->setupUi(this);
    fm::ui::setupDialogChrome(this);
    QList<fm::ui::FileSummaryList::Item> items;
    for (const FileItem &item : std::as_const(m_context.items))
        items.append({item.name, item.isDir ? tr("폴더") : formatBytes(item.size), item.kind, item.isDir, item.readOnly});
    ui->itemsList->setItems(items);
    ui->header->setSubtitle(tr("%1 · 합계 %2").arg(m_context.sourceDir, formatBytes(m_context.totalSize())));
    auto *group = new QButtonGroup(this);
    group->addButton(ui->trashRadio);
    group->addButton(ui->permRadio);
    group->setExclusive(true);
    ui->trashRadio->setAccessibleDescription(tr("삭제 방식"));
    connect(ui->trashRadio, &QRadioButton::toggled, this, &DeleteDialog::updateMode);
    connect(ui->goButton, &QPushButton::clicked, this, &QDialog::accept);
    // 창 안에서도 칩의 뜻을 살린다: Del → 휴지통, Shift+Del → 영구 삭제
    auto *del = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    connect(del, &QShortcut::activated, this, [this] { if (ui->trashRadio->isEnabled()) ui->trashRadio->setChecked(true); });
    auto *shiftDel = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete), this);
    connect(shiftDel, &QShortcut::activated, this, [this] { ui->permRadio->setChecked(true); });

    const VolumeInfo volume = probeOf(m_context).volume(m_context.sourceDir);
    if (volume.valid && !volume.hasRecycleBin) {
        ui->trashRadio->setEnabled(false);
        permanent = true;
    }
    setPermanent(permanent);
}

DeleteDialog::~DeleteDialog() = default;

bool DeleteDialog::isPermanent() const
{
    return ui->permRadio->isChecked();
}

void DeleteDialog::setPermanent(bool permanent)
{
    (permanent ? ui->permRadio : ui->trashRadio)->setChecked(true);
    updateMode();
    // 영구 삭제는 Enter로 바로 실행되지 않게 처음 포커스를 취소에(PLAN §11)
    (permanent ? ui->cancelButton : ui->goButton)->setFocus();
}

void DeleteDialog::updateMode()
{
    const bool permanent = ui->permRadio->isChecked();
    const int count = int(m_context.items.size());
    ui->header->setTone(permanent ? fm::ui::DialogHeader::Danger : fm::ui::DialogHeader::Info);
    ui->header->setTitle(permanent ? tr("%1개 항목을 영구 삭제할까요?").arg(count) : tr("%1개 항목을 휴지통으로 옮길까요?").arg(count));
    ui->trashBanner->setVisible(!permanent);
    ui->permBanner->setVisible(permanent);
    if (!ui->trashRadio->isEnabled()) {
        ui->permBanner->setTone(fm::ui::Banner::Warn);
        ui->permBanner->setText(tr("이 드라이브에는 휴지통이 없습니다. 삭제한 항목은 복구할 수 없습니다."));
    }
    ui->goButton->setRole(permanent ? fm::ui::Button::Danger : fm::ui::Button::Primary);
    ui->goButton->setText(permanent ? tr("영구 삭제") : tr("휴지통으로 이동"));
}

DeleteRequest DeleteDialog::request() const
{
    return {isPermanent(), ui->readOnlyCheck->isChecked()};
}

QStringList DeleteDialog::variants()
{
    return {u"delete.trash"_s, u"delete.permanent"_s, u"delete.single"_s, u"delete.many"_s, u"delete.noRecycleBin"_s,
            u"delete.readOnlyIncluded"_s};
}

void DeleteDialog::applyVariant(const QString &id)
{
    auto refill = [this] {
        QList<fm::ui::FileSummaryList::Item> items;
        for (const FileItem &item : std::as_const(m_context.items))
            items.append({item.name, item.isDir ? tr("폴더") : formatBytes(item.size), item.kind, item.isDir, item.readOnly});
        ui->itemsList->setItems(items);
        ui->header->setSubtitle(tr("%1 · 합계 %2").arg(m_context.sourceDir, formatBytes(m_context.totalSize())));
        updateMode();
    };
    if (id == u"delete.permanent") {
        setPermanent(true);
    } else if (id == u"delete.trash") {
        setPermanent(false);
    } else if (id == u"delete.single") {
        m_context.items = m_context.items.mid(2, 1);
        refill();
    } else if (id == u"delete.many") {
        const QList<FileItem> base = m_context.items;
        for (int i = 0; i < 125; ++i) {
            FileItem item = base.at(i % base.size());
            item.name = u"build-log_%1.txt"_s.arg(i + 1, 3, 10, u'0');
            item.size = 4096 + i * 311;
            item.kind = fileKindFor(item.name, false);
            m_context.items.append(item);
        }
        refill();
    } else if (id == u"delete.noRecycleBin") {
        m_context.sourceDir = u"\\\\nas01\\reports"_s;
        ui->trashRadio->setEnabled(false);
        refill();
        setPermanent(true);
    } else if (id == u"delete.readOnlyIncluded") {
        if (m_context.items.size() > 1)
            m_context.items[1].readOnly = true;
        refill();
    }
}

// ================================================================ NewFileDialog

const QList<NewFileDialog::Template> &NewFileDialog::templates()
{
    static const QList<Template> list = {
        {QString(), QString()}, {u".txt"_s, QString()}, {u".md"_s, QString()}, {u".cpp"_s, QString()},
        {u".h"_s, QString()},   {QString(), u"CMakeLists.txt"_s}, {u".json"_s, QString()}, {u".ui"_s, QString()},
    };
    return list;
}

NewFileDialog::NewFileDialog(const FileOpContext &context, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::NewFileDialog>())
    , m_context(context)
{
    ui->setupUi(this);
    fm::ui::setupDialogChrome(this);
    ui->header->setSubtitle(tr("위치 · %1").arg(m_context.sourceDir));
    ui->nameHelp->hide();
    m_base = m_context.suggestedName.isEmpty() ? tr("새 파일") : m_context.suggestedName;
    const QList<fm::ui::ChoiceCard *> cards = {ui->template1, ui->template2, ui->template3, ui->template4,
                                               ui->template5, ui->template6, ui->template7, ui->template8};
    for (fm::ui::ChoiceCard *card : cards)
        connect(card, &QRadioButton::toggled, this, [this](bool on) { if (on) recompute(); });
    connect(ui->nameEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        // 이름을 고치면 마지막 확장자를 뺀 부분이 새 기준 이름
        static const QRegularExpression lastExt(u"\\.[^.\\\\]*$"_s);
        m_base = QString(text).remove(lastExt);
        validate();
    });
    connect(ui->nameEdit, &QLineEdit::textChanged, this, &NewFileDialog::validate);
    connect(ui->createButton, &QPushButton::clicked, this, &QDialog::accept);
    recompute();
    // 처음 포커스: 이름의 기준 부분만 선택
    ui->nameEdit->setFocus();
    ui->nameEdit->setSelection(0, int(m_base.size()));
}

NewFileDialog::~NewFileDialog() = default;

int NewFileDialog::templateIndex() const
{
    const QList<fm::ui::ChoiceCard *> cards = {ui->template1, ui->template2, ui->template3, ui->template4,
                                               ui->template5, ui->template6, ui->template7, ui->template8};
    for (int i = 0; i < cards.size(); ++i) {
        if (cards.at(i)->isChecked())
            return i;
    }
    return 0;
}

void NewFileDialog::setTemplateIndex(int index)
{
    const QList<fm::ui::ChoiceCard *> cards = {ui->template1, ui->template2, ui->template3, ui->template4,
                                               ui->template5, ui->template6, ui->template7, ui->template8};
    if (index >= 0 && index < cards.size())
        cards.at(index)->setChecked(true);
}

QString NewFileDialog::fileName() const
{
    return ui->nameEdit->text();
}

void NewFileDialog::recompute()
{
    const Template &t = templates().at(templateIndex());
    ui->nameEdit->setText(!t.fixedName.isEmpty() ? t.fixedName : m_base + t.extension);
    validate();
}

void NewFileDialog::validate()
{
    const QString name = ui->nameEdit->text().trimmed();
    QString error;
    if (!name.isEmpty()) {
        if (const QChar bad = firstInvalidChar(name); !bad.isNull())
            error = tr("쓸 수 없는 문자: %1").arg(bad);
        else if (isReservedName(name))
            error = tr("예약된 이름입니다: %1").arg(name.section(u'.', 0, 0));
        else if (probeOf(m_context).exists(joinPath(m_context.sourceDir, name)))
            error = tr("같은 이름의 파일이 이미 있습니다");
    }
    ui->nameHelp->setText(error);
    ui->nameHelp->setVisible(!error.isEmpty());
    ui->createButton->setEnabled(!name.isEmpty() && error.isEmpty());
}

NewFileRequest NewFileDialog::request() const
{
    return {fileName(), templateIndex(), ui->openEditorCheck->isChecked(), ui->utf8Check->isChecked()};
}

QStringList NewFileDialog::variants()
{
    QStringList ids = {u"newfile.default"_s};
    for (int i = 0; i < 8; ++i)
        ids.append(u"newfile.tpl%1"_s.arg(i));
    ids << u"newfile.exists"_s << u"newfile.invalid"_s << u"newfile.empty"_s;
    return ids;
}

void NewFileDialog::applyVariant(const QString &id)
{
    if (id.startsWith(u"newfile.tpl")) {
        setTemplateIndex(id.mid(11).toInt());
    } else if (id == u"newfile.exists") {
        m_base = u"PanelView"_s;
        setTemplateIndex(3);
        recompute();
    } else if (id == u"newfile.invalid") {
        ui->nameEdit->setText(u"Banded?View.h"_s);
    } else if (id == u"newfile.empty") {
        ui->nameEdit->clear();
    }
}

// ================================================================ NewFolderDialog

NewFolderDialog::NewFolderDialog(const FileOpContext &context, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::NewFolderDialog>())
    , m_context(context)
{
    ui->setupUi(this);
    fm::ui::setupDialogChrome(this);
    ui->header->setSubtitle(tr("위치 · %1").arg(m_context.sourceDir));
    ui->nameEdit->setFont(fm::style::monoFont(13));
    connect(ui->nameEdit, &QLineEdit::textChanged, this, &NewFolderDialog::updatePlan);
    connect(ui->createButton, &QPushButton::clicked, this, &QDialog::accept);
    ui->nameEdit->setText(m_context.suggestedName.isEmpty() ? tr("새 폴더") : m_context.suggestedName);
    updatePlan();
    ui->nameEdit->setFocus();
    if (m_context.suggestedName.isEmpty())
        ui->nameEdit->selectAll();
}

NewFolderDialog::~NewFolderDialog() = default;

void NewFolderDialog::updatePlan()
{
    m_plan = planFolders(m_context.sourceDir, ui->nameEdit->text(), probeOf(m_context));
    ui->planView->setPlan(m_plan.nodes);
    if (m_plan.help.isEmpty()) {
        ui->nameHelp->setText(tr("\\ 로 나누면 하위 폴더까지 한 번에 만듭니다."));
        ui->nameHelp->setTone(fm::ui::Label::Default);
    } else {
        ui->nameHelp->setText(m_plan.help);
        ui->nameHelp->setTone(m_plan.helpIsError ? fm::ui::Label::Danger : fm::ui::Label::Warn);
    }
    ui->createButton->setEnabled(m_plan.canCreate);
}

NewFolderRequest NewFolderDialog::request() const
{
    return {m_plan.relativePath, ui->goCheck->isChecked()};
}

QStringList NewFolderDialog::variants()
{
    return {u"newfolder.nested"_s, u"newfolder.single"_s, u"newfolder.allExist"_s, u"newfolder.invalid"_s, u"newfolder.deep"_s};
}

void NewFolderDialog::applyVariant(const QString &id)
{
    if (id == u"newfolder.nested")
        ui->nameEdit->setText(u"platform\\win32\\shim"_s);
    else if (id == u"newfolder.single")
        ui->nameEdit->setText(tr("새 폴더"));
    else if (id == u"newfolder.allExist")
        ui->nameEdit->setText(u"platform"_s);
    else if (id == u"newfolder.invalid")
        ui->nameEdit->setText(u"platform\\win?32"_s);
    else if (id == u"newfolder.deep")
        ui->nameEdit->setText(u"platform\\win32\\shim\\x64\\debug\\cache\\obj\\tmp"_s);
}

} // namespace fm::dialogs
