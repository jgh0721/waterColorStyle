#include "FmDesignerPlugin.h"

#include "DesignerPreview.h"

#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QExtensionManager>

#include <QVariant>

using namespace Qt::StringLiterals;

namespace fm::designer {

WidgetPlugin::WidgetPlugin(WidgetInfo info, QObject *parent)
    : QObject(parent)
    , m_info(std::move(info))
{
}

QWidget *WidgetPlugin::createWidget(QWidget *parent)
{
    const bool samples = m_designer || qEnvironmentVariableIsSet("FMSTYLE_DESIGNER_SAMPLES");
    return PreviewController::instance().prepare(m_info.create(parent, samples));
}

void WidgetPlugin::initialize(QDesignerFormEditorInterface *core)
{
    if (m_initialized)
        return;
    m_initialized = true;
    // QUiLoader는 core 없이 부른다 — Designer 안에서만 예시 데이터와 오른쪽 클릭 메뉴를 붙인다.
    m_designer = core != nullptr;
    if (!core)
        return;
    QExtensionManager *manager = core->extensionManager();
    if (manager && !manager->property("_fm_preview_task_menu").toBool()) {
        manager->registerExtensions(new PreviewTaskMenuFactory(manager), Q_TYPEID(QDesignerTaskMenuExtension));
        manager->setProperty("_fm_preview_task_menu", true);
    }
}

FmWidgetCollection::FmWidgetCollection(QObject *parent)
    : QObject(parent)
{
    const QList<WidgetInfo> catalog = widgetCatalog();
    for (const WidgetInfo &info : catalog)
        m_widgets.append(new WidgetPlugin(info, this));
}

} // namespace fm::designer
