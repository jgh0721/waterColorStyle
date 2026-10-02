#include "fmsettings/AppSettings.h"

#include <QDir>
#include <QJsonArray>

#include <utility>

using namespace Qt::StringLiterals;

namespace fm::settings {

namespace {

namespace fl = fm::filelist;
namespace fs = fm::style;

// ---------------------------------------------------------------- 열거 ↔ 문자열 (정수 저장 금지 — 순서 변경에 안전)

template <typename E>
struct Name
{
    E value;
    const char *name;
};

template <typename E, std::size_t N>
QJsonValue enumToJson(E value, const Name<E> (&table)[N])
{
    for (const Name<E> &n : table) {
        if (n.value == value)
            return QString::fromLatin1(n.name);
    }
    return QString::fromLatin1(table[0].name);
}

template <typename E, std::size_t N>
E enumFromJson(const QJsonValue &json, const Name<E> (&table)[N], E fallback)
{
    const QString text = json.toString();
    for (const Name<E> &n : table) {
        if (text == QLatin1StringView(n.name))
            return n.value;
    }
    return fallback;
}

constexpr Name<Scheme> kScheme[] = {{Scheme::System, "system"}, {Scheme::Light, "light"}, {Scheme::Dark, "dark"}};
constexpr Name<fs::Design> kDesign[] = {{fs::Design::Standard, "standard"}, {fs::Design::Watercolor, "watercolor"}};
constexpr Name<DarkTone> kDarkTone[] = {{DarkTone::Gray, "gray"}, {DarkTone::Navy, "navy"}};
constexpr Name<AppearanceSettings::Density> kDensity[] = {
    {AppearanceSettings::Density::Compact, "compact"}, {AppearanceSettings::Density::Normal, "normal"},
    {AppearanceSettings::Density::Relaxed, "relaxed"}};
constexpr Name<GeneralSettings::Startup> kStartup[] = {{GeneralSettings::Startup::RestoreLastTabs, "restore"},
                                                       {GeneralSettings::Startup::HomeFolder, "home"},
                                                       {GeneralSettings::Startup::SpecificFolders, "folders"}};
constexpr Name<GeneralSettings::TrayIcon> kTray[] = {{GeneralSettings::TrayIcon::Never, "never"},
                                                     {GeneralSettings::TrayIcon::WhileBusy, "busy"},
                                                     {GeneralSettings::TrayIcon::Always, "always"}};
constexpr Name<TabSettings::NewTabPosition> kNewTab[] = {{TabSettings::NewTabPosition::RightOfCurrent, "right"},
                                                         {TabSettings::NewTabPosition::End, "end"}};
constexpr Name<TabSettings::TabTitle> kTabTitle[] = {{TabSettings::TabTitle::FolderName, "folder"},
                                                     {TabSettings::TabTitle::DriveAndFolder, "driveFolder"},
                                                     {TabSettings::TabTitle::FullPath, "path"}};
constexpr Name<fl::ViewMode> kViewMode[] = {{fl::ViewMode::OneLine, "oneLine"}, {fl::ViewMode::TwoLine, "twoLine"},
                                            {fl::ViewMode::Auto, "auto"}, {fl::ViewMode::Thumbnails, "thumbnails"}};
constexpr Name<fl::RecordSeparator> kSeparator[] = {{fl::RecordSeparator::None, "none"}, {fl::RecordSeparator::Zebra, "zebra"},
                                                    {fl::RecordSeparator::Line, "line"}, {fl::RecordSeparator::Space, "space"},
                                                    {fl::RecordSeparator::Tint, "tint"}};
constexpr Name<fl::NameElide> kElide[] = {{fl::NameElide::MiddleKeepExtension, "middleKeepExt"}, {fl::NameElide::End, "end"},
                                          {fl::NameElide::Middle, "middle"}};
constexpr Name<PanelSettings::SizeUnit> kSizeUnit[] = {{PanelSettings::SizeUnit::Auto, "auto"}, {PanelSettings::SizeUnit::Bytes, "bytes"},
                                                       {PanelSettings::SizeUnit::KB, "kb"}, {PanelSettings::SizeUnit::MB, "mb"}};
constexpr Name<fl::InactiveCursor> kInactive[] = {{fl::InactiveCursor::Dashed, "dashed"}, {fl::InactiveCursor::Hidden, "hidden"},
                                                  {fl::InactiveCursor::SameAsActive, "sameAsActive"}};
constexpr Name<fl::ThumbnailAppearance::Info> kInfo[] = {
    {fl::ThumbnailAppearance::Info::None, "none"}, {fl::ThumbnailAppearance::Info::Size, "size"},
    {fl::ThumbnailAppearance::Info::Date, "date"}, {fl::ThumbnailAppearance::Info::SizeDate, "sizeDate"}};
constexpr Name<ThumbSettings::Provider> kProvider[] = {{ThumbSettings::Provider::WindowsShell, "shell"},
                                                       {ThumbSettings::Provider::Builtin, "builtin"},
                                                       {ThumbSettings::Provider::ShellThenBuiltin, "shellThenBuiltin"}};
constexpr Name<FileOpsSettings::Conflict> kConflict[] = {
    {FileOpsSettings::Conflict::Ask, "ask"}, {FileOpsSettings::Conflict::Overwrite, "overwrite"},
    {FileOpsSettings::Conflict::OverwriteIfNewer, "overwriteIfNewer"}, {FileOpsSettings::Conflict::Skip, "skip"},
    {FileOpsSettings::Conflict::KeepBoth, "keepBoth"}};
constexpr Name<FileOpsSettings::Links> kLinks[] = {{FileOpsSettings::Links::CopyAsLink, "link"},
                                                   {FileOpsSettings::Links::CopyTarget, "target"},
                                                   {FileOpsSettings::Links::Skip, "skip"}};
constexpr Name<FileOpsSettings::DeleteMode> kDeleteMode[] = {{FileOpsSettings::DeleteMode::RecycleBin, "recycleBin"},
                                                             {FileOpsSettings::DeleteMode::Permanent, "permanent"}};
constexpr Name<FileOpsSettings::ReadOnly> kReadOnly[] = {{FileOpsSettings::ReadOnly::Ask, "ask"},
                                                         {FileOpsSettings::ReadOnly::Delete, "delete"},
                                                         {FileOpsSettings::ReadOnly::Skip, "skip"}};
constexpr Name<FileOpsSettings::RecordMode> kRecord[] = {{FileOpsSettings::RecordMode::Single, "single"},
                                                         {FileOpsSettings::RecordMode::Double, "double"},
                                                         {FileOpsSettings::RecordMode::Auto, "auto"}};
constexpr Name<FileOpsSettings::RenameProblem> kRenameProblem[] = {{FileOpsSettings::RenameProblem::Block, "block"},
                                                                   {FileOpsSettings::RenameProblem::SkipProblems, "skip"}};
constexpr Name<ElevationSettings::HelperLifetime> kHelper[] = {{ElevationSettings::HelperLifetime::PerOperation, "perOperation"},
                                                               {ElevationSettings::HelperLifetime::FiveMinutes, "fiveMinutes"},
                                                               {ElevationSettings::HelperLifetime::UntilExit, "untilExit"}};
constexpr Name<ElevationSettings::Ownership> kOwnership[] = {{ElevationSettings::Ownership::Ask, "ask"},
                                                             {ElevationSettings::Ownership::SkipSilently, "skip"}};
constexpr Name<ElevationSettings::OwnershipButton> kOwnershipButton[] = {{ElevationSettings::OwnershipButton::Skip, "skip"},
                                                                         {ElevationSettings::OwnershipButton::TakeOwnership, "take"},
                                                                         {ElevationSettings::OwnershipButton::Cancel, "cancel"}};
constexpr Name<KeyBindingSettings::ProgressEsc> kProgressEsc[] = {
    {KeyBindingSettings::ProgressEsc::ConfirmCancel, "confirm"}, {KeyBindingSettings::ProgressEsc::CancelImmediately, "cancel"},
    {KeyBindingSettings::ProgressEsc::HideWindow, "hide"}};
constexpr Name<fl::GroupCondition::Field> kField[] = {
    {fl::GroupCondition::Field::Extension, "extension"}, {fl::GroupCondition::Field::Name, "name"},
    {fl::GroupCondition::Field::Attributes, "attributes"}, {fl::GroupCondition::Field::MimeType, "mime"},
    {fl::GroupCondition::Field::Size, "size"}, {fl::GroupCondition::Field::Modified, "modified"},
    {fl::GroupCondition::Field::Created, "created"}};
constexpr Name<fl::GroupCondition::Op> kOp[] = {
    {fl::GroupCondition::Op::AnyOf, "anyOf"}, {fl::GroupCondition::Op::NoneOf, "noneOf"}, {fl::GroupCondition::Op::Wildcard, "wildcard"},
    {fl::GroupCondition::Op::Regex, "regex"}, {fl::GroupCondition::Op::Contains, "contains"}, {fl::GroupCondition::Op::Equals, "equals"},
    {fl::GroupCondition::Op::Has, "has"}, {fl::GroupCondition::Op::HasNot, "hasNot"}, {fl::GroupCondition::Op::MimeStartsWith, "mimeStarts"},
    {fl::GroupCondition::Op::MimeEquals, "mimeEquals"}, {fl::GroupCondition::Op::AtLeast, "atLeast"}, {fl::GroupCondition::Op::AtMost, "atMost"},
    {fl::GroupCondition::Op::SizeEquals, "sizeEquals"}, {fl::GroupCondition::Op::Between, "between"}, {fl::GroupCondition::Op::Within, "within"},
    {fl::GroupCondition::Op::OlderThan, "olderThan"}};
constexpr Name<fl::FileGroupSettings::Merge> kMerge[] = {{fl::FileGroupSettings::Merge::FirstOnly, "first"},
                                                         {fl::FileGroupSettings::Merge::PerProperty, "perProperty"}};
constexpr Name<fl::ColumnDef::Kind> kKind[] = {{fl::ColumnDef::Kind::BuiltinField, "field"},
                                               {fl::ColumnDef::Kind::WindowsProperty, "property"},
                                               {fl::ColumnDef::Kind::Expression, "expression"}};
constexpr Name<fl::ColumnDef::BandPos> kBand[] = {{fl::ColumnDef::BandPos::Row0Full, "row0"}, {fl::ColumnDef::BandPos::Row1, "row1"},
                                                  {fl::ColumnDef::BandPos::Hidden, "hidden"}};
constexpr Name<fl::ColumnDef::Empty> kEmpty[] = {{fl::ColumnDef::Empty::Dash, "dash"}, {fl::ColumnDef::Empty::Blank, "blank"},
                                                 {fl::ColumnDef::Empty::Fallback, "fallback"}};
constexpr Name<fl::ColumnDef::Sort> kSort[] = {{fl::ColumnDef::Sort::NaturalName, "natural"}, {fl::ColumnDef::Sort::Date, "date"},
                                               {fl::ColumnDef::Sort::Number, "number"}, {fl::ColumnDef::Sort::NumberTuple, "tuple"},
                                               {fl::ColumnDef::Sort::Text, "text"}};
constexpr Name<fl::ColumnSetRule::Type> kRule[] = {
    {fl::ColumnSetRule::Type::Default, "default"}, {fl::ColumnSetRule::Type::GroupRatio, "groupRatio"},
    {fl::ColumnSetRule::Type::PathWildcard, "path"}, {fl::ColumnSetRule::Type::KnownFolder, "knownFolder"},
    {fl::ColumnSetRule::Type::Manual, "manual"}};

// ---------------------------------------------------------------- 값 도우미

QJsonValue colorToJson(const std::optional<QColor> &c)
{
    return c ? QJsonValue(fs::colorHex(*c)) : QJsonValue();
}

std::optional<QColor> colorFromJson(const QJsonValue &v)
{
    const QColor c = fs::parseColorHex(v.toString());
    return c.isValid() ? std::optional<QColor>(c) : std::nullopt;
}

QJsonArray stringsToJson(const QStringList &list)
{
    return QJsonArray::fromStringList(list);
}

QStringList stringsFromJson(const QJsonValue &v, const QStringList &fallback = {})
{
    if (!v.isArray())
        return fallback;
    QStringList out;
    for (const QJsonValue &item : v.toArray())
        out.append(item.toString());
    return out;
}

int intOr(const QJsonObject &o, const char *key, int fallback)
{
    const QJsonValue v = o.value(QLatin1StringView(key));
    return v.isDouble() ? v.toInt() : fallback;
}

bool boolOr(const QJsonObject &o, const char *key, bool fallback)
{
    const QJsonValue v = o.value(QLatin1StringView(key));
    return v.isBool() ? v.toBool() : fallback;
}

QString stringOr(const QJsonObject &o, const char *key, const QString &fallback)
{
    const QJsonValue v = o.value(QLatin1StringView(key));
    return v.isString() ? v.toString() : fallback;
}

QString alignToText(Qt::Alignment a)
{
    return (a & Qt::AlignRight) ? u"right"_s : (a & Qt::AlignHCenter) ? u"center"_s : u"left"_s;
}

Qt::Alignment alignFromText(const QString &t)
{
    return t == u"right" ? Qt::AlignRight : t == u"center" ? Qt::AlignHCenter : Qt::AlignLeft;
}

// ---------------------------------------------------------------- 구역별

QJsonObject groupsToJson(const fl::FileGroupSettings &s)
{
    QJsonArray groups;
    for (const fl::FileGroup &g : s.groups) {
        QJsonArray conditions;
        for (const fl::GroupCondition &c : g.conditions)
            conditions.append(QJsonObject{{u"field"_s, enumToJson(c.field, kField)}, {u"op"_s, enumToJson(c.op, kOp)}, {u"value"_s, c.value}});
        QJsonObject style{{u"textLight"_s, colorToJson(g.style.textLight)}, {u"textDark"_s, colorToJson(g.style.textDark)},
                          {u"backLight"_s, colorToJson(g.style.backLight)}, {u"backDark"_s, colorToJson(g.style.backDark)},
                          {u"darkFromLight"_s, g.style.darkFromLight}, {u"bold"_s, g.style.bold}, {u"italic"_s, g.style.italic},
                          {u"underline"_s, g.style.underline}, {u"strike"_s, g.style.strike}};
        groups.append(QJsonObject{{u"id"_s, g.id}, {u"name"_s, g.name}, {u"builtin"_s, g.builtin}, {u"matchAll"_s, g.matchAll},
                                  {u"conditions"_s, conditions}, {u"style"_s, style}});
    }
    return QJsonObject{{u"merge"_s, enumToJson(s.merge, kMerge)}, {u"items"_s, groups}};
}

fl::FileGroupSettings groupsFromJson(const QJsonObject &o)
{
    fl::FileGroupSettings s = fl::FileGroupSettings::defaults();
    if (o.isEmpty())
        return s;
    s.merge = enumFromJson(o.value(u"merge"_s), kMerge, s.merge);
    if (!o.value(u"items"_s).isArray())
        return s;
    s.groups.clear();
    for (const QJsonValue &v : o.value(u"items"_s).toArray()) {
        const QJsonObject go = v.toObject();
        fl::FileGroup g;
        g.id = go.value(u"id"_s).toString();
        g.name = go.value(u"name"_s).toString();
        g.builtin = go.value(u"builtin"_s).toBool();
        g.matchAll = go.value(u"matchAll"_s).toBool();
        for (const QJsonValue &cv : go.value(u"conditions"_s).toArray()) {
            const QJsonObject co = cv.toObject();
            g.conditions.append({enumFromJson(co.value(u"field"_s), kField, fl::GroupCondition::Field::Extension),
                                 enumFromJson(co.value(u"op"_s), kOp, fl::GroupCondition::Op::AnyOf), co.value(u"value"_s).toString()});
        }
        const QJsonObject so = go.value(u"style"_s).toObject();
        g.style.textLight = colorFromJson(so.value(u"textLight"_s));
        g.style.textDark = colorFromJson(so.value(u"textDark"_s));
        g.style.backLight = colorFromJson(so.value(u"backLight"_s));
        g.style.backDark = colorFromJson(so.value(u"backDark"_s));
        g.style.darkFromLight = so.value(u"darkFromLight"_s).toBool();
        g.style.bold = so.value(u"bold"_s).toBool();
        g.style.italic = so.value(u"italic"_s).toBool();
        g.style.underline = so.value(u"underline"_s).toBool();
        g.style.strike = so.value(u"strike"_s).toBool();
        s.groups.append(g);
    }
    return s;
}

QJsonObject columnsToJson(const fl::ColumnSettings &s)
{
    QJsonArray sets;
    for (const fl::ColumnSet &set : s.sets) {
        QJsonArray columns;
        for (const fl::ColumnDef &d : set.columns) {
            columns.append(QJsonObject{{u"id"_s, d.id}, {u"title"_s, d.title}, {u"kind"_s, enumToJson(d.kind, kKind)}, {u"source"_s, d.source},
                                       {u"width"_s, d.width}, {u"align"_s, alignToText(d.align)}, {u"single"_s, d.showInSingleLine},
                                       {u"bandPos"_s, enumToJson(d.bandPos, kBand)}, {u"empty"_s, enumToJson(d.empty, kEmpty)},
                                       {u"fallback"_s, d.fallbackColumnId}, {u"sort"_s, enumToJson(d.sort, kSort)}, {u"custom"_s, d.custom},
                                       {u"sample"_s, d.sample}, {u"folderValues"_s, stringsToJson(d.folderValues)}});
        }
        const QJsonObject rule{{u"type"_s, enumToJson(set.rule.type, kRule)}, {u"group"_s, set.rule.groupId},
                               {u"ratio"_s, set.rule.ratioPercent}, {u"path"_s, set.rule.pathPattern}, {u"knownFolder"_s, set.rule.knownFolder}};
        sets.append(QJsonObject{{u"id"_s, set.id}, {u"name"_s, set.name}, {u"autoApply"_s, set.autoApply}, {u"rule"_s, rule},
                                {u"columns"_s, columns}});
    }
    return QJsonObject{{u"sets"_s, sets}};
}

fl::ColumnSettings columnsFromJson(const QJsonObject &o)
{
    fl::ColumnSettings s = fl::ColumnSettings::defaults();
    if (!o.value(u"sets"_s).isArray())
        return s;
    s.sets.clear();
    for (const QJsonValue &v : o.value(u"sets"_s).toArray()) {
        const QJsonObject so = v.toObject();
        fl::ColumnSet set;
        set.id = so.value(u"id"_s).toString();
        set.name = so.value(u"name"_s).toString();
        set.autoApply = so.value(u"autoApply"_s).toBool();
        const QJsonObject ro = so.value(u"rule"_s).toObject();
        set.rule.type = enumFromJson(ro.value(u"type"_s), kRule, fl::ColumnSetRule::Type::Manual);
        set.rule.groupId = ro.value(u"group"_s).toString();
        set.rule.ratioPercent = intOr(ro, "ratio", 60);
        set.rule.pathPattern = ro.value(u"path"_s).toString();
        set.rule.knownFolder = ro.value(u"knownFolder"_s).toString();
        for (const QJsonValue &cv : so.value(u"columns"_s).toArray()) {
            const QJsonObject co = cv.toObject();
            fl::ColumnDef d;
            d.id = co.value(u"id"_s).toString();
            d.title = co.value(u"title"_s).toString();
            d.kind = enumFromJson(co.value(u"kind"_s), kKind, fl::ColumnDef::Kind::BuiltinField);
            d.source = co.value(u"source"_s).toString();
            d.width = intOr(co, "width", 100);
            d.align = alignFromText(co.value(u"align"_s).toString());
            d.showInSingleLine = boolOr(co, "single", true);
            d.bandPos = enumFromJson(co.value(u"bandPos"_s), kBand, fl::ColumnDef::BandPos::Row1);
            d.empty = enumFromJson(co.value(u"empty"_s), kEmpty, fl::ColumnDef::Empty::Dash);
            d.fallbackColumnId = co.value(u"fallback"_s).toString();
            d.sort = enumFromJson(co.value(u"sort"_s), kSort, fl::ColumnDef::Sort::Text);
            d.custom = co.value(u"custom"_s).toBool();
            d.sample = co.value(u"sample"_s).toString();
            d.folderValues = stringsFromJson(co.value(u"folderValues"_s));
            set.columns.append(d);
        }
        s.sets.append(set);
    }
    return s;
}

QString keysToText(const QList<QKeySequence> &keys)
{
    QStringList parts;
    for (const QKeySequence &k : keys)
        parts.append(k.toString(QKeySequence::PortableText));
    return parts.join(u';');
}

QList<QKeySequence> keysFromText(const QString &text)
{
    QList<QKeySequence> out;
    for (const QString &part : text.split(u';', Qt::SkipEmptyParts))
        out.append(QKeySequence::fromString(part, QKeySequence::PortableText));
    return out;
}

} // namespace

fm::filelist::ListAppearance PanelSettings::toListAppearance(const AppearanceSettings &look) const
{
    fl::ListAppearance a;
    a.density = look.density;
    a.fontFamily = look.listFontFamily;
    a.fontPx = look.listFontPx;
    a.oneLineSeparator = separator1;
    a.twoLineSeparator = separator2;
    a.nameBelow = nameBelow;
    a.invertCursor = inverseCursor;
    a.invertSelection = inverseSelection;
    a.boldSelection = boldSelection;
    a.inactiveCursor = inactiveCursor;
    a.nameElide = nameOverflow;
    a.autoWidth = autoSwitchWidthPx;
    a.autoPercent = autoSwitchTruncatedPct;
    return a;
}

fm::filelist::ThumbnailAppearance ThumbSettings::toThumbnailAppearance(const PanelSettings &panel) const
{
    fl::ThumbnailAppearance a;
    a.size = sizePx;
    a.nameLines = nameLines;
    a.info = info;
    a.fill = fill;
    a.badges = typeBadge;
    a.invertCursor = panel.inverseCursor;
    a.invertSelection = panel.inverseSelection;
    a.boldSelection = panel.boldSelection;
    a.inactiveCursor = panel.inactiveCursor;
    return a;
}

QJsonObject AppSettings::toJson() const
{
    QJsonObject root;
    root[u"version"_s] = version;
    const AppearanceSettings &a = appearance;
    root[u"appearance"_s] = QJsonObject{
        {u"scheme"_s, enumToJson(a.scheme, kScheme)}, {u"design"_s, enumToJson(a.design, kDesign)},
        {u"darkTone"_s, enumToJson(a.darkTone, kDarkTone)}, {u"darkTitleBar"_s, a.darkTitleBar},
        {u"coloredTitleBar"_s, a.coloredTitleBar}, {u"listFont"_s, a.listFontFamily}, {u"listFontPx"_s, a.listFontPx},
        {u"monoFont"_s, a.monoFontFamily}, {u"monoFontPx"_s, a.monoFontPx}, {u"density"_s, enumToJson(a.density, kDensity)},
        {u"displayScale"_s, a.displayScalePercent}};
    const GeneralSettings &g = general;
    root[u"general"_s] = QJsonObject{{u"language"_s, g.language}, {u"startup"_s, enumToJson(g.startup, kStartup)},
                                     {u"startupFolders"_s, stringsToJson(g.startupFolders)}, {u"singleInstance"_s, g.singleInstance},
                                     {u"showCommandLine"_s, g.showCommandLine}, {u"showFunctionKeyBar"_s, g.showFunctionKeyBar},
                                     {u"trayIcon"_s, enumToJson(g.trayIcon, kTray)}};
    root[u"tabs"_s] = QJsonObject{{u"newTabPosition"_s, enumToJson(tabs.newTabPosition, kNewTab)},
                                  {u"title"_s, enumToJson(tabs.title, kTabTitle)}, {u"rememberViewPerTab"_s, tabs.rememberViewPerTab}};
    root[u"theme"_s] = QJsonObject{{u"schemeId"_s, theme.schemeId}, {u"scheme"_s, theme.scheme.toJson()},
                                   {u"editBothVariants"_s, theme.editBothVariants}};
    const PanelSettings &p = panel;
    root[u"panel"_s] = QJsonObject{
        {u"defaultViewMode"_s, enumToJson(p.defaultViewMode, kViewMode)}, {u"autoWidth"_s, p.autoSwitchWidthPx},
        {u"autoPercent"_s, p.autoSwitchTruncatedPct}, {u"separator1"_s, enumToJson(p.separator1, kSeparator)},
        {u"separator2"_s, enumToJson(p.separator2, kSeparator)}, {u"nameBelow"_s, p.nameBelow},
        {u"nameOverflow"_s, enumToJson(p.nameOverflow, kElide)}, {u"showHidden"_s, p.showHidden}, {u"showProtectedOs"_s, p.showProtectedOs},
        {u"foldersFirst"_s, p.foldersFirst}, {u"sizeUnit"_s, enumToJson(p.sizeUnit, kSizeUnit)}, {u"dateFormat"_s, p.dateFormat},
        {u"inverseCursor"_s, p.inverseCursor}, {u"inverseSelection"_s, p.inverseSelection}, {u"boldSelection"_s, p.boldSelection},
        {u"inactiveCursor"_s, enumToJson(p.inactiveCursor, kInactive)}};
    const ThumbSettings &t = thumbs;
    root[u"thumbs"_s] = QJsonObject{
        {u"size"_s, t.sizePx}, {u"nameLines"_s, t.nameLines}, {u"info"_s, enumToJson(t.info, kInfo)}, {u"fill"_s, t.fill},
        {u"typeBadge"_s, t.typeBadge}, {u"provider"_s, enumToJson(t.provider, kProvider)}, {u"targets"_s, t.targets},
        {u"iconsOnlyOnNetwork"_s, t.iconsOnlyOnNetworkRemovable}, {u"concurrency"_s, t.concurrency},
        {u"autoFolders"_s, t.autoThumbnailFolders}, {u"autoPercent"_s, t.autoThumbnailPct}};
    root[u"groups"_s] = groupsToJson(groups);
    root[u"columns"_s] = columnsToJson(columns);
    const FileOpsSettings &f = fileOps;
    root[u"fileOps"_s] = QJsonObject{
        {u"onConflict"_s, enumToJson(f.onConflict, kConflict)}, {u"verifyHash"_s, f.verifyHash}, {u"keepAttributes"_s, f.keepAttributes},
        {u"copyAcl"_s, f.copyAcl}, {u"copyAds"_s, f.copyAds}, {u"links"_s, enumToJson(f.links, kLinks)},
        {u"deleteMode"_s, enumToJson(f.deleteMode, kDeleteMode)}, {u"confirmDelete"_s, f.confirmDelete},
        {u"readOnly"_s, enumToJson(f.readOnly, kReadOnly)}, {u"progressDelayMs"_s, f.progressDelayMs},
        {u"progressDetailed"_s, f.progressDetailed}, {u"closeWhenDone"_s, f.closeWhenDone}, {u"notifyWhenDone"_s, f.notifyWhenDone},
        {u"maxConcurrentJobs"_s, f.maxConcurrentJobs}, {u"renamePreview"_s, enumToJson(f.renamePreview, kRecord)},
        {u"renameDefaultPreset"_s, f.renameDefaultPreset}, {u"renameUndoDepth"_s, f.renameUndoDepth},
        {u"renameOnProblem"_s, enumToJson(f.renameOnProblem, kRenameProblem)}};
    const ElevationSettings &e = elevation;
    root[u"elevation"_s] = QJsonObject{
        {u"preflight"_s, e.preflight}, {u"applyToRemaining"_s, e.applyToRemainingDefault},
        {u"helperLifetime"_s, enumToJson(e.helperLifetime, kHelper)}, {u"uacTimeoutSec"_s, e.uacTimeoutSec},
        {u"takeOwnership"_s, enumToJson(e.takeOwnership, kOwnership)}, {u"ownershipDefault"_s, enumToJson(e.ownershipDefault, kOwnershipButton)},
        {u"backupAcl"_s, e.backupAclBeforeChange}, {u"adminTitleWarning"_s, e.adminTitleWarning},
        {u"protectedPaths"_s, stringsToJson(e.protectedPathsUser)}, {u"logRetentionDays"_s, e.logRetentionDays}};
    QJsonObject overrides;
    for (auto it = keys.overrides.cbegin(); it != keys.overrides.cend(); ++it)
        overrides[it.key()] = keysToText(it.value());
    root[u"keys"_s] = QJsonObject{{u"layout"_s, keys.layout}, {u"overrides"_s, overrides},
                                  {u"alwaysShowMnemonics"_s, keys.alwaysShowMnemonics},
                                  {u"progressEsc"_s, enumToJson(keys.progressEsc, kProgressEsc)}};
    root[u"dialog"_s] = QJsonObject{{u"lastPage"_s, dialog.lastPage}, {u"geometry"_s, QString::fromLatin1(dialog.geometry.toBase64())},
                                    {u"themeView"_s, dialog.themeView}};
    auto tabsToJson = [](const QList<SessionState::Tab> &tabs) {
        QJsonArray array;
        for (const SessionState::Tab &t : tabs)
            array.append(QJsonObject{{u"local"_s, t.local}, {u"path"_s, t.path}, {u"mode"_s, enumToJson(t.mode, kViewMode)},
                                     {u"modeSet"_s, t.modeSet}});
        return array;
    };
    if (!session.isEmpty()) {
        root[u"session"_s] = QJsonObject{{u"left"_s, tabsToJson(session.left)}, {u"right"_s, tabsToJson(session.right)},
                                         {u"leftCurrent"_s, session.leftCurrent}, {u"rightCurrent"_s, session.rightCurrent},
                                         {u"rightActive"_s, session.rightActive}};
    }
    return root;
}

AppSettings AppSettings::fromJson(const QJsonObject &root)
{
    AppSettings s;
    const AppSettings d;  // 기본값
    s.version = intOr(root, "version", 1);

    const QJsonObject a = root.value(u"appearance"_s).toObject();
    s.appearance.scheme = enumFromJson(a.value(u"scheme"_s), kScheme, d.appearance.scheme);
    s.appearance.design = enumFromJson(a.value(u"design"_s), kDesign, d.appearance.design);
    s.appearance.darkTone = enumFromJson(a.value(u"darkTone"_s), kDarkTone, d.appearance.darkTone);
    s.appearance.darkTitleBar = boolOr(a, "darkTitleBar", d.appearance.darkTitleBar);
    s.appearance.coloredTitleBar = boolOr(a, "coloredTitleBar", d.appearance.coloredTitleBar);
    s.appearance.listFontFamily = stringOr(a, "listFont", d.appearance.listFontFamily);
    s.appearance.listFontPx = std::clamp(intOr(a, "listFontPx", d.appearance.listFontPx), 9, 20);
    s.appearance.monoFontFamily = stringOr(a, "monoFont", d.appearance.monoFontFamily);
    s.appearance.monoFontPx = std::clamp(intOr(a, "monoFontPx", d.appearance.monoFontPx), 9, 18);
    s.appearance.density = enumFromJson(a.value(u"density"_s), kDensity, d.appearance.density);
    s.appearance.displayScalePercent = intOr(a, "displayScale", 0);

    const QJsonObject g = root.value(u"general"_s).toObject();
    s.general.language = stringOr(g, "language", d.general.language);
    s.general.startup = enumFromJson(g.value(u"startup"_s), kStartup, d.general.startup);
    s.general.startupFolders = stringsFromJson(g.value(u"startupFolders"_s));
    s.general.singleInstance = boolOr(g, "singleInstance", d.general.singleInstance);
    s.general.showCommandLine = boolOr(g, "showCommandLine", d.general.showCommandLine);
    s.general.showFunctionKeyBar = boolOr(g, "showFunctionKeyBar", d.general.showFunctionKeyBar);
    s.general.trayIcon = enumFromJson(g.value(u"trayIcon"_s), kTray, d.general.trayIcon);

    const QJsonObject t = root.value(u"tabs"_s).toObject();
    s.tabs.newTabPosition = enumFromJson(t.value(u"newTabPosition"_s), kNewTab, d.tabs.newTabPosition);
    s.tabs.title = enumFromJson(t.value(u"title"_s), kTabTitle, d.tabs.title);
    s.tabs.rememberViewPerTab = boolOr(t, "rememberViewPerTab", d.tabs.rememberViewPerTab);

    const QJsonObject th = root.value(u"theme"_s).toObject();
    s.theme.schemeId = stringOr(th, "schemeId", d.theme.schemeId);
    if (const auto scheme = fs::ColorScheme::fromJson(th.value(u"scheme"_s).toObject()))
        s.theme.scheme = *scheme;
    s.theme.editBothVariants = boolOr(th, "editBothVariants", true);

    const QJsonObject p = root.value(u"panel"_s).toObject();
    s.panel.defaultViewMode = enumFromJson(p.value(u"defaultViewMode"_s), kViewMode, d.panel.defaultViewMode);
    s.panel.autoSwitchWidthPx = std::clamp(intOr(p, "autoWidth", 640), 200, 4000);
    s.panel.autoSwitchTruncatedPct = std::clamp(intOr(p, "autoPercent", 25), 1, 100);
    s.panel.separator1 = enumFromJson(p.value(u"separator1"_s), kSeparator, d.panel.separator1);
    s.panel.separator2 = enumFromJson(p.value(u"separator2"_s), kSeparator, d.panel.separator2);
    s.panel.nameBelow = boolOr(p, "nameBelow", d.panel.nameBelow);
    s.panel.nameOverflow = enumFromJson(p.value(u"nameOverflow"_s), kElide, d.panel.nameOverflow);
    s.panel.showHidden = boolOr(p, "showHidden", d.panel.showHidden);
    s.panel.showProtectedOs = boolOr(p, "showProtectedOs", d.panel.showProtectedOs);
    s.panel.foldersFirst = boolOr(p, "foldersFirst", d.panel.foldersFirst);
    s.panel.sizeUnit = enumFromJson(p.value(u"sizeUnit"_s), kSizeUnit, d.panel.sizeUnit);
    s.panel.dateFormat = stringOr(p, "dateFormat", d.panel.dateFormat);
    s.panel.inverseCursor = boolOr(p, "inverseCursor", d.panel.inverseCursor);
    s.panel.inverseSelection = boolOr(p, "inverseSelection", d.panel.inverseSelection);
    s.panel.boldSelection = boolOr(p, "boldSelection", d.panel.boldSelection);
    s.panel.inactiveCursor = enumFromJson(p.value(u"inactiveCursor"_s), kInactive, d.panel.inactiveCursor);

    const QJsonObject tb = root.value(u"thumbs"_s).toObject();
    s.thumbs.sizePx = intOr(tb, "size", 96);
    s.thumbs.nameLines = std::clamp(intOr(tb, "nameLines", 2), 0, 2);
    s.thumbs.info = enumFromJson(tb.value(u"info"_s), kInfo, d.thumbs.info);
    s.thumbs.fill = boolOr(tb, "fill", false);
    s.thumbs.typeBadge = boolOr(tb, "typeBadge", true);
    s.thumbs.provider = enumFromJson(tb.value(u"provider"_s), kProvider, d.thumbs.provider);
    s.thumbs.targets = intOr(tb, "targets", d.thumbs.targets);
    s.thumbs.iconsOnlyOnNetworkRemovable = boolOr(tb, "iconsOnlyOnNetwork", true);
    s.thumbs.concurrency = std::clamp(intOr(tb, "concurrency", 4), 1, 16);
    s.thumbs.autoThumbnailFolders = boolOr(tb, "autoFolders", true);
    s.thumbs.autoThumbnailPct = std::clamp(intOr(tb, "autoPercent", 70), 1, 100);

    s.groups = groupsFromJson(root.value(u"groups"_s).toObject());
    s.columns = columnsFromJson(root.value(u"columns"_s).toObject());

    const QJsonObject f = root.value(u"fileOps"_s).toObject();
    s.fileOps.onConflict = enumFromJson(f.value(u"onConflict"_s), kConflict, d.fileOps.onConflict);
    s.fileOps.verifyHash = boolOr(f, "verifyHash", false);
    s.fileOps.keepAttributes = boolOr(f, "keepAttributes", true);
    s.fileOps.copyAcl = boolOr(f, "copyAcl", false);
    s.fileOps.copyAds = boolOr(f, "copyAds", true);
    s.fileOps.links = enumFromJson(f.value(u"links"_s), kLinks, d.fileOps.links);
    s.fileOps.deleteMode = enumFromJson(f.value(u"deleteMode"_s), kDeleteMode, d.fileOps.deleteMode);
    s.fileOps.confirmDelete = boolOr(f, "confirmDelete", true);
    s.fileOps.readOnly = enumFromJson(f.value(u"readOnly"_s), kReadOnly, d.fileOps.readOnly);
    s.fileOps.progressDelayMs = intOr(f, "progressDelayMs", 1000);
    s.fileOps.progressDetailed = boolOr(f, "progressDetailed", true);
    s.fileOps.closeWhenDone = boolOr(f, "closeWhenDone", true);
    s.fileOps.notifyWhenDone = boolOr(f, "notifyWhenDone", true);
    s.fileOps.maxConcurrentJobs = intOr(f, "maxConcurrentJobs", 1);
    s.fileOps.renamePreview = enumFromJson(f.value(u"renamePreview"_s), kRecord, d.fileOps.renamePreview);
    s.fileOps.renameDefaultPreset = stringOr(f, "renameDefaultPreset", d.fileOps.renameDefaultPreset);
    s.fileOps.renameUndoDepth = intOr(f, "renameUndoDepth", 20);
    s.fileOps.renameOnProblem = enumFromJson(f.value(u"renameOnProblem"_s), kRenameProblem, d.fileOps.renameOnProblem);

    const QJsonObject e = root.value(u"elevation"_s).toObject();
    s.elevation.preflight = boolOr(e, "preflight", true);
    s.elevation.applyToRemainingDefault = boolOr(e, "applyToRemaining", true);
    s.elevation.helperLifetime = enumFromJson(e.value(u"helperLifetime"_s), kHelper, d.elevation.helperLifetime);
    s.elevation.uacTimeoutSec = intOr(e, "uacTimeoutSec", 120);
    s.elevation.takeOwnership = enumFromJson(e.value(u"takeOwnership"_s), kOwnership, d.elevation.takeOwnership);
    s.elevation.ownershipDefault = enumFromJson(e.value(u"ownershipDefault"_s), kOwnershipButton, d.elevation.ownershipDefault);
    s.elevation.backupAclBeforeChange = boolOr(e, "backupAcl", true);
    s.elevation.adminTitleWarning = boolOr(e, "adminTitleWarning", true);
    s.elevation.protectedPathsUser = stringsFromJson(e.value(u"protectedPaths"_s), d.elevation.protectedPathsUser);
    s.elevation.logRetentionDays = intOr(e, "logRetentionDays", 30);

    const QJsonObject k = root.value(u"keys"_s).toObject();
    s.keys.layout = stringOr(k, "layout", d.keys.layout);
    const QJsonObject overrides = k.value(u"overrides"_s).toObject();
    for (auto it = overrides.begin(); it != overrides.end(); ++it)
        s.keys.overrides.insert(it.key(), keysFromText(it.value().toString()));
    s.keys.alwaysShowMnemonics = boolOr(k, "alwaysShowMnemonics", false);
    s.keys.progressEsc = enumFromJson(k.value(u"progressEsc"_s), kProgressEsc, d.keys.progressEsc);

    const QJsonObject dl = root.value(u"dialog"_s).toObject();
    s.dialog.lastPage = stringOr(dl, "lastPage", d.dialog.lastPage);
    s.dialog.geometry = QByteArray::fromBase64(dl.value(u"geometry"_s).toString().toLatin1());
    s.dialog.themeView = stringOr(dl, "themeView", d.dialog.themeView);

    const QJsonObject se = root.value(u"session"_s).toObject();
    auto tabsFromJson = [](const QJsonValue &value) {
        QList<SessionState::Tab> tabs;
        for (const QJsonValue &v : value.toArray()) {
            const QJsonObject o = v.toObject();
            SessionState::Tab t;
            t.local = boolOr(o, "local", false);
            t.path = stringOr(o, "path", QString());
            t.mode = enumFromJson(o.value(u"mode"_s), kViewMode, fl::ViewMode::Auto);
            t.modeSet = boolOr(o, "modeSet", false);
            if (!t.path.isEmpty())
                tabs.append(t);
        }
        return tabs;
    };
    s.session.left = tabsFromJson(se.value(u"left"_s));
    s.session.right = tabsFromJson(se.value(u"right"_s));
    s.session.leftCurrent = intOr(se, "leftCurrent", 0);
    s.session.rightCurrent = intOr(se, "rightCurrent", 0);
    s.session.rightActive = boolOr(se, "rightActive", true);
    return s;
}

Sections differingSections(const AppSettings &a, const AppSettings &b)
{
    Sections s;
    if (!(a.appearance == b.appearance))
        s |= Section::Appearance;
    if (!(a.general == b.general))
        s |= Section::General;
    if (!(a.tabs == b.tabs))
        s |= Section::Tabs;
    if (!(a.theme == b.theme))
        s |= Section::Theme;
    if (!(a.panel == b.panel))
        s |= Section::Panel;
    if (!(a.thumbs == b.thumbs))
        s |= Section::Thumbs;
    if (!(a.groups == b.groups))
        s |= Section::Groups;
    if (!(a.columns == b.columns))
        s |= Section::Columns;
    if (!(a.fileOps == b.fileOps))
        s |= Section::FileOps;
    if (!(a.elevation == b.elevation))
        s |= Section::Elevation;
    if (!(a.keys == b.keys))
        s |= Section::Keys;
    if (!(a.dialog == b.dialog))
        s |= Section::Dialog;
    if (!(a.session == b.session))
        s |= Section::Session;
    return s;
}

QStringList builtinProtectedPaths()
{
    auto env = [](const char *name, const QString &fallback) {
        const QString v = qEnvironmentVariable(name);
        return QDir::toNativeSeparators(v.isEmpty() ? fallback : v);
    };
    const QString programFiles = env("ProgramFiles", u"C:\\Program Files"_s);
    return {env("SystemRoot", u"C:\\Windows"_s), programFiles, env("ProgramFiles(x86)", u"C:\\Program Files (x86)"_s),
            env("ProgramData", u"C:\\ProgramData"_s), programFiles + u"\\WindowsApps"_s};
}

} // namespace fm::settings
