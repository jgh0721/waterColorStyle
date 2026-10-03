#include "ThumbnailCompare.h"

#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmfilelist/LocalFileSource.h>
#include <fmfilelist/MockFileSource.h>
#include <fmfilelist/ThumbnailView.h>
#include <fmstyle/StyleProps.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/Card.h>
#include <fmwidgets/Label.h>
#include <fmwidgets/SegmentedControl.h>

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

using namespace Qt::StringLiterals;

namespace fm::app {

namespace {

namespace fl = fm::filelist;
namespace fs = fm::style;

/// 프로세스 전용 바이트(커밋된 개인 메모리) · 작업 집합.
std::pair<qint64, qint64> processMemory()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc), sizeof(pmc)))
        return {qint64(pmc.PrivateUsage), qint64(pmc.WorkingSetSize)};
#endif
    return {-1, -1};
}

QString megabytes(qint64 bytes)
{
    if (bytes < 0)
        return u"—"_s;
    return u"%1 MB"_s.arg(double(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
}

QString millis(double ms)
{
    return ms < 0 ? u"—"_s : u"%1 ms"_s.arg(ms, 0, 'f', ms < 10 ? 2 : 1);
}

/// 모의 부하 — 사진 · 영상 · PDF가 섞인 큰 폴더. loading이면 그림 있는 항목은 "만드는 중"으로 시작한다.
QList<fl::FileEntry> mockEntries(int count, bool loading)
{
    struct Pattern
    {
        const char *stem;
        const char *ext;
        const char *type;
        fl::Kind kind;
        fl::Art art;
        qreal aspect;
        const char *badge;
        qint64 size;
    };
    constexpr qint64 MB = 1024 * 1024;
    static const Pattern patterns[] = {
        {"IMG_", "heic", "HEIC 파일", fl::Kind::Img, fl::Art::Photo, 1.33, "HEIC", 3 * MB},
        {"Screenshot_", "png", "PNG 이미지", fl::Kind::Img, fl::Art::Shot, 1.6, "PNG", 2 * MB},
        {"Recording_", "mkv", "MKV 동영상", fl::Kind::Img, fl::Art::Video, 1.78, "", 900 * MB},
        {"Report_", "pdf", "PDF 문서", fl::Kind::Pdf, fl::Art::Pdf, 0.77, "PDF", 12 * MB},
        {"DSC", "arw", "ARW 파일", fl::Kind::Img, fl::Art::Photo, 1.5, "ARW", 24 * MB},
        {"setup_", "exe", "응용 프로그램", fl::Kind::Exe, fl::Art::None, 0, "", 180 * MB},
        {"archive_", "7z", "7Z 압축 파일", fl::Kind::Zip, fl::Art::None, 0, "", 80 * MB},
        {"notes_", "txt", "텍스트 문서", fl::Kind::Doc, fl::Art::None, 0, "", 4096},
    };
    const QDateTime base(QDate(2026, 9, 30), QTime(18, 0));
    QList<fl::FileEntry> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        fl::FileEntry e;
        if (i % 40 == 0) {
            e.stem = u"Folder_%1"_s.arg(i / 40, 4, 10, u'0');
            e.kind = fl::Kind::Folder;
            e.typeName = u"파일 폴더"_s;
        } else {
            const Pattern &p = patterns[i % std::size(patterns)];
            e.stem = QString::fromLatin1(p.stem) + u"%1"_s.arg(i, 6, 10, u'0');
            e.ext = QString::fromLatin1(p.ext);
            e.typeName = QString::fromUtf8(p.type);
            e.kind = p.kind;
            e.size = p.size + (qint64(i) * 7919) % (p.size / 4 + 1);
            e.art = loading && p.art != fl::Art::None ? fl::Art::Loading : p.art;
            e.aspect = p.aspect;
            e.badge = p.art == fl::Art::Video ? u"%1:%2"_s.arg(i % 60).arg(i % 59, 2, 10, u'0') : QString::fromLatin1(p.badge);
        }
        e.modified = base.addSecs(-qint64(i) * 97);
        e.attributes = fl::Archive;
        e.path = u"D:\\Mock\\"_s + e.fullName();
        out.append(e);
    }
    return out;
}

} // namespace

ThumbnailCompare::ThumbnailCompare(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(u"섬네일 비교 — Qt 목록 · Qtitan 카드"_s);
    setObjectName(u"ThumbnailCompare"_s);
    fs::setDensity(this, fs::Density::Dialog);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(10);

    // 도구 줄 — 원본 · 도착 흉내 · 크기 · 스크롤 측정 · 메모리
    auto *bar = new QHBoxLayout();
    bar->setSpacing(10);
    m_source = new QComboBox(this);
    m_source->addItems({u"샘플 — 사진 폴더 9개"_s, u"모의 부하 — 1만 개"_s, u"모의 부하 — 10만 개"_s, u"실제 폴더(읽기 전용)…"_s});
    m_source->setAccessibleName(u"원본"_s);
    m_arrival = new QCheckBox(u"섬네일 도착 흉내(&A)"_s, this);
    m_arrival->setToolTip(u"모의 부하: 그림이 \"만드는 중\"으로 시작해 위에서부터 차례로 도착한다(두 구현이 같은 dataChanged를 받는다)"_s);
    m_arrival->setChecked(true);
    m_size = new fm::ui::SegmentedControl(this);
    m_size->setItems({u"64"_s, u"96"_s, u"160"_s, u"256"_s});
    m_size->setCurrentIndex(1);
    m_size->setAccessibleName(u"섬네일 크기"_s);
    m_measure = new fm::ui::Button(u"스크롤 측정(&M)"_s, this);
    m_measure->setRole(fm::ui::Button::Primary);
    m_memory = new fm::ui::Label(QString(), fm::ui::Label::Summary, this);
    bar->addWidget(new fm::ui::Label(u"원본"_s, fm::ui::Label::Meta, this));
    bar->addWidget(m_source);
    bar->addWidget(m_arrival);
    bar->addSpacing(8);
    bar->addWidget(new fm::ui::Label(u"크기"_s, fm::ui::Label::Meta, this));
    bar->addWidget(m_size);
    bar->addSpacing(8);
    bar->addWidget(m_measure);
    bar->addStretch(1);
    bar->addWidget(m_memory);
    root->addLayout(bar);

    // 두 구현
    auto *views = new QHBoxLayout();
    views->setSpacing(12);
    const QString titles[] = {u"Qt 목록 — QListView IconMode + 델리게이트"_s, u"Qtitan 카드 — CardGrid + 카드 그리기 훅(Q7)"_s};
    for (int b = 0; b < 2; ++b) {
        auto *col = new QVBoxLayout();
        col->setSpacing(6);
        col->addWidget(new fm::ui::Label(titles[b], fm::ui::Label::SectionTitle, this));
        auto *card = new fm::ui::Card(this);
        m_columns[std::size_t(b)] = new QVBoxLayout(card);
        m_columns[std::size_t(b)]->setContentsMargins(1, 1, 1, 1);
        col->addWidget(card, 1);
        views->addLayout(col, 1);
    }
#if !FM_WITH_QTITAN
    auto *noCards = new QLabel(u"QtitanDataGrid 없이 빌드해 카드 구현이 없습니다(FMSTYLE_WITH_QTITAN=OFF)."_s, this);
    noCards->setAlignment(Qt::AlignCenter);
    noCards->setWordWrap(true);
    m_columns[1]->addWidget(noCards);
#endif
    root->addLayout(views, 1);

    // 결과 표
    auto *bottom = new QHBoxLayout();
    bottom->setSpacing(12);
    m_table = new QStandardItemModel(0, 3, this);
    m_table->setHorizontalHeaderLabels({u"측정"_s, u"Qt 목록"_s, u"Qtitan 카드"_s});
    for (int c = 1; c < 3; ++c)   // 값 열은 숫자와 같이 오른쪽 정렬
        m_table->horizontalHeaderItem(c)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto *table = new QTreeView(this);
    table->setObjectName(u"compareResults"_s);
    table->setModel(m_table);
    table->setRootIsDecorated(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setFixedHeight(28 + 5 * 26 + 4);
    table->header()->resizeSection(0, 220);
    table->header()->resizeSection(1, 160);
    fs::setFlatHeader(table);
    m_status = new fm::ui::Label(QString(), fm::ui::Label::Help, this);
    m_status->setWordWrap(true);
    bottom->addWidget(table, 3);
    bottom->addWidget(m_status, 2);
    root->addLayout(bottom);

    m_arrivalTimer = new QTimer(this);
    m_arrivalTimer->setInterval(16);
    connect(m_arrivalTimer, &QTimer::timeout, this, &ThumbnailCompare::arrivalTick);
    m_memoryTimer = new QTimer(this);
    m_memoryTimer->setInterval(1000);
    connect(m_memoryTimer, &QTimer::timeout, this, &ThumbnailCompare::refreshMemory);
    m_memoryTimer->start();

    connect(m_source, &QComboBox::activated, this, [this](int index) {
        if (index == 3) {
            const QString dir = QFileDialog::getExistingDirectory(this, u"비교할 폴더(읽기 전용)"_s, QDir::homePath());
            if (dir.isEmpty()) {
                m_source->setCurrentIndex(int(m_current));
                return;
            }
            load(Source::Folder, dir);
            return;
        }
        load(Source(index));
    });
    connect(m_size, &fm::ui::SegmentedControl::currentIndexChanged, this, [this] { applySize(); });
    connect(m_measure, &QPushButton::clicked, this, &ThumbnailCompare::measureScroll);

    resize(1440, 900);
    refreshTable();
    refreshMemory();
}

ThumbnailCompare::~ThumbnailCompare()
{
    // 그리드가 모델보다 먼저 사라져야 한다(Qtitan DelegateAdapter — PATCHES.md)
    for (fl::ThumbnailBackend *v : m_views)
        delete (v ? v->widget() : nullptr);
}

void ThumbnailCompare::load(Source source, const QString &path)
{
    m_arrivalTimer->stop();
    m_current = source;
    {
        const QSignalBlocker block(m_source);
        m_source->setCurrentIndex(int(source));
    }
    // 뷰 → 프록시 → 원본 순서로 지운다
    for (fl::ThumbnailBackend *&v : m_views) {
        delete (v ? v->widget() : nullptr);
        v = nullptr;
    }
    delete m_proxy;
    m_proxy = nullptr;
    delete m_mock;
    m_mock = nullptr;
    delete m_local;
    m_local = nullptr;
    m_final.clear();
    m_results = {};
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    const qint64 before = processMemory().first;
    m_proxy = new fl::FileSortProxy(this);
    if (source == Source::Folder) {
        m_local = new fl::LocalFileSource(this);
        m_local->setShowHidden(false);
        m_proxy->setSourceModel(m_local->model());
        m_proxy->sort(fl::NameColumn, Qt::AscendingOrder);
        connect(m_local->model(), &fl::FileSystemListProxy::loaded, this, [this, before] {
            if (m_views[0])
                return;  // 한 번만(이후의 갱신은 뷰가 받는다)
            m_modelBytes = processMemory().first - before;
            rebuildViews();
            attachMeasured();
            Q_EMIT loaded();
        });
        m_local->setPath(path);
        m_status->setText(u"%1 읽는 중…"_s.arg(QDir::toNativeSeparators(path)));
        return;
    }
    const bool mock = source == Source::Mock10k || source == Source::Mock100k;
    if (mock) {
        const int count = source == Source::Mock10k ? 10000 : 100000;
        m_final = mockEntries(count, false);
        m_mock = new fl::FileListModel(m_arrival->isChecked() ? mockEntries(count, true) : m_final, this);
    } else {
        m_mock = new fl::FileListModel(fl::MockFileSource::thumbnailPreview().entries, this);
    }
    m_proxy->setSourceModel(m_mock);
    m_proxy->sort(-1);  // 원본 순서 그대로
    m_modelBytes = processMemory().first - before;
    rebuildViews();
    attachMeasured();
    if (mock && m_arrival->isChecked())
        startArrival();
    Q_EMIT loaded();
}

void ThumbnailCompare::rebuildViews()
{
    auto *list = new fl::ThumbnailListView(this);
#if FM_WITH_QTITAN
    auto *cards = new fl::ThumbnailCardView(this);
    m_views = {list, cards};
#else
    m_views = {list, nullptr};  // 카드 칸은 안내 문구(생성자)
#endif
    for (int b = 0; b < 2; ++b) {
        if (!m_views[std::size_t(b)])
            continue;
        QWidget *w = m_views[std::size_t(b)]->widget();
        fs::setPaneActive(w, true);
        m_columns[std::size_t(b)]->addWidget(w);
    }
    applySize();
}

void ThumbnailCompare::attachMeasured()
{
    // 같은 모델을 한쪽씩 물려 연결(배치 + 첫 그리기) 시간과 메모리 증가를 잰다
    for (int b = 0; b < 2; ++b) {
        fl::ThumbnailBackend *v = m_views[std::size_t(b)];
        if (!v)
            continue;
        layout()->activate();
        const qint64 before = processMemory().first;
        QElapsedTimer t;
        t.start();
        v->setModel(m_proxy);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        v->scrollArea()->viewport()->repaint();
        m_results[std::size_t(b)].attachMs = double(t.nsecsElapsed()) / 1e6;
        const qint64 after = processMemory().first;
        m_results[std::size_t(b)].memoryBytes = before >= 0 && after >= 0 ? after - before : -1;
        v->setCursorRow(0);
    }
    refreshTable();
}

void ThumbnailCompare::startArrival()
{
    m_arrived = 0;
    m_arrivalTimer->start();
}

void ThumbnailCompare::arrivalTick()
{
    if (!m_mock || m_arrived >= m_final.size()) {
        m_arrivalTimer->stop();
        refreshTable();
        return;
    }
    constexpr int kBatch = 400;
    m_mock->replaceEntries(m_arrived, m_final.mid(m_arrived, kBatch));
    m_arrived = std::min<int>(m_arrived + kBatch, int(m_final.size()));
    refreshTable();
}

void ThumbnailCompare::measureScroll()
{
    // 위에서 아래까지 40단계 — 단계마다 값 바꾸기 + 미뤄진 배치 처리 + 동기 다시 그리기
    constexpr int kSteps = 40;
    for (int b = 0; b < 2; ++b) {
        fl::ThumbnailBackend *v = m_views[std::size_t(b)];
        Result &r = m_results[std::size_t(b)];
        if (!v)
            continue;
        QAbstractScrollArea *area = v->scrollArea();
        QScrollBar *sb = area->verticalScrollBar();
        sb->setValue(0);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        const int maximum = sb->maximum();
        double total = 0, worst = 0;
        int steps = 0;
        for (int i = 1; i <= kSteps && maximum > 0; ++i) {
            QElapsedTimer t;
            t.start();
            sb->setValue(int(qint64(maximum) * i / kSteps));
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            area->viewport()->repaint();
            const double ms = double(t.nsecsElapsed()) / 1e6;
            total += ms;
            worst = std::max(worst, ms);
            ++steps;
        }
        r.scrollSteps = steps;
        r.scrollAvgMs = steps ? total / steps : -1;
        r.scrollMaxMs = steps ? worst : -1;
        // 맨 위로 되돌리기 — Qtitan은 보이는 행 배치를 이벤트 루프에서 하므로 바로 찍으면 맨 아래가 남는다
        sb->setValue(0);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        area->viewport()->repaint();
    }
    refreshTable();
}

void ThumbnailCompare::applySize()
{
    static const int sizes[] = {64, 96, 160, 256};
    fl::ThumbnailAppearance a;
    a.size = sizes[std::clamp(m_size->currentIndex(), 0, 3)];
    for (fl::ThumbnailBackend *v : m_views) {
        if (v)
            v->setAppearance(a);
    }
}

void ThumbnailCompare::refreshTable()
{
    m_table->removeRows(0, m_table->rowCount());
    auto row = [this](const QString &name, auto value) {
        QList<QStandardItem *> items{new QStandardItem(name)};
        for (int b = 0; b < 2; ++b) {
            auto *item = new QStandardItem(value(m_results[std::size_t(b)]));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            items.append(item);
        }
        m_table->appendRow(items);
    };
    row(u"모델 연결(배치 + 첫 그리기)"_s, [](const Result &r) { return millis(r.attachMs); });
    row(u"메모리 증가(연결 직후)"_s, [](const Result &r) { return megabytes(r.memoryBytes); });
    row(u"스크롤 평균(단계당)"_s, [](const Result &r) { return millis(r.scrollAvgMs); });
    row(u"스크롤 최대(단계)"_s, [](const Result &r) { return millis(r.scrollMaxMs); });
    row(u"스크롤 단계"_s, [](const Result &r) { return r.scrollSteps ? QString::number(r.scrollSteps) : u"—"_s; });

    const int count = m_proxy ? m_proxy->rowCount() : 0;
    QString status = u"항목 %1개 · 모델(두 구현 공통) %2"_s.arg(count).arg(megabytes(m_modelBytes));
    if (!m_final.isEmpty() && m_arrival->isChecked())
        status += u" · 섬네일 도착 %1 / %2"_s.arg(m_arrived).arg(m_final.size());
    status += u"\n측정은 같은 프로세스 안에서 한 번씩 잰 값이라 참고용이다. 스크롤은 위 → 아래 40단계, 단계마다 동기 다시 그리기 시간."_s;
    m_status->setText(status);
}

void ThumbnailCompare::refreshMemory()
{
    const auto [privateBytes, workingSet] = processMemory();
    m_memory->setText(u"프로세스 전용 %1 · 작업 집합 %2"_s.arg(megabytes(privateBytes), megabytes(workingSet)));
}

void ThumbnailCompare::setArrivalSimulation(bool on)
{
    m_arrival->setChecked(on);
}

QString ThumbnailCompare::lastReport() const
{
    QString out = u"원본 %1 · 항목 %2개 · 모델 %3\n"_s.arg(m_source->currentText()).arg(m_proxy ? m_proxy->rowCount() : 0).arg(megabytes(m_modelBytes));
    const char *names[] = {"Qt 목록", "Qtitan 카드"};
    for (int b = 0; b < 2; ++b) {
        const Result &r = m_results[std::size_t(b)];
        out += u"%1: 연결 %2 · 메모리 %3 · 스크롤 평균 %4 · 최대 %5 (%6단계)\n"_s.arg(QString::fromUtf8(names[b]), millis(r.attachMs),
                                                                                megabytes(r.memoryBytes), millis(r.scrollAvgMs),
                                                                                millis(r.scrollMaxMs))
                   .arg(r.scrollSteps);
    }
    return out;
}

} // namespace fm::app
