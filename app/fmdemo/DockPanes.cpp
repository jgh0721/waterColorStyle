#include "DockPanes.h"

#include <fmdialogs/ProgressDialog.h>
#include <fmdialogs/ProgressSimulator.h>
#include <fmfilelist/FileIconPainter.h>
#include <fmfilelist/FileRoles.h>
#include <fmfilelist/ThumbnailPainter.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFontDatabase>
#include <QHeaderView>
#include <QIconEngine>
#include <QImageReader>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;
namespace fl = fm::filelist;
namespace fs = fm::style;
namespace fd = fm::dialogs;

namespace fm::app {

namespace {

constexpr qint64 kMaxImageBytes = 64 * 1024 * 1024;  // 이보다 큰 그림은 읽지 않는다(종류 아이콘)
constexpr qint64 kTextHeadBytes = 16 * 1024;         // 글 파일은 앞부분만
constexpr int kImageBox = 480;                       // 읽을 때 줄이는 크기(논리 px)

/// 글자색을 토큰으로 — 테마가 바뀌면 다시 칠한다.
void tintLabel(QLabel *label, fs::Token token)
{
    auto apply = [label, token] {
        QPalette pal = label->palette();
        pal.setColor(QPalette::WindowText, fs::themeColorsFor(label)[token]);
        label->setPalette(pal);
    };
    apply();
    QObject::connect(&fs::ThemeManager::instance(), &fs::ThemeManager::changed, label, apply);
}

/// 폴더 트리 아이콘 — 목록과 같은 폴더 글리프를 그릴 때의 테마 색으로(디자인 · 다크 전환을 따른다).
class FolderIconEngine : public QIconEngine
{
public:
    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        const fs::ThemeColors &tc = fs::ThemeManager::instance().colors();
        const qreal side = std::min(rect.width(), rect.height());
        const QRectF box(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0, side, side);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        fl::FileIconPainter::paint(painter, box, fl::Kind::Folder, tc[fs::Token::Fg2], tc, mode == QIcon::Disabled ? 0.5 : 1.0);
        painter->restore();
    }
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }
    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QPixmap pm(size * scale);
        pm.setDevicePixelRatio(scale);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        paint(&p, QRect(QPoint(), size), mode, state);
        return pm;
    }
    QIconEngine *clone() const override { return new FolderIconEngine; }
};

/// 드라이브는 셸 아이콘, 폴더는 목록과 같은 글리프.
class TreeIconProvider : public QFileIconProvider
{
public:
    TreeIconProvider() : m_folder(new FolderIconEngine) { setOptions(DontUseCustomDirectoryIcons); }

    QIcon icon(IconType type) const override
    {
        return type == Folder ? m_folder : QFileIconProvider::icon(type);
    }
    QIcon icon(const QFileInfo &info) const override
    {
        return info.isRoot() ? QFileIconProvider::icon(info) : m_folder;
    }

private:
    QIcon m_folder;
};

TreeIconProvider *treeIconProvider()
{
    static TreeIconProvider provider;  // 모델보다 오래 산다
    return &provider;
}

bool isTextFile(const QString &ext, fl::Kind kind)
{
    static const QStringList known = {u"txt"_s, u"md"_s, u"log"_s, u"ini"_s, u"cfg"_s, u"conf"_s, u"json"_s, u"xml"_s,
                                      u"csv"_s, u"tsv"_s, u"yml"_s, u"yaml"_s, u"toml"_s, u"cmake"_s, u"bat"_s, u"cmd"_s,
                                      u"ps1"_s, u"sh"_s, u"html"_s, u"htm"_s, u"css"_s, u"svg"_s, u"gitignore"_s};
    return kind == fl::Kind::Code || known.contains(ext, Qt::CaseInsensitive);
}

/// "읽기 전용 · 보관" — 속성 글자(rahs)의 긴 이름.
QString attributeNames(int attributes)
{
    QStringList names;
    if (attributes & fl::ReadOnly)
        names << u"읽기 전용"_s;
    if (attributes & fl::Archive)
        names << u"보관"_s;
    if (attributes & fl::Hidden)
        names << u"숨김"_s;
    if (attributes & fl::System)
        names << u"시스템"_s;
    if (attributes & fl::ReparsePoint)
        names << u"연결 지점"_s;
    return names.isEmpty() ? u"없음"_s : names.join(u" · "_s);
}

QString parentPath(const QString &path)
{
    return QDir::toNativeSeparators(QFileInfo(QDir::fromNativeSeparators(path)).path());
}

} // namespace

// ================================================================== 폴더 트리

FolderTreePane::FolderTreePane(QWidget *parent)
    : QTreeView(parent)
{
    setObjectName(u"folderTree"_s);
    setHeaderHidden(true);
    setUniformRowHeights(true);
    setFrameShape(QFrame::NoFrame);
    setEditTriggers(NoEditTriggers);
    setSelectionMode(SingleSelection);
    header()->setStretchLastSection(false);
    connect(this, &QTreeView::expanded, this, &FolderTreePane::fitColumn);
    connect(this, &QTreeView::collapsed, this, &FolderTreePane::fitColumn);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &FolderTreePane::fitColumn);
    connect(this, &QTreeView::clicked, this, &FolderTreePane::emitFolder);
    connect(this, &QTreeView::activated, this, [this](const QModelIndex &index) {
        // 한 번 눌러 활성화하는 스타일이면 clicked가 이미 보냈다 — Enter만 받는다
        if (!style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, nullptr, this))
            emitFolder(index);
    });
}

void FolderTreePane::showEvent(QShowEvent *event)
{
    ensureModel();
    QTreeView::showEvent(event);
}

void FolderTreePane::resizeEvent(QResizeEvent *event)
{
    QTreeView::resizeEvent(event);
    fitColumn();
}

void FolderTreePane::fitColumn()
{
    if (m_model)
        header()->resizeSection(0, std::max(viewport()->width(), sizeHintForColumn(0)));
}

void FolderTreePane::ensureModel()
{
    if (m_model)
        return;
    m_model = new QFileSystemModel(this);
    m_model->setIconProvider(treeIconProvider());
    m_model->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
    m_model->setReadOnly(true);
    m_model->setRootPath(QString());  // 내 PC — 드라이브부터
    connect(m_model, &QFileSystemModel::directoryLoaded, this, &FolderTreePane::fitColumn);
    setModel(m_model);
    for (int c = 1; c < m_model->columnCount(); ++c)
        hideColumn(c);
    follow(m_pending);
}

void FolderTreePane::follow(const QString &path)
{
    m_pending = path;
    if (!m_model)
        return;
    const QModelIndex index = path.isEmpty() ? QModelIndex() : m_model->index(QDir::fromNativeSeparators(path));
    if (!index.isValid()) {
        selectionModel()->clear();
        return;
    }
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    scrollTo(index);  // 접힌 상위 폴더를 펼친다
}

void FolderTreePane::emitFolder(const QModelIndex &index)
{
    if (m_model && index.isValid())
        Q_EMIT folderActivated(QDir::toNativeSeparators(m_model->filePath(index)));
}

// ================================================================== 미리보기

/// 그림 칸 — 실제 이미지(맞춰 줄임 · 1 px 테두리), 샘플의 가짜 섬네일, 종류 아이콘, 빈 상태 안내.
class PreviewPane::Picture : public QWidget
{
public:
    enum class Mode : std::uint8_t { Empty, Image, Art, Icon };

    using QWidget::QWidget;

    void clear() { set(Mode::Empty); }
    void setImage(const QImage &image)
    {
        m_image = image;
        set(Mode::Image);
    }
    void setArt(fl::Art art, qreal aspect)
    {
        m_art = art;
        m_aspect = aspect;
        set(Mode::Art);
    }
    void setKind(fl::Kind kind)
    {
        m_kind = kind;
        set(Mode::Icon);
    }

    QSize sizeHint() const override { return {240, 200}; }
    QSize minimumSizeHint() const override { return {96, 80}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF box = QRectF(rect()).adjusted(12, 12, -12, -12);
        switch (m_mode) {
        case Mode::Image: {
            const QSizeF natural = m_image.deviceIndependentSize();
            const QSizeF size = natural.width() > box.width() || natural.height() > box.height()
                                    ? natural.scaled(box.size(), Qt::KeepAspectRatio)
                                    : natural;
            const QRectF r(box.center().x() - size.width() / 2.0, box.center().y() - size.height() / 2.0, size.width(),
                           size.height());
            p.drawImage(r, m_image);
            p.setPen(QPen(tc[fs::Token::Line], 1));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(-0.5, -0.5, 0.5, 0.5));
            break;
        }
        case Mode::Art: {
            const qreal aspect = m_aspect > 0 ? m_aspect : 4.0 / 3.0;
            const QSizeF size = QSizeF(aspect * 100, 100).scaled(box.size().boundedTo(QSizeF(280, 280)), Qt::KeepAspectRatio);
            fl::ThumbnailPainter::paintMockArt(&p, QRectF(box.center().x() - size.width() / 2.0,
                                                          box.center().y() - size.height() / 2.0, size.width(),
                                                          size.height()),
                                               m_art);
            break;
        }
        case Mode::Icon: {
            const qreal g = std::min({box.width(), box.height(), 72.0}) * (m_kind == fl::Kind::Up ? 0.75 : 1.0);
            fl::FileIconPainter::paint(&p, QRectF(box.center().x() - g / 2.0, box.center().y() - g / 2.0, g, g), m_kind,
                                       tc[fs::Token::Fg3], tc);
            break;
        }
        case Mode::Empty:
            p.setPen(tc[fs::Token::Fg3]);
            p.drawText(box, Qt::AlignCenter | Qt::TextWordWrap, u"파일을 고르면 여기에 미리보기가 보입니다"_s);
            break;
        }
    }

private:
    void set(Mode mode)
    {
        m_mode = mode;
        if (mode != Mode::Image)
            m_image = QImage();
        update();
    }

    Mode m_mode = Mode::Empty;
    QImage m_image;
    fl::Art m_art = fl::Art::None;
    qreal m_aspect = 0;
    fl::Kind m_kind = fl::Kind::Other;
};

PreviewPane::PreviewPane(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(u"previewPane"_s);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 10);
    v->setSpacing(4);
    m_picture = new Picture;
    m_text = new QPlainTextEdit;
    m_text->setReadOnly(true);
    m_text->setFrameShape(QFrame::NoFrame);
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSizeF(9);
    m_text->setFont(mono);
    m_stack = new QStackedWidget;
    m_stack->addWidget(m_picture);
    m_stack->addWidget(m_text);
    m_name = new QLabel;
    m_info = new QLabel;
    QFont bold = m_name->font();
    bold.setWeight(QFont::DemiBold);
    m_name->setFont(bold);
    for (QLabel *label : {m_name, m_info}) {
        label->setWordWrap(true);
        label->setContentsMargins(10, 0, 10, 0);
        // 긴 이름이 도크(펼친 창)의 최소 폭을 늘리지 않게 — 줄 바꿈 높이만 따른다
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    tintLabel(m_info, fs::Token::Fg2);
    v->addWidget(m_stack, 1);
    v->addWidget(m_name);
    v->addWidget(m_info);
    showItem({}, false);
}

void PreviewPane::showItem(const QModelIndex &index, bool local)
{
    if (!index.isValid()) {
        m_picture->clear();
        m_stack->setCurrentWidget(m_picture);
        m_name->clear();
        m_info->clear();
        m_name->hide();
        m_info->hide();
        m_content = u"empty"_s;
        m_path.clear();
        return;
    }
    const bool up = index.data(fl::IsUpRole).toBool();
    const bool dir = index.data(fl::IsDirRole).toBool();
    const auto kind = fl::Kind(index.data(fl::KindRole).toInt());
    const QString name = index.data(fl::FullNameRole).toString();
    const QString path = index.data(fl::FilePathRole).toString();
    const QString ext = index.data(fl::ExtRole).toString();

    m_name->setText(up ? u"상위 폴더"_s : name);
    m_name->setToolTip(up ? QString() : name);
    QStringList info;
    if (!up)
        info << index.data(fl::TypeNameRole).toString();
    if (const QString size = index.data(fl::SizeTextRole).toString(); !size.isEmpty())
        info << size;
    if (const QDateTime modified = index.data(fl::ModifiedRole).toDateTime(); modified.isValid() && !up)
        info << fl::formatDate(modified);
    info.removeAll(QString());
    m_info->setText(info.join(u" · "_s));
    m_name->show();
    m_info->setVisible(!info.isEmpty());

    // 실제 파일만 연다(샘플 경로가 이 PC에 우연히 있어도 읽지 않는다)
    const QFileInfo file(path);
    const bool real = local && !up && !dir && file.isFile();
    if (real && path == m_path && m_content != u"icon")
        return;
    m_path = real ? path : QString();
    if (real && fl::isImageExtension(ext) && file.size() <= kMaxImageBytes) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const int box = int(kImageBox * devicePixelRatioF());
        const QSize natural = reader.size();
        const bool scaled = natural.isValid() && (natural.width() > box || natural.height() > box);
        if (scaled)
            reader.setScaledSize(natural.scaled(box, box, Qt::KeepAspectRatio));
        QImage image = reader.read();
        if (!image.isNull()) {
            if (scaled)
                image.setDevicePixelRatio(devicePixelRatioF());  // 줄여 읽은 그림은 화면 픽셀 그대로
            m_picture->setImage(image);
            m_stack->setCurrentWidget(m_picture);
            m_content = u"image"_s;
            return;
        }
    }
    if (real && isTextFile(ext, kind)) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            m_text->setPlainText(QString::fromUtf8(f.read(kTextHeadBytes)));
            m_stack->setCurrentWidget(m_text);
            m_content = u"text"_s;
            return;
        }
    }
    m_stack->setCurrentWidget(m_picture);
    const auto art = fl::Art(index.data(fl::ArtRole).toInt());
    if (art == fl::Art::Shot || art == fl::Art::Video || art == fl::Art::Pdf || art == fl::Art::Photo) {
        m_picture->setArt(art, index.data(fl::AspectRole).toReal());
        m_content = u"art"_s;
    } else if (const QImage thumb = index.data(fl::ThumbnailRole).value<QImage>(); !thumb.isNull()) {
        m_picture->setImage(thumb);
        m_content = u"image"_s;
    } else {
        m_picture->setKind(kind);
        m_content = u"icon"_s;
    }
}

// ================================================================== 속성

PropertiesPane::PropertiesPane(QWidget *parent)
    : QTreeWidget(parent)
{
    setObjectName(u"propertiesPane"_s);
    setColumnCount(2);
    setHeaderLabels({u"항목"_s, u"값"_s});
    setRootIsDecorated(false);
    setUniformRowHeights(true);
    setFrameShape(QFrame::NoFrame);
    setTextElideMode(Qt::ElideMiddle);
    setWordWrap(false);
    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header()->setStretchLastSection(true);
    fs::setFlatHeader(this);
}

void PropertiesPane::showItem(const QModelIndex &index, bool local)
{
    clear();
    if (!index.isValid())
        return;
    auto add = [this](const QString &key, const QString &value) {
        auto *item = new QTreeWidgetItem(this, {key, value});
        item->setToolTip(1, value);
    };
    const bool up = index.data(fl::IsUpRole).toBool();
    const bool dir = index.data(fl::IsDirRole).toBool();
    const QString path = index.data(fl::FilePathRole).toString();
    add(u"이름"_s, up ? u".."_s : index.data(fl::FullNameRole).toString());
    add(u"종류"_s, up ? u"상위 폴더"_s : index.data(fl::TypeNameRole).toString());
    if (!dir) {
        const qint64 bytes = index.data(fl::SizeBytesRole).toLongLong();
        add(u"크기"_s, bytes >= 0 ? u"%1 (%2 바이트)"_s.arg(fl::formatSize(bytes), QLocale().toString(bytes)) : u"—"_s);
    }
    if (const QDateTime modified = index.data(fl::ModifiedRole).toDateTime(); modified.isValid())
        add(u"수정한 날짜"_s, fl::formatDate(modified, u"yyyy-MM-dd HH:mm:ss"_s));
    const QFileInfo file(path);
    if (local && file.exists()) {
        if (const QDateTime born = file.birthTime(); born.isValid())
            add(u"만든 날짜"_s, fl::formatDate(born, u"yyyy-MM-dd HH:mm:ss"_s));
        if (!dir && fl::isImageExtension(index.data(fl::ExtRole).toString())) {
            if (const QSize px = QImageReader(path).size(); px.isValid())
                add(u"그림 크기"_s, u"%1 × %2 픽셀"_s.arg(px.width()).arg(px.height()));
        }
    }
    const int attributes = index.data(fl::AttributesRole).toInt();
    add(u"속성"_s, u"%1 — %2"_s.arg(fl::attributeText(attributes), attributeNames(attributes)));
    if (!path.isEmpty())
        add(u"위치"_s, parentPath(path));
    add(u"원본"_s, local ? u"이 PC (읽기 전용)"_s : u"샘플 데이터"_s);
}

QString PropertiesPane::value(const QString &key) const
{
    for (int i = 0; i < topLevelItemCount(); ++i) {
        if (topLevelItem(i)->text(0) == key)
            return topLevelItem(i)->text(1);
    }
    return {};
}

// ================================================================== 작업 대기열

namespace {

constexpr int kProgressStateRole = Qt::UserRole + 1;  // "paused" · "" — 막대 색(fmProgress)

/// 진행 열 — 항목 바탕(선택 · 마우스 올림) 위에 스타일의 진행 막대. 일시 중지 색은 위젯 대신
/// 옵션의 styleObject에 fmProgress를 실어 넘긴다.
class JobProgressDelegate : public QStyledItemDelegate
{
public:
    explicit JobProgressDelegate(QObject *parent)
        : QStyledItemDelegate(parent)
        , m_paused(new QObject(this))
    {
        m_paused->setProperty(fs::props::kProgressState, u"paused"_s);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem item = option;
        initStyleOption(&item, index);
        item.text.clear();
        const QWidget *w = option.widget;
        QStyle *style = w ? w->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, w);

        QStyleOptionProgressBar bar;
        bar.state = (option.state & QStyle::State_Enabled) | QStyle::State_Horizontal;
        bar.direction = option.direction;
        bar.palette = option.palette;
        bar.fontMetrics = option.fontMetrics;
        const int h = std::min(8, option.rect.height() - 4);
        bar.rect = QRect(option.rect.left() + 6, option.rect.center().y() - h / 2 + 1, option.rect.width() - 12, h);
        bar.minimum = 0;
        bar.maximum = 100;
        bar.progress = std::clamp(index.data(Qt::UserRole).toInt(), 0, 100);
        bar.textVisible = false;
        if (index.data(kProgressStateRole).toString() == u"paused")
            bar.styleObject = m_paused;
        style->drawControl(QStyle::CE_ProgressBar, &bar, painter, w);
    }

private:
    QObject *m_paused;
};

QString jobStatus(const fd::ProgressDialog *job, int *percent, bool *paused)
{
    const fd::ProgressSimulator *sim = job->simulator();
    *percent = sim ? sim->percent() : 0;
    *paused = false;
    if (job->isWaiting())
        return u"대기 중"_s;
    if (job->isDone() || job->property("fmCompleted").toBool()) {
        *percent = 100;
        return u"완료"_s;
    }
    *paused = job->isPaused();
    if (*paused)
        return u"일시 중지 — %1 %"_s.arg(*percent);
    const QString remaining = sim ? sim->remainingText() : QString();
    return remaining.isEmpty() || remaining == u"—" ? u"%1 %"_s.arg(*percent)
                                                    : u"%1 % · %2 남음"_s.arg(*percent).arg(remaining);
}

} // namespace

JobsPane::JobsPane(Provider provider, QWidget *parent)
    : QWidget(parent)
    , m_provider(std::move(provider))
{
    setObjectName(u"jobsPane"_s);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    m_list = new QTreeWidget;
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({u"작업"_s, u"진행"_s, u"상태"_s});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setItemDelegateForColumn(1, new JobProgressDelegate(m_list));
    m_list->header()->resizeSection(0, 220);
    m_list->header()->resizeSection(1, 160);
    m_list->header()->setStretchLastSection(true);
    fs::setFlatHeader(m_list);
    m_empty = new QLabel(u"진행 중인 파일 작업이 없습니다"_s);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    tintLabel(m_empty, fs::Token::Fg3);
    m_stack = new QStackedWidget;
    m_stack->addWidget(m_empty);
    m_stack->addWidget(m_list);
    v->addWidget(m_stack);

    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &JobsPane::refresh);
    connect(m_list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const auto id = item->data(0, Qt::UserRole).value<quintptr>();
        for (fd::ProgressDialog *job : m_provider()) {
            if (quintptr(job) == id) {
                job->show();
                job->raise();
                job->activateWindow();
            }
        }
    });
    refresh();
}

void JobsPane::refresh()
{
    const QList<fd::ProgressDialog *> jobs = m_provider ? m_provider() : QList<fd::ProgressDialog *>();
    bool same = jobs.size() == m_list->topLevelItemCount();
    for (int i = 0; same && i < jobs.size(); ++i)
        same = m_list->topLevelItem(i)->data(0, Qt::UserRole).value<quintptr>() == quintptr(jobs.at(i));
    if (!same) {
        m_list->clear();
        for (fd::ProgressDialog *job : jobs) {
            auto *item = new QTreeWidgetItem(m_list);
            item->setData(0, Qt::UserRole, QVariant::fromValue(quintptr(job)));
        }
    }
    for (int i = 0; i < jobs.size(); ++i) {
        QTreeWidgetItem *item = m_list->topLevelItem(i);
        int percent = 0;
        bool paused = false;
        const QString status = jobStatus(jobs.at(i), &percent, &paused);
        item->setText(0, jobs.at(i)->summaryText());
        item->setToolTip(0, jobs.at(i)->windowTitle());
        item->setData(1, Qt::UserRole, percent);
        item->setData(1, kProgressStateRole, paused ? u"paused"_s : QString());
        item->setText(2, status);
    }
    m_stack->setCurrentWidget(jobs.isEmpty() ? static_cast<QWidget *>(m_empty) : m_list);
}

void JobsPane::showEvent(QShowEvent *event)
{
    refresh();
    m_timer.start();
    QWidget::showEvent(event);
}

void JobsPane::hideEvent(QHideEvent *event)
{
    m_timer.stop();
    QWidget::hideEvent(event);
}

} // namespace fm::app
