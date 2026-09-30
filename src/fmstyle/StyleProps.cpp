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
    return ButtonRole::Normal;
}

void setSmall(QWidget *widget, bool enabled)
{
    setAndRefresh(widget, props::kSize, enabled ? QVariant(u"small"_s) : QVariant(), true);
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
