#include "fmfilelist/FileRoles.h"

#include <QImageReader>
#include <QSet>

using namespace Qt::StringLiterals;

namespace fm::filelist {

using fm::style::Token;

Token kindToken(Kind kind) noexcept
{
    switch (kind) {
    case Kind::Folder: return Token::Folder;
    case Kind::Exe:    return Token::KExe;
    case Kind::Pdf:    return Token::KPdf;
    case Kind::Img:    return Token::KImg;
    case Kind::Zip:    return Token::KZip;
    case Kind::Code:   return Token::KCode;
    case Kind::Sys:    return Token::KSys;
    case Kind::Up:
    case Kind::Doc:
    case Kind::Other:  return Token::KDoc;
    }
    return Token::KDoc;
}

Kind kindForExtension(const QString &ext, bool system)
{
    static const QSet<QString> exe = {u"exe"_s, u"msi"_s, u"cmd"_s, u"bat"_s, u"com"_s, u"ps1"_s, u"msix"_s, u"appx"_s};
    static const QSet<QString> img = {u"png"_s, u"jpg"_s, u"jpeg"_s, u"gif"_s, u"bmp"_s, u"webp"_s, u"heic"_s,
                                      u"tif"_s, u"tiff"_s, u"ico"_s, u"svg"_s, u"mkv"_s, u"mp4"_s, u"mov"_s,
                                      u"avi"_s, u"wmv"_s, u"webm"_s};
    static const QSet<QString> zip = {u"zip"_s, u"7z"_s, u"rar"_s, u"gz"_s, u"tar"_s, u"xz"_s, u"bz2"_s, u"cab"_s, u"iso"_s};
    static const QSet<QString> code = {u"txt"_s, u"json"_s, u"natvis"_s, u"clang-format"_s, u"editorconfig"_s,
                                       u"gitignore"_s, u"gitattributes"_s, u"c"_s, u"cc"_s, u"cpp"_s, u"cxx"_s,
                                       u"h"_s, u"hpp"_s, u"ui"_s, u"qrc"_s, u"cmake"_s, u"py"_s, u"js"_s, u"ts"_s,
                                       u"css"_s, u"html"_s, u"xml"_s, u"yml"_s, u"yaml"_s, u"toml"_s, u"patch"_s,
                                       u"ps1xml"_s, u"sln"_s, u"vcxproj"_s, u"pro"_s};
    static const QSet<QString> doc = {u"md"_s, u"doc"_s, u"docx"_s, u"xls"_s, u"xlsx"_s, u"ppt"_s, u"pptx"_s,
                                      u"hwp"_s, u"rtf"_s, u"odt"_s, u"csv"_s, u"log"_s};
    static const QSet<QString> sys = {u"ini"_s, u"dll"_s, u"sys"_s, u"lnk"_s, u"dat"_s, u"inf"_s, u"cat"_s, u"db"_s};
    const QString e = ext.toLower();
    if (system)
        return Kind::Sys;
    if (e == u"pdf")
        return Kind::Pdf;
    if (exe.contains(e))
        return Kind::Exe;
    if (img.contains(e))
        return Kind::Img;
    if (zip.contains(e))
        return Kind::Zip;
    if (code.contains(e))
        return Kind::Code;
    if (sys.contains(e))
        return Kind::Sys;
    if (doc.contains(e) || e.isEmpty())
        return Kind::Doc;
    return Kind::Other;
}

void splitFileName(const QString &name, bool isDir, QString *stem, QString *ext)
{
    const qsizetype dot = name.lastIndexOf(u'.');
    if (isDir || dot <= 0 || dot == name.size() - 1) {
        *stem = name;
        ext->clear();
        return;
    }
    *stem = name.left(dot);
    *ext = name.mid(dot + 1);
}

QString formatSize(qint64 bytes)
{
    if (bytes < 0)
        return QString();
    if (bytes < 1024)
        return u"%1 B"_s.arg(bytes);
    static const char *const units[] = {"KB", "MB", "GB", "TB", "PB"};
    double value = double(bytes) / 1024.0;
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    // 목업 규칙: KB · MB는 100 미만 소수 한 자리, GB 이상은 10 미만 두 자리 · 100 미만 한 자리.
    int decimals = 0;
    if (unit <= 1)
        decimals = value < 99.95 ? 1 : 0;
    else
        decimals = value < 9.995 ? 2 : (value < 99.95 ? 1 : 0);
    return u"%1 %2"_s.arg(QString::number(value, 'f', decimals), QLatin1StringView(units[unit]));
}

QString formatDate(const QDateTime &time)
{
    return time.isValid() ? time.toString(u"yyyy-MM-dd HH:mm"_s) : QString();
}

QString attributeText(int attributes)
{
    QString s(4, u'-');
    if (attributes & ReadOnly)
        s[0] = u'r';
    if (attributes & Archive)
        s[1] = u'a';
    if (attributes & Hidden)
        s[2] = u'h';
    if (attributes & System)
        s[3] = u's';
    return s;
}

bool isImageExtension(const QString &ext)
{
    static const QSet<QString> formats = [] {
        QSet<QString> set;
        const auto list = QImageReader::supportedImageFormats();
        for (const QByteArray &f : list)
            set.insert(QString::fromLatin1(f).toLower());
        set.insert(u"jpg"_s);
        return set;
    }();
    return formats.contains(ext.toLower());
}

bool isShellThumbnailExtension(const QString &ext)
{
    static const QSet<QString> set = {u"mp4"_s, u"mkv"_s, u"mov"_s, u"avi"_s, u"wmv"_s, u"webm"_s, u"m4v"_s,
                                      u"pdf"_s, u"heic"_s, u"heif"_s, u"psd"_s, u"ttf"_s, u"otf"_s};
    return set.contains(ext.toLower());
}

} // namespace fm::filelist
