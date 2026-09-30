// FmStyle 갤러리 — 라이트와 다크를 나란히 놓고 컨트롤 상태를 한 화면에 보여 준다.
//   fmstyle_gallery                       창으로 실행 (위쪽 단추로 시안1 · 시안2 전환)
//   fmstyle_gallery --design watercolor   시안2(워터컬러)로 시작
//   fmstyle_gallery --shot gallery.png    스크린샷을 저장하고 끝냄 (QT_QPA_PLATFORM=offscreen 가능)
//   fmstyle_gallery --accent "#0F7A6E"    강조색을 바꿔 파생 규칙 확인

#include <fmstyle/FmStyle.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/TransferGraph.h>

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCommandLineParser>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRadioButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QStylePainter>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cmath>
#include <numbers>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace {

// 도구 모음 아이콘 — 목업의 SVG 경로를 QPainter로 옮긴 간단한 모양.
QIcon glyph(const QString &kind, const QColor &color)
{
    const qreal dpr = 2.0;
    QPixmap pm(QSize(16, 16) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.4);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    QPainterPath path;
    if (kind == u"back") {
        path.moveTo(10, 3); path.lineTo(5, 8); path.lineTo(10, 13);
    } else if (kind == u"forward") {
        path.moveTo(6, 3); path.lineTo(11, 8); path.lineTo(6, 13);
    } else if (kind == u"up") {
        path.moveTo(8, 13); path.lineTo(8, 3.5); path.moveTo(4.5, 7); path.lineTo(8, 3.5); path.lineTo(11.5, 7);
    } else if (kind == u"copy") {
        path.addRoundedRect(QRectF(5, 5, 8.5, 8.5), 1.5, 1.5);
        path.moveTo(11, 5); path.lineTo(11, 3.5); path.lineTo(3.5, 3.5); path.lineTo(3.5, 11); path.lineTo(5, 11);
    } else if (kind == u"trash") {
        path.moveTo(2.5, 4); path.lineTo(13.5, 4); path.moveTo(6, 4); path.lineTo(6, 2.5); path.lineTo(10, 2.5);
        path.lineTo(10, 4); path.moveTo(4, 4); path.lineTo(4.7, 13.5); path.lineTo(11.3, 13.5); path.lineTo(12, 4);
    }
    p.drawPath(path);
    return QIcon(pm);
}

QIcon folderIcon(const QColor &color)
{
    const qreal dpr = 2.0;
    QPixmap pm(QSize(16, 16) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.moveTo(1.5, 4.2);
    path.quadTo(1.5, 2.6, 3.1, 2.6);
    path.lineTo(6.3, 2.6);
    path.lineTo(7.9, 4.2);
    path.lineTo(12.9, 4.2);
    path.quadTo(14.5, 4.2, 14.5, 5.8);
    path.lineTo(14.5, 12.2);
    path.quadTo(14.5, 13.8, 12.9, 13.8);
    path.lineTo(3.1, 13.8);
    path.quadTo(1.5, 13.8, 1.5, 12.2);
    path.closeSubpath();
    p.fillPath(path, color);
    return QIcon(pm);
}

QIcon fileIcon(const QColor &line, const QColor &band)
{
    const qreal dpr = 2.0;
    QPixmap pm(QSize(16, 16) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(line, 1.2));
    QPainterPath path;
    path.moveTo(3.5, 1.5); path.lineTo(9.1, 1.5); path.lineTo(12.5, 4.9); path.lineTo(12.5, 14.5);
    path.lineTo(3.5, 14.5); path.closeSubpath();
    p.drawPath(path);
    p.fillRect(QRectF(5, 8.6, 6, 3.6), band);
    return QIcon(pm);
}

QLabel *caption(const QString &text, const fs::ThemeColors &tc, int px = 12)
{
    auto *l = new QLabel(text);
    QFont f = l->font();
    f.setPixelSize(px);
    l->setFont(f);
    QPalette pal = l->palette();
    pal.setColor(QPalette::WindowText, tc[fs::Token::Fg3]);
    l->setPalette(pal);
    return l;
}

QGroupBox *section(const QString &title)
{
    auto *box = new QGroupBox(title);
    return box;
}

QPushButton *button(const QString &text, fs::ButtonRole role, const QString &preview = {}, bool enabled = true)
{
    auto *b = new QPushButton(text);
    fs::setButtonRole(b, role);
    if (!preview.isEmpty())
        fs::setPreviewState(b, preview);
    b->setEnabled(enabled);
    return b;
}

// 버튼 역할 × 상태 표
QWidget *buttonsSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"버튼"_s);
    auto *grid = new QGridLayout(box);
    grid->setContentsMargins(14, 12, 14, 14);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    const QStringList states{u"보통"_s, u"마우스 올림"_s, u"누름"_s, u"키보드 포커스"_s, u"사용 안 함"_s};
    for (int c = 0; c < states.size(); ++c)
        grid->addWidget(caption(states[c], tc), 0, c + 1);
    struct Row { QString name; QString text; fs::ButtonRole role; };
    const Row rows[] = {
        {u"보조"_s, u"취소"_s, fs::ButtonRole::Normal},
        {u"기본"_s, u"복사"_s, fs::ButtonRole::Primary},
        {u"위험"_s, u"영구 삭제"_s, fs::ButtonRole::Danger},
    };
    int r = 1;
    for (const Row &row : rows) {
        grid->addWidget(caption(row.name, tc), r, 0);
        grid->addWidget(button(row.text, row.role), r, 1);
        grid->addWidget(button(row.text, row.role, u"hover"_s), r, 2);
        grid->addWidget(button(row.text, row.role, u"pressed"_s), r, 3);
        grid->addWidget(button(row.text, row.role, u"focus"_s), r, 4);
        grid->addWidget(button(row.text, row.role, {}, false), r, 5);
        ++r;
    }

    // 작은 버튼 · 도구 버튼
    grid->addWidget(caption(u"작게 · 도구"_s, tc), r, 0);
    auto *tools = new QHBoxLayout;
    tools->setSpacing(2);
    auto *smallButton = new QPushButton(u"캐시 비우기"_s);
    fs::setSmall(smallButton);
    tools->addWidget(smallButton);
    tools->addSpacing(12);
    const QColor iconColor = tc[fs::Token::Fg2];
    const QStringList kinds{u"back"_s, u"forward"_s, u"up"_s, u"copy"_s, u"trash"_s};
    for (int i = 0; i < kinds.size(); ++i) {
        auto *t = new QToolButton;
        t->setAutoRaise(true);
        t->setIcon(glyph(kinds[i], iconColor));
        if (i == 1)
            fs::setPreviewState(t, u"hover"_s);
        if (i == 2)
            fs::setPreviewState(t, u"pressed"_s);
        tools->addWidget(t);
    }
    tools->addStretch(1);
    grid->addLayout(tools, r, 1, 1, 5);
    grid->setColumnStretch(6, 1);
    return box;
}

QWidget *inputsSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"입력"_s);
    auto *grid = new QGridLayout(box);
    grid->setContentsMargins(14, 12, 14, 14);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);

    auto *empty = new QLineEdit;
    empty->setPlaceholderText(u"새 이름"_s);
    auto *focused = new QLineEdit(u"Qt-6.11.0-offline-installer.exe"_s);
    fs::setPreviewState(focused, u"focus"_s);
    auto *disabled = new QLineEdit(u"C:\\Windows\\System32"_s);
    disabled->setEnabled(false);
    grid->addWidget(caption(u"보통"_s, tc), 0, 0);
    grid->addWidget(caption(u"포커스"_s, tc), 0, 1);
    grid->addWidget(caption(u"사용 안 함"_s, tc), 0, 2);
    grid->addWidget(empty, 1, 0);
    grid->addWidget(focused, 1, 1);
    grid->addWidget(disabled, 1, 2);

    auto *combo = new QComboBox;
    combo->addItems({u"가운데 줄임 · 확장자 유지"_s, u"끝을 줄임"_s, u"두 줄로 감쌈"_s});
    auto *editable = new QComboBox;
    editable->setEditable(true);
    editable->addItems({u"yyyy-MM-dd HH:mm"_s, u"yy-MM-dd"_s});
    auto *spin = new QSpinBox;
    spin->setRange(1, 16);
    spin->setValue(4);
    spin->setSuffix(u" 개"_s);
    fs::setPreviewState(spin, u"hover"_s);
    grid->addWidget(caption(u"콤보 상자"_s, tc), 2, 0);
    grid->addWidget(caption(u"편집 가능한 콤보"_s, tc), 2, 1);
    grid->addWidget(caption(u"스핀 상자 (마우스 올림)"_s, tc), 2, 2);
    grid->addWidget(combo, 3, 0);
    grid->addWidget(editable, 3, 1);
    grid->addWidget(spin, 3, 2);
    for (int c = 0; c < 3; ++c)
        grid->setColumnStretch(c, 1);
    return box;
}

QWidget *choicesSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"선택"_s);
    auto *v = new QVBoxLayout(box);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(10);

    auto *row1 = new QHBoxLayout;
    row1->setSpacing(18);
    auto *c1 = new QCheckBox(u"끔"_s);
    auto *c2 = new QCheckBox(u"켬"_s);
    c2->setChecked(true);
    auto *c3 = new QCheckBox(u"일부"_s);
    c3->setTristate(true);
    c3->setCheckState(Qt::PartiallyChecked);
    auto *c4 = new QCheckBox(u"마우스 올림"_s);
    fs::setPreviewState(c4, u"hover"_s);
    auto *c5 = new QCheckBox(u"사용 안 함"_s);
    c5->setChecked(true);
    c5->setEnabled(false);
    auto *r1 = new QRadioButton(u"위 그룹 하나만"_s);
    auto *r2 = new QRadioButton(u"속성별로 합치기"_s);
    r2->setChecked(true);
    for (QWidget *w : {static_cast<QWidget *>(c1), static_cast<QWidget *>(c2), static_cast<QWidget *>(c3),
                       static_cast<QWidget *>(c4), static_cast<QWidget *>(c5), static_cast<QWidget *>(r1),
                       static_cast<QWidget *>(r2)})
        row1->addWidget(w);
    row1->addStretch(1);
    v->addLayout(row1);

    auto *row2 = new QHBoxLayout;
    row2->setSpacing(18);
    const auto makeSwitch = [](const QString &text, bool on, const QString &preview, bool enabled) {
        auto *s = new QCheckBox(text);
        fs::setSwitch(s);
        s->setChecked(on);
        if (!preview.isEmpty())
            fs::setPreviewState(s, preview);
        s->setEnabled(enabled);
        return s;
    };
    row2->addWidget(makeSwitch(u"끔"_s, false, {}, true));
    row2->addWidget(makeSwitch(u"켬"_s, true, {}, true));
    row2->addWidget(makeSwitch(u"켬"_s, true, u"hover"_s, true));
    row2->addWidget(makeSwitch(u"포커스"_s, false, u"focus"_s, true));
    row2->addWidget(makeSwitch(u"켬"_s, true, {}, false));
    row2->addSpacing(8);

    // 세그먼트: 목업의 표시 방식 1줄 · 2줄 · 자동 · 섬네일
    auto *seg = new QHBoxLayout;
    seg->setSpacing(0);
    auto *group = new QButtonGroup(box);
    QList<QAbstractButton *> segments;
    for (const QString &t : {u"1줄"_s, u"2줄"_s, u"자동"_s, u"섬네일"_s}) {
        auto *b = new QPushButton(t);
        segments.append(b);
        group->addButton(b);
        seg->addWidget(b);
    }
    fs::setSegments(segments);
    segments[2]->setChecked(true);
    fs::setPreviewState(segments[1], u"hover"_s);
    row2->addLayout(seg);
    row2->addStretch(1);
    v->addLayout(row2);
    Q_UNUSED(tc)
    return box;
}

QWidget *panePreview(const fs::ThemeColors &tc, bool active)
{
    auto *pane = new QWidget;
    auto *v = new QVBoxLayout(pane);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto *tabs = new QTabBar;
    tabs->setDrawBase(true);
    tabs->setExpanding(false);
    tabs->addTab(active ? u"Downloads"_s : u"fm-core"_s);
    tabs->addTab(active ? u"Backup"_s : u"qtitan-samples"_s);
    tabs->addTab(u"C:\\"_s);
    fs::setPaneActive(tabs, active);
    v->addWidget(tabs);

    auto *tree = new QTreeWidget;
    tree->setRootIsDecorated(false);
    tree->setUniformRowHeights(true);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree->setColumnCount(3);
    tree->setHeaderLabels({u"이름"_s, u"크기"_s, u"수정한 날짜"_s});
    tree->headerItem()->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setStretchLastSection(false);
    tree->header()->resizeSection(1, 72);
    tree->header()->resizeSection(2, 120);
    tree->setSortingEnabled(false);
    tree->header()->setSortIndicatorShown(true);
    tree->header()->setSortIndicator(0, Qt::AscendingOrder);
    tree->setFrameShape(QFrame::NoFrame);
    fs::setPaneActive(tree, active);

    struct Row { QString name; QString size; QString date; bool dir; bool sel; fs::Token kind; };
    const Row rows[] = {
        {u"build"_s, u"폴더"_s, u"2026-09-27 22:40"_s, true, false, fs::Token::Folder},
        {u"src"_s, u"폴더"_s, u"2026-09-28 08:57"_s, true, false, fs::Token::Folder},
        {u"CMakeLists.txt"_s, u"6.8 KB"_s, u"2026-09-27 22:31"_s, false, true, fs::Token::KCode},
        {u"CMakePresets.json"_s, u"3.1 KB"_s, u"2026-09-27 22:31"_s, false, true, fs::Token::KCode},
        {u"README.md"_s, u"8.3 KB"_s, u"2026-09-24 21:15"_s, false, false, fs::Token::KDoc},
        {u"build_release_x64.cmd"_s, u"1.9 KB"_s, u"2026-09-27 22:35"_s, false, false, fs::Token::KExe},
        {u"vc_redist.x64.exe"_s, u"24.4 MB"_s, u"2026-09-22 11:07"_s, false, false, fs::Token::KExe},
        {u"sysinternals-suite.zip"_s, u"51.3 MB"_s, u"2026-09-20 09:33"_s, false, false, fs::Token::KZip},
        {u"Screenshot 2026-09-27.png"_s, u"2.1 MB"_s, u"2026-09-27 23:14"_s, false, false, fs::Token::KImg},
    };
    QTreeWidgetItem *current = nullptr;
    for (const Row &row : rows) {
        auto *it = new QTreeWidgetItem(tree, {row.name, row.size, row.date});
        it->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        it->setForeground(1, tc[fs::Token::Fg2]);
        it->setForeground(2, tc[fs::Token::Fg2]);
        it->setIcon(0, row.dir ? folderIcon(tc[fs::Token::Folder]) : fileIcon(tc[fs::Token::Fg3], tc[row.kind]));
        it->setSelected(row.sel);
        if (row.name == u"README.md")
            current = it;
    }
    tree->setCurrentItem(current, 0, QItemSelectionModel::NoUpdate);
    // 행 7개가 보이는 높이 (디자인마다 머리글 · 행 높이가 다르다)
    tree->setFixedHeight(tree->header()->sizeHint().height() + tree->sizeHintForRow(0) * 7);
    v->addWidget(tree);
    return pane;
}

QWidget *listsSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"탭 · 목록 — 왼쪽 활성 패널, 오른쪽 비활성 패널"_s);
    auto *h = new QHBoxLayout(box);
    h->setContentsMargins(14, 12, 14, 14);
    h->setSpacing(14);
    for (const bool active : {true, false}) {
        auto *frame = new QFrame;
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setLineWidth(1);
        auto *l = new QVBoxLayout(frame);
        l->setContentsMargins(1, 1, 1, 1);
        l->addWidget(panePreview(tc, active));
        h->addWidget(frame, 1);
    }
    return box;
}

QWidget *progressSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"진행 · 메뉴"_s);
    auto *h = new QHBoxLayout(box);
    h->setContentsMargins(14, 12, 14, 14);
    h->setSpacing(24);

    auto *left = new QVBoxLayout;
    left->setSpacing(6);
    struct Bar { QString name; int value; QString state; };
    const Bar bars[] = {{u"복사 중 62 %"_s, 62, {}}, {u"일시 정지 38 %"_s, 38, u"paused"_s}, {u"오류 80 %"_s, 80, u"error"_s}};
    for (const Bar &b : bars) {
        left->addWidget(caption(b.name, tc));
        auto *pb = new QProgressBar;
        pb->setRange(0, 100);
        pb->setValue(b.value);
        pb->setTextVisible(false);
        fs::setProgressState(pb, b.state);
        left->addWidget(pb);
        left->addSpacing(4);
    }
    left->addWidget(caption(u"작은 크기 · 글자 표시 41 %"_s, tc));
    auto *thin = new QProgressBar;
    thin->setRange(0, 100);
    thin->setValue(41);
    thin->setFormat(u"%p%"_s);
    fs::setSmall(thin);
    left->addWidget(thin);
    left->addSpacing(4);
    left->addWidget(caption(u"메뉴 막대"_s, tc));
    auto *bar = new QMenuBar;
    for (const QString &t : {u"파일(&F)"_s, u"편집(&E)"_s, u"선택(&S)"_s, u"보기(&V)"_s, u"설정(&O)"_s})
        bar->addMenu(t);
    bar->setNativeMenuBar(false);
    left->addWidget(bar);
    left->addStretch(1);
    h->addLayout(left, 1);

    // 메뉴 — 갤러리에서만 일반 위젯으로 끼워 넣어 모양을 보인다.
    auto *menu = new QMenu;
    menu->setWindowFlags(Qt::Widget);
    auto *copy = menu->addAction(glyph(u"copy"_s, tc[fs::Token::Fg2]), u"복사(&C)\tF5"_s);
    menu->addAction(u"이동(&M)\tF6"_s);
    menu->addAction(u"이름 변경(&R)\tF2"_s);
    menu->addSeparator();
    auto *hidden = menu->addAction(u"숨김 파일 표시(&H)\tCtrl+H"_s);
    hidden->setCheckable(true);
    hidden->setChecked(true);
    auto *sort = menu->addMenu(u"정렬 기준(&O)"_s);
    sort->addAction(u"이름"_s);
    auto *paste = menu->addAction(u"붙여넣기(&P)\tCtrl+V"_s);
    paste->setEnabled(false);
    menu->setActiveAction(copy);
    menu->setFixedSize(menu->sizeHint());
    auto *menuColumn = new QVBoxLayout;
    menuColumn->addWidget(caption(u"메뉴 (첫 항목 마우스 올림)"_s, tc));
    menuColumn->addWidget(menu);
    menuColumn->addStretch(1);
    h->addLayout(menuColumn);
    QTimer::singleShot(0, menu, [menu, copy] { menu->setActiveAction(copy); });
    return box;
}

// 복사 흉내 — 모든 그래프가 같은 값을 받는다. 처음 28초는 미리 돌려 그래프를 채운다.
struct TransferSimulator
{
    static constexpr qint64 kTotal = qint64(6.24 * 1024 * 1024 * 1024);
    static constexpr qint64 kStepMs = 250;

    qint64 bytes = 0;
    qint64 ms = 0;
    bool paused = false;
    QList<fm::ui::TransferGraph *> graphs;
    QList<QLabel *> readouts;
    QRandomGenerator rng{42};

    static TransferSimulator &instance()
    {
        static TransferSimulator sim;
        return sim;
    }

    double speedAt(double t)
    {
        constexpr double MB = 1024.0 * 1024.0;
        constexpr double pi = std::numbers::pi;
        double v = 38.0 + 2.5 * std::sin(t / 3.1) + 1.5 * std::sin(t / 1.3 + 1.0);
        if (t > 7.0 && t < 12.0)
            v -= 6.0 * std::sin((t - 7.0) / 5.0 * pi);   // 잠깐 느려짐
        if (t > 21.0 && t < 24.5)
            v += 9.0 * std::sin((t - 21.0) / 3.5 * pi);  // 잠깐 빨라짐
        if (t > 28.0)  // 미리 돌린 뒤에는 오르내림을 크게 해서 선이 움직이는 모습을 보인다
            v += 9.0 * std::sin((t - 28.0) / 2.2) * std::sin((t - 28.0) / 7.0);
        v += (rng.generateDouble() - 0.5) * 2.4;
        return std::max(0.0, v) * MB;
    }

    QList<qint64> history{0};  // 미리 돌린 구간의 단계별 누적 바이트

    static bool pausedAt(qint64 t) { return t > 15000 && t <= 16500; }  // 미리 돌린 구간의 일시 정지

    void attach(fm::ui::TransferGraph *g)
    {
        g->start(kTotal);
        for (qsizetype k = 0; k < history.size(); ++k) {
            const qint64 t = k * kStepMs;
            g->setPaused(pausedAt(t));
            g->addSample(history.at(k), t);
        }
        g->setPaused(paused);
        graphs.append(g);
        QObject::connect(g, &QObject::destroyed, [g] { instance().graphs.removeAll(g); });
    }

    void preroll(qint64 untilMs)
    {
        while (ms < untilMs) {
            if (!pausedAt(ms + kStepMs))
                bytes += qint64(speedAt(ms / 1000.0) * kStepMs / 1000.0);
            ms += kStepMs;
            history.append(bytes);
        }
    }

    void step()
    {
        if (!paused)
            bytes = std::min(kTotal, bytes + qint64(speedAt(ms / 1000.0) * kStepMs / 1000.0));
        ms += kStepMs;
        for (auto *g : std::as_const(graphs)) {
            g->setPaused(paused);
            g->addSample(bytes, ms);
        }
        if (bytes >= kTotal) {  // 끝나면 처음부터 다시
            bytes = 0;
            ms = 0;
            for (auto *g : std::as_const(graphs))
                g->start(kTotal);
        }
        refreshReadouts();
    }

    void refreshReadouts()
    {
        if (graphs.isEmpty())
            return;
        const auto *g = graphs.constFirst();
        const QString text = u"%1 %  ·  현재 %2  ·  평균 %3"_s
                                 .arg(QString::number(100.0 * double(bytes) / double(kTotal), 'f', 0),
                                      paused ? u"일시 정지"_s : fm::ui::TransferGraph::formatRate(g->currentSpeed()),
                                      fm::ui::TransferGraph::formatRate(g->averageSpeed()));
        for (QLabel *l : std::as_const(readouts))
            l->setText(text);
    }
};

QWidget *graphSection(const fs::ThemeColors &tc)
{
    auto &sim = TransferSimulator::instance();
    auto *box = section(u"처리 속도 그래프 — 현재 속도 선이 속도에 따라 움직임"_s);
    auto *v = new QVBoxLayout(box);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(10);

    auto *controls = new QHBoxLayout;
    auto *pause = new QPushButton(u"일시 정지(&P)"_s);
    fs::setSmall(pause);
    QObject::connect(pause, &QPushButton::clicked, pause, [] {
        auto &s = TransferSimulator::instance();
        s.paused = !s.paused;
    });
    auto *readout = caption(QString(), tc, 12);
    sim.readouts.append(readout);
    QObject::connect(readout, &QObject::destroyed, [readout] { TransferSimulator::instance().readouts.removeAll(readout); });
    controls->addWidget(pause);
    controls->addSpacing(8);
    controls->addWidget(readout, 1);
    v->addLayout(controls);

    auto *byProgress = new fm::ui::TransferGraph;
    byProgress->setAxis(fm::ui::TransferGraph::Axis::Progress);
    byProgress->setProperty("fmPreviewHover", 0.09);  // 갤러리: 마우스 읽기 모양
    sim.attach(byProgress);
    v->addWidget(byProgress);

    auto *byTime = new fm::ui::TransferGraph;
    byTime->setAxis(fm::ui::TransferGraph::Axis::Time);
    byTime->setSpeedLimit(50.0 * 1024 * 1024);
    sim.attach(byTime);
    v->addWidget(byTime);
    sim.refreshReadouts();
    return box;
}

// 도구 설명 모양 — QToolTip은 마우스를 올려야 뜨므로 같은 바탕을 그리는 라벨로 보인다.
class TipPreview : public QLabel
{
public:
    explicit TipPreview(const QString &text)
        : QLabel(text)
    {
        QFont f = font();
        f.setPixelSize(12);
        setFont(f);
        setMargin(1 + style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, nullptr, this));
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStylePainter p(this);
        QStyleOptionFrame opt;
        opt.initFrom(this);
        p.drawPrimitive(QStyle::PE_PanelTipLabel, opt);
        p.setPen(palette().color(QPalette::ToolTipText));
        p.drawText(contentsRect(), Qt::AlignCenter, text());
    }
};

// 도구 모음 · 스크롤 막대 · 상태 표시줄 · 도구 설명 · MDI 창 제목 표시줄
QWidget *chromeSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"도구 모음 · 스크롤 막대 · 상태 표시줄 · 도구 설명 · MDI 창"_s);
    auto *grid = new QGridLayout(box);
    grid->setContentsMargins(14, 12, 14, 14);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(8);

    auto *toolbar = new QToolBar;
    toolbar->setMovable(false);
    const QColor iconColor = tc[fs::Token::Fg2];
    for (const QString &k : {u"back"_s, u"forward"_s, u"up"_s})
        toolbar->addAction(glyph(k, iconColor), k);
    toolbar->addSeparator();
    for (const QString &k : {u"copy"_s, u"trash"_s})
        toolbar->addAction(glyph(k, iconColor), k);
    grid->addWidget(toolbar, 0, 0);

    auto *sb = new QScrollBar(Qt::Horizontal);
    sb->setRange(0, 100);
    sb->setPageStep(35);
    sb->setValue(20);
    grid->addWidget(sb, 1, 0);

    auto *status = new QStatusBar;
    status->setSizeGripEnabled(false);
    status->addWidget(new QLabel(u"3개 항목 선택 · 12.4 MB"_s), 1);
    status->addPermanentWidget(new QLabel(u"D:\\ 여유 128 GB"_s));
    grid->addWidget(status, 2, 0);
    grid->addWidget(new TipPreview(u"새로 고침 (Ctrl+R)"_s), 3, 0, Qt::AlignLeft | Qt::AlignTop);

    auto *mdi = new QMdiArea;
    mdi->setBackground(tc[fs::Token::Grid]);
    auto *inner = new QLabel(u"  대상 폴더에 같은 이름이 있습니다."_s);
    inner->setAutoFillBackground(true);
    auto *sub = mdi->addSubWindow(inner);
    sub->setWindowTitle(u"복사 (F5)"_s);
    sub->setGeometry(10, 8, 300, 90);
    auto *inactive = mdi->addSubWindow(new QLabel);
    inactive->setWindowTitle(u"비활성 창"_s);
    inactive->setGeometry(170, 70, 220, 60);
    mdi->setActiveSubWindow(sub);
    mdi->setFixedHeight(150);
    grid->addWidget(mdi, 0, 1, 4, 1);
    grid->setRowStretch(3, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    return box;
}

QWidget *buildTile(fs::Variant variant)
{
    const fs::ThemeColors &tc = fs::ThemeManager::instance().colors(variant);
    auto *tile = new QWidget;
    tile->setAutoFillBackground(true);
    fs::ThemeScope::set(tile, tc);

    auto *v = new QVBoxLayout(tile);
    v->setContentsMargins(20, 16, 20, 20);
    v->setSpacing(14);
    const QString design = fs::designLabel(tc.design());
    auto *title = caption(design + (variant == fs::Variant::Light ? u" · 라이트"_s : u" · 다크"_s), tc, 12);
    QFont tf = title->font();
    tf.setWeight(QFont::DemiBold);
    title->setFont(tf);
    v->addWidget(title);
    v->addWidget(buttonsSection(tc));
    v->addWidget(inputsSection(tc));
    v->addWidget(choicesSection(tc));
    v->addWidget(listsSection(tc));
    v->addWidget(progressSection(tc));
    v->addWidget(chromeSection(tc));
    v->addWidget(graphSection(tc));
    v->addStretch(1);
    return tile;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"fmstyle_gallery"_s);

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption shotOption(u"shot"_s, u"스크린샷을 저장하고 끝냅니다."_s, u"file"_s);
    const QCommandLineOption accentOption(u"accent"_s, u"강조색 기준 색 (#RRGGBB)."_s, u"color"_s);
    const QCommandLineOption noFixOption(u"no-fix"_s, u"대비 자동 보정을 끕니다."_s);
    const QCommandLineOption delayOption(u"shot-delay"_s, u"스크린샷까지 기다릴 시간(ms)."_s, u"ms"_s, u"400"_s);
    const QCommandLineOption designOption(u"design"_s, u"시작 디자인: standard(시안1) · watercolor(시안2)."_s,
                                          u"name"_s, u"standard"_s);
    parser.addOptions({shotOption, accentOption, noFixOption, delayOption, designOption});
    parser.process(app);

    auto &theme = fs::ThemeManager::instance();
    const QString designName = parser.value(designOption).toLower();
    if (designName == u"watercolor" || designName == u"2")
        theme.setDesign(fs::Design::Watercolor);
    theme.install(app);
    if (parser.isSet(accentOption) || parser.isSet(noFixOption)) {
        fs::ThemeSeeds seeds;
        if (parser.isSet(accentOption))
            seeds.accent = QColor(parser.value(accentOption));
        seeds.fixContrast = !parser.isSet(noFixOption);
        theme.setSeeds(seeds);
    }

    TransferSimulator::instance().preroll(28'000);

    QWidget window;
    window.setWindowTitle(u"FmStyle 갤러리"_s);
    auto *outer = new QVBoxLayout(&window);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // 디자인 전환 — ThemeManager::setDesign()이 앱 스타일을 바꾸고, 타일은 새로 만든다.
    auto *top = new QWidget;
    auto *topRow = new QHBoxLayout(top);
    topRow->setContentsMargins(20, 10, 20, 10);
    topRow->setSpacing(0);
    auto *designGroup = new QButtonGroup(top);
    QList<QAbstractButton *> designButtons;
    for (const fs::Design d : {fs::Design::Standard, fs::Design::Watercolor}) {
        auto *b = new QPushButton(fs::designLabel(d));
        designButtons.append(b);
        designGroup->addButton(b, int(d));
        topRow->addWidget(b);
    }
    fs::setSegments(designButtons);
    designButtons[int(theme.design())]->setChecked(true);
    topRow->addStretch(1);
    outer->addWidget(top);

    auto *tiles = new QWidget;
    auto *h = new QHBoxLayout(tiles);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    const auto rebuild = [h] {
        while (QLayoutItem *item = h->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        h->addWidget(buildTile(fs::Variant::Light), 1);
        h->addWidget(buildTile(fs::Variant::Dark), 1);
        TransferSimulator::instance().refreshReadouts();
    };
    rebuild();
    outer->addWidget(tiles, 1);
    QObject::connect(designGroup, &QButtonGroup::idClicked, &window, [&theme, rebuild](int id) {
        theme.setDesign(fs::Design(id));
        rebuild();
    });
    window.resize(1480, 1700);
    window.show();

    QTimer ticker;
    QObject::connect(&ticker, &QTimer::timeout, [] { TransferSimulator::instance().step(); });
    ticker.start(int(TransferSimulator::kStepMs));

    if (parser.isSet(shotOption)) {
        const QString file = parser.value(shotOption);
        QTimer::singleShot(parser.value(delayOption).toInt(), &window, [&window, file] {
            window.grab().save(file);
            QApplication::quit();
        });
    }
    return app.exec();
}
