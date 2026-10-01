#pragma once

// Qt Widgets Designer 플러그인 — 파일 관리자 위젯을 위젯 상자의 "FmStyle — …" 묶음 다섯 개에 넣는다
// (기본 · 대화상자 · 설정 · 메인 창 · 파일 목록). 미리보기 디자인 · 변형은 DesignerPreview.h.

#include <QtUiPlugin/QDesignerCustomWidgetCollectionInterface>
#include <QtUiPlugin/QDesignerCustomWidgetInterface>

#include <QIcon>
#include <QObject>

#include <functional>

namespace fm::designer {

struct WidgetInfo
{
    QString className;    // "fm::ui::Switch" — .ui와 uic가 쓰는 이름
    QString group;        // 위젯 상자 묶음
    QString include;      // "fmwidgets/Switch.h"
    QString toolTip;
    QString whatsThis;
    QString domXml;       // 위젯 상자에서 끌어 놓을 때의 기본 속성
    QIcon icon;
    bool container = false;
    /// samples = Designer 안(또는 FMSTYLE_DESIGNER_SAMPLES)일 때 — 목록 · 그래프에 예시 데이터를 넣는다.
    /// QUiLoader로 .ui를 읽는 앱에는 넣지 않는다.
    std::function<QWidget *(QWidget *parent, bool samples)> create;
};

class WidgetPlugin : public QObject, public QDesignerCustomWidgetInterface
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    WidgetPlugin(WidgetInfo info, QObject *parent);

    QString name() const override { return m_info.className; }
    QString group() const override { return m_info.group; }
    QString toolTip() const override { return m_info.toolTip; }
    QString whatsThis() const override { return m_info.whatsThis; }
    QString includeFile() const override { return m_info.include; }
    QIcon icon() const override { return m_info.icon; }
    bool isContainer() const override { return m_info.container; }
    QWidget *createWidget(QWidget *parent) override;
    bool isInitialized() const override { return m_initialized; }
    void initialize(QDesignerFormEditorInterface *core) override;
    QString domXml() const override { return m_info.domXml; }

private:
    WidgetInfo m_info;
    bool m_initialized = false;
    bool m_designer = false;
};

/// 등록할 위젯 전체(WidgetCatalog.cpp).
QList<WidgetInfo> widgetCatalog();

class FmWidgetCollection : public QObject, public QDesignerCustomWidgetCollectionInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetCollectionInterface")
    Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)

public:
    explicit FmWidgetCollection(QObject *parent = nullptr);

    QList<QDesignerCustomWidgetInterface *> customWidgets() const override { return m_widgets; }

private:
    QList<QDesignerCustomWidgetInterface *> m_widgets;
};

} // namespace fm::designer
