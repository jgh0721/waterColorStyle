#pragma once

#include <QCache>
#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QThreadPool>

namespace fm::filelist {

/// 실제 파일의 섬네일을 백그라운드에서 만든다(QThreadPool). 결과는 경로 · 수정 시각 · 크기 기준 LRU 캐시에 둔다.
/// 이미지는 QImageReader 축소 읽기, 동영상 · PDF 등은 Windows 셸 섬네일(IShellItemImageFactory)을 쓴다.
class ThumbnailProvider : public QObject
{
    Q_OBJECT
public:
    /// 설정 › 섬네일 보기 › 만드는 방법.
    enum class Method { Shell, Builtin, ShellThenBuiltin };

    explicit ThumbnailProvider(QObject *parent = nullptr);
    ~ThumbnailProvider() override;

    /// 만들어 둔 섬네일(긴 변 kImageSize). 없으면 요청을 넣고 빈 이미지를 돌려준다 — 끝나면 ready(path).
    QImage thumbnail(const QString &path, const QDateTime &modified, qint64 size);
    /// 요청했지만 아직 끝나지 않았는지.
    bool isPending(const QString &path, const QDateTime &modified, qint64 size) const;
    /// 만들기에 실패했는지(대상이 아니거나 읽지 못함) — 이런 파일은 종류 아이콘으로 그린다.
    bool hasFailed(const QString &path, const QDateTime &modified, qint64 size) const;

    Method method() const noexcept { return m_method; }
    void setMethod(Method method);

    int maxThreads() const;
    void setMaxThreads(int count);

    /// 앱 자체 캐시만 비운다(셸 섬네일 캐시는 그대로).
    void clearCache();

    static constexpr int kImageSize = 256;

Q_SIGNALS:
    void ready(const QString &path);

private:
    static QString keyOf(const QString &path, const QDateTime &modified, qint64 size);
    void finished(const QString &key, const QString &path, const QImage &image);

    QThreadPool m_pool;
    QCache<QString, QImage> m_cache;
    QSet<QString> m_pending;
    QSet<QString> m_failed;
    Method m_method = Method::ShellThenBuiltin;
};

} // namespace fm::filelist
