#include "fmdialogs/MultiRenameDialog.h"

#include "ui_MultiRenameDialog.h"

#include "fmdialogs/RenamePreview.h"

#include <fmstyle/StylePaint.h>
#include <fmwidgets/DialogChrome.h>

#include <QInputDialog>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

using fm::filelist::ViewMode;

MultiRenameDialog::Context MultiRenameDialog::boardContext()
{
    Context c;
    c.files = RenameSample::files();
    c.folder = RenameSample::folder();
    c.existing = RenameSample::existing();
    c.rules = RenameSample::boardRules();

    RenameRules screenshot;
    screenshot.mask = u"[Y][M][D]_[t]"_s;
    screenshot.extCase = ExtCase::Lower;
    RenameRules lower;
    lower.mask = u"[N]"_s;
    lower.nameCase = NameCase::Lower;
    lower.extCase = ExtCase::Lower;
    c.presets = {{tr("사진 · 날짜_장소_번호"), c.rules}, {tr("스크린샷 · 날짜_시각"), screenshot},
                 {tr("원래 이름 · 모두 소문자"), lower}};

    // 지난번 변경: 11번 파일을 같은 규칙으로 바꿨다(되돌리면 원래 이름이 돌아온다).
    QList<RenameFile> before = c.files;
    before[10].name = u"IMG_20260915_134502.JPG"_s;
    c.history = {before};
    return c;
}

MultiRenameDialog::MultiRenameDialog(const Context &context, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::MultiRenameDialog>())
    , m_context(context)
{
    ui->setupUi(this);
    // 모델은 setupUi 뒤에 만든다 — 자식 삭제 순서상 미리보기(Qtitan)보다 늦게 없어진다.
    m_model = new RenamePreviewModel(this);

    fm::ui::DialogChromeOptions chrome;
    chrome.maximizeButton = true;
    chrome.fixedSize = false;
    fm::ui::setupDialogChrome(this, chrome);
    resize(1120, 764);
    setWindowTitle(tr("다중 이름 변경 — %1개 파일 · %2").arg(m_context.files.size()).arg(m_context.folder));

    ui->maskEdit->setFont(fm::style::monoFont(13));
    ui->extEdit->setFont(fm::style::monoFont(13));
    ui->extCaseSegment->setSegmentSize(fm::ui::SegmentedControl::Normal);
    ui->extCaseSegment->setFont(fm::style::pixelFont(font(), 12));  // 목업 .seg-b 12 px
    ui->recordSegment->setSegmentSize(fm::ui::SegmentedControl::Small);
    ui->previewView->setModel(m_model);
    // 기본 단추를 두지 않는다 — 마스크 입력 중 Enter로 실행되지 않게(02 §9.1).
    for (QPushButton *button : findChildren<QPushButton *>()) {
        button->setAutoDefault(false);
        button->setDefault(false);
    }

    // 토큰 단추: 커서 위치에 넣고 마스크 입력으로 돌아간다.
    for (fm::ui::TokenButton *token : ui->tokenBox->findChildren<fm::ui::TokenButton *>()) {
        connect(token, &QPushButton::clicked, this, [this, token] { insertToken(token->token()); });
    }

    auto refresh = [this] {
        if (!m_loading)
            updatePreview();
    };
    connect(ui->maskEdit, &QLineEdit::textChanged, this, refresh);
    connect(ui->extEdit, &QLineEdit::textChanged, this, refresh);
    connect(ui->findEdit, &QLineEdit::textChanged, this, refresh);
    connect(ui->replaceEdit, &QLineEdit::textChanged, this, refresh);
    connect(ui->regexCheck, &QCheckBox::toggled, this, refresh);
    connect(ui->caseSensitiveCheck, &QCheckBox::toggled, this, refresh);
    connect(ui->nameCaseCombo, &QComboBox::currentIndexChanged, this, refresh);
    connect(ui->extCaseSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, refresh);
    connect(ui->startSpin, &QSpinBox::valueChanged, this, refresh);
    connect(ui->stepSpin, &QSpinBox::valueChanged, this, refresh);
    connect(ui->digitsSpin, &QSpinBox::valueChanged, this, refresh);
    connect(ui->exifRadio, &QRadioButton::toggled, this, refresh);

    // 레코드 1줄 · 2줄 · 자동(Ctrl+1 · 2 · 3)
    connect(ui->recordSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, [this](int i) {
        ui->previewView->setViewMode(i == 0 ? ViewMode::OneLine : i == 1 ? ViewMode::TwoLine : ViewMode::Auto);
        updateInfo();
    });
    connect(ui->previewView, &RenamePreviewView::twoLineChanged, this, [this] { updateInfo(); });
    for (int i = 0; i < 3; ++i) {
        auto *shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key(Qt::Key_1 + i)), this);
        connect(shortcut, &QShortcut::activated, this, [this, i] { ui->recordSegment->setCurrentIndex(i); });
    }
    ui->recordSegment->setCurrentIndex(std::clamp(m_context.recordMode, 0, 2));
    // Ctrl+Z — 입력 상자에 포커스가 있으면 QLineEdit의 되돌리기가 먼저(ShortcutOverride).
    auto *undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this, &MultiRenameDialog::undo);
    connect(ui->undoButton, &QPushButton::clicked, this, &MultiRenameDialog::undo);
    connect(ui->renameButton, &QPushButton::clicked, this, &MultiRenameDialog::applyRename);

    // 프리셋
    for (const RenamePreset &p : std::as_const(m_context.presets))
        ui->presetCombo->addItem(p.name);
    ui->presetCombo->setCurrentIndex(std::clamp(m_context.preset, 0, int(m_context.presets.size()) - 1));
    connect(ui->presetCombo, &QComboBox::activated, this, [this](int i) {
        if (i >= 0 && i < m_context.presets.size())
            setRules(m_context.presets.at(i).rules);
    });
    connect(ui->savePresetButton, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("프리셋 저장"), tr("프리셋 이름(&N)"), QLineEdit::Normal,
                                                   ui->presetCombo->currentText(), &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        const int existing = ui->presetCombo->findText(name.trimmed());
        if (existing >= 0) {
            m_context.presets[existing].rules = rules();
            ui->presetCombo->setCurrentIndex(existing);
            return;
        }
        m_context.presets.append({name.trimmed(), rules()});
        ui->presetCombo->addItem(name.trimmed());
        ui->presetCombo->setCurrentIndex(ui->presetCombo->count() - 1);
    });
    connect(ui->deletePresetButton, &QPushButton::clicked, this, [this] {
        const int i = ui->presetCombo->currentIndex();
        if (i < 0 || i >= m_context.presets.size())
            return;
        m_context.presets.removeAt(i);
        ui->presetCombo->removeItem(i);
        updateButtons();
    });

    ui->findCard->installEventFilter(this);

    setRules(m_context.rules);
    ui->maskEdit->setFocus();
    ui->maskEdit->setCursorPosition(ui->maskEdit->text().size());
}

MultiRenameDialog::~MultiRenameDialog() = default;

bool MultiRenameDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->findCard && event->type() == QEvent::Resize)
        updateFlagsLayout();
    return QDialog::eventFilter(watched, event);
}

void MultiRenameDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // QDialog는 보일 때 첫 포커스 위젯에 Tab 이유의 FocusIn을 보내 QLineEdit이 전체 선택된다.
    // 목업은 마스크 끝에 커서만 둔다.
    QTimer::singleShot(0, this, [this] {
        ui->maskEdit->deselect();
        ui->maskEdit->setCursorPosition(ui->maskEdit->text().size());
    });
}

// 목업 flagsRow는 flex-wrap(간격 16) — 두 체크 상자가 한 줄에 안 들어가면 아래로 넘긴다.
void MultiRenameDialog::updateFlagsLayout()
{
    const QMargins m = ui->findLayout->contentsMargins();
    const int available = ui->findCard->width() - m.left() - m.right();
    const int needed = ui->regexCheck->sizeHint().width() + ui->flagsRow->spacing()
                       + ui->caseSensitiveCheck->sizeHint().width();
    ui->flagsRow->setDirection(needed <= available ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
}

RenameRules MultiRenameDialog::rules() const
{
    RenameRules r;
    r.mask = ui->maskEdit->text();
    r.extMask = ui->extEdit->text();
    r.find = ui->findEdit->text();
    r.replace = ui->replaceEdit->text();
    r.regex = ui->regexCheck->isChecked();
    r.caseSensitive = ui->caseSensitiveCheck->isChecked();
    r.nameCase = NameCase(std::max(0, ui->nameCaseCombo->currentIndex()));
    r.extCase = ExtCase(std::max(0, ui->extCaseSegment->currentIndex()));
    r.counterStart = ui->startSpin->value();
    r.counterStep = ui->stepSpin->value() == 0 ? 1 : ui->stepSpin->value();
    r.counterDigits = ui->digitsSpin->value();
    r.dateSource = ui->exifRadio->isChecked() ? DateSource::Captured : DateSource::Modified;
    return r;
}

void MultiRenameDialog::setRules(const RenameRules &r)
{
    m_loading = true;
    ui->maskEdit->setText(r.mask);
    ui->extEdit->setText(r.extMask);
    ui->findEdit->setText(r.find);
    ui->replaceEdit->setText(r.replace);
    ui->regexCheck->setChecked(r.regex);
    ui->caseSensitiveCheck->setChecked(r.caseSensitive);
    ui->nameCaseCombo->setCurrentIndex(int(r.nameCase));
    ui->extCaseSegment->setCurrentIndex(int(r.extCase));
    ui->startSpin->setValue(r.counterStart);
    ui->stepSpin->setValue(r.counterStep);
    ui->digitsSpin->setValue(r.counterDigits);
    (r.dateSource == DateSource::Captured ? ui->exifRadio : ui->modifiedRadio)->setChecked(true);
    m_loading = false;
    updatePreview();
}

QList<RenamePreviewRow> MultiRenameDialog::previewRows() const
{
    return m_model->rows();
}

RenameSummary MultiRenameDialog::summary() const
{
    return summarizeRename(m_model->rows());
}

RenamePreviewView *MultiRenameDialog::previewView() const
{
    return ui->previewView;
}

void MultiRenameDialog::updatePreview()
{
    const QString folderName = m_context.folder.section(u'\\', -1);
    m_model->setRows(previewRename(m_context.files, rules(), folderName, m_context.existing));
    ui->summaryLabel->setText(summary().text());
    updateInfo();
    updateButtons();
}

void MultiRenameDialog::updateInfo()
{
    const RenameSummary s = summary();
    QString info = tr("%1개 · 긴 이름 %2개").arg(s.total).arg(s.longCount);
    if (ui->previewView->viewMode() == ViewMode::Auto)
        info += ui->previewView->isTwoLine() ? tr(" · 자동으로 2줄 표시 중") : tr(" · 자동으로 1줄 표시 중");
    ui->previewInfo->setText(info);
}

void MultiRenameDialog::updateButtons()
{
    const RenameSummary s = summary();
    ui->renameButton->setEnabled(s.changed > 0 && !(m_context.blockOnProblems && s.bad > 0));
    ui->undoButton->setEnabled(canUndo());
    ui->deletePresetButton->setEnabled(ui->presetCombo->count() > 0);
}

void MultiRenameDialog::insertToken(const QString &token)
{
    ui->maskEdit->insert(token);
    ui->maskEdit->setFocus();
}

void MultiRenameDialog::applyRename()
{
    const QList<RenamePreviewRow> rows = m_model->rows();
    QList<RenamePreviewRow> done;
    QList<RenameFile> before = m_context.files;
    for (const RenamePreviewRow &row : rows) {
        if (row.state != RenamePreviewRow::Ok)
            continue;
        const int i = row.index - 1;
        if (i >= 0 && i < m_context.files.size()) {
            m_context.files[i].name = row.newName;
            done.append(row);
        }
    }
    if (done.isEmpty())
        return;
    if (m_context.undoDepth > 0) {
        m_context.history.append(before);
        while (m_context.history.size() > m_context.undoDepth)
            m_context.history.removeFirst();  // 오래된 기록부터
    }
    updatePreview();
    Q_EMIT renamed(done);
}

void MultiRenameDialog::undo()
{
    if (m_context.history.isEmpty())
        return;
    m_context.files = m_context.history.takeLast();
    updatePreview();
}

QStringList MultiRenameDialog::variants()
{
    return {u"multirename.default"_s, u"multirename.oneLine"_s, u"multirename.twoLine"_s, u"multirename.extKeep"_s,
            u"multirename.extUpper"_s, u"multirename.tokenAppend"_s, u"multirename.duplicates"_s,
            u"multirename.invalid"_s, u"multirename.autoOneLine"_s, u"multirename.blocked"_s,
            u"multirename.findReplace"_s, u"multirename.noUndo"_s};
}

void MultiRenameDialog::applyVariant(const QString &id)
{
    if (id == u"multirename.oneLine") {
        ui->recordSegment->setCurrentIndex(0);
    } else if (id == u"multirename.twoLine") {
        ui->recordSegment->setCurrentIndex(1);
    } else if (id == u"multirename.extKeep") {
        ui->extCaseSegment->setCurrentIndex(0);
    } else if (id == u"multirename.extUpper") {
        ui->extCaseSegment->setCurrentIndex(2);
    } else if (id == u"multirename.tokenAppend") {
        ui->maskEdit->end(false);
        insertToken(u"[t]"_s);
    } else if (id == u"multirename.duplicates") {
        ui->maskEdit->setText(u"[Y]-[M]-[D]"_s);
    } else if (id == u"multirename.invalid") {
        ui->maskEdit->setText(u"[Y]:[M]"_s);
    } else if (id == u"multirename.autoOneLine") {
        QList<RenameFile> shortOnes;
        for (const RenameFile &f : std::as_const(m_context.files)) {
            if (f.name.size() <= 40)
                shortOnes.append(f);
        }
        m_context.files = shortOnes;
        m_context.history.clear();
        setWindowTitle(tr("다중 이름 변경 — %1개 파일 · %2").arg(m_context.files.size()).arg(m_context.folder));
        updatePreview();
    } else if (id == u"multirename.blocked") {
        m_context.blockOnProblems = true;
        updateButtons();
    } else if (id == u"multirename.findReplace") {
        RenameRules r = rules();
        r.mask = u"[N]"_s;
        r.find = u"IMG_"_s;
        r.replace = u"제주_"_s;
        setRules(r);
    } else if (id == u"multirename.noUndo") {
        m_context.history.clear();
        updateButtons();
    }
}

} // namespace fm::dialogs
