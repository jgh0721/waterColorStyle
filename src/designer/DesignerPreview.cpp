#include "DesignerPreview.h"

#include <fmstyle/ThemeManager.h>

#include <QtDesigner/QExtensionManager>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QChildEvent>
#include <QProxyStyle>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#include <QWidget>

using namespace Qt::StringLiterals;

namespace fm::designer {

namespace fs = fm::style;

namespace {

constexpr char kPreviewProperty[] = "_fm_designer_preview";

/// 앱이 이미 파일 관리자 스타일을 쓰는가(예: 앱에서 QUiLoader로 .ui를 읽을 때). Qtitan 그리드가 앱 스타일을
/// CommonStyle로 감쌀 수 있어 프록시 사슬을 따라간다. 플러그인은 fmstyle을 따로 정적 링크하므로 클래스 이름으로 본다.
bool isFileManagerStyle(const QStyle *style)
{
    while (style) {
        if (style->inherits("fm::style::FmStyle") || style->inherits("fm::style::WatercolorStyle"))
            return true;
        const auto *proxy = qobject_cast<const QProxyStyle *>(style);
        style = proxy ? proxy->baseStyle() : nullptr;
    }
    return false;
}

QString variantKey(std::optional<fs::Variant> v)
{
    if (!v)
        return u"auto"_s;
    switch (*v) {
    case fs::Variant::Light: return u"light"_s;
    case fs::Variant::Dark: return u"dark"_s;
    case fs::Variant::Navy: return u"navy"_s;
    }
    return u"auto"_s;
}

std::optional<fs::Variant> variantFromKey(const QString &key)
{
    if (key == u"light")
        return fs::Variant::Light;
    if (key == u"dark")
        return fs::Variant::Dark;
    if (key == u"navy")
        return fs::Variant::Navy;
    return std::nullopt;
}

QSettings settings()
{
    return QSettings(u"FM Tools"_s, u"DesignerPlugin"_s);
}

} // namespace

// ---------------------------------------------------------------------------------------------
// PreviewController

PreviewController &PreviewController::instance()
{
    // Designer가 끝날 때까지 쓴다 — 미리보기 위젯이 스타일을 참조하므로 일부러 지우지 않는다.
    static auto *controller = new PreviewController;
    return *controller;
}

PreviewController::PreviewController()
{
    // 환경 변수가 있으면 이번 실행은 그 값, 없으면 지난번에 메뉴에서 고른 값
    const QString design = qEnvironmentVariable("FMSTYLE_DESIGN").trimmed().toLower();
    const QString saved = settings().value(u"design"_s).toString();
    const QString d = design.isEmpty() ? saved : design;
    m_design = (d == u"watercolor" || d == u"2" || d == u"시안2") ? fs::Design::Watercolor : fs::Design::Standard;
    const QString variant = qEnvironmentVariable("FMSTYLE_VARIANT").trimmed().toLower();
    m_variant = variantFromKey(variant.isEmpty() ? settings().value(u"variant"_s).toString() : variant);
}

void PreviewController::setDesign(fs::Design design)
{
    if (m_design == design)
        return;
    m_design = design;
    save();
    refresh();
}

void PreviewController::setVariant(std::optional<fs::Variant> variant)
{
    if (m_variant == variant)
        return;
    m_variant = variant;
    save();
    refresh();
}

void PreviewController::save() const
{
    QSettings s = settings();
    s.setValue(u"design"_s, m_design == fs::Design::Watercolor ? u"watercolor"_s : u"standard"_s);
    s.setValue(u"variant"_s, variantKey(m_variant));
}

fs::Variant PreviewController::effectiveVariant() const
{
    if (m_variant)
        return *m_variant;
    const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
    return dark ? fs::Variant::Dark : fs::Variant::Light;
}

QStyle *PreviewController::style(fs::Design design)
{
    QStyle *&s = m_styles[design == fs::Design::Watercolor ? 1 : 0];
    if (!s)
        s = fs::createStyle(design);
    return s;
}

QWidget *PreviewController::prepare(QWidget *widget)
{
    if (!widget || isFileManagerStyle(QApplication::style()))
        return widget;
    widget->setProperty(kPreviewProperty, true);
    widget->installEventFilter(this);
    m_roots.removeAll(nullptr);
    m_roots.append(widget);
    apply(widget);
    return widget;
}

bool PreviewController::isPreviewWidget(const QObject *object)
{
    return object && object->property(kPreviewProperty).toBool();
}

void PreviewController::apply(QWidget *widget)
{
    // 팔레트는 건드리지 않는다(applyPalette = false) — 팔레트를 바꾸면 .ui에 저장될 수 있다.
    fs::ThemeScope::set(widget, fs::ThemeManager::instance().colors(m_design, effectiveVariant()), false);
    styleTree(widget);
}

void PreviewController::styleTree(QWidget *widget)
{
    QStyle *s = style(m_design);
    widget->setStyle(s);
    const auto children = widget->findChildren<QWidget *>();
    for (QWidget *c : children)
        c->setStyle(s);
}

void PreviewController::refresh()
{
    m_roots.removeAll(nullptr);
    for (const QPointer<QWidget> &w : std::as_const(m_roots))
        apply(w);
}

bool PreviewController::eventFilter(QObject *watched, QEvent *event)
{
    // 컨테이너(카드 · 설정 행 · 대화상자 바닥)에 Designer로 넣은 위젯도 같은 스타일로 보이게 — 다 만들어진 뒤에 건다
    if (event->type() == QEvent::ChildAdded && isPreviewWidget(watched)) {
        if (auto *child = qobject_cast<QWidget *>(static_cast<QChildEvent *>(event)->child())) {
            QPointer<QWidget> guard(child);
            QTimer::singleShot(0, child, [this, guard] {
                if (guard)
                    styleTree(guard);
            });
        }
    }
    return QObject::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------------------------
// PreviewTaskMenu

PreviewTaskMenu::PreviewTaskMenu(QObject *parent)
    : QObject(parent)
{
    auto &controller = PreviewController::instance();
    auto *designs = new QActionGroup(this);
    for (const auto &[text, design] : {std::pair{u"미리보기 디자인 — 시안1 (기본)"_s, fs::Design::Standard},
                                       std::pair{u"미리보기 디자인 — 시안2 (워터컬러)"_s, fs::Design::Watercolor}}) {
        QAction *a = designs->addAction(text);
        a->setCheckable(true);
        a->setData(int(design));
        connect(a, &QAction::triggered, this, [&controller, design] { controller.setDesign(design); });
        m_actions.append(a);
    }
    auto *separator = new QAction(this);
    separator->setSeparator(true);
    m_actions.append(separator);
    auto *variants = new QActionGroup(this);
    const std::pair<QString, std::optional<fs::Variant>> variantItems[] = {
        {u"미리보기 변형 — Designer 밝기 따라가기"_s, std::nullopt},
        {u"미리보기 변형 — 라이트"_s, fs::Variant::Light},
        {u"미리보기 변형 — 다크"_s, fs::Variant::Dark},
        {u"미리보기 변형 — 다크(남색, 시안2)"_s, fs::Variant::Navy},
    };
    for (const auto &[text, variant] : variantItems) {
        QAction *a = variants->addAction(text);
        a->setCheckable(true);
        a->setData(variantKey(variant));
        connect(a, &QAction::triggered, this, [&controller, variant] { controller.setVariant(variant); });
        m_actions.append(a);
    }
}

void PreviewTaskMenu::sync() const
{
    const auto &controller = PreviewController::instance();
    for (QAction *a : m_actions) {
        if (a->isSeparator())
            continue;
        if (a->data().typeId() == QMetaType::Int)
            a->setChecked(a->data().toInt() == int(controller.design()));
        else
            a->setChecked(a->data().toString() == variantKey(controller.variant()));
    }
}

QList<QAction *> PreviewTaskMenu::taskActions() const
{
    sync();
    return m_actions;
}

// ---------------------------------------------------------------------------------------------
// PreviewTaskMenuFactory

PreviewTaskMenuFactory::PreviewTaskMenuFactory(QExtensionManager *parent)
    : QExtensionFactory(parent)
{
}

QObject *PreviewTaskMenuFactory::createExtension(QObject *object, const QString &iid, QObject *parent) const
{
    if (iid != Q_TYPEID(QDesignerTaskMenuExtension) || !PreviewController::isPreviewWidget(object))
        return nullptr;
    return new PreviewTaskMenu(parent);
}

} // namespace fm::designer
