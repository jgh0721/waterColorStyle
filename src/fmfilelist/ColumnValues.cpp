#include "fmfilelist/ColumnValues.h"

#include "fmfilelist/FileGroups.h"
#include "fmfilelist/FileListModel.h"
#include "ListPainting_p.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <propsys.h>
#include <propvarutil.h>
#include <shobjidl.h>
#endif

using namespace Qt::StringLiterals;

namespace fm::filelist {

namespace {

using K = ColumnDef::Kind;
using B = ColumnDef::BandPos;

struct ReadResult
{
    QHash<QString, QString> display;
    QHash<QString, QString> raw;
};

#ifdef Q_OS_WIN
// 작업 스레드 — COM을 스레드마다 연다. 속성 처리기가 없거나 값이 없으면 그 속성은 빠진다.
ReadResult readProperties(const QString &path, const QStringList &names)
{
    ReadResult result;
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IPropertyStore *store = nullptr;
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    if (SUCCEEDED(SHGetPropertyStoreFromParsingName(native.c_str(), nullptr, GPS_BESTEFFORT, IID_PPV_ARGS(&store)))) {
        for (const QString &name : names) {
            PROPERTYKEY key;
            const std::wstring wname = name.toStdWString();
            if (FAILED(PSGetPropertyKeyFromName(wname.c_str(), &key)))
                continue;
            PROPVARIANT value;
            PropVariantInit(&value);
            if (SUCCEEDED(store->GetValue(key, &value)) && value.vt != VT_EMPTY) {
                PWSTR text = nullptr;
                if (SUCCEEDED(PSFormatForDisplayAlloc(key, value, PDFF_DEFAULT, &text))) {
                    result.display.insert(name, QString::fromWCharArray(text));
                    CoTaskMemFree(text);
                }
                PWSTR raw = nullptr;
                if (SUCCEEDED(PropVariantToStringAlloc(value, &raw))) {
                    result.raw.insert(name, QString::fromWCharArray(raw));
                    CoTaskMemFree(raw);
                }
            }
            PropVariantClear(&value);
        }
        store->Release();
    }
    if (SUCCEEDED(init))
        CoUninitialize();
    return result;
}
#else
ReadResult readProperties(const QString &, const QStringList &)
{
    return {};
}
#endif

QString keyOf(const QStringList &names, const QString &path, const QDateTime &modified)
{
    return names.join(u'|') + u'#' + path + u'#' + QString::number(modified.toMSecsSinceEpoch());
}

/// 기본 세트의 기본 필드 열 폭 — 이 폭이면 목록 기본 배치의 치수(1줄 날짜 128 · 2줄 136 등)를 그대로 쓴다.
int defaultFieldWidth(int modelColumn)
{
    switch (modelColumn) {
    case ExtColumn:      return 64;
    case TypeColumn:     return 140;
    case SizeColumn:     return 84;
    case ModifiedColumn: return 136;
    case AttrColumn:     return 52;
    default:             return -1;
    }
}

QString knownFolderPath(const QString &id)
{
    QStandardPaths::StandardLocation location;
    if (id == u"Downloads")
        location = QStandardPaths::DownloadLocation;
    else if (id == u"Documents")
        location = QStandardPaths::DocumentsLocation;
    else if (id == u"Pictures")
        location = QStandardPaths::PicturesLocation;
    else if (id == u"Videos")
        location = QStandardPaths::MoviesLocation;
    else if (id == u"Music")
        location = QStandardPaths::MusicLocation;
    else if (id == u"Desktop")
        location = QStandardPaths::DesktopLocation;
    else
        return QString();
    return QDir::cleanPath(QStandardPaths::writableLocation(location));
}

QString builtinText(const QString &field, const FileEntry &e, const DisplayFormat &format)
{
    if (field == u"이름 (확장자 포함)")
        return e.fullName();
    if (field == u"이름")
        return e.stem;
    if (field == u"확장자")
        return e.ext;
    if (field == u"크기")
        return e.isDir() ? QString() : formatSize(e.size, format.sizeUnit);
    if (field == u"종류")
        return e.typeName;
    if (field == u"수정한 날짜")
        return formatDate(e.modified, format.dateFormat);
    if (field == u"만든 날짜")
        return formatDate(e.created, format.dateFormat);
    if (field == u"접근한 날짜")
        return formatDate(e.accessed, format.dateFormat);
    if (field == u"속성")
        return attributeText(e.attributes);
    if (field == u"경로")
        return e.path.isEmpty() ? QString() : QDir::toNativeSeparators(QFileInfo(e.path).path());
    return QString();
}

QString valueText(const ColumnDef &def, const FileEntry &e, PropertyReader *reader, const DisplayFormat &format)
{
    switch (def.kind) {
    case K::BuiltinField:
        return builtinText(def.source, e, format);
    case K::WindowsProperty:
        return reader && !e.isDir() ? reader->value(e.path, e.modified, def.source) : QString();
    case K::Expression: {
        // [이름]을 기본 필드 또는 Windows 속성 값으로 — 하나라도 비면 식 전체가 빈 값(빈 값 규칙을 따른다)
        static const QRegularExpression token(u"\\[([^\\]]+)\\]"_s);
        QString out;
        qsizetype last = 0;
        for (QRegularExpressionMatchIterator it = token.globalMatch(def.source); it.hasNext();) {
            const QRegularExpressionMatch m = it.next();
            const QString name = m.captured(1).trimmed();
            QString value = builtinText(name, e, format);
            if (value.isEmpty() && reader && !e.isDir())
                value = reader->rawValue(e.path, e.modified, name);
            if (value.isEmpty())
                return QString();
            out += def.source.mid(last, m.capturedStart() - last) + value;
            last = m.capturedEnd();
        }
        return out + def.source.mid(last);
    }
    }
    return QString();
}

} // namespace

// ---------------------------------------------------------------- PropertyReader

PropertyReader::PropertyReader(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(2);
    m_cache.setMaxCost(4000);  // 파일 수
}

PropertyReader::~PropertyReader()
{
    m_pool.clear();
    m_pool.waitForDone();
}

void PropertyReader::setProperties(const QStringList &canonicalNames)
{
    if (m_names == canonicalNames)
        return;
    m_names = canonicalNames;
    m_cache.clear();
    m_pending.clear();
}

const PropertyReader::Values *PropertyReader::values(const QString &path, const QDateTime &modified)
{
    if (m_names.isEmpty() || path.isEmpty())
        return nullptr;
    const QString key = keyOf(m_names, path, modified);
    if (const Values *v = m_cache.object(key))
        return v;
    if (m_pending.contains(key))
        return nullptr;
    m_pending.insert(key);
    QPointer<PropertyReader> self(this);
    m_pool.start([self, key, path, names = m_names] {
        const ReadResult result = readProperties(path, names);
        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, key, path, result] {
            if (!self || !self->m_pending.remove(key))
                return;  // 속성 목록이 바뀌어 버린 요청
            self->m_cache.insert(key, new Values{result.display, result.raw});
            Q_EMIT self->ready(path);
        }, Qt::QueuedConnection);
    });
    return nullptr;
}

QString PropertyReader::value(const QString &path, const QDateTime &modified, const QString &canonicalName)
{
    const Values *v = values(path, modified);
    return v ? v->display.value(canonicalName) : QString();
}

QString PropertyReader::rawValue(const QString &path, const QDateTime &modified, const QString &canonicalName)
{
    const Values *v = values(path, modified);
    return v ? v->raw.value(canonicalName, v->display.value(canonicalName)) : QString();
}

bool PropertyReader::isPending(const QString &path, const QDateTime &modified) const
{
    return m_pending.contains(keyOf(m_names, path, modified));
}

// ---------------------------------------------------------------- 세트 판정 · 열 배치 · 값

int builtinFieldColumn(const QString &field)
{
    if (field == u"이름 (확장자 포함)")
        return NameColumn;
    if (field == u"확장자")
        return ExtColumn;
    if (field == u"종류")
        return TypeColumn;
    if (field == u"크기")
        return SizeColumn;
    if (field == u"수정한 날짜")
        return ModifiedColumn;
    if (field == u"속성")
        return AttrColumn;
    return -1;
}

int resolveColumnSet(const ColumnSettings &settings, const QString &folder, bool local, const FileGroupMatcher *groups,
                     const QAbstractItemModel *rows)
{
    const QString path = QDir::cleanPath(QDir::fromNativeSeparators(folder));
    for (int i = 1; i < settings.sets.size(); ++i) {
        const ColumnSet &set = settings.sets.at(i);
        if (!set.autoApply)
            continue;
        switch (set.rule.type) {
        case ColumnSetRule::Type::PathWildcard: {
            const QString pattern = QDir::fromNativeSeparators(set.rule.pathPattern.trimmed());
            if (pattern.isEmpty())
                break;
            const QRegularExpression re(
                QRegularExpression::wildcardToRegularExpression(pattern, QRegularExpression::NonPathWildcardConversion),
                QRegularExpression::CaseInsensitiveOption);
            if (re.match(path).hasMatch())
                return i;
            break;
        }
        case ColumnSetRule::Type::KnownFolder: {
            const QString known = local ? knownFolderPath(set.rule.knownFolder) : QString();
            if (!known.isEmpty()
                && (path.compare(known, Qt::CaseInsensitive) == 0 || path.startsWith(known + u'/', Qt::CaseInsensitive)))
                return i;
            break;
        }
        case ColumnSetRule::Type::GroupRatio: {
            if (!groups || !rows)
                break;
            const QList<FileGroup> &list = groups->settings().groups;
            int groupIndex = -1;
            for (int g = 0; g < list.size(); ++g) {
                if (list.at(g).id == set.rule.groupId)
                    groupIndex = g;
            }
            if (groupIndex < 0)
                break;
            int files = 0;
            int matching = 0;
            const QDateTime now = QDateTime::currentDateTime();
            for (int r = 0; r < rows->rowCount(); ++r) {
                const QModelIndex idx = rows->index(r, NameColumn);
                if (idx.data(IsDirRole).toBool())
                    continue;
                FileFacts facts;
                facts.name = idx.data(FullNameRole).toString();
                facts.ext = idx.data(ExtRole).toString();
                facts.attributes = idx.data(AttributesRole).toInt();
                facts.size = idx.data(SizeBytesRole).toLongLong();
                facts.modified = idx.data(ModifiedRole).toDateTime();
                ++files;
                if (groups->matchingGroups(facts, now).contains(groupIndex))
                    ++matching;
            }
            if (files > 0 && matching * 100 >= set.rule.ratioPercent * files)
                return i;
            break;
        }
        case ColumnSetRule::Type::Default:
        case ColumnSetRule::Type::Manual:
            break;
        }
    }
    return 0;
}

ListColumnLayout mainLayoutForSet(const ColumnSet &set, QList<ColumnDef> *extras)
{
    using R = ListColumn::Role;
    using L = ListColumn::TwoLine;
    extras->clear();
    const ListColumnLayout standard = ListColumnLayout::standard();
    const ColumnSettings defaults = ColumnSettings::defaults();
    if (set.isDefault() && set.columns == defaults.sets.first().columns)
        return standard;

    ListColumnLayout layout;
    layout.columns.append(*standard.find(IconColumn));
    ListColumn name = *standard.find(NameColumn);
    for (const ColumnDef &d : set.columns) {
        if (d.bandPos == B::Row0Full && d.title != u"이름")
            name.caption = d.title;
    }
    layout.columns.append(name);
    for (const ColumnDef &d : set.columns) {
        if (d.bandPos == B::Row0Full)
            continue;  // 이름 — 행 0 전체
        const int model = d.kind == K::BuiltinField ? builtinFieldColumn(d.source) : -1;
        if (model == NameColumn || (!d.showInSingleLine && d.bandPos == B::Hidden))
            continue;
        ListColumn c;
        if (model >= 0) {
            c = *standard.find(model);
            if (d.width != defaultFieldWidth(model)) {
                c.width = d.width;
                c.twoLineWidth = -1;
            }
        } else {
            c.modelColumn = ColumnCount + int(extras->size());
            extras->append(d);
            c.role = R::Meta;
            c.width = d.width > 0 ? d.width : 100;
            c.tabular = d.sort == ColumnDef::Sort::Number || d.sort == ColumnDef::Sort::Date || d.sort == ColumnDef::Sort::NumberTuple;
        }
        c.caption = d.title;
        c.align = d.align;
        c.oneLine = d.showInSingleLine;
        c.twoLine = d.bandPos == B::Row1 ? L::Row1 : L::Hidden;
        layout.columns.append(c);
    }
    layout.columns.append(*standard.find(FillerColumn));
    return layout;
}

QString extraColumnText(const ColumnDef &def, const QList<ColumnDef> &all, const FileEntry &entry, PropertyReader *reader,
                        const DisplayFormat &format)
{
    if (entry.isUp())
        return QString();
    const QString text = valueText(def, entry, reader, format);
    if (!text.isEmpty())
        return text;
    switch (def.empty) {
    case ColumnDef::Empty::Dash:
        return u"—"_s;
    case ColumnDef::Empty::Blank:
        return QString();
    case ColumnDef::Empty::Fallback:
        for (const ColumnDef &other : all) {
            if (other.id == def.fallbackColumnId && other.id != def.id) {
                const QString fallback = valueText(other, entry, reader, format);
                return fallback.isEmpty() ? u"—"_s : fallback;
            }
        }
        return u"—"_s;
    }
    return QString();
}

std::optional<QVariant> ExtraColumns::data(const FileEntry &entry, int column, int role, const DisplayFormat &format) const
{
    const int i = column - ColumnCount;
    if (i < 0 || i >= defs.size())
        return std::nullopt;
    const ColumnDef &def = defs.at(i);
    switch (role) {
    case Qt::DisplayRole:
        return extraColumnText(def, all, entry, reader, format);
    case Qt::TextAlignmentRole:
        return int((def.align & Qt::AlignHorizontal_Mask) | Qt::AlignVCenter);
    case Qt::ToolTipRole:
        return def.kind == ColumnDef::Kind::BuiltinField ? QVariant() : QVariant(def.title + u" — "_s + def.source);
    default:
        return std::nullopt;
    }
}

QVariant ExtraColumns::headerData(int column, int role) const
{
    const int i = column - ColumnCount;
    if (i < 0 || i >= defs.size() || role != Qt::DisplayRole)
        return QVariant();
    return defs.at(i).title;
}

} // namespace fm::filelist
