#include "StateCodec_p.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace fm::dock::detail {

namespace {

constexpr const char *kSideKeys[4] = {"left", "top", "right", "bottom"};

QJsonObject nodeToJson(const LayoutNode &node)
{
    QJsonObject o;
    if (node.split) {
        o[u"split"_s] = node.orientation == Qt::Horizontal ? u"h"_s : u"v"_s;
        QJsonArray sizes;
        for (int s : node.sizes)
            sizes.append(s);
        o[u"sizes"_s] = sizes;
        QJsonArray children;
        for (const LayoutNode &child : node.children)
            children.append(nodeToJson(child));
        o[u"children"_s] = children;
    } else {
        o[u"area"_s] = QJsonArray::fromStringList(node.docks);
        if (!node.current.isEmpty())
            o[u"current"_s] = node.current;
    }
    if (node.central)
        o[u"central"_s] = true;
    return o;
}

QJsonArray rectToJson(const QRect &r)
{
    return QJsonArray{r.x(), r.y(), r.width(), r.height()};
}

struct Reader
{
    const StateLimits &limits;
    QString *error;
    int nodes = 0;
    QSet<QString> seen;   // 도크 id가 두 번 나오면 거절

    bool fail(const QString &why)
    {
        if (error)
            *error = why;
        return false;
    }

    bool readId(const QJsonValue &v, QString *out)
    {
        if (!v.isString())
            return fail(u"dock id must be a string"_s);
        const QString id = v.toString();
        if (id.isEmpty() || id.size() > limits.maxIdLength)
            return fail(u"bad dock id length"_s);
        if (seen.contains(id))
            return fail(u"duplicate dock id: "_s + id);
        seen.insert(id);
        *out = id;
        return true;
    }

    bool readNode(const QJsonValue &value, int depth, LayoutNode *out)
    {
        if (depth > limits.maxDepth)
            return fail(u"layout too deep"_s);
        if (++nodes > limits.maxNodes)
            return fail(u"too many layout nodes"_s);
        if (!value.isObject())
            return fail(u"layout node must be an object"_s);
        const QJsonObject o = value.toObject();
        out->central = o.value(u"central"_s).toBool(false);
        if (o.contains(u"split"_s)) {
            const QString split = o.value(u"split"_s).toString();
            if (split != u"h" && split != u"v")
                return fail(u"split must be h or v"_s);
            out->split = true;
            out->orientation = split == u"h" ? Qt::Horizontal : Qt::Vertical;
            const QJsonArray children = o.value(u"children"_s).toArray();
            const QJsonArray sizes = o.value(u"sizes"_s).toArray();
            for (const QJsonValue &child : children) {
                LayoutNode node;
                if (!readNode(child, depth + 1, &node))
                    return false;
                out->children.append(node);
            }
            for (const QJsonValue &s : sizes) {
                const int px = s.toInt(-1);
                if (px < 0 || px > 100000)
                    return fail(u"bad split size"_s);
                out->sizes.append(px);
            }
            if (!out->sizes.isEmpty() && out->sizes.size() != out->children.size())
                return fail(u"sizes and children differ"_s);
            return true;
        }
        if (!o.contains(u"area"_s))
            return fail(u"layout node needs split or area"_s);
        const QJsonArray docks = o.value(u"area"_s).toArray();
        for (const QJsonValue &d : docks) {
            QString id;
            if (!readId(d, &id))
                return false;
            out->docks.append(id);
        }
        out->current = o.value(u"current"_s).toString();
        if (!out->current.isEmpty() && !out->docks.contains(out->current))
            out->current.clear();
        return true;
    }
};

} // namespace

QStringList dockIds(const LayoutNode &node)
{
    if (!node.split)
        return node.docks;
    QStringList ids;
    for (const LayoutNode &child : node.children)
        ids += dockIds(child);
    return ids;
}

int stateUserVersion(const QByteArray &bytes)
{
    const QJsonDocument doc = QJsonDocument::fromJson(bytes);
    return doc.isObject() ? doc.object().value(u"user"_s).toInt(0) : 0;
}

namespace {

const LayoutNode *findIn(const LayoutNode &node, const QString &id)
{
    if (!node.split)
        return node.docks.contains(id) ? &node : nullptr;
    for (const LayoutNode &child : node.children) {
        if (const LayoutNode *hit = findIn(child, id))
            return hit;
    }
    return nullptr;
}

} // namespace

const LayoutNode *findAreaNode(const DockState &state, const QString &id)
{
    if (state.hasMain) {
        if (const LayoutNode *hit = findIn(state.main, id))
            return hit;
    }
    for (const FloatingState &f : state.floating) {
        if (const LayoutNode *hit = findIn(f.tree, id))
            return hit;
    }
    return nullptr;
}

QByteArray encodeState(const DockState &state)
{
    QJsonObject root;
    root[u"format"_s] = QString::fromLatin1(kStateFormat);
    root[u"version"_s] = kStateVersion;
    root[u"user"_s] = state.userVersion;
    if (state.hasMain) {
        QJsonObject main;
        main[u"tree"_s] = nodeToJson(state.main);
        root[u"main"_s] = main;
    }
    if (!state.floating.isEmpty()) {
        QJsonArray floating;
        for (const FloatingState &f : state.floating) {
            QJsonObject o;
            o[u"screen"_s] = f.screen;
            o[u"rect"_s] = rectToJson(f.rect);
            if (f.maximized)
                o[u"maximized"_s] = true;
            o[u"tree"_s] = nodeToJson(f.tree);
            floating.append(o);
        }
        root[u"floating"_s] = floating;
    }
    QJsonObject autoHide;
    for (int i = 0; i < 4; ++i) {
        if (state.autoHide[i].isEmpty())
            continue;
        QJsonArray side;
        for (const AutoHideState &a : state.autoHide[i]) {
            QJsonObject o;
            o[u"dock"_s] = a.dock;
            if (a.size > 0)
                o[u"size"_s] = a.size;
            side.append(o);
        }
        autoHide[QString::fromLatin1(kSideKeys[i])] = side;
    }
    if (!autoHide.isEmpty())
        root[u"autoHide"_s] = autoHide;
    if (!state.closed.isEmpty())
        root[u"closed"_s] = QJsonArray::fromStringList(state.closed);
    if (!state.locked.isEmpty())
        root[u"locked"_s] = QJsonArray::fromStringList(state.locked);
    if (!state.focus.isEmpty())
        root[u"focus"_s] = state.focus;
    if (!state.expanded.isEmpty())
        root[u"expanded"_s] = state.expanded;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

std::optional<DockState> decodeState(const QByteArray &bytes, int userVersion, QString *error,
                                     const StateLimits &limits)
{
    Reader reader{limits, error};
    if (bytes.isEmpty() || bytes.size() > limits.maxBytes) {
        reader.fail(u"state empty or too large"_s);
        return std::nullopt;
    }
    QJsonParseError parse{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
        reader.fail(u"not a JSON object"_s);
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    if (root.value(u"format"_s).toString() != QLatin1StringView(kStateFormat)
        || root.value(u"version"_s).toInt(-1) != kStateVersion) {
        reader.fail(u"unknown format or version"_s);
        return std::nullopt;
    }
    DockState state;
    state.userVersion = root.value(u"user"_s).toInt(0);
    if (state.userVersion != userVersion) {
        reader.fail(u"user version differs"_s);
        return std::nullopt;
    }
    if (root.contains(u"main"_s)) {
        state.hasMain = true;
        if (!reader.readNode(root.value(u"main"_s).toObject().value(u"tree"_s), 0, &state.main))
            return std::nullopt;
    }
    const QJsonArray floating = root.value(u"floating"_s).toArray();
    if (floating.size() > limits.maxFloating) {
        reader.fail(u"too many floating windows"_s);
        return std::nullopt;
    }
    for (const QJsonValue &v : floating) {
        const QJsonObject o = v.toObject();
        FloatingState f;
        f.screen = o.value(u"screen"_s).toString().left(256);
        const QJsonArray r = o.value(u"rect"_s).toArray();
        if (r.size() != 4) {
            reader.fail(u"bad floating rect"_s);
            return std::nullopt;
        }
        f.rect = QRect(r[0].toInt(), r[1].toInt(), std::clamp(r[2].toInt(), 80, 16000), std::clamp(r[3].toInt(), 60, 16000));
        f.maximized = o.value(u"maximized"_s).toBool(false);
        if (!reader.readNode(o.value(u"tree"_s), 0, &f.tree))
            return std::nullopt;
        state.floating.append(f);
    }
    const QJsonObject autoHide = root.value(u"autoHide"_s).toObject();
    for (int i = 0; i < 4; ++i) {
        const QJsonArray side = autoHide.value(QString::fromLatin1(kSideKeys[i])).toArray();
        for (const QJsonValue &v : side) {
            const QJsonObject o = v.toObject();
            AutoHideState a;
            if (!reader.readId(o.value(u"dock"_s), &a.dock))
                return std::nullopt;
            a.size = std::clamp(o.value(u"size"_s).toInt(0), 0, 16000);
            state.autoHide[i].append(a);
        }
    }
    for (const QJsonValue &v : root.value(u"closed"_s).toArray()) {
        const QString id = v.toString();
        if (!id.isEmpty() && id.size() <= limits.maxIdLength)
            state.closed.append(id);
    }
    for (const QJsonValue &v : root.value(u"locked"_s).toArray())
        state.locked.append(v.toString());
    state.focus = root.value(u"focus"_s).toString().left(limits.maxIdLength);
    state.expanded = root.value(u"expanded"_s).toString().left(limits.maxIdLength);
    return state;
}

} // namespace fm::dock::detail
