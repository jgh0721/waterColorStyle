#include "fmstyle/StyleProps.h"

#include <QAbstractButton>
#include <QVariant>
#include <QWidget>

using namespace Qt::StringLiterals;

namespace fm::style {

namespace {

void setAndRefresh(QWidget *w, const char *name, const QVariant &value, bool geometry)
{
    if (!w)
        return;
    w->setProperty(name, value);
    if (geometry)
        w->updateGeometry();
    w->update();
}

} // namespace

void setButtonRole(QWidget *button, ButtonRole role)
{
    QString value;
    switch (role) {
    case ButtonRole::Primary: value = u"primary"_s; break;
    case ButtonRole::Danger: value = u"danger"_s; break;
    case ButtonRole::Subtle: value = u"subtle"_s; break;
    case ButtonRole::Link: value = u"link"_s; break;
    case ButtonRole::Normal: break;
    }
    setAndRefresh(button, props::kRole, value.isEmpty() ? QVariant() : QVariant(value), false);
}

ButtonRole buttonRole(const QWidget *button)
{
    if (!button)
        return ButtonRole::Normal;
    const QString role = button->property(props::kRole).toString();
    if (role == u"primary")
        return ButtonRole::Primary;
    if (role == u"danger")
        return ButtonRole::Danger;
    if (role == u"subtle")
        return ButtonRole::Subtle;
    if (role == u"link")
        return ButtonRole::Link;
    return ButtonRole::Normal;
}

void setSmall(QWidget *widget, bool enabled)
{
    setAndRefresh(widget, props::kSize, enabled ? QVariant(u"small"_s) : QVariant(), true);
}

void setSizeVariant(QWidget *widget, SizeVariant variant)
{
    QString value;
    switch (variant) {
    case SizeVariant::Small: value = u"small"_s; break;
    case SizeVariant::Mini: value = u"mini"_s; break;
    case SizeVariant::Thin: value = u"thin"_s; break;
    case SizeVariant::Thick: value = u"thick"_s; break;
    case SizeVariant::Normal: break;
    }
    setAndRefresh(widget, props::kSize, value.isEmpty() ? QVariant() : QVariant(value), true);
}

SizeVariant sizeVariant(const QWidget *widget)
{
    const QString v = widget ? widget->property(props::kSize).toString() : QString();
    if (v == u"small")
        return SizeVariant::Small;
    if (v == u"mini")
        return SizeVariant::Mini;
    if (v == u"thin")
        return SizeVariant::Thin;
    if (v == u"thick")
        return SizeVariant::Thick;
    return SizeVariant::Normal;
}

void setDensity(QWidget *root, Density value)
{
    if (!root)
        return;
    const QVariant v = value == Density::Dialog   ? QVariant(u"dialog"_s)
                     : value == Density::Settings ? QVariant(u"settings"_s)
                                                  : QVariant();
    root->setProperty(props::kDensity, v);
    // 크기가 바뀌므로 안의 위젯이 크기 힌트를 다시 계산하게 한다.
    const auto children = root->findChildren<QWidget *>();
    for (QWidget *c : children)
        c->updateGeometry();
    root->updateGeometry();
    root->update();
}

Density density(const QWidget *widget)
{
    for (const QWidget *w = widget; w; w = w->parentWidget()) {
        const QVariant v = w->property(props::kDensity);
        if (v.isValid()) {
            const QString s = v.toString();
            return s == u"dialog" ? Density::Dialog : s == u"settings" ? Density::Settings : Density::Normal;
        }
        if (w->isWindow())
            break;
    }
    return Density::Normal;
}

void setFooter(QWidget *frame, bool on)
{
    setAndRefresh(frame, props::kFooter, on ? QVariant(true) : QVariant(), false);
}

void setKeyHint(QWidget *widget, const QString &keys)
{
    setAndRefresh(widget, props::kKeyHint, keys.isEmpty() ? QVariant() : QVariant(keys), true);
}

void setFlatHeader(QWidget *viewOrHeader, bool on)
{
    setAndRefresh(viewOrHeader, props::kHeader, on ? QVariant(u"flat"_s) : QVariant(), true);
}

void setInvalid(QWidget *widget, bool invalid)
{
    setAndRefresh(widget, props::kInvalid, invalid ? QVariant(true) : QVariant(), false);
}

void setSwitch(QWidget *checkBox, bool on)
{
    setAndRefresh(checkBox, props::kSwitch, on ? QVariant(true) : QVariant(), true);
}

void setSegments(const QList<QAbstractButton *> &buttons)
{
    const qsizetype n = buttons.size();
    for (qsizetype i = 0; i < n; ++i) {
        QAbstractButton *b = buttons.at(i);
        const QString pos = n == 1 ? u"only"_s
                          : i == 0 ? u"first"_s
                          : i == n - 1 ? u"last"_s
                                       : u"middle"_s;
        b->setCheckable(true);
        setAndRefresh(b, props::kSegment, pos, true);
    }
}

void setCard(QWidget *frame, bool on)
{
    setAndRefresh(frame, props::kCard, on ? QVariant(true) : QVariant(), false);
}

void setPaneActive(QWidget *widget, bool active)
{
    setAndRefresh(widget, props::kPaneActive, active, false);
}

void setProgressState(QWidget *progressBar, const QString &state)
{
    setAndRefresh(progressBar, props::kProgressState, state.isEmpty() ? QVariant() : QVariant(state),
                  false);
}

void setPreviewState(QWidget *widget, const QString &states)
{
    setAndRefresh(widget, props::kPreviewState, states.isEmpty() ? QVariant() : QVariant(states),
                  false);
}

} // namespace fm::style
