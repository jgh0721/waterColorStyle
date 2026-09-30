#include "fmdialogs/Planners.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

QString withSlash(QString dir)
{
    dir.replace(u'/', u'\\');
    if (!dir.endsWith(u'\\'))
        dir += u'\\';
    return dir;
}

bool isAbsolute(const QString &path)
{
    return path.startsWith(u"\\\\") || (path.size() >= 2 && path.at(1) == u':');
}

/// "D:\a\b\..\c\" → "D:\a\c\" — 상대 경로의 . · ..를 정리한다(뿌리 "D:\" · "\\서버\공유\" 위로는 올라가지 않는다).
QString cleanDir(const QString &dir)
{
    const QString d = withSlash(dir);
    QString root;
    QString rest = d;
    if (d.startsWith(u"\\\\")) {
        const qsizetype server = d.indexOf(u'\\', 2);
        const qsizetype share = server < 0 ? -1 : d.indexOf(u'\\', server + 1);
        if (share < 0)
            return d;
        root = d.left(share + 1);
        rest = d.mid(share + 1);
    } else if (d.size() >= 3 && d.at(1) == u':') {
        root = d.left(3);
        rest = d.mid(3);
    }
    QStringList parts;
    for (const QString &part : rest.split(u'\\', Qt::SkipEmptyParts)) {
        if (part == u".")
            continue;
        if (part == u"..") {
            if (!parts.isEmpty())
                parts.removeLast();
            continue;
        }
        parts.append(part);
    }
    return parts.isEmpty() ? root : root + parts.join(u'\\') + u'\\';
}

QString extensionOf(const QString &name)
{
    const qsizetype dot = name.lastIndexOf(u'.');
    return dot <= 0 ? QString() : name.mid(dot);
}

} // namespace

QChar firstInvalidChar(const QString &name)
{
    static const QString invalid = u"\\/:*?\"<>|"_s;
    for (const QChar c : name) {
        if (invalid.contains(c) || c.unicode() < 32)
            return c;
    }
    return QChar();
}

bool isReservedName(const QString &name)
{
    static const QRegularExpression reserved(u"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$"_s,
                                             QRegularExpression::CaseInsensitiveOption);
    const QString base = name.section(u'.', 0, 0).trimmed();
    return reserved.match(base).hasMatch();
}

bool hasTrailingDotOrSpace(const QString &name)
{
    return !name.isEmpty() && (name.endsWith(u'.') || name.endsWith(u' '));
}

// ---------------------------------------------------------------- MoveRenamePlan

QString MoveRenamePlan::operationText() const
{
    switch (operation) {
    case Rename:     return QObject::tr("이름 변경");
    case Move:
    case MoveRename: return QObject::tr("이동");  // 보드: 이름이 함께 바뀌어도 "이동"
    case NoChange:   return QObject::tr("변경 없음");
    }
    return QString();
}

QString MoveRenamePlan::volumeText() const
{
    return sameVolume ? QObject::tr("같은 볼륨 · 즉시 처리") : QObject::tr("다른 볼륨 · 복사 후 삭제");
}

QString MoveRenamePlan::statusText() const
{
    switch (issue) {
    case None:             return QObject::tr("이 이름을 쓸 수 있습니다");
    case Empty:            return QObject::tr("이름을 입력하세요");
    case InvalidChar:      return QObject::tr("쓸 수 없는 문자: %1").arg(detail);
    case Reserved:         return QObject::tr("예약된 이름입니다: %1").arg(detail);
    case TrailingDotSpace: return QObject::tr("이름 끝에 마침표나 공백을 쓸 수 없습니다");
    case Exists:           return QObject::tr("같은 이름이 이미 있습니다");
    case ExtensionChange:  return QObject::tr("확장자가 바뀝니다 (%1)").arg(detail);
    case NewFolder:        return QObject::tr("새 폴더를 만듭니다");
    case MissingFolder:    return QObject::tr("대상 폴더가 없습니다");
    }
    return QString();
}

MoveRenamePlan planMoveRename(const QString &input, const QString &sourceDir, const QString &originalName,
                              const MoveRenameOptions &options, const FileSystemProbe &probe)
{
    MoveRenamePlan plan;
    QString text = input.trimmed();
    text.replace(u'/', u'\\');
    const QString source = cleanDir(sourceDir);
    const qsizetype slash = text.lastIndexOf(u'\\');
    if (slash >= 0) {
        const QString dirPart = text.left(slash + 1);
        plan.directory = cleanDir(isAbsolute(dirPart) ? dirPart : source + dirPart);
        plan.name = text.mid(slash + 1);
    } else {
        plan.directory = source;
        plan.name = text;
    }
    const bool sameDir = plan.directory.compare(source, Qt::CaseInsensitive) == 0;
    const bool sameName = plan.name == originalName;
    plan.operation = sameDir ? (sameName ? MoveRenamePlan::NoChange : MoveRenamePlan::Rename)
                             : (plan.name.compare(originalName, Qt::CaseInsensitive) == 0 ? MoveRenamePlan::Move
                                                                                           : MoveRenamePlan::MoveRename);
    plan.sameVolume = probe.sameVolume(plan.directory, source);

    auto fail = [&](MoveRenamePlan::Issue issue, const QString &detail = {}) {
        plan.issue = issue;
        plan.detail = detail;
        plan.canProceed = false;
        return plan;
    };
    if (plan.name.isEmpty())
        return fail(MoveRenamePlan::Empty);
    if (const QChar bad = firstInvalidChar(plan.name); !bad.isNull())
        return fail(MoveRenamePlan::InvalidChar, QString(bad));
    if (isReservedName(plan.name))
        return fail(MoveRenamePlan::Reserved, plan.name.section(u'.', 0, 0));
    if (hasTrailingDotOrSpace(plan.name))
        return fail(MoveRenamePlan::TrailingDotSpace);
    const QString target = plan.directory + plan.name;
    const bool selfCaseChange = sameDir && plan.name.compare(originalName, Qt::CaseInsensitive) == 0;
    if (!sameName && !selfCaseChange && probe.exists(target))
        return fail(MoveRenamePlan::Exists);

    plan.canProceed = plan.operation != MoveRenamePlan::NoChange;
    const QString oldExt = extensionOf(originalName);
    const QString newExt = extensionOf(plan.name);
    if (oldExt.compare(newExt, Qt::CaseInsensitive) != 0 && !options.allowExtensionChange) {
        plan.issue = MoveRenamePlan::ExtensionChange;
        plan.detail = (oldExt.isEmpty() ? QObject::tr("없음") : oldExt) + u" → "_s
                    + (newExt.isEmpty() ? QObject::tr("없음") : newExt);
        return plan;  // 경고 — 확인은 할 수 있다(확인 때 한 번 더 묻는다)
    }
    if (!sameDir && !probe.exists(plan.directory)) {
        if (options.createDirectories) {
            plan.issue = MoveRenamePlan::NewFolder;
        } else {
            return fail(MoveRenamePlan::MissingFolder);
        }
    }
    return plan;
}

// ---------------------------------------------------------------- FolderPlan

FolderPlan planFolders(const QString &baseDir, const QString &input, const FileSystemProbe &probe)
{
    using Node = fm::ui::FolderPlanView::Node;
    FolderPlan plan;
    QString base = baseDir;
    base.replace(u'/', u'\\');
    while (base.size() > 3 && base.endsWith(u'\\'))
        base.chop(1);
    const QString baseName = base.section(u'\\', -1, -1, QString::SectionSkipEmpty);
    plan.nodes.append({baseName.isEmpty() ? base : baseName, Node::Current});

    QString text = input;
    text.replace(u'/', u'\\');
    QStringList parts;
    const auto raw = text.split(u'\\');
    for (const QString &part : raw) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty())
            parts.append(trimmed);
    }
    if (parts.isEmpty())
        return plan;  // 이름이 비어 있음 — 만들기 꺼짐, 도움말 없음

    bool invalid = isAbsolute(text.trimmed());
    bool newStarted = false;
    QString path = base;
    for (const QString &part : std::as_const(parts)) {
        path += u'\\' + part;
        Node node;
        node.name = part;
        const bool bad = invalid || part == u".." || part == u"." || !firstInvalidChar(part).isNull()
                      || isReservedName(part) || hasTrailingDotOrSpace(part);
        if (bad) {
            node.state = Node::Invalid;
            invalid = true;
        } else if (!newStarted && probe.exists(path)) {
            node.state = Node::Existing;
        } else {
            node.state = Node::New;
            newStarted = true;
        }
        plan.nodes.append(node);
    }
    plan.relativePath = parts.join(u'\\');
    if (invalid) {
        plan.help = QObject::tr("쓸 수 없는 이름이 있습니다");
        plan.helpIsError = true;
        plan.canCreate = false;
    } else if (!newStarted) {
        plan.help = QObject::tr("이미 있는 폴더입니다");
        plan.canCreate = false;
    } else {
        plan.canCreate = true;
    }
    return plan;
}

} // namespace fm::dialogs
