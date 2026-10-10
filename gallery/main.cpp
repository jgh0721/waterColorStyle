// FmStyle 갤러리 — 라이트와 다크를 나란히 놓고 컨트롤 상태를 한 화면에 보여 준다.
//   fmstyle_gallery                       창으로 실행 (위쪽 단추로 시안1 · 시안2 전환)
//   fmstyle_gallery --design watercolor   시안2(워터컬러)로 시작
//   fmstyle_gallery --shot gallery.png    스크린샷을 저장하고 끝냄 (QT_QPA_PLATFORM=offscreen 가능)
//   fmstyle_gallery --accent "#0F7A6E"    강조색을 바꿔 파생 규칙 확인
//   fmstyle_gallery --parts               대화상자 부품 구역만
//   fmstyle_gallery --qt-widgets          Qt 표준 위젯 구역만 (탭 네 방향 · 슬라이더 · 다이얼 · 도구 상자 · 달력 · 도크 · MDI · 항목 보기)
//   fmstyle_gallery --dark-tone navy      시안2 다크를 남색으로

#include <fmstyle/FmStyle.h>
#include <fmstyle/Glyphs.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/Banner.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/DialogFooter.h>
#include <fmwidgets/DialogHeader.h>
#include <fmwidgets/KeyChip.h>
#include <fmwidgets/Label.h>
#include <fmwidgets/ProgressBar.h>
#include <fmwidgets/SegmentedControl.h>
#include <fmwidgets/Tag.h>
#include <fmwidgets/BarListCard.h>
#include <fmwidgets/LaneLadder.h>
#include <fmwidgets/PairedTimeline.h>
#include <fmwidgets/Toast.h>
#include <fmwidgets/TransferGraph.h>
#include <fmstyle/StylePaint.h>

#include <QApplication>
#include <QButtonGroup>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QColumnView>
#include <QComboBox>
#include <QCommandLineParser>
#include <QDateEdit>
#include <QDial>
#include <QDockWidget>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
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
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QStatusBar>
#include <QStylePainter>
#include <QTabBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolBox>
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
    left->addWidget(caption(u"누적 막대 목록 — 막대 길이는 가장 긴 줄에 견준다"_s, tc));
    auto *usage = new fm::ui::BarListCard;
    usage->setSeries({{u"사용"_s, fs::Token::Accent}, {u"예약"_s, fs::Token::Accent, 0.47}});
    usage->setRows({{u"C:"_s, u"시스템"_s, {182, 24}},
                    {u"D:"_s, u"작업"_s, {96, 0}, {}, u"2"_s},
                    {u"E:"_s, u"백업"_s, {240, 60}}});
    usage->setBadgeLegend(u"읽지 못한 폴더"_s);
    left->addWidget(usage);
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

// 사다리 · 두 줄 시간 흐름 · 알림 — 자취를 보이는 위젯 셋.
QWidget *traceSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"사다리 · 두 줄 시간 흐름 · 알림"_s);
    auto *v = new QVBoxLayout(box);
    v->setSpacing(10);

    v->addWidget(caption(u"사다리 — 레인 사이의 자취. 줄 종류가 선 모양을 정한다"_s, tc, 12));
    auto *ladder = new fm::ui::LaneLadder;
    ladder->setLanes({{u"앱"_s, u"app.exe"_s},
                      {u"스캐너"_s, u"127.0.0.1"_s},
                      {u"목적지"_s, u"FAKE-vendor.test"_s}});
    ladder->setLaneWidth(240);
    ladder->addRung(0, 1, u"0 ms"_s, u"연결 · ClientHello"_s);
    ladder->addRung(1, 2, u"12 ms"_s, u"사외 TLS — 앵커로 검증"_s);
    ladder->addRung(0, 1, u"31 ms"_s, u"POST /v1/messages"_s);
    ladder->addRung(1, 1, u"33 ms"_s, u"표식을 더한다"_s, fm::ui::LaneLadder::Self);
    ladder->addRung(2, 1, u"402 ms"_s, u"응답 머리 200 · text/event-stream"_s);
    ladder->addRung(2, 1, u"2.1 s"_s, u"조각 열둘"_s, fm::ui::LaneLadder::Stream);
    ladder->addRung(2, 1, u"5.8 s"_s, u"프록시 뒤는 보이지 않는다"_s, fm::ui::LaneLadder::Inferred);
    ladder->addDivider(QString(), u"두 번째 교환"_s);
    ladder->addRung(0, 1, u"6.0 s"_s, u"GET /v1/models"_s);
    ladder->addRung(1, 2, u"6.4 s"_s, u"유휴 상한에 닿아 끊겼다"_s, fm::ui::LaneLadder::Failed);
    ladder->setCurrentRow(5);
    v->addWidget(ladder);

    v->addWidget(caption(u"두 줄 시간 흐름 — 같은 자 위의 두 쪽. 기울기가 지연, 띠가 빈 자리"_s, tc, 12));
    auto *tl = new fm::ui::PairedTimeline;
    tl->setRows({u"받음"_s, u"목적지 → 스캐너"_s}, {u"넘김"_s, u"스캐너 → 앱"_s});
    tl->setSpanMs(9000);
    const qint64 at[] = {400, 620, 790, 980, 1180, 4800, 5020, 5240, 8600};
    for (int i = 0; i < 9; ++i) {
        const auto kind = i == 0 ? fm::ui::PairedTimeline::First : fm::ui::PairedTimeline::Tick;
        tl->addMark(fm::ui::PairedTimeline::kTopRow, at[i], kind, i);
        // 넘긴 쪽은 조금 늦는다 — 마지막 하나는 넘기지 못하고 끊겼다.
        if (i < 8)
            tl->addMark(fm::ui::PairedTimeline::kBottomRow, at[i] + (i == 5 ? 380 : 40), kind, i);
    }
    tl->addMark(fm::ui::PairedTimeline::kBottomRow, 8700, fm::ui::PairedTimeline::Cut);
    v->addWidget(tl);

    v->addWidget(caption(u"알림 — 바탕 창 아래 가운데에 쌓인다. 누르면 닫히고, 마우스를 올리면 시계가 멈춘다"_s, tc, 12));
    auto *stage = new QWidget;
    stage->setMinimumHeight(132);
    stage->setAutoFillBackground(true);
    {
        QPalette pal = stage->palette();
        pal.setColor(QPalette::Window, tc[fs::Token::Alt]);
        stage->setPalette(pal);
    }
    auto *row = new QHBoxLayout;
    row->setSpacing(6);
    const struct { const char16_t *label; fm::ui::Toast::Tone tone; const char16_t *text; } kinds[] = {
        {u"알림", fm::ui::Toast::Info, u"스캔을 시작했다 — 디스크 단계."},
        {u"됨", fm::ui::Toast::Ok, u"보고를 <b>내보냈다</b> — report-2026.json."},
        {u"경고", fm::ui::Toast::Warn, u"못 본 범위가 있다 — 다른 사용자의 프로필."},
        {u"위험", fm::ui::Toast::Danger, u"서비스와 끊겼다."},
    };
    for (const auto &k : kinds) {
        auto *b = new QPushButton(QString::fromUtf16(k.label));
        QObject::connect(b, &QPushButton::clicked, stage, [stage, k] {
            fm::ui::Toast::showOver(stage, k.tone, QString::fromUtf16(k.text));
        });
        row->addWidget(b);
    }
    auto *keep = new QPushButton(u"단추가 있는 알림"_s);
    QObject::connect(keep, &QPushButton::clicked, stage, [stage] {
        auto *t = fm::ui::Toast::showOver(stage, fm::ui::Toast::Warn, u"피드가 오래됐다 — 38 일 전."_s, -1);
        if (t)
            t->setActionText(u"지금 받기"_s);
    });
    row->addWidget(keep);
    row->addStretch(1);
    v->addLayout(row);
    v->addWidget(stage);
    // 갤러리를 열면 하나는 떠 있게 — 가만히 있는 모습도 보여야 한다.
    QTimer::singleShot(0, stage, [stage] {
        fm::ui::Toast::showOver(stage, fm::ui::Toast::Ok, u"스캔을 마쳤다 — 자산 <b>412</b> · Shadow AI 7."_s, -1);
    });
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

// 대화상자 부품 — 머리 블록 · 배너 · 태그 · 키 칩 · 밀도별 입력 · 세그먼트 크기 · 진행 막대 두께 ·
// 평면 표 머리글 · 글자 역할 · 버튼 영역(키 칩 단추 · 링크 단추). 목업: 복사 · 삭제 · 진행 · 권한 대화상자.
QWidget *dialogPartsSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"대화상자 부품"_s);
    auto *v = new QVBoxLayout(box);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(12);

    // 머리 블록 3종
    auto *h1 = new fm::ui::DialogHeader;
    h1->setGlyph(fm::ui::glyph::Copy);
    h1->setTitle(u"3개 항목 복사"_s);
    h1->setSubtitle(u"Qt-6.11.0-windows-x64-msvc2026-offline-installer-with-debug-symbols.exe 외 2개 · 3.95 GB · D:\\Downloads"_s);
    h1->setSubtitleElide(Qt::ElideMiddle);
    auto *h2 = new fm::ui::DialogHeader;
    h2->setGlyph(fm::ui::glyph::Trash);
    h2->setTone(fm::ui::DialogHeader::Danger);
    h2->setTitle(u"3개 항목을 영구 삭제"_s);
    h2->setSubtitle(u"휴지통을 거치지 않으며 되돌릴 수 없습니다"_s);
    auto *h3 = new fm::ui::DialogHeader;
    h3->setGlyph(fm::ui::glyph::Shield);
    h3->setTone(fm::ui::DialogHeader::Warn);
    h3->setTitle(u"이름을 바꾸려면 관리자 권한이 필요합니다"_s);
    h3->setSubtitle(u"이 폴더의 항목은 관리자만 이름을 바꿀 수 있습니다."_s);
    h3->setSubtitleWrap(true);
    for (auto *h : {h1, h2, h3})
        v->addWidget(h);

    // 배너 3종
    v->addWidget(new fm::ui::Banner(fm::ui::Banner::Info, fm::ui::glyph::Info,
                                    u"휴지통으로 옮긴 항목은 <b>30일</b> 동안 되살릴 수 있습니다."_s));
    v->addWidget(new fm::ui::Banner(fm::ui::Banner::Warn, fm::ui::glyph::Shield,
                                    u"관리자 권한으로 남은 항목 12개를 처리하고 있습니다."_s));
    v->addWidget(new fm::ui::Banner(fm::ui::Banner::Danger, fm::ui::glyph::Warning,
                                    u"영구 삭제한 항목은 되돌릴 수 없습니다."_s));

    // 태그 · 키 칩
    auto *tags = new QHBoxLayout;
    tags->setSpacing(6);
    auto *okTag = new fm::ui::Tag(u"이 이름을 쓸 수 있습니다"_s, fm::ui::Tag::Ok);
    okTag->setGlyph(fm::ui::glyph::Check);
    tags->addWidget(new fm::ui::Tag(u"이동"_s, fm::ui::Tag::Info));
    tags->addWidget(new fm::ui::Tag(u"같은 볼륨 · 즉시 처리"_s, fm::ui::Tag::Mute));
    tags->addWidget(okTag);
    auto *bad = new fm::ui::Tag(u"잘못된 이름"_s, fm::ui::Tag::Bad);
    bad->setCompact(true);
    tags->addWidget(bad);
    tags->addSpacing(8);
    for (const QString &k : {u"F2"_s, u"Alt+1"_s, u"Del"_s})
        tags->addWidget(new fm::ui::KeyChip(k));
    tags->addStretch(1);
    v->addLayout(tags);

    // 대화상자 밀도의 입력(32) · 오류 입력 · 세그먼트 크기 · 진행 막대 두께
    auto *dense = new QWidget;
    fs::setDensity(dense, fs::Density::Dialog);
    auto *grid = new QGridLayout(dense);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);
    auto *path = new QLineEdit(u"E:\\Backup\\Installers\\"_s);
    path->setFont(fs::monoFont(13));
    auto *policy = new QComboBox;
    policy->addItem(u"매번 묻기"_s);
    auto *invalid = new QLineEdit(u"report:2026?.pdf"_s);
    fs::setInvalid(invalid);
    grid->addWidget(new fm::ui::Label(u"대상 폴더(D) · 대화상자 밀도 32"_s, fm::ui::Label::FieldLabel), 0, 0);
    grid->addWidget(new fm::ui::Label(u"파일이 이미 있을 때(E)"_s, fm::ui::Label::FieldLabel), 0, 1);
    grid->addWidget(path, 1, 0);
    grid->addWidget(policy, 1, 1);
    grid->addWidget(invalid, 2, 0);
    auto *help = new fm::ui::Label(u"이름에 쓸 수 없는 문자: ? :"_s, fm::ui::Label::Help);
    help->setTone(fm::ui::Label::Danger);
    grid->addWidget(help, 2, 1);
    auto *segs = new QHBoxLayout;
    segs->setSpacing(8);
    auto *mini = new fm::ui::SegmentedControl({u"진행률"_s, u"시간"_s});
    mini->setSegmentSize(fm::ui::SegmentedControl::Mini);
    auto *small = new fm::ui::SegmentedControl({u"1줄"_s, u"2줄"_s, u"자동"_s});
    small->setSegmentSize(fm::ui::SegmentedControl::Small);
    small->setCurrentIndex(2);
    auto *normal = new fm::ui::SegmentedControl({u"그대로"_s, u"소문자"_s, u"대문자"_s});
    normal->setExpanding(true);
    segs->addWidget(mini);
    segs->addWidget(small);
    segs->addWidget(normal, 1);
    grid->addLayout(segs, 3, 0, 1, 2);
    auto *thin = new fm::ui::ProgressBar;
    thin->setThickness(fm::ui::ProgressBar::ThinBar);
    thin->setValue(72);
    auto *thick = new fm::ui::ProgressBar;
    thick->setThickness(fm::ui::ProgressBar::ThickBar);
    thick->setValue(37);
    auto *busy = new fm::ui::ProgressBar;
    busy->setRange(0, 0);
    grid->addWidget(thin, 4, 0);
    grid->addWidget(thick, 4, 1);
    grid->addWidget(busy, 5, 0, 1, 2);
    v->addWidget(dense);

    // 글자 역할 · 평면 표 머리글
    auto *stats = new QHBoxLayout;
    stats->setSpacing(18);
    for (const auto &[label, value] : {std::pair{u"속도"_s, u"172 MB/s"_s}, {u"남은 시간"_s, u"약 14초"_s}, {u"항목"_s, u"2,418 / 3,902"_s}}) {
        auto *col = new QVBoxLayout;
        col->setSpacing(0);
        col->addWidget(new fm::ui::Label(label, fm::ui::Label::StatLabel));
        col->addWidget(new fm::ui::Label(value, fm::ui::Label::StatValue));
        stats->addLayout(col);
    }
    stats->addWidget(new fm::ui::Label(u"37%"_s, fm::ui::Label::BigNumber));
    stats->addStretch(1);
    v->addLayout(stats);
    auto *table = new QTreeWidget;
    table->setColumnCount(3);
    table->setHeaderLabels({u"원래 이름"_s, u"새 이름"_s, u"상태"_s});
    table->setRootIsDecorated(false);
    table->setFixedHeight(84);
    fs::setFlatHeader(table);
    for (const QStringList &row : {QStringList{u"DSC04417.arw"_s, u"2026-09-14_제주_001.arw"_s, u"변경"_s},
                                   QStringList{u"report?.pdf"_s, u"report?.pdf"_s, u"잘못된 이름"_s}})
        table->addTopLevelItem(new QTreeWidgetItem(row));
    v->addWidget(table);

    // 버튼 영역 — 키 칩 단추 · 링크 단추 · 키 칩 라디오 · 기본 · 취소
    auto *footer = new fm::ui::DialogFooter;
    auto *fl = new QHBoxLayout(footer);
    fl->setContentsMargins(24, 16, 24, 16);
    fl->setSpacing(8);
    auto *queue = new fm::ui::Button(u"대기열에 추가(&Q)"_s);
    queue->setKeyHint(u"F2"_s);
    auto *link = new fm::ui::Button(u"간단히 보기(&D)"_s);
    link->setRole(fm::ui::Button::Link);
    link->setGlyph(fm::ui::glyph::ChevronUp);
    auto *primary = new fm::ui::Button(u"복사(&C)"_s);
    primary->setRole(fm::ui::Button::Primary);
    primary->setDefault(true);
    auto *cancel = new fm::ui::Button(u"취소"_s);
    fl->addWidget(queue);
    fl->addWidget(link);
    fl->addStretch(1);
    fl->addWidget(primary);
    fl->addWidget(cancel);
    v->addWidget(footer);
    auto *radios = new QHBoxLayout;
    auto *trashRadio = new QRadioButton(u"휴지통으로 이동(&R)"_s);
    trashRadio->setChecked(true);
    fs::setKeyHint(trashRadio, u"Del"_s);
    auto *permRadio = new QRadioButton(u"영구 삭제(&P)"_s);
    fs::setKeyHint(permRadio, u"Shift+Del"_s);
    auto *danger = new fm::ui::Button(u"영구 삭제"_s);
    danger->setRole(fm::ui::Button::Danger);
    danger->setGlyph(fm::ui::glyph::Trash);
    auto *shieldButton = new QPushButton(u"관리자 권한으로 이름 바꾸기(&R)"_s);
    shieldButton->setIcon(shieldButton->style()->standardIcon(QStyle::SP_VistaShield, nullptr, shieldButton));
    radios->addWidget(trashRadio);
    radios->addWidget(permRadio);
    radios->addStretch(1);
    radios->addWidget(danger);
    v->addLayout(radios);
    v->addWidget(shieldButton, 0, Qt::AlignLeft);
    Q_UNUSED(tc)
    return box;
}

// =============================================================================================
// Qt 표준 위젯 — 목업에 없는 기본 위젯을 모아 두 디자인의 모양을 확인한다. 아직 Fusion이 그리는 것(도크 제목 ·
// 크기 조절 손잡이 · 떼어 내기 줄 · 컬러 표준 아이콘)도 같이 둔다.

// 최대화한 MDI 창이 주 창 메뉴 막대 오른쪽에 두는 단추 묶음(CC_MdiControls). 주 창이 최상위 창일 때만
// 생기므로 QMdiSubWindow의 ControllerWidget과 같은 방식으로 직접 그린다. 닫기 단추는 마우스 올림 모양.
class MdiControlsPreview : public QWidget
{
public:
    MdiControlsPreview() { setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); }

    QSize sizeHint() const override
    {
        QStyleOptionComplex opt;
        initOption(&opt);
        const int b = style()->pixelMetric(QStyle::PM_TitleBarButtonSize, &opt, this);
        return style()->sizeFromContents(QStyle::CT_MdiControls, &opt, QSize(3 * b, b), this);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStylePainter p(this);
        QStyleOptionComplex opt;
        initOption(&opt);
        p.drawComplexControl(QStyle::CC_MdiControls, opt);
    }

private:
    void initOption(QStyleOptionComplex *opt) const
    {
        opt->initFrom(this);
        opt->subControls = QStyle::SC_MdiMinButton | QStyle::SC_MdiNormalButton | QStyle::SC_MdiCloseButton;
        opt->activeSubControls = QStyle::SC_MdiCloseButton;
        opt->state |= QStyle::State_MouseOver;
    }
};

// 크기 조절 손잡이(CE_SizeGrip). QSizeGrip은 최상위 창 안의 위치로 모서리 방향을 정해 타일마다 방향이
// 달라지므로 오른쪽 아래 방향으로 고정해 그린다.
class SizeGripPreview : public QWidget
{
public:
    SizeGripPreview() { setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); }

    QSize sizeHint() const override
    {
        QStyleOptionSizeGrip opt;
        opt.initFrom(this);
        opt.corner = Qt::BottomRightCorner;
        return style()->sizeFromContents(QStyle::CT_SizeGrip, &opt, QSize(13, 13), this);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStylePainter p(this);
        QStyleOptionSizeGrip opt;
        opt.initFrom(this);
        opt.corner = Qt::BottomRightCorner;
        p.drawControl(QStyle::CE_SizeGrip, opt);
    }
};

// 표준 아이콘 하나. 그릴 때 스타일에 물어 타일의 ThemeScope를 따르게 한다.
class StandardIconPreview : public QWidget
{
public:
    StandardIconPreview(QStyle::StandardPixmap sp, int size)
        : m_sp(sp)
    {
        setFixedSize(size, size);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        style()->standardIcon(m_sp, nullptr, this).paint(&p, rect());
    }

private:
    QStyle::StandardPixmap m_sp;
};

QWidget *qtPage(const QString &text, const fs::ThemeColors &tc)
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(10, 8, 10, 8);
    l->addWidget(caption(text, tc));
    l->addStretch(1);
    return w;
}

QTabWidget *tabWidget(QTabWidget::TabPosition position, bool closable, const fs::ThemeColors &tc)
{
    auto *tabs = new QTabWidget;
    tabs->setTabPosition(position);
    tabs->setTabsClosable(closable);
    for (const QString &t : {u"문서"_s, u"받은 파일"_s, u"C:\\"_s})
        tabs->addTab(qtPage(t, tc), t);
    tabs->setCurrentIndex(1);
    return tabs;
}

QWidget *qtTabsSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"Qt 표준 위젯 — 탭 네 방향 · 닫기 단추 · 스크롤 화살표"_s);
    auto *grid = new QGridLayout(box);
    grid->setContentsMargins(14, 12, 14, 14);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(6);

    auto *north = tabWidget(QTabWidget::North, true, tc);
    north->setFixedHeight(96);
    if (QWidget *close = north->tabBar()->tabButton(2, QTabBar::RightSide))
        fs::setPreviewState(close, u"hover"_s);
    grid->addWidget(caption(u"위 (North) · 닫기 단추 — 세 번째 탭의 닫기 단추는 마우스 올림"_s, tc), 0, 0, 1, 2);
    grid->addWidget(north, 1, 0, 1, 2);

    auto *south = tabWidget(QTabWidget::South, false, tc);
    south->setFixedHeight(96);
    grid->addWidget(caption(u"아래 (South)"_s, tc), 2, 0, 1, 2);
    grid->addWidget(south, 3, 0, 1, 2);

    auto *west = tabWidget(QTabWidget::West, false, tc);
    auto *east = tabWidget(QTabWidget::East, false, tc);
    west->setFixedHeight(200);
    east->setFixedHeight(200);
    grid->addWidget(caption(u"왼쪽 (West)"_s, tc), 4, 0);
    grid->addWidget(caption(u"오른쪽 (East)"_s, tc), 4, 1);
    grid->addWidget(west, 5, 0);
    grid->addWidget(east, 5, 1);

    auto *scroll = new QTabBar;
    scroll->setUsesScrollButtons(true);
    scroll->setExpanding(false);
    scroll->setDrawBase(true);
    for (const QString &t : {u"문서"_s, u"받은 파일"_s, u"사진"_s, u"음악"_s, u"동영상"_s, u"바탕 화면"_s})
        scroll->addTab(t);
    scroll->setCurrentIndex(2);
    scroll->setFixedWidth(240);
    grid->addWidget(caption(u"넘친 탭 줄 — 스크롤 화살표"_s, tc), 6, 0, 1, 2);
    grid->addWidget(scroll, 7, 0, 1, 2, Qt::AlignLeft);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    return box;
}

QWidget *qtRangesSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"Qt 표준 위젯 — 슬라이더 · 다이얼 · 날짜 · 도구 상자 · 달력"_s);
    auto *grid = new QGridLayout(box);
    grid->setContentsMargins(14, 12, 14, 14);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(6);

    auto *ticks = new QSlider(Qt::Horizontal);
    ticks->setRange(0, 100);
    ticks->setValue(40);
    ticks->setTickPosition(QSlider::TicksBelow);
    ticks->setTickInterval(10);
    auto *states = new QSlider(Qt::Horizontal);
    states->setRange(0, 100);
    states->setValue(55);
    fs::setPreviewState(states, u"hover,focus"_s);
    auto *disabled = new QSlider(Qt::Horizontal);
    disabled->setRange(0, 100);
    disabled->setValue(65);
    disabled->setEnabled(false);
    auto *vertical = new QSlider(Qt::Vertical);
    vertical->setRange(0, 100);
    vertical->setValue(70);
    vertical->setTickPosition(QSlider::TicksRight);
    vertical->setTickInterval(20);
    vertical->setFixedHeight(110);
    auto *dial = new QDial;
    dial->setRange(0, 100);
    dial->setValue(30);
    dial->setNotchesVisible(true);
    dial->setFixedSize(76, 76);
    grid->addWidget(caption(u"가로 · 눈금"_s, tc), 0, 0);
    grid->addWidget(caption(u"마우스 올림 · 키보드 포커스"_s, tc), 0, 1);
    grid->addWidget(caption(u"세로"_s, tc), 0, 2);
    grid->addWidget(caption(u"다이얼"_s, tc), 0, 3);
    grid->addWidget(ticks, 1, 0);
    grid->addWidget(states, 1, 1);
    grid->addWidget(vertical, 1, 2, 5, 1, Qt::AlignHCenter | Qt::AlignTop);
    grid->addWidget(dial, 1, 3, 5, 1, Qt::AlignTop);

    auto *date = new QDateEdit(QDate(2026, 9, 27));
    date->setCalendarPopup(true);
    auto *plusMinus = new QSpinBox;
    plusMinus->setRange(0, 10);
    plusMinus->setValue(3);
    plusMinus->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    grid->addWidget(caption(u"사용 안 함"_s, tc), 2, 0);
    grid->addWidget(caption(u"날짜 (달력 팝업)"_s, tc), 2, 1);
    grid->addWidget(disabled, 3, 0);
    grid->addWidget(date, 3, 1);
    grid->addWidget(caption(u"스핀 상자 (± 기호)"_s, tc), 4, 0);
    grid->addWidget(plusMinus, 5, 0);

    auto *toolBox = new QToolBox;
    toolBox->addItem(qtPage(u"즐겨찾기 폴더 5개"_s, tc), u"즐겨찾기"_s);
    toolBox->addItem(qtPage(u"최근 위치 12개"_s, tc), u"최근 위치"_s);
    toolBox->addItem(qtPage(u"연결 안 됨"_s, tc), u"네트워크 (사용 안 함)"_s);
    toolBox->setItemEnabled(2, false);
    toolBox->setFixedHeight(200);
    auto *calendar = new QCalendarWidget;
    calendar->setSelectedDate(QDate(2026, 9, 27));
    grid->addWidget(caption(u"도구 상자"_s, tc), 6, 0);
    grid->addWidget(caption(u"달력 — 날짜 팝업과 같은 위젯"_s, tc), 6, 1, 1, 3);
    grid->addWidget(toolBox, 7, 0);
    grid->addWidget(calendar, 7, 1, 1, 3);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    return box;
}

// 주 창 구성 — 도크 제목 · 단추, 최대화한 MDI 창, 넘친 도구 모음(확장 단추) · 손잡이, 상태 표시줄 · 크기 조절 손잡이.
QWidget *qtMainWindow(const fs::ThemeColors &tc)
{
    auto *mw = new QMainWindow;
    mw->setWindowFlags(Qt::Widget);
    QMenuBar *menuBar = mw->menuBar();
    menuBar->setNativeMenuBar(false);
    for (const QString &t : {u"파일(&F)"_s, u"편집(&E)"_s, u"창(&W)"_s})
        menuBar->addMenu(t);
    menuBar->setCornerWidget(new MdiControlsPreview, Qt::TopRightCorner);

    QToolBar *toolbar = mw->addToolBar(u"도구"_s);
    toolbar->setMovable(true);
    const QColor iconColor = tc[fs::Token::Fg2];
    for (int i = 0; i < 4; ++i) {
        for (const QString &k : {u"back"_s, u"forward"_s, u"up"_s, u"copy"_s, u"trash"_s})
            toolbar->addAction(glyph(k, iconColor), k);
    }

    auto *mdi = new QMdiArea;
    mdi->setBackground(tc[fs::Token::Grid]);
    auto *doc = new QLabel(u"  최대화한 MDI 창"_s);
    doc->setAutoFillBackground(true);
    QMdiSubWindow *sub = mdi->addSubWindow(doc);
    sub->setWindowTitle(u"복사 (F5)"_s);
    sub->showMaximized();
    mw->setCentralWidget(mdi);

    auto *dock = new QDockWidget(u"폴더"_s);
    dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
                      | QDockWidget::DockWidgetFloatable);
    dock->setWidget(qtPage(u"C:\\  D:\\  E:\\"_s, tc));
    mw->addDockWidget(Qt::LeftDockWidgetArea, dock);

    QStatusBar *status = mw->statusBar();
    status->setSizeGripEnabled(false);
    status->addWidget(new QLabel(u"항목 3개"_s), 1);
    status->addPermanentWidget(new SizeGripPreview);
    mw->setFixedHeight(240);
    return mw;
}

QWidget *qtWindowSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"Qt 표준 위젯 — 주 창 구성 · 떼어 내기 메뉴 · 표준 아이콘"_s);
    auto *v = new QVBoxLayout(box);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(6);

    v->addWidget(caption(u"도크 · 최대화한 MDI 창과 메뉴 막대 단추 묶음 · 넘친 도구 모음 · 크기 조절 손잡이"_s, tc));
    auto *row = new QHBoxLayout;
    row->setSpacing(14);
    row->addWidget(qtMainWindow(tc), 1);

    auto *menu = new QMenu;
    menu->setWindowFlags(Qt::Widget);
    menu->setTearOffEnabled(true);
    menu->addAction(u"이름"_s);
    menu->addAction(u"크기"_s);
    menu->addAction(u"수정한 날짜"_s);
    menu->setFixedSize(menu->sizeHint());
    auto *menuColumn = new QVBoxLayout;
    menuColumn->addWidget(caption(u"떼어 내기 메뉴"_s, tc));
    menuColumn->addWidget(menu);
    menuColumn->addStretch(1);
    row->addLayout(menuColumn);
    v->addLayout(row);

    v->addSpacing(6);
    v->addWidget(caption(u"표준 아이콘 — 단색 아이콘(화살표 · 창 단추 · 확장 · 새로 고침)과 방패는 테마 색, 나머지는 Fusion"_s, tc));
    auto *icons = new QGridLayout;
    icons->setHorizontalSpacing(10);
    icons->setVerticalSpacing(8);
    struct Icon { QStyle::StandardPixmap sp; QString name; int size; };
    const Icon list[] = {
        {QStyle::SP_MessageBoxInformation, u"정보"_s, 32},
        {QStyle::SP_MessageBoxWarning, u"경고"_s, 32},
        {QStyle::SP_MessageBoxCritical, u"오류"_s, 32},
        {QStyle::SP_MessageBoxQuestion, u"질문"_s, 32},
        {QStyle::SP_DirIcon, u"폴더"_s, 16},
        {QStyle::SP_FileIcon, u"파일"_s, 16},
        {QStyle::SP_DriveHDIcon, u"드라이브"_s, 16},
        {QStyle::SP_TrashIcon, u"휴지통"_s, 16},
        {QStyle::SP_FileDialogNewFolder, u"새 폴더"_s, 16},
        {QStyle::SP_FileDialogDetailedView, u"자세히"_s, 16},
        {QStyle::SP_ArrowBack, u"뒤로"_s, 16},
        {QStyle::SP_BrowserReload, u"새로 고침"_s, 16},
        {QStyle::SP_DialogOkButton, u"확인"_s, 16},
        {QStyle::SP_DialogCancelButton, u"취소"_s, 16},
        {QStyle::SP_TitleBarCloseButton, u"제목 닫기"_s, 16},
        {QStyle::SP_TitleBarNormalButton, u"제목 복원"_s, 16},
        {QStyle::SP_DockWidgetCloseButton, u"도크 닫기"_s, 16},
        {QStyle::SP_TabCloseButton, u"탭 닫기"_s, 16},
        {QStyle::SP_LineEditClearButton, u"입력 지우기"_s, 16},
        {QStyle::SP_ToolBarHorizontalExtensionButton, u"도구 모음 확장"_s, 16},
        {QStyle::SP_VistaShield, u"방패 (바꿈)"_s, 16},
    };
    constexpr int kColumns = 7;
    int i = 0;
    for (const Icon &icon : list) {
        auto *cell = new QVBoxLayout;
        cell->setSpacing(3);
        cell->addWidget(new StandardIconPreview(icon.sp, icon.size), 0, Qt::AlignHCenter | Qt::AlignBottom);
        cell->addWidget(caption(icon.name, tc, 11), 0, Qt::AlignHCenter | Qt::AlignTop);
        icons->addLayout(cell, i / kColumns, i % kColumns);
        ++i;
    }
    v->addLayout(icons);
    return box;
}

// 한 줄을 마우스 올림 모양으로 — 마우스 올림은 커서가 있을 때만 생기므로 갤러리에서는 상태를 더해 보인다.
class HoverRowDelegate : public QStyledItemDelegate
{
public:
    HoverRowDelegate(int row, QObject *parent)
        : QStyledItemDelegate(parent)
        , m_row(row)
    {
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem o(option);
        if (index.row() == m_row)
            o.state |= QStyle::State_MouseOver;
        QStyledItemDelegate::paint(p, o, index);
    }

private:
    int m_row;
};

// 끌어 놓기 위치 표시(PE_IndicatorItemViewItemDrop) — 끌고 있을 때만 보이므로 보기와 같은 방식으로 직접 그린다.
// 위는 항목 사이(높이 0 = 선), 아래는 항목 위(사각형).
class DropIndicatorPreview : public QWidget
{
public:
    DropIndicatorPreview() { setFixedHeight(64); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), palette().base());
        p.setPen(palette().color(QPalette::Text));
        p.drawText(QRect(10, 4, width() - 20, 20), Qt::AlignVCenter, u"보고서.pdf"_s);
        p.drawText(QRect(10, 36, width() - 20, 20), Qt::AlignVCenter, u"사진"_s);
        p.setPen(QPen());  // 보기의 painter처럼 기본 펜
        QStyleOption opt;
        opt.initFrom(this);
        opt.rect = QRect(6, 28, width() - 12, 0);
        style()->drawPrimitive(QStyle::PE_IndicatorItemViewItemDrop, &opt, &p, this);
        opt.rect = QRect(6, 36, width() - 12, 20);
        style()->drawPrimitive(QStyle::PE_IndicatorItemViewItemDrop, &opt, &p, this);
    }
};

// 항목 보기 — 표(가로 · 세로 머리글, 모서리 단추, 격자, 교차 행, 체크, 아이콘, 정렬 표시, 행 선택, 셀 편집기) ·
// 트리(펼침 · 체크) · 목록(체크 · 사용 안 함) · 아이콘 모드(선택한 아이콘) · 열 보기(화살표 · 크기 손잡이) ·
// 끌어 놓기 표시.
QWidget *qtItemViewsSection(const fs::ThemeColors &tc)
{
    auto *box = section(u"Qt 표준 위젯 — 항목 보기 (표 · 트리 · 목록 · 아이콘 모드 · 열 보기)"_s);
    auto *grid = new QGridLayout(box);
    grid->setContentsMargins(14, 12, 14, 14);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(6);

    const QIcon folder = folderIcon(tc[fs::Token::Folder]);
    const QIcon doc = fileIcon(tc[fs::Token::Fg3], tc[fs::Token::KDoc]);
    const QIcon image = fileIcon(tc[fs::Token::Fg3], tc[fs::Token::KImg]);

    auto *table = new QTableWidget(5, 3);
    table->setHorizontalHeaderLabels({u"이름"_s, u"크기"_s, u"종류"_s});
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSortIndicatorShown(true);
    table->horizontalHeader()->setSortIndicator(1, Qt::DescendingOrder);
    struct Row { QString name; QString size; QString kind; Qt::CheckState check; int icon; };
    const Row rows[] = {
        {u"문서"_s, u"—"_s, u"폴더"_s, Qt::Unchecked, 0},
        {u"보고서.pdf"_s, u"2.4 MB"_s, u"PDF"_s, Qt::Checked, 1},
        {u"사진_001.jpg"_s, u"5.1 MB"_s, u"JPEG"_s, Qt::PartiallyChecked, 2},
        {u"메모.txt"_s, u"3 KB"_s, u"텍스트"_s, Qt::Unchecked, 1},
        {u"백업.zip"_s, u"128 MB"_s, u"압축"_s, Qt::Unchecked, 1},
    };
    for (int r = 0; r < 5; ++r) {
        auto *name = new QTableWidgetItem(rows[r].icon == 0 ? folder : rows[r].icon == 2 ? image : doc, rows[r].name);
        name->setCheckState(rows[r].check);
        table->setItem(r, 0, name);
        auto *size = new QTableWidgetItem(rows[r].size);
        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(r, 1, size);
        table->setItem(r, 2, new QTableWidgetItem(rows[r].kind));
    }
    table->resizeColumnToContents(0);
    table->selectRow(1);
    table->setItemDelegate(new HoverRowDelegate(0, table));
    table->openPersistentEditor(table->item(3, 2));
    table->resizeRowsToContents();
    table->setFixedHeight(table->horizontalHeader()->sizeHint().height() + 5 * table->rowHeight(0) + 4);
    grid->addWidget(caption(u"표 — 가운데 맞춤 머리글의 정렬 표시 · 첫 줄 마우스 올림 · '종류' 칸 편집 중"_s, tc),
                    0, 0, 1, 2);
    grid->addWidget(table, 1, 0, 1, 2);

    auto *tree = new QTreeWidget;
    tree->setHeaderLabels({u"이름"_s, u"크기"_s});
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setStretchLastSection(false);
    tree->header()->resizeSection(1, 64);
    auto *root = new QTreeWidgetItem(tree, {u"D:\\Work"_s, QString()});
    root->setIcon(0, folder);
    auto *src = new QTreeWidgetItem(root, {u"src"_s, QString()});
    src->setIcon(0, folder);
    for (const QString &n : {u"main.cpp"_s, u"style.cpp"_s}) {
        auto *f = new QTreeWidgetItem(src, {n, u"12 KB"_s});
        f->setIcon(0, doc);
        f->setCheckState(0, n == u"main.cpp" ? Qt::Checked : Qt::Unchecked);
    }
    auto *docs = new QTreeWidgetItem(root, {u"docs"_s, QString()});
    docs->setIcon(0, folder);
    new QTreeWidgetItem(docs, {u"PLAN.md"_s, u"40 KB"_s});
    auto *other = new QTreeWidgetItem(tree, {u"E:\\Backup"_s, QString()});
    other->setIcon(0, folder);
    new QTreeWidgetItem(other, {u"2026-09"_s, QString()});
    root->setExpanded(true);
    src->setExpanded(true);
    tree->setCurrentItem(src->child(1));
    tree->setFixedHeight(190);

    auto *list = new QListWidget;
    for (const QString &n : {u"보고서.pdf"_s, u"사진_001.jpg"_s, u"메모.txt"_s, u"백업.zip"_s, u"잠긴 파일"_s}) {
        auto *it = new QListWidgetItem(n == u"사진_001.jpg" ? image : doc, n, list);
        if (n == u"메모.txt") {
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            it->setCheckState(Qt::Checked);
        }
        if (n == u"잠긴 파일")
            it->setFlags(it->flags() & ~Qt::ItemIsEnabled);
    }
    list->setCurrentRow(1);
    list->setItemDelegate(new HoverRowDelegate(3, list));
    list->setFixedHeight(190);
    grid->addWidget(caption(u"트리 — 펼침 · 체크 · 현재 항목 선택"_s, tc), 2, 0);
    grid->addWidget(caption(u"목록 — 체크 · 넷째 줄 마우스 올림 · 사용 안 함(마지막)"_s, tc), 2, 1);
    grid->addWidget(tree, 3, 0);
    grid->addWidget(list, 3, 1);

    auto *icons = new QListWidget;
    icons->setViewMode(QListView::IconMode);
    icons->setIconSize(QSize(32, 32));
    icons->setGridSize(QSize(84, 70));
    icons->setResizeMode(QListView::Adjust);
    icons->setMovement(QListView::Static);
    const QColor fileLine = tc[fs::Token::Fg3];
    for (const QString &n : {u"문서"_s, u"사진"_s, u"보고서.pdf"_s, u"사진_001.jpg"_s}) {
        const bool dir = n == u"문서" || n == u"사진";
        const QIcon icon = dir ? fs::glyphIcon(fs::Glyph::Folder, tc[fs::Token::Folder], 32)
                               : fs::glyphIcon(fs::Glyph::File, fileLine, 32,
                                               tc[n.endsWith(u".jpg") ? fs::Token::KImg : fs::Token::KPdf]);
        new QListWidgetItem(icon, n, icons);
    }
    icons->item(0)->setSelected(true);
    icons->item(2)->setSelected(true);
    icons->setFixedHeight(170);

    auto *model = new QStandardItemModel(box);
    QStandardItem *selectedChild = nullptr;
    for (const QString &top : {u"C:\\"_s, u"D:\\"_s, u"E:\\"_s}) {
        auto *item = new QStandardItem(folder, top);
        for (const QString &child : {u"Program Files"_s, u"Users"_s, u"Windows"_s}) {
            auto *c = new QStandardItem(folder, child);
            c->appendRow(new QStandardItem(doc, u"readme.txt"_s));
            item->appendRow(c);
            if (top == u"C:\\" && child == u"Users")
                selectedChild = c;
        }
        model->appendRow(item);
    }
    auto *columns = new QColumnView;
    columns->setModel(model);
    columns->setColumnWidths({90, 110, 110});
    columns->setCurrentIndex(model->indexFromItem(selectedChild));
    columns->setFixedHeight(150);

    auto *right = new QVBoxLayout;
    right->setSpacing(6);
    right->addWidget(columns);
    right->addWidget(caption(u"끌어 놓기 표시 — 항목 사이(선) · 항목 위(사각형)"_s, tc));
    right->addWidget(new DropIndicatorPreview);
    grid->addWidget(caption(u"아이콘 모드 — 첫째 · 셋째 선택"_s, tc), 4, 0);
    grid->addWidget(caption(u"열 보기 — 하위가 있는 항목의 화살표 · 열 크기 손잡이"_s, tc), 4, 1);
    grid->addWidget(icons, 5, 0, Qt::AlignTop);
    grid->addLayout(right, 5, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    return box;
}

// --parts: 대화상자 부품 구역만 (목업 대조용)
bool g_partsOnly = false;
// --qt-widgets: Qt 표준 위젯 구역만
bool g_qtWidgetsOnly = false;

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
    auto *title = caption(design + u" · "_s + fs::variantLabel(variant), tc, 12);
    QFont tf = title->font();
    tf.setWeight(QFont::DemiBold);
    title->setFont(tf);
    v->addWidget(title);
    if (g_partsOnly) {
        v->addWidget(dialogPartsSection(tc));
        v->addStretch(1);
        return tile;
    }
    if (g_qtWidgetsOnly) {
        v->addWidget(qtTabsSection(tc));
        v->addWidget(qtRangesSection(tc));
        v->addWidget(qtWindowSection(tc));
        v->addWidget(qtItemViewsSection(tc));
        v->addStretch(1);
        return tile;
    }
    v->addWidget(buttonsSection(tc));
    v->addWidget(dialogPartsSection(tc));
    v->addWidget(inputsSection(tc));
    v->addWidget(choicesSection(tc));
    v->addWidget(listsSection(tc));
    v->addWidget(progressSection(tc));
    v->addWidget(chromeSection(tc));
    v->addWidget(graphSection(tc));
    v->addWidget(traceSection(tc));
    v->addWidget(qtTabsSection(tc));
    v->addWidget(qtRangesSection(tc));
    v->addWidget(qtWindowSection(tc));
    v->addWidget(qtItemViewsSection(tc));
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
    const QCommandLineOption partsOption(u"parts"_s, u"대화상자 부품 구역만 보입니다."_s);
    const QCommandLineOption qtWidgetsOption(u"qt-widgets"_s, u"Qt 표준 위젯 구역만 보입니다."_s);
    const QCommandLineOption toneOption(u"dark-tone"_s, u"다크 색조: gray · navy (시안2)."_s, u"tone"_s, u"gray"_s);
    parser.addOptions({shotOption, accentOption, noFixOption, delayOption, designOption, partsOption, qtWidgetsOption,
                       toneOption});
    parser.process(app);
    g_partsOnly = parser.isSet(partsOption);
    g_qtWidgetsOnly = parser.isSet(qtWidgetsOption);
    if (parser.value(toneOption) == u"navy"_s)
        fs::ThemeManager::instance().setDarkTone(fs::ThemeManager::DarkTone::Navy);

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
        // 다크(남색)는 시안2 전용 — Main 보드처럼 라이트 · 다크 · 다크(남색) 세 타일
        if (fs::ThemeManager::instance().design() == fs::Design::Watercolor)
            h->addWidget(buildTile(fs::Variant::Navy), 1);
        TransferSimulator::instance().refreshReadouts();
    };
    rebuild();
    const bool shot = parser.isSet(shotOption);
    if (shot) {
        // 스크린샷은 내용 전체 — 창이 내용 높이만큼 커진다.
        outer->addWidget(tiles, 1);
    } else {
        auto *scroll = new QScrollArea;
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setWidget(tiles);
        outer->addWidget(scroll, 1);
    }
    QObject::connect(designGroup, &QButtonGroup::idClicked, &window, [&theme, rebuild](int id) {
        theme.setDesign(fs::Design(id));
        rebuild();
    });
    int height = 1700;
    if (!shot) {
        if (const QScreen *screen = QGuiApplication::primaryScreen())
            height = std::min(height, screen->availableGeometry().height() - 40);
    }
    window.resize(1480, height);
    window.show();

    QTimer ticker;
    QObject::connect(&ticker, &QTimer::timeout, [] { TransferSimulator::instance().step(); });
    ticker.start(int(TransferSimulator::kStepMs));

    if (shot) {
        const QString file = parser.value(shotOption);
        QTimer::singleShot(parser.value(delayOption).toInt(), &window, [&window, file] {
            window.grab().save(file);
            QApplication::quit();
        });
    }
    return app.exec();
}
