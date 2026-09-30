#include "fmfilelist/ThumbnailProvider.h"

#include "fmfilelist/FileRoles.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPointer>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shobjidl.h>
#endif

using namespace Qt::StringLiterals;

namespace fm::filelist {

namespace {

#ifdef Q_OS_WIN
// 셸 섬네일(탐색기 섬네일 캐시). 섬네일 처리기가 없는 파일은 실패한다(SIIGBF_THUMBNAILONLY — 아이콘으로 대신하지 않음).
QImage shellThumbnail(const QString &path, int px)
{
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    QImage image;
    IShellItemImageFactory *factory = nullptr;
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    if (SUCCEEDED(SHCreateItemFromParsingName(native.c_str(), nullptr, IID_PPV_ARGS(&factory)))) {
        HBITMAP bitmap = nullptr;
        if (SUCCEEDED(factory->GetImage(SIZE{px, px}, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &bitmap))) {
            image = QImage::fromHBITMAP(bitmap);
            DeleteObject(bitmap);
        }
        factory->Release();
    }
    if (SUCCEEDED(init))
        CoUninitialize();
    return image;
}
#endif

QImage readImage(const QString &path, int px)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && (size.width() > px || size.height() > px))
        reader.setScaledSize(size.scaled(px, px, Qt::KeepAspectRatio));
    return reader.read();
}

QImage makeThumbnail(const QString &path, ThumbnailProvider::Method method)
{
    const int px = ThumbnailProvider::kImageSize;
    const QString ext = QFileInfo(path).suffix();
    QImage image;
#ifdef Q_OS_WIN
    if (method != ThumbnailProvider::Method::Builtin && (isImageExtension(ext) || isShellThumbnailExtension(ext)))
        image = shellThumbnail(path, px);
#endif
    if (image.isNull() && method != ThumbnailProvider::Method::Shell && isImageExtension(ext))
        image = readImage(path, px);
    if (!image.isNull() && (image.width() > px || image.height() > px))
        image = image.scaled(px, px, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}

} // namespace

ThumbnailProvider::ThumbnailProvider(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(4);
    m_cache.setMaxCost(96 * 1024);  // KB — 256 px 섬네일 약 380장
}

ThumbnailProvider::~ThumbnailProvider()
{
    m_pool.clear();
    m_pool.waitForDone();
}

QString ThumbnailProvider::keyOf(const QString &path, const QDateTime &modified, qint64 size)
{
    return path + u'|' + QString::number(modified.toMSecsSinceEpoch()) + u'|' + QString::number(size);
}

QImage ThumbnailProvider::thumbnail(const QString &path, const QDateTime &modified, qint64 size)
{
    const QString key = keyOf(path, modified, size);
    if (const QImage *image = m_cache.object(key))
        return *image;
    if (m_pending.contains(key) || m_failed.contains(key))
        return QImage();
    m_pending.insert(key);
    const Method method = m_method;
    QPointer<ThumbnailProvider> self(this);
    m_pool.start([self, key, path, method] {
        const QImage image = makeThumbnail(path, method);
        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, key, path, image] {
            if (self)
                self->finished(key, path, image);
        }, Qt::QueuedConnection);
    });
    return QImage();
}

void ThumbnailProvider::finished(const QString &key, const QString &path, const QImage &image)
{
    m_pending.remove(key);
    if (image.isNull())
        m_failed.insert(key);
    else
        m_cache.insert(key, new QImage(image), int(std::max<qsizetype>(1, image.sizeInBytes() / 1024)));
    Q_EMIT ready(path);
}

bool ThumbnailProvider::isPending(const QString &path, const QDateTime &modified, qint64 size) const
{
    return m_pending.contains(keyOf(path, modified, size));
}

bool ThumbnailProvider::hasFailed(const QString &path, const QDateTime &modified, qint64 size) const
{
    return m_failed.contains(keyOf(path, modified, size));
}

void ThumbnailProvider::setMethod(Method method)
{
    if (m_method == method)
        return;
    m_method = method;
    m_failed.clear();  // 방법을 바꾸면 실패한 파일도 다시 시도한다
}

int ThumbnailProvider::maxThreads() const
{
    return m_pool.maxThreadCount();
}

void ThumbnailProvider::setMaxThreads(int count)
{
    m_pool.setMaxThreadCount(std::clamp(count, 1, 16));
}

void ThumbnailProvider::clearCache()
{
    m_cache.clear();
    m_failed.clear();
}

} // namespace fm::filelist
