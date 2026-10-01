#pragma once

// Designer 안의 미리보기 — 플러그인이 만든 위젯에 파일 관리자 스타일과 토큰 색(ThemeScope)을 건다.
// 디자인(시안1 · 시안2)과 변형(라이트 · 다크 · 남색 · Designer 팔레트 따름)은 위젯 오른쪽 클릭 메뉴에서 바꾸고,
// 바꾸면 열려 있는 모든 미리보기 위젯에 바로 반영된다. 고른 값은 다음 실행에도 남는다(QSettings).
// 처음 값은 환경 변수 FMSTYLE_DESIGN(standard · watercolor) · FMSTYLE_VARIANT(light · dark · navy)가 정한다.

#include <fmstyle/ThemeTokens.h>

#include <QtDesigner/QDesignerTaskMenuExtension>
#include <QtDesigner/QExtensionFactory>

#include <QObject>
#include <QPointer>

#include <array>
#include <optional>

class QAction;
class QStyle;
class QWidget;

namespace fm::designer {

class PreviewController : public QObject
{
    Q_OBJECT
public:
    static PreviewController &instance();

    fm::style::Design design() const noexcept { return m_design; }
    void setDesign(fm::style::Design design);
    /// 비어 있으면 Designer 팔레트의 밝기로 라이트 · 다크를 고른다.
    std::optional<fm::style::Variant> variant() const noexcept { return m_variant; }
    void setVariant(std::optional<fm::style::Variant> variant);

    /// 플러그인이 만든 위젯을 미리보기로 등록하고 스타일 · 색을 건다. 앱이 이미 파일 관리자 스타일이면 그대로 둔다.
    QWidget *prepare(QWidget *widget);
    /// 등록된 위젯인가(작업 메뉴를 붙일 대상).
    static bool isPreviewWidget(const QObject *object);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    PreviewController();
    void apply(QWidget *widget);
    void styleTree(QWidget *widget);
    void refresh();
    void save() const;
    fm::style::Variant effectiveVariant() const;
    QStyle *style(fm::style::Design design);

    fm::style::Design m_design = fm::style::Design::Standard;
    std::optional<fm::style::Variant> m_variant;
    std::array<QStyle *, 2> m_styles{};
    QList<QPointer<QWidget>> m_roots;
};

/// 위젯 오른쪽 클릭 메뉴 — "미리보기 디자인" · "미리보기 변형".
class PreviewTaskMenu : public QObject, public QDesignerTaskMenuExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerTaskMenuExtension)

public:
    explicit PreviewTaskMenu(QObject *parent);

    QAction *preferredEditAction() const override { return nullptr; }
    QList<QAction *> taskActions() const override;

private:
    void sync() const;
    QList<QAction *> m_actions;
};

class PreviewTaskMenuFactory : public QExtensionFactory
{
    Q_OBJECT
public:
    explicit PreviewTaskMenuFactory(QExtensionManager *parent);

protected:
    QObject *createExtension(QObject *object, const QString &iid, QObject *parent) const override;
};

} // namespace fm::designer
