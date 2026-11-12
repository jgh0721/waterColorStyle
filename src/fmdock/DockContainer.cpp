#include "DockInternal_p.h"

#include <QEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <numeric>

using namespace Qt::StringLiterals;
namespace fs = fm::style;

namespace fm::dock {

namespace {

using detail::CentralPlaceholder;
using detail::DockSplitter;

bool isPlaceholder(const QWidget *w)
{
    return qobject_cast<const CentralPlaceholder *>(w) != nullptr;
}

/// 빈 중앙 자리를 뺀 자식 수.
int realCount(const QSplitter *s)
{
    int n = 0;
    for (int i = 0; i < s->count(); ++i) {
        if (!isPlaceholder(s->widget(i)))
            ++n;
    }
    return n;
}

QWidget *realChild(const QSplitter *s, int index)
{
    for (int i = 0; i < s->count(); ++i) {
        if (isPlaceholder(s->widget(i)))
            continue;
        if (index-- == 0)
            return s->widget(i);
    }
    return nullptr;
}

int sum(const QList<int> &sizes)
{
    return std::accumulate(sizes.cbegin(), sizes.cend(), 0);
}

/// 크기 목록을 total에 맞게 비례 조정한다(합이 0이면 고르게).
QList<int> scaled(const QList<int> &sizes, int total)
{
    QList<int> out;
    const int s = sum(sizes);
    if (sizes.isEmpty())
        return out;
    if (s <= 0) {
        for (qsizetype i = 0; i < sizes.size(); ++i)
            out.append(std::max(1, total / int(sizes.size())));
        return out;
    }
    for (int v : sizes)
        out.append(std::max(1, int(qint64(v) * total / s)));
    return out;
}

void dropSplitter(DockSplitter *s)
{
    s->hide();
    s->setParent(nullptr);
    s->deleteLater();
}

constexpr char kPendingSizes[] = "fmPendingSizes";

/// 분할의 크기 — 아직 배치되지 않았으면(보이기 전) 마지막으로 정해 둔 값, 그것도 없으면 고르게.
QList<int> sizesOf(const QSplitter *s)
{
    const QList<int> real = s->sizes();
    if (s->isVisible() && sum(real) > 0)
        return real;
    const QList<int> pending = s->property(kPendingSizes).value<QList<int>>();
    if (pending.size() == s->count())
        return pending;
    return QList<int>(s->count(), 300);
}

/// 크기를 정한다 — 보이기 전이면 다음 계산을 위해 따로 기억한다(QSplitter는 비율로만 쓴다).
void applySizes(QSplitter *s, const QList<int> &sizes)
{
    s->setSizes(sizes);
    if (s->isVisible())
        s->setProperty(kPendingSizes, QVariant());
    else
        s->setProperty(kPendingSizes, QVariant::fromValue(sizes));
}

} // namespace

// =============================================================================================
// DockContainer

DockContainer::DockContainer(DockManager *manager, QWidget *parent, FloatingWindow *floating)
    : QFrame(parent)
    , m_manager(manager)
    , m_floating(floating)
{
    setObjectName(floating ? u"fmFloatingDockContainer"_s : u"fmDockContainer"_s);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
}

DockContainer::~DockContainer()
{
    m_dying = true;
}

QList<DockArea *> DockContainer::dockAreas() const
{
    QList<DockArea *> out;
    for (DockArea *a : m_areas) {
        if (a)
            out.append(a);
    }
    return out;
}

QList<DockArea *> DockContainer::openedDockAreas() const
{
    QList<DockArea *> out;
    for (DockArea *a : m_areas) {
        if (a && a->openDockWidgetsCount() > 0)
            out.append(a);
    }
    return out;
}

QList<DockWidget *> DockContainer::dockWidgets() const
{
    QList<DockWidget *> out;
    for (DockArea *a : m_areas) {
        if (a)
            out += a->dockWidgets();
    }
    return out;
}

QList<DockWidget *> DockContainer::openedDockWidgets() const
{
    QList<DockWidget *> out;
    for (DockArea *a : m_areas) {
        if (a)
            out += a->openDockWidgets();
    }
    return out;
}

DockArea *DockContainer::dockAreaAt(const QPoint &globalPos) const
{
    for (DockArea *a : m_areas) {
        if (a && a->isVisible() && QRect(a->mapToGlobal(QPoint(0, 0)), a->size()).contains(globalPos))
            return a;
    }
    return nullptr;
}

QList<DockArea *> DockContainer::centralAreas() const
{
    QList<DockArea *> out;
    for (DockArea *a : m_areas) {
        if (a && a->isCentral())
            out.append(a);
    }
    return out;
}

QWidget *DockContainer::centralPlaceholder() const
{
    return m_placeholder;
}

bool DockContainer::hasCentralRegion() const
{
    return m_central != nullptr;
}

DockSplitter *DockContainer::centralSplitter() const
{
    return m_central;
}

DockWidget *DockContainer::topLevelDockWidget() const
{
    const QList<DockWidget *> open = openedDockWidgets();
    return open.size() == 1 ? open.first() : nullptr;
}

DockArea *DockContainer::createArea()
{
    auto *area = new DockArea(m_manager, this);
    m_areas.append(area);
    if (m_manager)
        Q_EMIT m_manager->dockAreaCreated(area);
    return area;
}

void DockContainer::setRoot(DockSplitter *root)
{
    if (m_root == root)
        return;
    if (m_root)
        m_layout->removeWidget(m_root);
    m_root = root;
    if (root) {
        root->setParent(this);
        m_layout->addWidget(root);
        root->show();
    }
}

int DockContainer::splitSize(QSplitter *splitter, int index, int fallback) const
{
    const QList<int> sizes = sizesOf(splitter);
    const int v = index >= 0 && index < sizes.size() ? sizes[index] : 0;
    return v > 0 ? v : fallback;
}

void DockContainer::resizeSlot(QWidget *descendant, Qt::Orientation orientation, int size)
{
    QWidget *child = descendant;
    auto *s = child ? qobject_cast<DockSplitter *>(child->parentWidget()) : nullptr;
    while (s && s->orientation() != orientation) {
        child = s;
        s = qobject_cast<DockSplitter *>(s->parentWidget());
    }
    if (!s)
        return;
    const int at = s->indexOf(child);
    QList<int> sizes = sizesOf(s);
    if (at < 0 || at >= sizes.size())
        return;
    const int total = std::max(sum(sizes), size + 1);
    QList<int> others = sizes;
    others.removeAt(at);
    QList<int> out = scaled(others, std::max(1, total - size));
    out.insert(at, std::max(1, size));
    applySizes(s, out);
}

void DockContainer::ensureCentralRegion()
{
    if (m_central)
        return;
    auto *central = new DockSplitter(Qt::Horizontal, m_manager);
    central->setCentral(true);
    m_placeholder = new CentralPlaceholder(m_manager, central);
    central->addWidget(m_placeholder);
    if (!m_root) {
        setRoot(central);
    } else {
        m_root->addWidget(central);
        QList<int> sizes = sizesOf(m_root);
        if (sum(sizes) > 0)
            sizes.last() = std::max(sizes.last(), sum(sizes) / 2);
        applySizes(m_root, sizes);
    }
    m_central = central;
    updateStretch();
    updateVisibility();
}

void DockContainer::insertArea(DockArea *area, DropArea where, DockArea *target)
{
    if (!area)
        return;
    if (area->parentWidget() != this && !qobject_cast<DockSplitter *>(area->parentWidget()))
        area->setParent(this);
    if (!m_areas.contains(area))
        m_areas.append(area);
    const bool equal = m_manager && m_manager->testConfigFlag(DockManager::EqualSplitOnInsertion);

    if (where == DropArea::Center && !target) {
        ensureCentralRegion();
        m_central->addWidget(area);
        if (realCount(m_central) > 1) {
            const QList<int> sizes = sizesOf(m_central);
            applySizes(m_central, scaled(QList<int>(sizes.size(), 1), std::max(1, sum(sizes))));
        }
    } else if (where == DropArea::Center) {
        // 탭으로 넣는 경우는 호출 측이 영역을 만들지 않는다 — 여기 오면 target 옆(오른쪽)
        insertArea(area, DropArea::Right, target);
        return;
    } else if (auto *parent = target ? qobject_cast<DockSplitter *>(target->parentWidget()) : nullptr) {
        const Qt::Orientation o = isHorizontal(where) ? Qt::Horizontal : Qt::Vertical;
        const bool after = !isLeading(where);
        const int idx = parent->indexOf(target);
        const QList<int> before = sizesOf(parent);
        if (parent->orientation() == o || (realCount(parent) == 1 && !parent->isCentral())) {
            const bool turned = parent->orientation() != o;
            parent->setOrientation(o);
            const int targetSize = turned ? 600 : splitSize(parent, idx, 300);
            const int at = after ? idx + 1 : idx;
            parent->insertWidget(at, area);
            QList<int> sizes = turned ? QList<int>(before.size(), 600) : before;
            if (sizes.size() != parent->count() - 1)
                sizes = QList<int>(parent->count() - 1, 300);
            const int half = std::max(1, targetSize / 2);
            sizes[idx] = std::max(1, targetSize - half);
            sizes.insert(at, half);
            if (equal)
                sizes = scaled(QList<int>(sizes.size(), 1), std::max(1, sum(sizes)));
            applySizes(parent, sizes);
        } else {
            int extent = o == Qt::Horizontal ? target->width() : target->height();
            if (!target->isVisible() || extent < 40)
                extent = 400;
            auto *split = new DockSplitter(o, m_manager);
            parent->insertWidget(idx, split);
            split->addWidget(target);
            split->insertWidget(after ? 1 : 0, area);
            const int half = std::max(1, extent / 2);
            applySizes(split, {half, half});
            if (before.size() == parent->count())
                applySizes(parent, before);
        }
    } else {
        // 컨테이너 가장자리
        const Qt::Orientation o = isHorizontal(where) ? Qt::Horizontal : Qt::Vertical;
        const bool after = !isLeading(where);
        int extent = o == Qt::Horizontal ? width() : height();
        if (!isVisible() || extent < 200)
            extent = o == Qt::Horizontal ? 1200 : 800;   // 아직 배치 전 — 비율만 맞으면 된다
        const int newSize = std::max(150, extent / 4);
        if (!m_root) {
            setRoot(new DockSplitter(o, m_manager));
            m_root->addWidget(area);
        } else if (m_root != m_central && (m_root->orientation() == o || realCount(m_root) <= 1)) {
            const bool turned = m_root->orientation() != o;
            m_root->setOrientation(o);
            const QList<int> before = turned ? QList<int>(m_root->count(), extent / std::max(1, m_root->count()))
                                             : sizesOf(m_root);
            const int total = std::max(sum(before), newSize + 1);
            const int at = after ? m_root->count() : 0;
            m_root->insertWidget(at, area);
            QList<int> sizes = scaled(before, std::max(1, total - newSize));
            sizes.insert(at, newSize);
            if (equal)
                sizes = scaled(QList<int>(sizes.size(), 1), std::max(1, sum(sizes)));
            applySizes(m_root, sizes);
        } else {
            DockSplitter *old = m_root;
            m_layout->removeWidget(old);
            m_root = nullptr;
            auto *split = new DockSplitter(o, m_manager);
            split->addWidget(old);
            split->insertWidget(after ? 1 : 0, area);
            const int oldSize = std::max(1, extent - newSize);
            applySizes(split, after ? QList<int>{oldSize, newSize} : QList<int>{newSize, oldSize});
            setRoot(split);
        }
    }
    updateStretch();
    updateVisibility();
    Q_EMIT dockAreasChanged();
}

void DockContainer::removeAreaIfEmpty(DockArea *area)
{
    if (!area || area->dockWidgetsCount() > 0 || m_dying)
        return;
    detachArea(area);
    area->deleteLater();
}

void DockContainer::detachArea(DockArea *area)
{
    m_areas.removeAll(area);
    auto *parent = qobject_cast<DockSplitter *>(area->parentWidget());
    area->hide();
    area->setParent(nullptr);
    if (parent)
        collapse(parent);
    updateStretch();
    updateVisibility();
    Q_EMIT dockAreasChanged();
}

void DockContainer::collapse(DockSplitter *s)
{
    while (s) {
        auto *parent = qobject_cast<DockSplitter *>(s->parentWidget());
        if (s->isCentral()) {
            // 중앙 영역은 남긴다. 자식이 분할 하나뿐이면 그 분할의 자식을 끌어올린다.
            if (realCount(s) == 1) {
                if (auto *only = qobject_cast<DockSplitter *>(realChild(s, 0)); only && !only->isCentral()) {
                    const QList<int> sizes = sizesOf(only);
                    s->setOrientation(only->orientation());
                    const int at = s->indexOf(only);
                    QList<QWidget *> children;
                    for (int i = 0; i < only->count(); ++i)
                        children.append(only->widget(i));
                    for (int i = 0; i < children.size(); ++i)
                        s->insertWidget(at + i, children[i]);
                    dropSplitter(only);
                    QList<int> all = sizesOf(s);
                    for (int i = 0; i < sizes.size() && at + i < all.size(); ++i)
                        all[at + i] = sizes[i];
                    applySizes(s, all);
                }
            }
            break;
        }
        const int n = s->count();
        if (n == 0) {
            if (!parent) {
                if (s == m_root) {
                    m_layout->removeWidget(s);
                    m_root = nullptr;
                    dropSplitter(s);
                }
                break;
            }
            dropSplitter(s);
            s = parent;
            continue;
        }
        if (n == 1) {
            QWidget *child = s->widget(0);
            if (!parent) {
                if (auto *cs = qobject_cast<DockSplitter *>(child); cs && s == m_root) {
                    m_layout->removeWidget(s);
                    m_root = nullptr;
                    setRoot(cs);
                    dropSplitter(s);
                }
                break;
            }
            const int idx = parent->indexOf(s);
            const QList<int> sizes = sizesOf(parent);
            parent->insertWidget(idx, child);
            dropSplitter(s);
            if (sizes.size() == parent->count())
                applySizes(parent, sizes);
            if (auto *cs = qobject_cast<DockSplitter *>(child);
                cs && !cs->isCentral() && cs->orientation() == parent->orientation()) {
                // 같은 방향 분할을 펼친다
                const int at = parent->indexOf(cs);
                const QList<int> outer = sizesOf(parent);
                const int slot = at >= 0 && at < outer.size() ? outer[at] : 0;
                const QList<int> inner = scaled(sizesOf(cs), std::max(1, slot));
                QList<QWidget *> kids;
                for (int i = 0; i < cs->count(); ++i)
                    kids.append(cs->widget(i));
                for (int i = 0; i < kids.size(); ++i)
                    parent->insertWidget(at + i, kids[i]);
                dropSplitter(cs);
                QList<int> merged = outer;
                if (at >= 0 && at < merged.size()) {
                    merged.removeAt(at);
                    for (int i = 0; i < inner.size(); ++i)
                        merged.insert(at + i, inner[i]);
                }
                if (merged.size() == parent->count())
                    applySizes(parent, merged);
            }
            s = parent;
            continue;
        }
        break;
    }
}

void DockContainer::updateStretch()
{
    const auto splitters = findChildren<DockSplitter *>();
    for (DockSplitter *s : splitters) {
        for (int i = 0; i < s->count(); ++i)
            s->setStretchFactor(i, 0);
    }
    if (!m_central)
        return;
    QWidget *child = m_central;
    for (auto *p = qobject_cast<DockSplitter *>(m_central->parentWidget()); p;
         p = qobject_cast<DockSplitter *>(p->parentWidget())) {
        for (int i = 0; i < p->count(); ++i)
            p->setStretchFactor(i, p->widget(i) == child ? 1 : 0);
        child = p;
    }
}

void DockContainer::updateVisibility()
{
    if (m_updatingVisibility || m_dying)
        return;
    m_updatingVisibility = true;
    std::function<bool(QWidget *)> refresh = [&](QWidget *w) -> bool {
        if (auto *a = qobject_cast<DockArea *>(w))
            return a->openDockWidgetsCount() > 0;
        if (auto *s = qobject_cast<DockSplitter *>(w)) {
            bool any = false;
            for (int i = 0; i < s->count(); ++i) {
                QWidget *c = s->widget(i);
                if (isPlaceholder(c))
                    continue;
                if (refresh(c))
                    any = true;
            }
            if (s->isCentral()) {
                if (m_placeholder)
                    m_placeholder->setVisible(!any);
                any = true;
            }
            if (s != m_root && s->isHidden() == any)
                s->setVisible(any);
            return any;
        }
        return !w->isHidden();
    };
    const bool any = m_root && refresh(m_root);
    if (m_root && m_root->isHidden() == any)
        m_root->setVisible(any);
    m_updatingVisibility = false;
    if (m_floating) {
        for (DockArea *a : std::as_const(m_areas)) {
            if (a)
                a->updateTitleBar();
        }
        m_floating->containerChanged();
    }
}

void DockContainer::areaVisibilityChanged()
{
    updateVisibility();
}

void DockContainer::clearLayout()
{
    if (m_root) {
        m_layout->removeWidget(m_root);
        DockSplitter *root = m_root;
        m_root = nullptr;
        root->hide();
        root->setParent(nullptr);
        root->deleteLater();
    }
    for (DockArea *a : std::as_const(m_areas)) {
        if (a && !a->parentWidget()) {
            a->deleteLater();
        } else if (a && a->parentWidget() == this) {
            a->hide();
            a->setParent(nullptr);
            a->deleteLater();
        }
    }
    m_areas.clear();
    m_central = nullptr;
    m_placeholder = nullptr;
}

// =============================================================================================
// DockSplitter · 손잡이 · 빈 중앙 자리

namespace detail {

DockSplitter::DockSplitter(Qt::Orientation orientation, DockManager *manager, QWidget *parent)
    : QSplitter(orientation, parent)
    , m_manager(manager)
{
    setObjectName(u"fmDockSplitter"_s);
    setChildrenCollapsible(false);
    setOpaqueResize(manager && manager->testConfigFlag(DockManager::OpaqueSplitterResize));
    applyMetrics();
}

void DockSplitter::applyMetrics()
{
    setHandleWidth(metricsOf(this).splitter);
}

QSplitterHandle *DockSplitter::createHandle()
{
    return new DockSplitterHandle(orientation(), this);
}

void DockSplitter::changeEvent(QEvent *event)
{
    QSplitter::changeEvent(event);
    if (event->type() == QEvent::StyleChange)
        applyMetrics();
}

DockSplitterHandle::DockSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent)
{
    setAttribute(Qt::WA_Hover);
}

void DockSplitterHandle::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    fs::paintDockSplitterHandle(&p, rect(), m_hover, colorsOf(this));
}

bool DockSplitterHandle::event(QEvent *event)
{
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave) {
        m_hover = event->type() == QEvent::HoverEnter;
        update();
    }
    return QSplitterHandle::event(event);
}

CentralPlaceholder::CentralPlaceholder(DockManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    setObjectName(u"fmDockCentralPlaceholder"_s);
    setMinimumSize(80, 60);
}

void CentralPlaceholder::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const fs::ThemeColors &tc = colorsOf(this);
    p.fillRect(rect(), tc[fs::Token::Surface]);
    const QString text = m_manager ? m_manager->centralPlaceholderText() : QString();
    if (text.isEmpty())
        return;
    QFont f = font();
    f.setPixelSize(13);
    p.setFont(f);
    p.setPen(tc[fs::Token::Fg3]);
    p.drawText(rect().adjusted(16, 16, -16, -16), Qt::AlignCenter | Qt::TextWordWrap, text);
}

} // namespace detail

} // namespace fm::dock
