#include "fmfilelist/MockFileSource.h"

using namespace Qt::StringLiterals;

namespace fm::filelist::MockFileSource {

namespace {

constexpr qint64 KB = 1024;
constexpr qint64 MB = 1024 * KB;
constexpr qint64 GB = 1024 * MB;

QDateTime at(const QString &text)
{
    return QDateTime::fromString(text, u"yyyy-MM-dd HH:mm"_s);
}

FileEntry file(const QString &stem, const QString &ext, const QString &type, qint64 size, const QString &date,
               const QString &attrs, Kind kind, bool marked = false)
{
    FileEntry e;
    e.stem = stem;
    e.ext = ext;
    e.typeName = type;
    e.kind = kind;
    e.size = size;
    e.modified = at(date);
    if (attrs.size() == 4) {
        e.attributes = (attrs[0] == u'r' ? ReadOnly : 0) | (attrs[1] == u'a' ? Archive : 0)
                     | (attrs[2] == u'h' ? Hidden : 0) | (attrs[3] == u's' ? System : 0);
    }
    e.marked = marked;
    return e;
}

FileEntry folder(const QString &name, const QString &date, const QString &attrs = u"----"_s)
{
    FileEntry e = file(name, QString(), u"파일 폴더"_s, -1, date, attrs, Kind::Folder);
    return e;
}

FileEntry up()
{
    FileEntry e;
    e.stem = u".."_s;
    e.kind = Kind::Up;
    e.typeName = u"상위 폴더"_s;
    return e;
}

FileEntry withArt(FileEntry e, Art art, qreal aspect, const QString &badge)
{
    e.art = art;
    e.aspect = aspect;
    e.badge = badge;
    return e;
}

void setPaths(MockFolder &folder)
{
    for (FileEntry &e : folder.entries)
        e.path = e.isUp() ? QString() : folder.path + u'\\' + e.fullName();
}

} // namespace

MockFolder left()
{
    MockFolder f;
    f.path = u"D:\\Work\\fm-core"_s;
    f.tabs = {u"fm-core"_s, u"qtitan-samples"_s, u"C:\\"_s};
    f.cursor = 7;
    f.volumeLabel = u"새 볼륨"_s;
    f.freeText = u"여유 312 GB / 1.82 TB"_s;
    // 크기(바이트)는 보드 문구로 반올림했을 때 같아지고, 합계가 상태 줄 "9.9 KB / 41.1 KB"가 되게 골랐다.
    f.entries = {
        up(),
        folder(u".git"_s, u"2026-09-28 09:12"_s, u"--h-"_s),
        folder(u"build"_s, u"2026-09-27 22:40"_s),
        folder(u"cmake"_s, u"2026-09-02 14:05"_s),
        folder(u"docs"_s, u"2026-09-19 11:30"_s),
        folder(u"include"_s, u"2026-09-25 18:22"_s),
        folder(u"resources"_s, u"2026-09-21 10:03"_s),
        folder(u"src"_s, u"2026-09-28 08:57"_s),
        folder(u"third_party"_s, u"2026-08-30 16:44"_s),
        file(u".clang-format"_s, QString(), u"CLANG-FORMAT 파일"_s, 1370, u"2026-08-12 10:20"_s, u"-a--"_s, Kind::Code),
        file(u".editorconfig"_s, QString(), u"EDITORCONFIG 파일"_s, 412, u"2026-08-12 10:20"_s, u"-a--"_s, Kind::Code),
        file(u".gitignore"_s, QString(), u"GITIGNORE 파일"_s, 688, u"2026-09-03 17:41"_s, u"-a--"_s, Kind::Code),
        file(u"CHANGELOG"_s, u"md"_s, u"Markdown 문서"_s, 14950, u"2026-09-26 19:08"_s, u"-a--"_s, Kind::Doc),
        file(u"CMakeLists"_s, u"txt"_s, u"텍스트 문서"_s, 6963, u"2026-09-27 22:31"_s, u"-a--"_s, Kind::Code, true),
        file(u"CMakePresets"_s, u"json"_s, u"JSON 파일"_s, 3174, u"2026-09-27 22:31"_s, u"-a--"_s, Kind::Code, true),
        file(u"fm-core"_s, u"natvis"_s, u"NATVIS 파일"_s, 2400, u"2026-09-10 13:52"_s, u"-a--"_s, Kind::Code),
        file(u"LICENSE"_s, QString(), u"파일"_s, 1024, u"2026-08-01 09:00"_s, u"ra--"_s, Kind::Doc),
        file(u"README"_s, u"md"_s, u"Markdown 문서"_s, 8499, u"2026-09-24 21:15"_s, u"-a--"_s, Kind::Doc),
        file(u"vcpkg"_s, u"json"_s, u"JSON 파일"_s, 689, u"2026-09-18 15:27"_s, u"-a--"_s, Kind::Code),
        file(u"build_release_x64"_s, u"cmd"_s, u"Windows 명령 스크립트"_s, 1946, u"2026-09-27 22:35"_s, u"-a--"_s, Kind::Exe),
    };
    setPaths(f);
    return f;
}

MockFolder right()
{
    MockFolder f;
    f.path = u"D:\\Downloads"_s;
    f.tabs = {u"Downloads"_s, u"Backup"_s};
    f.cursor = 3;
    f.volumeLabel = u"새 볼륨"_s;
    f.freeText = u"여유 312 GB / 1.82 TB"_s;
    // 합계가 상태 줄 "3.95 GB / 7.00 GB"가 되게 골랐다.
    f.entries = {
        up(),
        file(u"Qt-6.11.0-windows-x64-msvc2026-offline-installer-with-debug-symbols"_s, u"exe"_s, u"응용 프로그램"_s,
             qint64(3.74 * GB), u"2026-09-26 18:02"_s, u"-a--"_s, Kind::Exe, true),
        file(u"QtitanDataGrid-9.3.0-Windows-MSVC2026-x64-Setup"_s, u"exe"_s, u"응용 프로그램"_s,
             186 * MB, u"2026-09-26 18:40"_s, u"-a--"_s, Kind::Exe, true),
        withArt(file(u"2026년 3분기 보안 감사 보고서 — 커널 미니필터 드라이버 서명 정책 검토 (최종본 v3)"_s, u"pdf"_s,
                     u"PDF 문서"_s, qint64(12.4 * MB), u"2026-09-25 16:11"_s, u"-a--"_s, Kind::Pdf),
                Art::Pdf, 0.77, u"PDF"_s),
        withArt(file(u"Screenshot 2026-09-27 231455 - 듀얼 패널 레이아웃 비교 (밴드 2줄 vs 단일 행)"_s, u"png"_s,
                     u"PNG 이미지"_s, qint64(2.1 * MB), u"2026-09-27 23:14"_s, u"-a--"_s, Kind::Img),
                Art::Shot, 1.6, u"PNG"_s),
        file(u"design-review_file-operation-broker_elevation-flow_2026-09-24"_s, u"md"_s, u"Markdown 문서"_s,
             qint64(18.2 * KB), u"2026-09-24 20:47"_s, u"-a--"_s, Kind::Doc),
        file(u"Windows SDK 10.0.26100 — Debugging Tools for Windows (x64, x86, ARM64)"_s, u"msi"_s,
             u"Windows Installer 패키지"_s, qint64(42.8 * MB), u"2026-09-22 11:05"_s, u"-a--"_s, Kind::Exe),
        file(u"vc_redist.x64"_s, u"exe"_s, u"응용 프로그램"_s, qint64(24.4 * MB), u"2026-09-22 11:07"_s, u"-a--"_s,
             Kind::Exe, true),
        file(u"sysinternals-suite_process-monitor_procexp_autoruns_2026-09"_s, u"zip"_s, u"압축(ZIP) 폴더"_s,
             qint64(51.3 * MB), u"2026-09-20 09:33"_s, u"-a--"_s, Kind::Zip),
        withArt(file(u"Recording_2026-09-26_아키텍처 리뷰 세션 전체 녹화본 (화면 공유 · 발표…_10bit_part1_of_3"_s, u"mkv"_s,
                     u"MKV 동영상"_s, qint64(2.86 * GB), u"2026-09-26 22:58"_s, u"-a--"_s, Kind::Img),
                Art::Video, 1.78, u"1:42:08"_s),
        file(u"font-pack_Cascadia-Code-NF_2026-07"_s, u"7z"_s, u"7Z 압축 파일"_s, 88 * MB, u"2026-09-19 14:26"_s,
             u"-a--"_s, Kind::Zip),
        file(u"desktop"_s, u"ini"_s, u"구성 설정"_s, 282, u"2026-09-01 08:00"_s, u"-ahs"_s, Kind::Sys),
    };
    setPaths(f);
    return f;
}

MockFolder settingsPanelPreview()
{
    MockFolder f;
    f.path = u"D:\\Downloads"_s;
    f.cursor = 1;
    f.entries = {
        file(u"Qt-6.11.0-windows-x64-msvc2026-offline-installer-with-debug-symbols"_s, u"exe"_s, u"응용 프로그램"_s,
             qint64(3.74 * GB), u"2026-09-26 18:02"_s, u"-a--"_s, Kind::Exe, true),
        withArt(file(u"2026년 3분기 보안 감사 보고서 — 커널 미니필터 드라이버 서명 정책 검토"_s, u"pdf"_s, u"PDF 문서"_s,
                     qint64(12.4 * MB), u"2026-09-25 16:11"_s, u"-a--"_s, Kind::Pdf),
                Art::Pdf, 0.77, u"PDF"_s),
        withArt(file(u"Screenshot 2026-09-27 231455 - 듀얼 패널 레이아웃 비교"_s, u"png"_s, u"PNG 이미지"_s,
                     qint64(2.1 * MB), u"2026-09-27 23:14"_s, u"-a--"_s, Kind::Img, true),
                Art::Shot, 1.6, u"PNG"_s),
        file(u"sysinternals-suite_process-monitor_procexp_autoruns_2026-09"_s, u"zip"_s, u"압축(ZIP) 폴더"_s,
             qint64(51.3 * MB), u"2026-09-20 09:33"_s, u"-a--"_s, Kind::Zip, true),
        file(u"desktop"_s, u"ini"_s, u"구성 설정"_s, 282, u"2026-09-01 08:00"_s, u"-ahs"_s, Kind::Sys),
    };
    setPaths(f);
    return f;
}

MockFolder thumbnailPreview()
{
    MockFolder f;
    f.path = u"D:\\Pictures"_s;
    f.cursor = 2;
    f.entries = {
        folder(u"Screenshots"_s, u"2026-09-27 21:40"_s),
        withArt(file(u"Screenshot 2026-09-27 231455 - 듀얼 패널 레이아웃 비교"_s, u"png"_s, u"PNG 이미지"_s,
                     qint64(2.1 * MB), u"2026-09-27 23:14"_s, u"-a--"_s, Kind::Img, true),
                Art::Shot, 1.6, u"PNG"_s),
        withArt(file(u"Recording_2026-09-26_아키텍처 리뷰 세션 전체 녹화본"_s, u"mkv"_s, u"MKV 동영상"_s,
                     qint64(2.86 * GB), u"2026-09-26 22:58"_s, u"-a--"_s, Kind::Img),
                Art::Video, 1.78, u"1:42:08"_s),
        withArt(file(u"2026년 3분기 보안 감사 보고서 — 서명 정책 검토"_s, u"pdf"_s, u"PDF 문서"_s,
                     qint64(12.4 * MB), u"2026-09-25 16:11"_s, u"-a--"_s, Kind::Pdf, true),
                Art::Pdf, 0.77, u"PDF"_s),
        withArt(file(u"IMG_2041"_s, u"heic"_s, u"HEIC 파일"_s, qint64(3.4 * MB), u"2026-09-21 14:02"_s, u"-a--"_s,
                     Kind::Img),
                Art::Photo, 1.33, u"HEIC"_s),
        withArt(file(u"IMG_2042"_s, u"heic"_s, u"HEIC 파일"_s, qint64(3.1 * MB), u"2026-09-21 14:03"_s, u"-a--"_s,
                     Kind::Img),
                Art::Loading, 0, QString()),
        file(u"QtitanDataGrid-9.3.0-Windows-MSVC2026-x64-Setup"_s, u"exe"_s, u"응용 프로그램"_s, 186 * MB,
             u"2026-09-26 18:40"_s, u"-a--"_s, Kind::Exe),
        file(u"font-pack_Cascadia-Code-NF_2026-07"_s, u"7z"_s, u"7Z 압축 파일"_s, 88 * MB, u"2026-09-19 14:26"_s,
             u"-a--"_s, Kind::Zip),
        file(u"desktop"_s, u"ini"_s, u"구성 설정"_s, 282, u"2026-09-01 08:00"_s, u"-ahs"_s, Kind::Sys),
    };
    setPaths(f);
    return f;
}

} // namespace fm::filelist::MockFileSource
