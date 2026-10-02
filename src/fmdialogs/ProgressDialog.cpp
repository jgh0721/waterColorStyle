#include "fmdialogs/ProgressDialog.h"

#include "ui_ProgressDialog.h"

#include "fmdialogs/FileOpContext.h"
#include "fmdialogs/ProgressSimulator.h"

#include <fmwidgets/DialogChrome.h>

#include <QKeyEvent>
#include <QLocale>
#include <QMessageBox>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

QString kindText(ProgressDialog::Kind kind)
{
    switch (kind) {
    case ProgressDialog::Copy:            return QObject::tr("복사");
    case ProgressDialog::Move:            return QObject::tr("이동");
    case ProgressDialog::Delete:          return QObject::tr("삭제");
    case ProgressDialog::DeletePermanent: return QObject::tr("영구 삭제");
    }
    return QString();
}

QString arrowJoin(const QString &a, const QString &b)
{
    return b.isEmpty() ? a : a + u" → "_s + b;  // 간단히 모드 부제의 화살표는 글자(U+2192)
}

} // namespace

ProgressDialog::Operation ProgressDialog::boardCopy()
{
    constexpr qint64 MB = 1024 * 1024;
    constexpr qint64 GB = 1024 * MB;
    Operation op;
    op.kind = Copy;
    op.source = u"D:\\Downloads"_s;
    op.target = u"E:\\Backup\\Installers"_s;
    // 앞의 두 파일(0.21 GiB)을 먼저 옮기고 지금은 세 번째(3.74 GiB)를 옮기는 중(02 §7.3)
    op.fileNames = {u"QtitanDataGrid-9.3.0-Windows-MSVC2026-x64-Setup.exe"_s, u"vc_redist.x64.exe"_s,
                    u"Qt-6.11.0-windows-x64-msvc2026-offline-installer-with-debug-symbols.exe"_s};
    op.fileSizes = {186 * MB, qint64(24.4 * MB), qint64(3.74 * GB)};
    op.policyText = tr("같은 이름이 있으면 매번 묻기");
    return op;
}

ProgressDialog::ProgressDialog(const Operation &operation, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::ProgressDialog>())
    , m_op(operation)
    , m_sim(new ProgressSimulator(this))
{
    ui->setupUi(this);
    fm::ui::DialogChromeOptions chrome;
    chrome.icon = m_op.elevated ? fm::ui::glyph::Shield : fm::ui::glyph::App;
    chrome.minimizeButton = true;
    chrome.fixedSize = false;
    fm::ui::setupDialogChrome(this, chrome);
    ui->axisSegment->setSegmentSize(fm::ui::SegmentedControl::Mini);
    ui->axisSegment->setCurrentIndex(0);

    connect(ui->axisSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, [this](int i) {
        ui->transferGraph->setAxis(i == 1 ? fm::ui::TransferGraph::Time : fm::ui::TransferGraph::Progress);
    });
    connect(ui->transferGraph, &fm::ui::TransferGraph::peakChanged, this, [this](double peak) {
        ui->peakLabel->setText(tr("최대 %1").arg(fm::ui::TransferGraph::formatRate(peak)));
    });
    connect(ui->modeLink, &QPushButton::clicked, this, [this] { setMode(m_mode == Detail ? Compact : Detail); });
    connect(ui->pauseButton, &QPushButton::clicked, this, [this] { setPaused(!isPaused()); });
    connect(ui->backgroundButton, &QPushButton::clicked, this, [this] {
        hide();
        Q_EMIT backgroundRequested();
    });
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_sim, &ProgressSimulator::ticked, this, &ProgressDialog::refresh);
    connect(m_sim, &ProgressSimulator::restarted, this, [this] { ui->transferGraph->start(m_sim->total()); });
    connect(m_sim, &ProgressSimulator::finished, this, &ProgressDialog::finish);

    m_sim->setLoop(false);
    m_sim->setFiles(m_op.fileSizes);
    ui->transferGraph->start(m_sim->total());
    m_sim->start();
    ui->closeWhenDoneCheck->setChecked(m_op.closeWhenDone);
    setMode(m_op.kind == Copy && !m_op.elevated && m_op.detailed ? Detail : Compact);
    refresh();
}

ProgressDialog::~ProgressDialog() = default;

void ProgressDialog::setDemoLoop(bool on)
{
    m_demoLoop = on;
    m_sim->setLoop(on);
}

void ProgressDialog::prerollDemo()
{
    m_sim->reset();
    // 목업 정적 값: 3.95 GB 중 1.50 GB(37 %). 스크립트의 9초 미리 계산은 약 32 %라 바이트로 맞춘다.
    m_sim->prerollTo(qint64(1.50 * 1024 * 1024 * 1024));
    ui->transferGraph->start(m_sim->total());
    for (const ProgressSimulator::Sample &s : m_sim->samples()) {
        ui->transferGraph->setPaused(s.paused);
        ui->transferGraph->addSample(s.bytes, s.ms);
    }
    refresh();
}

void ProgressDialog::finish()
{
    refresh();
    if (ui->closeWhenDoneCheck->isChecked() || m_mode == Compact) {
        accept();
        Q_EMIT completed();
        return;
    }
    // 완료 상태 — 일시 정지 · 백그라운드는 숨기고 취소를 닫기로
    m_done = true;
    setWindowTitle(tr("100% · %1 완료").arg(kindText(m_op.kind)));
    ui->header->setTone(fm::ui::DialogHeader::Ok);
    ui->header->setGlyph(fm::ui::glyph::Check);
    ui->speedValue->setText(u"—"_s);
    ui->remainValue->setText(tr("완료"));
    ui->pauseButton->hide();
    ui->backgroundButton->hide();
    ui->cancelButton->setText(tr("닫기"));
    disconnect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::accept);
    Q_EMIT completed();
}

QString ProgressDialog::summaryText() const
{
    return tr("%1 · %2개 항목").arg(kindText(m_op.kind)).arg(m_op.fileNames.size());
}

void ProgressDialog::setWaiting(bool waiting)
{
    if (m_waiting == waiting || m_static)
        return;
    m_waiting = waiting;
    m_sim->setPaused(waiting);
    ui->transferGraph->setPaused(waiting);
    ui->pauseButton->setEnabled(!waiting);  // 대기 중에는 일시 정지 · 재개가 없다
    refresh();
    updateButtons();
}

void ProgressDialog::keyPressEvent(QKeyEvent *event)
{
    // Esc — 설정 › 키보드 › 진행 창에서 Esc(확인 후 취소 · 바로 취소 · 창 숨기기). 끝난 창은 닫기.
    if (event->key() != Qt::Key_Escape || event->modifiers() != Qt::NoModifier) {
        QDialog::keyPressEvent(event);
        return;
    }
    event->accept();
    if (m_done) {
        accept();
        return;
    }
    using Esc = Operation::EscAction;
    Esc action = m_op.esc;
    if (action == Esc::HideWindow && m_op.elevated)
        action = Esc::ConfirmCancel;  // 관리자 작업은 백그라운드로 보내지 않는다(03 §4)
    switch (action) {
    case Esc::CancelImmediately:
        reject();
        break;
    case Esc::HideWindow:
        hide();
        Q_EMIT backgroundRequested();
        break;
    case Esc::ConfirmCancel:
        if (QMessageBox::question(this, tr("작업 취소"), tr("%1 작업을 취소할까요?").arg(kindText(m_op.kind)),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            == QMessageBox::Yes)
            reject();
        break;
    }
}

bool ProgressDialog::isPaused() const
{
    return m_static ? ui->compactBar->state() == fm::ui::ProgressBar::Paused : m_sim->isPaused();
}

void ProgressDialog::setPaused(bool paused)
{
    if (m_static) {
        ui->compactBar->setState(paused ? fm::ui::ProgressBar::Paused : fm::ui::ProgressBar::Normal);
        updateTitle();
        updateButtons();
        return;
    }
    m_sim->setPaused(paused);
    ui->transferGraph->setPaused(paused);
    refresh();
    updateButtons();
}

void ProgressDialog::setMode(Mode mode)
{
    m_mode = mode;
    const bool detail = mode == Detail;
    ui->detailPanel->setVisible(detail);
    ui->compactBlock->setVisible(!detail);
    ui->bodyLayout->setSpacing(detail ? 16 : 14);
    const bool switchable = m_op.kind == Copy && !m_op.elevated;
    ui->modeLink->setVisible(switchable);
    ui->modeLink->setText(detail ? tr("간단히 보기(&D)") : tr("자세히 보기(&D)"));
    ui->modeLink->setGlyph(detail ? fm::ui::glyph::ChevronUp : fm::ui::glyph::ChevronDown);
    ui->closeWhenDoneCheck->setVisible(detail);
    ui->elevationBanner->setVisible(m_op.elevated);
    ui->pathLabel->setVisible(!detail && (m_op.kind == Delete || m_op.kind == DeletePermanent));
    refresh();
    updateButtons();
    fitSize();
}

void ProgressDialog::fitSize()
{
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    if (m_mode == Detail) {
        setFixedSize(640, 524);
        return;
    }
    layout()->activate();
    setFixedSize(520, heightForWidth(520) > 0 ? heightForWidth(520) : sizeHint().height());
}

void ProgressDialog::refresh()
{
    if (m_done)
        return;  // 완료 상태는 finish()가 채운 그대로 둔다
    const bool paused = isPaused();
    const auto barState = paused ? fm::ui::ProgressBar::Paused : fm::ui::ProgressBar::Normal;
    const int count = int(m_op.fileNames.size());
    const QLocale locale(QLocale::Korean);

    // 머리 블록
    if (m_mode == Detail) {
        ui->header->setTitle(m_op.source);
        ui->header->setTitleTo(m_op.target);
        ui->header->setGlyph(fm::ui::glyph::Copy);
        ui->header->setTone(paused ? fm::ui::DialogHeader::Mute : fm::ui::DialogHeader::Info);
        const QString base = tr("%1개 항목 · %2").arg(count).arg(formatBytes(m_sim->total()));
        ui->header->setSubtitle(base + u" · "_s + (paused ? tr("일시 정지됨") : m_op.policyText));
    } else if (!m_static) {
        ui->header->setTitleTo(QString());
        switch (m_op.kind) {
        case Copy:
            ui->header->setTitle(tr("%1 복사 중").arg(m_op.fileNames.value(m_sim->fileIndex())));
            ui->header->setGlyph(fm::ui::glyph::Copy);
            ui->header->setSubtitle(arrowJoin(m_op.source, m_op.target));
            break;
        case Move:
            ui->header->setTitle(paused ? tr("이동 일시 정지됨") : tr("이동 중"));
            ui->header->setGlyph(paused ? fm::ui::glyph::Pause : fm::ui::glyph::Move);
            ui->header->setSubtitle(arrowJoin(m_op.source, m_op.target));
            break;
        case Delete:
            ui->header->setTitle(tr("휴지통으로 옮기는 중"));
            ui->header->setGlyph(fm::ui::glyph::Trash);
            ui->header->setSubtitle(m_op.source);
            break;
        case DeletePermanent:
            ui->header->setTitle(tr("영구 삭제하는 중"));
            ui->header->setGlyph(fm::ui::glyph::Trash);
            ui->header->setSubtitle(m_op.source);
            break;
        }
        ui->header->setTone(paused ? fm::ui::DialogHeader::Mute
                            : m_op.kind == DeletePermanent ? fm::ui::DialogHeader::Danger
                                                           : fm::ui::DialogHeader::Info);
    }
    if (m_static) {
        updateTitle();
        return;
    }

    const qint64 total = std::max<qint64>(1, m_sim->total());
    const int overall = int(1000.0 * double(m_sim->bytes()) / double(total));
    const qint64 fileSize = std::max<qint64>(1, m_sim->fileSize());
    const int index = m_sim->fileIndex();
    // 자세히
    ui->currentName->setText(m_op.fileNames.value(index));
    ui->currentBytes->setText(tr("%1 / %2").arg(formatBytes(m_sim->fileDone()), formatBytes(m_sim->fileSize())));
    ui->currentBar->setValue(int(1000.0 * double(m_sim->fileDone()) / double(fileSize)));
    ui->currentBar->setState(barState);
    ui->overallPct->setText(tr("%1%").arg(m_sim->percent()));
    ui->overallBytes->setText(tr("%1 / %2").arg(formatBytes(m_sim->bytes()), formatBytes(m_sim->total())));
    ui->overallBar->setValue(overall);
    ui->overallBar->setState(barState);
    ui->speedValue->setText(paused ? u"—"_s : fm::ui::TransferGraph::formatRate(m_sim->speed()));
    ui->remainValue->setText(m_sim->remainingText());
    ui->itemsValue->setText(tr("%1 / %2").arg(index + 1).arg(count));
    ui->elapsedValue->setText(m_sim->elapsedText());
    if (!m_sim->samples().isEmpty() && m_sim->isRunning()) {
        const ProgressSimulator::Sample &s = m_sim->samples().constLast();
        ui->transferGraph->addSample(s.bytes, s.ms);
    }
    // 간단히
    ui->compactBar->setValue(overall);
    ui->compactBar->setState(barState);
    switch (m_op.kind) {
    case Copy:
        ui->metaLeft->setText(tr("%1 / %2 · %3 / %4 항목").arg(formatBytes(m_sim->bytes()), formatBytes(m_sim->total()))
                                  .arg(index + 1).arg(count));
        break;
    case Move:
        ui->metaLeft->setText(tr("%1 / %2 항목 · %3 / %4").arg(locale.toString(index + 1), locale.toString(count),
                                                                 formatBytes(m_sim->bytes()), formatBytes(m_sim->total())));
        break;
    case Delete:
    case DeletePermanent:
        ui->metaLeft->setText(tr("%1 / %2 항목").arg(locale.toString(index + 1), locale.toString(count)));
        ui->pathLabel->setText(m_op.source + u'\\' + m_op.fileNames.value(index));
        break;
    }
    ui->metaRight->setText(tr("남은 시간 %1").arg(m_sim->remainingText()));
    updateTitle();
}

void ProgressDialog::updateTitle()
{
    const bool paused = isPaused();
    const int pct = m_static ? m_staticPercent : m_sim->percent();
    const QString kind = kindText(m_op.kind);
    if (m_waiting)
        setWindowTitle(tr("대기 중 — %1").arg(kind));
    else if (paused)
        setWindowTitle(tr("%1% · 일시 정지됨 — %2").arg(pct).arg(kind));
    else if (m_op.elevated && m_op.adminTitle)
        setWindowTitle(tr("%1% · %2 중 (관리자)").arg(pct).arg(kind));
    else
        setWindowTitle(tr("%1% · %2 중").arg(pct).arg(kind));
}

void ProgressDialog::updateButtons()
{
    const bool paused = isPaused() && !m_waiting;  // 대기 중은 일시 정지가 아니다
    const bool deleting = m_op.kind == Delete || m_op.kind == DeletePermanent;
    ui->pauseButton->setVisible(!deleting);
    // 일시 정지 중 "재개"는 기본 단추(PLAN §11)
    ui->pauseButton->setRole(paused ? fm::ui::Button::Primary : fm::ui::Button::Normal);
    ui->pauseButton->setText(paused ? tr("재개(&P)") : tr("일시 정지(&P)"));
    ui->pauseButton->setGlyph(m_op.elevated && !paused ? fm::ui::glyph::None
                              : paused                 ? fm::ui::glyph::Play
                                                       : fm::ui::glyph::Pause);
    if (m_op.elevated && !paused)
        ui->pauseButton->setIcon(QIcon());
    ui->backgroundButton->setVisible(!m_op.elevated);
}

void ProgressDialog::setStatic(int percentTimes10, const QString &left, const QString &right, const QString &path)
{
    m_static = true;
    m_sim->stop();
    m_staticPercent = percentTimes10 / 10;
    ui->compactBar->setValue(percentTimes10);
    ui->metaLeft->setText(left);
    ui->metaRight->setText(right);
    ui->pathLabel->setText(path);
}

QStringList ProgressDialog::variants()
{
    return {u"progress.copy.detail.running"_s, u"progress.copy.detail.paused"_s, u"progress.copy.compact.running"_s,
            u"progress.move.compact.paused"_s, u"progress.delete.compact.running"_s, u"progress.copy.compact.elevated"_s,
            u"progress.delete.compact.permanent"_s};
}

void ProgressDialog::applyVariant(const QString &id)
{
    if (id == u"progress.copy.detail.running") {
        prerollDemo();
    } else if (id == u"progress.copy.detail.paused") {
        prerollDemo();
        setPaused(true);
    } else if (id == u"progress.copy.compact.running") {
        prerollDemo();
        setMode(Compact);
    } else if (id == u"progress.move.compact.paused") {
        m_op.kind = Move;
        m_op.source = u"D:\\Work\\fm-core\\build"_s;
        m_op.target = u"E:\\Archive\\build-2026-09"_s;
        setStatic(620, tr("2,418 / 3,902 항목 · 1.18 GB / 1.90 GB"), tr("남은 시간 —"), QString());
        ui->header->setTitleTo(QString());
        ui->header->setTitle(tr("이동 일시 정지됨"));
        ui->header->setGlyph(fm::ui::glyph::Pause);
        ui->header->setTone(fm::ui::DialogHeader::Mute);
        ui->header->setSubtitle(arrowJoin(m_op.source, m_op.target));
        setMode(Compact);
        setPaused(true);
    } else if (id == u"progress.delete.compact.running" || id == u"progress.delete.compact.permanent") {
        const bool permanent = id.endsWith(u"permanent");
        m_op.kind = permanent ? DeletePermanent : Delete;
        m_op.source = u"D:\\Work\\fm-core\\build\\CMakeFiles"_s;
        setStatic(310, tr("1,204 / 3,880 항목"), tr("남은 시간 약 20초"),
                  u"D:\\Work\\fm-core\\build\\CMakeFiles\\fm-core.dir\\src\\panel\\BandedPanelView.cpp.obj"_s);
        ui->header->setTitleTo(QString());
        ui->header->setTitle(permanent ? tr("영구 삭제하는 중") : tr("휴지통으로 옮기는 중"));
        ui->header->setGlyph(fm::ui::glyph::Trash);
        ui->header->setTone(permanent ? fm::ui::DialogHeader::Danger : fm::ui::DialogHeader::Info);
        ui->header->setSubtitle(m_op.source);
        setMode(Compact);
    } else if (id == u"progress.copy.compact.elevated") {
        m_op.elevated = true;
        m_op.source = u"D:\\Downloads"_s;
        m_op.target = u"C:\\Program Files\\FM Tools\\redist"_s;
        fm::ui::DialogChromeOptions chrome;
        chrome.icon = fm::ui::glyph::Shield;
        chrome.fixedSize = false;
        fm::ui::setupDialogChrome(this, chrome);
        setStatic(720, tr("17.6 MB / 24.4 MB · 1 / 3 항목"), tr("남은 시간 약 1초"), QString());
        ui->header->setTitleTo(QString());
        ui->header->setTitle(tr("vc_redist.x64.exe 복사 중"));
        ui->header->setGlyph(fm::ui::glyph::Copy);
        ui->header->setTone(fm::ui::DialogHeader::Info);
        ui->header->setSubtitle(arrowJoin(m_op.source, m_op.target));
        setMode(Compact);
    }
    updateTitle();
    updateButtons();
}

} // namespace fm::dialogs
