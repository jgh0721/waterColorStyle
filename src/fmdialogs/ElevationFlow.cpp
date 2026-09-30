#include "fmdialogs/ElevationFlow.h"

#include "fmdialogs/ElevationDialog.h"
#include "fmdialogs/ProgressDialog.h"

#include <fmwidgets/Button.h>
#include <fmwidgets/DialogChrome.h>
#include <fmwidgets/DialogFooter.h>
#include <fmwidgets/DialogHeader.h>
#include <fmwidgets/Label.h>

#include <QHBoxLayout>
#include <QTimer>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

using elev::Choice;
using elev::Operation;

namespace {

constexpr qint64 MB = 1024 * 1024;
const QString kCopySource = u"D:\\Downloads"_s;
const QString kCopyTarget = u"C:\\Program Files\\FM Tools\\redist\\"_s;
const QString kDeleteSource = u"D:\\Work\\fm-core\\build"_s;

} // namespace

// ---------------------------------------------------------------------------------------------
// UacSimulationDialog

UacSimulationDialog::UacSimulationDialog(const QString &program, int timeoutSeconds, QWidget *parent)
    : QDialog(parent)
    , m_timer(new QTimer(this))
    , m_left(std::max(1, timeoutSeconds))
{
    fm::ui::DialogChromeOptions chrome;
    chrome.icon = fm::ui::glyph::Shield;
    chrome.fixedSize = false;
    fm::ui::setupDialogChrome(this, chrome);
    setWindowTitle(tr("사용자 계정 컨트롤 (시뮬레이션)"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *body = new QWidget(this);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(24, 8, 24, 20);
    layout->setSpacing(12);
    auto *header = new fm::ui::DialogHeader(body);
    header->setSubtitleWrap(true);
    header->setTone(fm::ui::DialogHeader::Warn);
    header->setGlyph(fm::ui::glyph::Shield);
    header->setTitle(tr("이 앱이 디바이스를 변경하도록 허용하시겠어요?"));
    header->setSubtitle(program);
    layout->addWidget(header);
    auto *note = new fm::ui::Label(tr("실제 Windows 확인 창이 아니라 권한 흐름을 확인하기 위한 흉내입니다. "
                                      "파일과 권한은 바뀌지 않습니다."),
                                   fm::ui::Label::Help, body);
    layout->addWidget(note);
    m_countdown = new fm::ui::Label(QString(), fm::ui::Label::Help, body);
    m_countdown->setObjectName(u"countdown"_s);
    layout->addWidget(m_countdown);
    layout->addStretch(1);
    root->addWidget(body, 1);

    auto *footer = new fm::ui::DialogFooter(this);
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(24, 16, 24, 16);
    footerLayout->setSpacing(8);
    footerLayout->addStretch(1);
    auto *yes = new fm::ui::Button(tr("예(&Y)"), footer);
    yes->setObjectName(u"yesButton"_s);
    yes->setRole(fm::ui::Button::Primary);
    yes->setDefault(true);
    auto *no = new fm::ui::Button(tr("아니요(&N)"), footer);
    no->setObjectName(u"noButton"_s);
    footerLayout->addWidget(yes);
    footerLayout->addWidget(no);
    root->addWidget(footer);
    connect(yes, &QPushButton::clicked, this, [this] { respond(Yes); });
    connect(no, &QPushButton::clicked, this, [this] { respond(No); });

    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &UacSimulationDialog::tick);
    m_timer->start();
    m_countdown->setText(countdownText(m_left));
    root->activate();
    setFixedSize(460, std::max(260, root->totalHeightForWidth(460)));
}

QString UacSimulationDialog::countdownText(int seconds)
{
    return tr("응답이 없으면 %1초 뒤 시간 초과로 처리합니다(설정 › 관리자 권한 › UAC 응답 기다리기).").arg(seconds);
}

void UacSimulationDialog::tick()
{
    if (--m_left <= 0) {
        respond(Timeout);
        return;
    }
    m_countdown->setText(countdownText(m_left));
}

void UacSimulationDialog::respond(Answer answer)
{
    if (m_done)
        return;
    m_done = true;
    m_timer->stop();
    Q_EMIT answered(answer);
    if (answer == Yes)
        accept();
    else
        QDialog::reject();
}

void UacSimulationDialog::reject()
{
    respond(No);  // Esc · 닫기 = 아니요
}

// ---------------------------------------------------------------------------------------------
// ElevationFlow

ElevationFlow::ElevationFlow(Scenario scenario, QWidget *window)
    : QObject(window)
    , m_scenario(scenario)
    , m_window(window)
{
    m_items = {
        {{u"vc_redist.x64.exe"_s, false, qint64(24.4 * MB), fm::style::Token::KExe}, kCopySource},
        {{u"FMToolsShellExt.dll"_s, false, qint64(3.1 * MB), fm::style::Token::KExe}, kCopySource},
        {{u"redist-manifest.json"_s, false, 12 * 1024, fm::style::Token::KCode}, kCopySource},
    };
}

ElevationFlow::~ElevationFlow() = default;

void ElevationFlow::start()
{
    m_log.clear();
    m_finished = false;
    m_copied = 0;
    m_skipped = 0;
    if (m_scenario == CopyToProtected) {
        note(tr("시작: %1개 항목을 %2로 복사").arg(m_items.size()).arg(kCopyTarget));
        askCopy(0);
    } else {
        note(tr("시작: %1의 42개 항목 삭제(작업 전 사전 확인 켬)").arg(kDeleteSource));
        startDeleteFlow();
    }
}

void ElevationFlow::note(const QString &step)
{
    m_log.append(step);
    Q_EMIT stepChanged(step);
}

void ElevationFlow::finish(const QString &summary)
{
    m_finished = true;
    m_current = nullptr;
    note(tr("끝: %1").arg(summary));
    Q_EMIT finished(summary);
}

// 이전 창이 닫힌 뒤 다음 단계를 연다(결정 신호는 창이 닫히기 전에 온다).
void ElevationFlow::later(std::function<void()> step)
{
    QTimer::singleShot(0, this, std::move(step));
}

// ---------------------------------------------------------------- 복사 → 보호된 대상

void ElevationFlow::askCopy(int index)
{
    if (index >= m_items.size()) {
        finish(tr("복사 %1개 · 건너뜀 %2개").arg(m_copied).arg(m_skipped));
        return;
    }
    const Item &item = m_items.at(index);
    const int remaining = int(m_items.size()) - 1 - index;
    note(tr("거부: %1 — 대상 폴더에 쓸 권한 없음").arg(item.info.name));
    m_current = ElevationDialog::ask(m_window, elev::prompts::copyDenied(item.info, kCopyTarget, remaining, true),
                                     [this, index, remaining](const elev::Result &r) {
        note(tr("선택: %1%2").arg(elev::choiceName(r.choice), r.checked ? tr(" · 남은 항목에도 적용") : QString()));
        switch (r.choice) {
        case Choice::Elevate:
            requestUac([this, index] { copyElevated(index); }, [this, index] { askCopyFailed(index); });
            break;
        case Choice::Skip:
            ++m_skipped;
            if (r.checked) {
                m_skipped += remaining;
                later([this] { finish(tr("복사 %1개 · 건너뜀 %2개").arg(m_copied).arg(m_skipped)); });
            } else {
                later([this, index] { askCopy(index + 1); });
            }
            break;
        default:
            later([this] { finish(tr("작업 취소")); });
            break;
        }
    });
}

void ElevationFlow::askCopyFailed(int index)
{
    const Item &item = m_items.at(index);
    const int remaining = int(m_items.size()) - 1 - index;
    m_current = ElevationDialog::ask(m_window, elev::prompts::elevationFailed(Operation::Copy, item.info, kCopyTarget),
                                     [this, index, remaining](const elev::Result &r) {
        note(tr("선택: %1%2").arg(elev::choiceName(r.choice), r.checked ? tr(" · 남은 항목 건너뛰기") : QString()));
        switch (r.choice) {
        case Choice::Retry:
            requestUac([this, index] { copyElevated(index); }, [this, index] { askCopyFailed(index); });
            break;
        case Choice::Skip:
            ++m_skipped;
            if (r.checked) {
                m_skipped += remaining;
                later([this] { finish(tr("복사 %1개 · 건너뜀 %2개").arg(m_copied).arg(m_skipped)); });
            } else {
                later([this, index] { askCopy(index + 1); });
            }
            break;
        default:
            later([this] { finish(tr("작업 취소")); });
            break;
        }
    });
}

// 승인 → 권한 상승 도우미가 남은 항목을 처리한다(진행 창 위 관리자 배너 — 03 §0 3단계)
void ElevationFlow::copyElevated(int index)
{
    QStringList names;
    QList<qint64> sizes;
    for (int i = index; i < m_items.size(); ++i) {
        names.append(m_items.at(i).info.name);
        sizes.append(std::max<qint64>(1, m_items.at(i).info.size));
    }
    note(tr("관리자 권한으로 %1개 복사 — 진행 창에 관리자 배너").arg(names.size()));
    showProgress(true, ProgressDialog::Copy, names, sizes);
    m_copied += int(names.size());
    finish(tr("복사 %1개(관리자) · 건너뜀 %2개").arg(m_copied).arg(m_skipped));
}

void ElevationFlow::requestUac(std::function<void()> approved, std::function<void()> denied)
{
    later([this, approved = std::move(approved), denied = std::move(denied)] {
        note(tr("UAC 확인 창(시뮬레이션)"));
        auto *dialog = new UacSimulationDialog(tr("FM Tools 권한 상승 도우미 — 확인된 게시자: FM Tools"), m_uacTimeout, m_window);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowModality(m_window ? Qt::WindowModal : Qt::ApplicationModal);
        connect(dialog, &UacSimulationDialog::answered, this, [this, approved, denied](UacSimulationDialog::Answer a) {
            note(a == UacSimulationDialog::Yes  ? tr("UAC: 예")
                 : a == UacSimulationDialog::No ? tr("UAC: 아니요")
                                                : tr("UAC: 시간 초과"));
            later(a == UacSimulationDialog::Yes ? approved : denied);
        });
        m_current = dialog;
        dialog->open();
    });
}

void ElevationFlow::showProgress(bool elevated, int kind, const QStringList &names, const QList<qint64> &sizes)
{
    ProgressDialog::Operation op;
    op.kind = ProgressDialog::Kind(kind);
    op.elevated = elevated;
    op.source = kind == ProgressDialog::Copy ? kCopySource : kDeleteSource;
    op.target = kind == ProgressDialog::Copy ? kCopyTarget.chopped(1) : QString();
    op.fileNames = names;
    op.fileSizes = sizes;
    op.policyText = tr("같은 이름이 있으면 매번 묻기");
    auto *progress = new ProgressDialog(op, m_window);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->show();
}

// ---------------------------------------------------------------- 삭제 → 소유권

void ElevationFlow::startDeleteFlow()
{
    note(tr("사전 확인: 42개 중 3개는 관리자 권한 필요"));
    m_current = ElevationDialog::ask(m_window, elev::prompts::board(u"preflight"_s), [this](const elev::Result &r) {
        note(tr("선택: %1").arg(elev::choiceName(r.choice)));
        auto skipNeedingAdmin = [this] {
            QStringList names;
            QList<qint64> sizes;
            for (int i = 1; i <= 39; ++i) {
                names.append(u"obj\\unit_%1.obj"_s.arg(i, 2, 10, QChar(u'0')));
                sizes.append(6 * MB);
            }
            showProgress(false, ProgressDialog::Delete, names, sizes);
            finish(tr("권한이 필요한 3개를 건너뛰고 39개 삭제"));
        };
        switch (r.choice) {
        case Choice::Elevate:
            requestUac([this] { deleteElevated(); },
                       [this, skipNeedingAdmin] {
                           const elev::ItemInfo item{u"fm.exe"_s, false, qint64(8.2 * MB), fm::style::Token::KExe};
                           m_current = ElevationDialog::ask(
                               m_window,
                               elev::prompts::elevationFailed(Operation::Delete, item,
                                                              kDeleteSource + u"\\msi-staging\\FM Tools\\"_s),
                               [this, skipNeedingAdmin](const elev::Result &f) {
                                   note(tr("선택: %1").arg(elev::choiceName(f.choice)));
                                   if (f.choice == Choice::Retry)
                                       requestUac([this] { deleteElevated(); }, [this] { finish(tr("UAC 거부 — 작업 중단")); });
                                   else if (f.choice == Choice::Skip)
                                       later(skipNeedingAdmin);
                                   else
                                       later([this] { finish(tr("작업 취소")); });
                               });
                       });
            break;
        case Choice::SkipNeedingAdmin:
            later(skipNeedingAdmin);
            break;
        default:
            later([this] { finish(tr("작업 취소")); });
            break;
        }
    });
}

// 도우미가 처리하던 중 TrustedInstaller 소유 폴더에서 다시 거부된다 → 소유권 창(기본 단추 = 건너뛰기)
void ElevationFlow::deleteElevated()
{
    note(tr("관리자 권한으로 삭제 중 — WindowsApps 폴더에서 관리자도 거부"));
    m_current = ElevationDialog::ask(m_window, elev::prompts::board(u"ownership"_s), [this](const elev::Result &r) {
        note(tr("선택: %1").arg(elev::choiceName(r.choice)));
        if (r.choice == Choice::Cancel) {
            later([this] { finish(tr("작업 취소")); });
            return;
        }
        const bool take = r.choice == Choice::TakeOwnership;
        if (take)
            note(tr("소유권 가져옴 — 바꾸기 전 원래 권한 저장"));
        later([this, take] {
            QStringList names;
            QList<qint64> sizes;
            const int count = take ? 42 : 41;
            for (int i = 1; i <= count; ++i) {
                names.append(u"obj\\unit_%1.obj"_s.arg(i, 2, 10, QChar(u'0')));
                sizes.append(6 * MB);
            }
            showProgress(true, ProgressDialog::Delete, names, sizes);
            finish(take ? tr("관리자 권한으로 42개 삭제 · 소유권 1개 변경") : tr("관리자 권한으로 41개 삭제 · 1개 건너뜀"));
        });
    });
}

} // namespace fm::dialogs
