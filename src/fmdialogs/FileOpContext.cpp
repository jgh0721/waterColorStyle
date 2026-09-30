#include "fmdialogs/FileOpContext.h"

#include <fmfilelist/FileRoles.h>

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

QString normalized(QString path)
{
    path.replace(u'/', u'\\');
    while (path.size() > 3 && path.endsWith(u'\\'))
        path.chop(1);
    if (path.size() == 3 && path.endsWith(u'\\') && !path.startsWith(u"\\\\"))
        path.chop(1);  // "D:\" → "D:"
    return path.toLower();
}

QString driveOf(const QString &path)
{
    if (path.startsWith(u"\\\\")) {
        const qsizetype second = path.indexOf(u'\\', 2);
        const qsizetype third = second < 0 ? -1 : path.indexOf(u'\\', second + 1);
        return third < 0 ? path : path.left(third);
    }
    return path.left(2).toUpper();
}

} // namespace

bool FileSystemProbe::sameVolume(const QString &a, const QString &b) const
{
    const VolumeInfo va = volume(a);
    const VolumeInfo vb = volume(b);
    if (va.valid && vb.valid)
        return va.drive.compare(vb.drive, Qt::CaseInsensitive) == 0;
    return driveOf(a).compare(driveOf(b), Qt::CaseInsensitive) == 0;
}

bool LocalProbe::exists(const QString &path) const
{
    return QFileInfo::exists(QDir::fromNativeSeparators(path));
}

VolumeInfo LocalProbe::volume(const QString &path) const
{
    VolumeInfo v;
    // 대상이 아직 없으면 있는 상위 폴더로 올라가 볼륨을 찾는다
    QString probe = QDir::fromNativeSeparators(path);
    while (!probe.isEmpty() && !QFileInfo::exists(probe)) {
        const QString parent = QFileInfo(probe).path();
        if (parent == probe)
            break;
        probe = parent;
    }
    const QStorageInfo storage(probe.isEmpty() ? path : probe);
    if (!storage.isValid() || !storage.isReady())
        return v;
    v.valid = true;
    v.drive = QDir::toNativeSeparators(storage.rootPath());
    if (v.drive.size() >= 2 && v.drive.at(1) == u':')
        v.drive = v.drive.left(2).toUpper();
    v.label = storage.name().isEmpty() ? QObject::tr("로컬 디스크") : storage.name();
    v.fileSystem = QString::fromLatin1(storage.fileSystemType());
    v.available = storage.bytesAvailable();
    v.total = storage.bytesTotal();
    v.hasRecycleBin = !v.drive.startsWith(u"\\\\");
    return v;
}

MockProbe::MockProbe()
{
    // 샘플 드라이브에 있는 폴더 · 파일(목업 문구가 나오도록)
    for (const QString &p : {u"D:"_s, u"D:\\Downloads"_s, u"D:\\Work"_s, u"D:\\Work\\fm-core"_s, u"D:\\Work\\fm-core\\src"_s,
                             u"D:\\Work\\fm-core\\src\\panel"_s, u"D:\\Work\\fm-core\\src\\platform"_s,
                             u"D:\\Work\\qtitan-samples"_s, u"D:\\Backup"_s, u"D:\\Archive"_s, u"D:\\Archive\\2026-Q3"_s,
                             u"D:\\Photos"_s, u"D:\\Photos\\2026-09 제주"_s, u"E:"_s, u"E:\\Backup"_s,
                             u"E:\\Backup\\Installers"_s, u"E:\\Backup\\Docs"_s, u"\\\\nas01\\reports"_s,
                             u"F:"_s, u"D:\\Work\\fm-core\\src\\panel\\PanelView.cpp"_s,
                             u"D:\\Archive\\2026-Q3\\2026년 3분기 보안 감사 보고서 (최종).pdf"_s,
                             u"D:\\Photos\\2026-09 제주\\2026-09-14_제주_007.jpg"_s})
        addPath(p);
}

void MockProbe::addPath(const QString &path)
{
    m_paths.append(normalized(path));
}

bool MockProbe::exists(const QString &path) const
{
    return m_paths.contains(normalized(path));
}

VolumeInfo MockProbe::volume(const QString &path) const
{
    VolumeInfo v;
    const QString drive = driveOf(path);
    constexpr qint64 GB = 1024LL * 1024 * 1024;
    constexpr qint64 TB = 1024LL * GB;
    if (drive == u"D:") {
        v = {true, drive, QObject::tr("새 볼륨"), u"NTFS"_s, 312 * GB, qint64(1.82 * TB), true};
    } else if (drive == u"E:") {
        v = {true, drive, QObject::tr("백업"), u"NTFS"_s, 812 * GB, qint64(1.82 * TB), true};
    } else if (drive == u"F:") {
        v = {true, drive, u"USB"_s, u"exFAT"_s, qint64(1.20 * GB), qint64(29.8 * GB), false};
    } else if (drive.startsWith(u"\\\\")) {
        v = {true, drive, u"reports"_s, u"SMB"_s, 2 * TB, 8 * TB, false};
    }
    return v;
}

qint64 FileOpContext::totalSize() const
{
    qint64 total = 0;
    for (const FileItem &item : items)
        total += item.size;
    return total;
}

QString FileOpContext::countLabel() const
{
    if (items.isEmpty())
        return QString();
    if (items.size() == 1)
        return items.first().name;
    return QObject::tr("%1 외 %2개").arg(items.first().name).arg(items.size() - 1);
}

QString formatBytes(qint64 bytes)
{
    return fm::filelist::formatSize(bytes);
}

fm::ui::ChoiceCard::FileKind fileKindFor(const QString &name, bool isDir)
{
    using fm::ui::ChoiceCard;
    if (isDir)
        return ChoiceCard::DocKind;
    QString stem, ext;
    fm::filelist::splitFileName(name, false, &stem, &ext);
    switch (fm::filelist::kindForExtension(ext)) {
    case fm::filelist::Kind::Exe:  return ChoiceCard::ExeKind;
    case fm::filelist::Kind::Pdf:  return ChoiceCard::PdfKind;
    case fm::filelist::Kind::Img:  return ChoiceCard::ImageKind;
    case fm::filelist::Kind::Zip:  return ChoiceCard::ArchiveKind;
    case fm::filelist::Kind::Code: return ChoiceCard::CodeKind;
    case fm::filelist::Kind::Sys:  return ChoiceCard::SystemKind;
    case fm::filelist::Kind::Up:
    case fm::filelist::Kind::Folder:
    case fm::filelist::Kind::Doc:
    case fm::filelist::Kind::Other: break;
    }
    return ChoiceCard::DocKind;
}

namespace BoardContext {

const MockProbe &probe()
{
    static const MockProbe instance;
    return instance;
}

namespace {

constexpr qint64 KB = 1024;
constexpr qint64 MB = 1024 * KB;
constexpr qint64 GB = 1024 * MB;

FileItem item(const QString &name, qint64 size)
{
    FileItem i;
    i.name = name;
    i.size = size;
    i.kind = fileKindFor(name, false);
    return i;
}

QList<FileItem> downloadsSelection()
{
    return {item(u"Qt-6.11.0-windows-x64-msvc2026-offline-installer-with-debug-symbols.exe"_s, qint64(3.74 * GB)),
            item(u"QtitanDataGrid-9.3.0-Windows-MSVC2026-x64-Setup.exe"_s, 186 * MB),
            item(u"vc_redist.x64.exe"_s, qint64(24.4 * MB))};
}

} // namespace

FileOpContext copy()
{
    FileOpContext c;
    c.items = downloadsSelection();
    c.sourceDir = u"D:\\Downloads"_s;
    c.targetDir = u"E:\\Backup\\Installers\\"_s;
    c.recentTargets = {u"E:\\Backup\\Installers\\"_s, u"D:\\Archive\\2026-Q3\\"_s, u"E:\\Backup\\Docs\\"_s,
                       u"\\\\nas01\\reports\\"_s};
    c.probe = &probe();
    return c;
}

FileOpContext moveRename()
{
    FileOpContext c;
    c.items = {item(u"2026년 3분기 보안 감사 보고서 — 커널 미니필터 드라이버 서명 정책 검토 (최종본 v3).pdf"_s, qint64(12.4 * MB))};
    c.sourceDir = u"D:\\Downloads"_s;
    c.targetDir = u"D:\\Archive\\2026-Q3"_s;
    c.recentTargets = {u"D:\\Archive\\2026-Q3"_s, u"E:\\Backup\\Docs"_s, u"\\\\nas01\\reports"_s};
    c.probe = &probe();
    return c;
}

FileOpContext remove()
{
    FileOpContext c;
    c.items = downloadsSelection();
    c.sourceDir = u"D:\\Downloads"_s;
    c.probe = &probe();
    return c;
}

FileOpContext newFile()
{
    FileOpContext c;
    c.sourceDir = u"D:\\Work\\fm-core\\src\\panel"_s;
    c.suggestedName = u"BandedPanelView"_s;
    c.probe = &probe();
    return c;
}

FileOpContext newFolder()
{
    FileOpContext c;
    c.sourceDir = u"D:\\Work\\fm-core\\src"_s;
    c.suggestedName = u"platform\\win32\\shim"_s;
    c.probe = &probe();
    return c;
}

} // namespace BoardContext

} // namespace fm::dialogs
