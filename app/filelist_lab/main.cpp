// 파일 목록 실험실 (P3) — FileListView(Qtitan) · 섬네일 두 구현 · 샘플/실제 폴더 원본을 두 패널에서 비교한다.
//   filelist_lab                                        Main 보드 기본 상태(왼쪽 샘플 1줄, 오른쪽 샘플 자동, 오른쪽 활성)
//   filelist_lab --design watercolor --scheme navy      시안2 · 다크(남색)
//   filelist_lab --left local:C:\Windows --left-mode thumb --thumb-backend qtitan
//   filelist_lab --sep2 tint --name-below --inv-cursor --inv-sel
//   filelist_lab --shot out.png                         스크린샷을 저장하고 끝냄
//
// 원본: mock-left(D:\Work\fm-core) · mock-right(D:\Downloads) · mock-thumbs(설정 › 섬네일 미리보기) · local:<경로>(읽기 전용)
// 조작: 클릭 = 커서만(TC 방식), Insert · Space = 표시, Enter · 두 번 클릭 = 폴더 열기, Backspace = 상위 폴더, Ctrl+휠 = 섬네일 크기.

#include <fmfilelist/FileListStats.h>
#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/FileSortProxy.h>
#include <fmfilelist/LocalFileSource.h>
#include <fmfilelist/MockFileSource.h>
#include <fmfilelist/ThumbnailView.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/SegmentedControl.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCommandLineParser>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;
namespace fl = fm::filelist;
namespace fs = fm::style;

namespace {

struct LabSettings
{
    fl::ListAppearance list;
    fl::ThumbnailAppearance thumbs;
    fl::ThumbnailView::Backend backend = fl::ThumbnailView::QtList;
};

fl::ViewMode modeFrom(const QString &name, fl::ViewMode fallback)
{
    if (name == u"1")
        return fl::ViewMode::OneLine;
    if (name == u"2")
        return fl::ViewMode::TwoLine;
    if (name == u"auto")
        return fl::ViewMode::Auto;
    if (name == u"thumb")
        return fl::ViewMode::Thumbnails;
    return fallback;
}

fl::RecordSeparator separatorFrom(const QString &name, fl::RecordSeparator fallback)
{
    if (name == u"none")
        return fl::RecordSeparator::None;
    if (name == u"zebra")
        return fl::RecordSeparator::Zebra;
    if (name == u"line")
        return fl::RecordSeparator::Line;
    if (name == u"space")
        return fl::RecordSeparator::Space;
    if (name == u"tint")
        return fl::RecordSeparator::Tint;
    return fallback;
}

QComboBox *combo(const QStringList &items, int current)
{
    auto *c = new QComboBox;
    c->addItems(items);
    c->setCurrentIndex(current);
    return c;
}

// ---------------------------------------------------------------- 패널
class Pane : public QWidget
{
    Q_OBJECT
public:
    explicit Pane(LabSettings *settings, QWidget *parent = nullptr)
        : QWidget(parent), m_settings(settings)
    {
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);

        auto *bar = new QWidget;
        auto *h = new QHBoxLayout(bar);
        h->setContentsMargins(8, 4, 8, 4);
        h->setSpacing(8);
        m_source = combo({u"샘플 — D:\\Work\\fm-core"_s, u"샘플 — D:\\Downloads"_s, u"샘플 — 섬네일 미리보기"_s,
                          u"실제 폴더(읽기 전용)"_s}, 0);
        m_path = new QLineEdit;
        m_path->setFont(fs::monoFont(12));
        m_path->setPlaceholderText(u"경로 입력 후 Enter"_s);
        m_mode = new fm::ui::SegmentedControl({u"1줄"_s, u"2줄"_s, u"자동"_s, u"섬네일"_s});
        m_mode->setSegmentSize(fm::ui::SegmentedControl::Small);
        m_mode->setItemToolTips({u"한 줄에 모든 열"_s, u"이름과 메타데이터를 두 줄로"_s, u"잘리는 이름이 많으면 2줄로"_s,
                                 u"섬네일 (Ctrl+Shift+4)"_s});
        h->addWidget(m_source);
        h->addWidget(m_path, 1);
        h->addWidget(m_mode);
        v->addWidget(bar);

        m_stack = new QStackedWidget;
        m_list = new fl::FileListView;
        m_thumbs = new fl::ThumbnailView;
        m_stack->addWidget(m_list);
        m_stack->addWidget(m_thumbs);
        v->addWidget(m_stack, 1);

        auto *status = new QWidget;
        auto *sh = new QHBoxLayout(status);
        sh->setContentsMargins(12, 4, 12, 4);
        m_statusLeft = new QLabel;
        m_statusRight = new QLabel;
        for (QLabel *l : {m_statusLeft, m_statusRight})
            l->setFont(fs::withTabularNumbers(fs::pixelFont(l->font(), 12)));
        sh->addWidget(m_statusLeft, 1);
        sh->addWidget(m_statusRight);
        v->addWidget(status);

        m_proxy = new fl::FileSortProxy(this);
        m_mock = new fl::FileListModel(this);

        connect(m_source, &QComboBox::currentIndexChanged, this, [this](int i) { setSourceIndex(i); });
        connect(m_path, &QLineEdit::returnPressed, this, [this] {
            if (!m_local)
                setSourceIndex(3);
            m_local->setPath(m_path->text());
        });
        connect(m_mode, &fm::ui::SegmentedControl::currentIndexChanged, this, [this](int i) {
            setViewMode(fl::ViewMode(i));
            Q_EMIT activateRequested(this);
        });
        for (QObject *view : {static_cast<QObject *>(m_list), static_cast<QObject *>(m_thumbs)}) {
            connect(view, SIGNAL(paneActivated()), this, SLOT(requestActivate()));
            connect(view, SIGNAL(activated(QModelIndex)), this, SLOT(open(QModelIndex)));
            connect(view, SIGNAL(upRequested()), this, SLOT(goUp()));
        }
        connect(m_list, &fl::FileListView::marksChanged, this, &Pane::updateStatus);
        connect(m_proxy, &QAbstractItemModel::dataChanged, this, &Pane::updateStatus);
        connect(m_proxy, &QAbstractItemModel::modelReset, this, &Pane::updateStatus);
        connect(m_proxy, &QAbstractItemModel::rowsInserted, this, &Pane::updateStatus);
        connect(m_proxy, &QAbstractItemModel::rowsRemoved, this, &Pane::updateStatus);
        connect(m_thumbs, &fl::ThumbnailView::sizeChanged, this, [this](int size) {
            m_settings->thumbs.size = size;
            Q_EMIT thumbnailSizeChanged(size);
        });
        m_list->setModel(m_proxy);
        m_thumbs->setModel(m_proxy);
    }

    void setSource(const QString &spec)
    {
        if (spec == u"mock-right")
            m_source->setCurrentIndex(1);
        else if (spec == u"mock-thumbs")
            m_source->setCurrentIndex(2);
        else if (spec.startsWith(u"local:")) {
            setSourceIndex(3);
            m_source->blockSignals(true);
            m_source->setCurrentIndex(3);
            m_source->blockSignals(false);
            m_local->setPath(spec.mid(6));
        } else {
            m_source->setCurrentIndex(0);
            setSourceIndex(0);
        }
    }

    void setViewMode(fl::ViewMode mode)
    {
        m_viewMode = mode;
        m_mode->blockSignals(true);
        m_mode->setCurrentIndex(int(mode));
        m_mode->blockSignals(false);
        const int cursor = currentCursor();
        m_list->setViewMode(mode);
        m_stack->setCurrentWidget(mode == fl::ViewMode::Thumbnails ? static_cast<QWidget *>(m_thumbs) : m_list);
        setCursor(cursor);
    }

    void setActive(bool active)
    {
        m_list->setPaneActive(active);
        m_thumbs->setPaneActive(active);
    }

    void applySettings()
    {
        m_list->setAppearance(m_settings->list);
        m_thumbs->setAppearance(m_settings->thumbs);
        m_thumbs->setBackend(m_settings->backend);
    }

    QString title() const { return m_local ? m_local->path() : m_mockPath; }

Q_SIGNALS:
    void activateRequested(Pane *pane);
    void thumbnailSizeChanged(int size);

private Q_SLOTS:
    void requestActivate() { Q_EMIT activateRequested(this); }

    void open(const QModelIndex &index)
    {
        if (m_local)
            m_local->open(index);
    }

    void goUp()
    {
        if (m_local)
            m_local->cdUp();
    }

    void updateStatus()
    {
        const fl::FileListStats stats = fl::FileListStats::compute(m_proxy);
        m_statusLeft->setText(stats.selectionText());
        QString right = stats.secondaryText();
        if (m_local)
            right += u"  ·  "_s + m_local->driveName() + u"  "_s + m_local->freeSpaceText();
        m_statusRight->setText(right);
    }

private:
    int currentCursor() const
    {
        return m_stack->currentWidget() == m_thumbs ? m_thumbs->cursorRow() : m_list->cursorRow();
    }

    void setCursor(int row)
    {
        m_list->setCursorRow(row);
        m_thumbs->setCursorRow(row);
    }

    void setSourceIndex(int index)
    {
        if (index == 3) {
            if (!m_local) {
                m_local = new fl::LocalFileSource(this);
                connect(m_local, &fl::LocalFileSource::pathChanged, this, [this](const QString &path, const QString &child) {
                    m_path->setText(path);
                    m_pendingChild = child;
                    QTimer::singleShot(0, this, &Pane::updateStatus);
                });
                connect(m_local->model(), &fl::FileSystemListProxy::loaded, this, [this] {
                    // 위로 올라왔으면 방금 나온 폴더에 커서를 둔다(TC 방식)
                    int row = 0;
                    for (int r = 0; r < m_proxy->rowCount() && !m_pendingChild.isEmpty(); ++r) {
                        if (m_proxy->index(r, fl::NameColumn).data(fl::FullNameRole).toString() == m_pendingChild) {
                            row = r;
                            break;
                        }
                    }
                    m_pendingChild.clear();
                    setCursor(row);
                    updateStatus();
                });
                m_local->setPath(QDir::homePath());
            }
            m_proxy->setSourceModel(m_local->model());
            m_proxy->setShowSystem(false);
            m_proxy->sort(fl::NameColumn, Qt::AscendingOrder);
            m_list->setSortIndicator(fl::NameColumn, Qt::AscendingOrder, true);
            m_path->setText(m_local->path());
            setCursor(0);
        } else {
            const fl::MockFolder folder = index == 1 ? fl::MockFileSource::right()
                                        : index == 2 ? fl::MockFileSource::thumbnailPreview()
                                                     : fl::MockFileSource::left();
            if (m_local) {
                m_local->deleteLater();
                m_local = nullptr;
            }
            m_mock->setEntries(folder.entries);
            m_proxy->setSourceModel(m_mock);
            m_proxy->setShowSystem(true);
            m_proxy->sort(-1);  // 보드의 순서 그대로
            m_list->setSortIndicator(fl::NameColumn, Qt::AscendingOrder, false);  // "이름 ↑" 표시만
            m_mockPath = folder.path;
            m_path->setText(folder.path);
            setCursor(folder.cursor);
        }
        updateStatus();
    }

    LabSettings *m_settings;
    QComboBox *m_source = nullptr;
    QLineEdit *m_path = nullptr;
    fm::ui::SegmentedControl *m_mode = nullptr;
    QStackedWidget *m_stack = nullptr;
    fl::FileListView *m_list = nullptr;
    fl::ThumbnailView *m_thumbs = nullptr;
    QLabel *m_statusLeft = nullptr;
    QLabel *m_statusRight = nullptr;
    fl::FileSortProxy *m_proxy = nullptr;
    fl::FileListModel *m_mock = nullptr;
    fl::LocalFileSource *m_local = nullptr;
    QString m_mockPath;
    QString m_pendingChild;
    fl::ViewMode m_viewMode = fl::ViewMode::OneLine;
};

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"filelist_lab"_s);

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption designOption(u"design"_s, u"standard | watercolor"_s, u"name"_s, u"standard"_s);
    const QCommandLineOption schemeOption(u"scheme"_s, u"light | dark | navy"_s, u"name"_s, u"light"_s);
    const QCommandLineOption leftOption(u"left"_s, u"mock-left | mock-right | mock-thumbs | local:<경로>"_s, u"source"_s, u"mock-left"_s);
    const QCommandLineOption rightOption(u"right"_s, u"mock-left | mock-right | mock-thumbs | local:<경로>"_s, u"source"_s, u"mock-right"_s);
    const QCommandLineOption leftModeOption(u"left-mode"_s, u"1 | 2 | auto | thumb"_s, u"mode"_s, u"1"_s);
    const QCommandLineOption rightModeOption(u"right-mode"_s, u"1 | 2 | auto | thumb"_s, u"mode"_s, u"auto"_s);
    const QCommandLineOption sep1Option(u"sep1"_s, u"1줄 구분: none | zebra | line | space | tint"_s, u"name"_s, u"none"_s);
    const QCommandLineOption sep2Option(u"sep2"_s, u"2줄 구분: none | zebra | line | space | tint"_s, u"name"_s, u"line"_s);
    const QCommandLineOption nameBelowOption(u"name-below"_s, u"2줄 레코드에서 이름을 아래 줄에"_s);
    const QCommandLineOption invCursorOption(u"inv-cursor"_s, u"역상 커서"_s);
    const QCommandLineOption invSelOption(u"inv-sel"_s, u"역상 선택"_s);
    const QCommandLineOption backendOption(u"thumb-backend"_s, u"qt | qtitan"_s, u"name"_s, u"qt"_s);
    const QCommandLineOption thumbSizeOption(u"thumb-size"_s, u"64 | 96 | 160 | 256"_s, u"px"_s, u"96"_s);
    const QCommandLineOption activeOption(u"active"_s, u"left | right"_s, u"side"_s, u"right"_s);
    const QCommandLineOption sizeOption(u"size"_s, u"창 크기 WxH"_s, u"size"_s, u"1440x760"_s);
    const QCommandLineOption shotOption(u"shot"_s, u"스크린샷을 저장하고 끝냅니다."_s, u"file"_s);
    const QCommandLineOption delayOption(u"shot-delay"_s, u"스크린샷까지 기다릴 시간(ms)."_s, u"ms"_s, u"700"_s);
    parser.addOptions({designOption, schemeOption, leftOption, rightOption, leftModeOption, rightModeOption, sep1Option,
                       sep2Option, nameBelowOption, invCursorOption, invSelOption, backendOption, thumbSizeOption,
                       activeOption, sizeOption, shotOption, delayOption});
    parser.process(app);

    auto &theme = fs::ThemeManager::instance();
    theme.setDesign(parser.value(designOption) == u"watercolor"_s ? fs::Design::Watercolor : fs::Design::Standard);
    const QString scheme = parser.value(schemeOption);
    theme.setDarkTone(scheme == u"navy"_s ? fs::ThemeManager::DarkTone::Navy : fs::ThemeManager::DarkTone::Gray);
    theme.setScheme(scheme == u"light"_s ? fs::ThemeManager::Scheme::Light : fs::ThemeManager::Scheme::Dark);
    theme.install(app);

    LabSettings settings;
    settings.list.oneLineSeparator = separatorFrom(parser.value(sep1Option), fl::RecordSeparator::None);
    settings.list.twoLineSeparator = separatorFrom(parser.value(sep2Option), fl::RecordSeparator::Line);
    settings.list.nameBelow = parser.isSet(nameBelowOption);
    settings.list.invertCursor = parser.isSet(invCursorOption);
    settings.list.invertSelection = parser.isSet(invSelOption);
    settings.thumbs.invertCursor = settings.list.invertCursor;
    settings.thumbs.invertSelection = settings.list.invertSelection;
    settings.thumbs.size = parser.value(thumbSizeOption).toInt();
    settings.backend = parser.value(backendOption) == u"qtitan"_s ? fl::ThumbnailView::QtitanCards : fl::ThumbnailView::QtList;

    QWidget window;
    window.setWindowTitle(u"파일 목록 실험실 — FileListView · 섬네일 두 구현"_s);
    auto *root = new QVBoxLayout(&window);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 위쪽: 디자인 · 색 구성표 · 표시 설정
    auto *top = new QWidget;
    auto *bar = new QHBoxLayout(top);
    bar->setContentsMargins(10, 6, 10, 6);
    bar->setSpacing(8);
    const QStringList separators = {u"없음"_s, u"교차 배경"_s, u"구분선"_s, u"여백"_s, u"틴트"_s};
    auto *design = combo({u"시안1 · 기본"_s, u"시안2 · 워터컬러"_s}, int(theme.design()));
    auto *variant = combo({u"라이트"_s, u"다크"_s, u"다크(남색)"_s}, scheme == u"navy"_s ? 2 : (scheme == u"dark"_s ? 1 : 0));
    auto *sep1 = combo(separators, int(settings.list.oneLineSeparator));
    auto *sep2 = combo(separators, int(settings.list.twoLineSeparator));
    auto *namePos = combo({u"이름 위"_s, u"이름 아래"_s}, settings.list.nameBelow ? 1 : 0);
    auto *invCursor = new QCheckBox(u"역상 커서"_s);
    auto *invSel = new QCheckBox(u"역상 선택"_s);
    invCursor->setChecked(settings.list.invertCursor);
    invSel->setChecked(settings.list.invertSelection);
    auto *backend = combo({u"섬네일: Qt 목록"_s, u"섬네일: Qtitan 카드"_s}, int(settings.backend));
    auto *thumbSize = combo({u"64"_s, u"96"_s, u"160"_s, u"256"_s}, 1);
    thumbSize->setCurrentText(QString::number(settings.thumbs.size));
    for (auto [label, w] : {std::pair<QString, QWidget *>{u"디자인"_s, design}, {u"색"_s, variant},
                            {u"1줄 구분"_s, sep1}, {u"2줄 구분"_s, sep2}, {u"이름"_s, namePos}}) {
        bar->addWidget(new QLabel(label));
        bar->addWidget(w);
    }
    bar->addWidget(invCursor);
    bar->addWidget(invSel);
    bar->addStretch(1);
    bar->addWidget(backend);
    bar->addWidget(new QLabel(u"크기"_s));
    bar->addWidget(thumbSize);
    root->addWidget(top);

    auto *splitter = new QSplitter;
    splitter->setChildrenCollapsible(false);
    auto *left = new Pane(&settings);
    auto *right = new Pane(&settings);
    splitter->addWidget(left);
    splitter->addWidget(right);
    root->addWidget(splitter, 1);

    auto applyAll = [&] {
        left->applySettings();
        right->applySettings();
    };
    auto activate = [&](Pane *pane) {
        left->setActive(pane == left);
        right->setActive(pane == right);
        window.setWindowTitle(pane->title() + u" — 파일 목록 실험실"_s);
    };
    QObject::connect(left, &Pane::activateRequested, &window, activate);
    QObject::connect(right, &Pane::activateRequested, &window, activate);

    applyAll();
    left->setSource(parser.value(leftOption));
    right->setSource(parser.value(rightOption));
    left->setViewMode(modeFrom(parser.value(leftModeOption), fl::ViewMode::OneLine));
    right->setViewMode(modeFrom(parser.value(rightModeOption), fl::ViewMode::Auto));
    activate(parser.value(activeOption) == u"left"_s ? left : right);

    QObject::connect(design, &QComboBox::currentIndexChanged, &window, [&](int i) {
        QMetaObject::invokeMethod(&window, [&, i] { theme.setDesign(i == 1 ? fs::Design::Watercolor : fs::Design::Standard); },
                                  Qt::QueuedConnection);
    });
    QObject::connect(variant, &QComboBox::currentIndexChanged, &window, [&](int i) {
        QMetaObject::invokeMethod(&window, [&, i] {
            theme.setDarkTone(i == 2 ? fs::ThemeManager::DarkTone::Navy : fs::ThemeManager::DarkTone::Gray);
            theme.setScheme(i == 0 ? fs::ThemeManager::Scheme::Light : fs::ThemeManager::Scheme::Dark);
        }, Qt::QueuedConnection);
    });
    QObject::connect(sep1, &QComboBox::currentIndexChanged, &window, [&](int i) {
        settings.list.oneLineSeparator = fl::RecordSeparator(i);
        applyAll();
    });
    QObject::connect(sep2, &QComboBox::currentIndexChanged, &window, [&](int i) {
        settings.list.twoLineSeparator = fl::RecordSeparator(i);
        applyAll();
    });
    QObject::connect(namePos, &QComboBox::currentIndexChanged, &window, [&](int i) {
        settings.list.nameBelow = i == 1;
        applyAll();
    });
    QObject::connect(invCursor, &QCheckBox::toggled, &window, [&](bool on) {
        settings.list.invertCursor = settings.thumbs.invertCursor = on;
        applyAll();
    });
    QObject::connect(invSel, &QCheckBox::toggled, &window, [&](bool on) {
        settings.list.invertSelection = settings.thumbs.invertSelection = on;
        applyAll();
    });
    QObject::connect(backend, &QComboBox::currentIndexChanged, &window, [&](int i) {
        settings.backend = fl::ThumbnailView::Backend(i);
        applyAll();
    });
    QObject::connect(thumbSize, &QComboBox::currentTextChanged, &window, [&](const QString &text) {
        settings.thumbs.size = text.toInt();
        applyAll();
    });
    auto syncSize = [&](int size) {
        thumbSize->blockSignals(true);
        thumbSize->setCurrentText(QString::number(size));
        thumbSize->blockSignals(false);
        applyAll();
    };
    QObject::connect(left, &Pane::thumbnailSizeChanged, &window, syncSize);
    QObject::connect(right, &Pane::thumbnailSizeChanged, &window, syncSize);

    const QStringList wh = parser.value(sizeOption).split(u'x');
    window.resize(wh.value(0).toInt() > 0 ? wh.value(0).toInt() : 1440, wh.value(1).toInt() > 0 ? wh.value(1).toInt() : 760);
    splitter->setSizes({1, 1});
    window.show();

    if (parser.isSet(shotOption)) {
        const QString file = parser.value(shotOption);
        QTimer::singleShot(parser.value(delayOption).toInt(), &window, [&window, file] {
            window.grab().save(file);
            QApplication::quit();
        });
    }
    return app.exec();
}

#include "main.moc"
