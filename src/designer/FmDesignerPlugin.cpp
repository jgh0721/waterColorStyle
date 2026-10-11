#include "FmDesignerPlugin.h"

#include "DesignerPreview.h"

#include <fmwidgets/LayoutBoxes.h>

#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerFormWindowCursorInterface>
#include <QtDesigner/QDesignerFormWindowInterface>
#include <QtDesigner/QDesignerWidgetDataBaseInterface>
#include <QtDesigner/QExtensionManager>

#if FM_DESIGNER_PROMOTIONS
#include <QtDesigner/private/widgetdatabase_p.h>
#endif

#include <QVariant>

using namespace Qt::StringLiterals;

namespace fm::designer {

namespace {

/// 승격(Promote to …)으로 고를 수 있게 미리 등록하는 클래스 — 위젯 상자에 놓는 위젯이 아니라 Designer가 따로 다루는
/// 메뉴 · 메뉴 막대. 메뉴 막대 · 메뉴를 오른쪽 클릭 → 승격 대상에 바로 보인다(헤더 · 기반 클래스를 손으로 적지 않는다).
struct Promotion
{
    const char *className;
    const char *baseClass;
    const char *include;
};
constexpr Promotion kPromotions[] = {
    {"fm::ui::MenuBar", "QMenuBar", "fmwidgets/Menu.h"},
    {"fm::ui::Menu", "QMenu", "fmwidgets/Menu.h"},
};

void registerPromotions(QDesignerFormEditorInterface *core)
{
#if FM_DESIGNER_PROMOTIONS
    QDesignerWidgetDataBaseInterface *db = core->widgetDataBase();
    if (!db)
        return;
    for (const Promotion &p : kPromotions) {
        const QString name = QString::fromLatin1(p.className);
        if (db->indexOfClassName(name) >= 0)
            continue;
        qdesigner_internal::appendDerived(db, name, u"FmStyle — 승격"_s, QString::fromLatin1(p.baseClass),
                                          QString::fromLatin1(p.include), true, true);
    }
#else
    Q_UNUSED(core)
#endif
}

} // namespace

WidgetPlugin::WidgetPlugin(WidgetInfo info, QObject *parent)
    : QObject(parent)
    , m_info(std::move(info))
{
}

QWidget *WidgetPlugin::createWidget(QWidget *parent)
{
    const bool samples = m_designer || qEnvironmentVariableIsSet("FMSTYLE_DESIGNER_SAMPLES");
    QWidget *widget = m_info.create(parent, samples);
    if (auto *box = qobject_cast<fm::ui::LayoutBox *>(widget); box && m_designer) {
        // 배치 상자: 자식을 끌어 옮기면 차례를 바꾸고, 그 차례를 폼 커서로 적어 .ui에 남긴다(되돌리기 가능)
        box->setDesignMode(true);
        QObject::connect(box, &fm::ui::LayoutBox::itemOrderChanged, box, [box](const QStringList &order) {
            if (QDesignerFormWindowInterface *form = QDesignerFormWindowInterface::findFormWindow(box))
                form->cursor()->setWidgetProperty(box, u"itemOrder"_s, order);
        });
    }
    return PreviewController::instance().prepare(widget);
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
        registerPromotions(core);
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
