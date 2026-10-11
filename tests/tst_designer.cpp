// Qt Widgets Designer 플러그인(P8) 테스트 — 플러그인 적재, 등록 정보(이름 · 묶음 · 헤더 · domXml), 모든 위젯 생성 · 파괴,
// 프로젝트의 모든 .ui를 QUiLoader로 열어 사용자 위젯이 제 클래스로 만들어지는지.
// FM_TEST_SHOTS=<폴더>를 주면 위젯 모음과 .ui마다 화면을 저장한다(Designer 미리보기와 같은 스타일 · 색).

#if FM_TEST_DESIGNER_CORE
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerWidgetDataBaseInterface>
#include <QtDesigner/qdesigner_components.h>
#endif
#include <QtUiPlugin/QDesignerCustomWidgetCollectionInterface>
#include <QtUiPlugin/QDesignerCustomWidgetInterface>
#include <QtUiTools/QUiLoader>

#include <QApplication>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QPluginLoader>
#include <QSet>
#include <QStyle>
#include <QTest>
#include <QXmlStreamReader>

using namespace Qt::StringLiterals;

namespace {

QStringList g_messages;

void collectMessages(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (type != QtDebugMsg)
        g_messages.append(message);
}

/// .ui 안의 사용자 위젯(이름 → 클래스).
QList<std::pair<QString, QString>> customWidgetsIn(const QString &file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QList<std::pair<QString, QString>> out;
    QXmlStreamReader xml(&f);
    while (!xml.atEnd()) {
        if (xml.readNextStartElement() && xml.name() == u"widget") {
            const QString cls = xml.attributes().value(u"class"_s).toString();
            if (cls.startsWith(u"fm::"))
                out.append({xml.attributes().value(u"name"_s).toString(), cls});
        }
    }
    return out;
}

bool isFileManagerStyle(const QStyle *style)
{
    return style && (style->inherits("fm::style::FmStyle") || style->inherits("fm::style::WatercolorStyle"));
}

} // namespace

class TestDesigner : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void catalog();
    void createAll();
    void uiFiles_data();
    void uiFiles();
    void layoutBoxForm();
    void promotions();  // 마지막 — 플러그인을 Designer 안처럼 초기화한다

private:
    QPluginLoader m_loader;
    QList<QDesignerCustomWidgetInterface *> m_widgets;
};

void TestDesigner::initTestCase()
{
    m_loader.setFileName(QString::fromUtf8(FM_DESIGNER_PLUGIN_PATH));
    QVERIFY2(m_loader.load(), qPrintable(m_loader.errorString()));
    auto *collection = qobject_cast<QDesignerCustomWidgetCollectionInterface *>(m_loader.instance());
    QVERIFY(collection);
    m_widgets = collection->customWidgets();
}

void TestDesigner::catalog()
{
    // 기존 6종 + .ui에서 쓰는 위젯 · §6.3 부품 · 메인 창 부품 · 파일 목록 3종
    QVERIFY2(m_widgets.size() >= 50, qPrintable(QString::number(m_widgets.size())));  // 기본 16 · 대화상자 13 · 설정 10 · 메인 창 8 · 파일 목록 3
    QSet<QString> names;
    QSet<QString> groups;
    const QStringList includeRoots = {u"fmstyle"_s, u"fmwidgets"_s, u"fmfilelist"_s, u"fmdialogs"_s, u"fmsettings"_s};
    for (QDesignerCustomWidgetInterface *w : std::as_const(m_widgets)) {
        const QString name = w->name();
        QVERIFY2(!names.contains(name), qPrintable(name));
        names.insert(name);
        groups.insert(w->group());
        QVERIFY2(w->group().startsWith(u"FmStyle — "_s), qPrintable(name));
        QVERIFY2(!w->toolTip().isEmpty() && !w->whatsThis().isEmpty(), qPrintable(name));
        QVERIFY2(!w->icon().isNull(), qPrintable(name));
        // 헤더는 uic가 그대로 #include 한다 — 프로젝트의 include 폴더에 있어야 한다
        bool found = false;
        for (const QString &root : includeRoots)
            found = found || QFileInfo::exists(u"%1/src/%2/include/%3"_s.arg(QString::fromUtf8(FM_SOURCE_DIR), root, w->includeFile()));
        QVERIFY2(found, qPrintable(name + u" → "_s + w->includeFile()));
        // 끌어 놓을 때의 domXml: 첫 widget의 class = 이름
        QXmlStreamReader xml(w->domXml());
        QString cls;
        while (!xml.atEnd() && cls.isEmpty()) {
            if (xml.readNextStartElement() && xml.name() == u"widget")
                cls = xml.attributes().value(u"class"_s).toString();
        }
        QVERIFY2(!xml.hasError(), qPrintable(name + u": "_s + xml.errorString()));
        QCOMPARE(cls, name);
    }
    QCOMPARE(groups.size(), 5);
    for (const char *container : {"fm::ui::Card", "fm::ui::SettingRow", "fm::ui::DialogFooter", "fm::ui::FlowBox", "fm::ui::FlexBox"}) {
        const auto it = std::find_if(m_widgets.cbegin(), m_widgets.cend(), [&](auto *w) { return w->name() == QLatin1String(container); });
        QVERIFY2(it != m_widgets.cend() && (*it)->isContainer(), container);
    }
}

void TestDesigner::createAll()
{
    // Designer 안처럼 예시 데이터를 넣어 만든다 — 앱이 파일 관리자 스타일이 아니므로 미리보기 스타일이 걸린다
    qputenv("FMSTYLE_DESIGNER_SAMPLES", "1");
    QWidget board;
    auto *grid = new QGridLayout(&board);
    int i = 0;
    for (QDesignerCustomWidgetInterface *w : std::as_const(m_widgets)) {
        QWidget *widget = w->createWidget(&board);
        QVERIFY2(widget, qPrintable(w->name()));
        QVERIFY2(widget->inherits(w->name().toLatin1().constData()), qPrintable(w->name()));
        QVERIFY2(isFileManagerStyle(widget->style()), qPrintable(w->name()));
        auto *caption = new QLabel(w->name().section(u"::"_s, -1), &board);
        grid->addWidget(caption, (i / 3) * 2, i % 3);
        grid->addWidget(widget, (i / 3) * 2 + 1, i % 3);
        ++i;
    }
    board.resize(1800, 2400);
    board.show();
    QVERIFY(QTest::qWaitForWindowExposed(&board));
    QTest::qWait(100);
    if (const QString dir = qEnvironmentVariable("FM_TEST_SHOTS"); !dir.isEmpty())
        board.grab().save(dir + u"/designer-widgets.png"_s);
    qunsetenv("FMSTYLE_DESIGNER_SAMPLES");
}

void TestDesigner::uiFiles_data()
{
    QTest::addColumn<QString>("file");
    const QString root = QString::fromUtf8(FM_SOURCE_DIR);
    int count = 0;
    for (const QString &dir : {u"src"_s, u"examples"_s}) {
        QDirIterator it(root + u'/' + dir, {u"*.ui"_s}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString file = it.next();
            QTest::newRow(qPrintable(QFileInfo(file).fileName())) << file;
            ++count;
        }
    }
    QVERIFY(count >= 17);
}

void TestDesigner::uiFiles()
{
    QFETCH(QString, file);
    QUiLoader loader;
    loader.clearPluginPaths();
    loader.addPluginPath(QFileInfo(QString::fromUtf8(FM_DESIGNER_PLUGIN_PATH)).absolutePath());
    QVERIFY(loader.availableWidgets().contains(u"fm::ui::SettingRow"_s));

    QFile f(file);
    QVERIFY(f.open(QIODevice::ReadOnly));
    g_messages.clear();
    const QtMessageHandler previous = qInstallMessageHandler(collectMessages);
    std::unique_ptr<QWidget> form(loader.load(&f));
    qInstallMessageHandler(previous);
    QVERIFY2(form, qPrintable(loader.errorString()));
    // 속성 · 클래스를 모르면 QFormBuilder가 경고한다(예: 위젯 속성 이름이 바뀌었는데 .ui가 그대로일 때)
    for (const QString &m : std::as_const(g_messages))
        QVERIFY2(!m.contains(u"property"_s, Qt::CaseInsensitive) && !m.contains(u"unknown"_s, Qt::CaseInsensitive), qPrintable(m));

    // 승격 전용(위젯 상자에 없는) 메뉴 · 메뉴 막대는 QUiLoader가 기반 클래스로 만든다 — uic는 fm::ui 클래스로
    const QHash<QString, QString> promotionOnly = {{u"fm::ui::MenuBar"_s, u"QMenuBar"_s}, {u"fm::ui::Menu"_s, u"QMenu"_s}};
    for (const auto &[name, cls] : customWidgetsIn(file)) {
        QWidget *w = form->objectName() == name ? form.get() : form->findChild<QWidget *>(name);
        QVERIFY2(w, qPrintable(name));
        const QString expected = promotionOnly.value(cls, cls);
        QVERIFY2(w->inherits(expected.toLatin1().constData()), qPrintable(name + u" : "_s + cls + u" ≠ "_s + QString::fromLatin1(w->metaObject()->className())));
    }
    if (const QString dir = qEnvironmentVariable("FM_TEST_SHOTS"); !dir.isEmpty()) {
        form->show();
        QVERIFY(QTest::qWaitForWindowExposed(form.get()));
        form->grab().save(dir + u"/ui-"_s + QFileInfo(file).completeBaseName() + u".png"_s);
    }
}

void TestDesigner::layoutBoxForm()
{
    // QUiLoader: 배치 상자가 .ui의 자식을 스스로 배치 — itemOrder(문서가 맨 앞) · 자식 동적 속성 flexGrow(찾기 칸이 남는 폭)
    QUiLoader loader;
    loader.clearPluginPaths();
    loader.addPluginPath(QFileInfo(QString::fromUtf8(FM_DESIGNER_PLUGIN_PATH)).absolutePath());
    QFile f(QString::fromUtf8(FM_SOURCE_DIR) + u"/examples/designer/LayoutDemo.ui"_s);
    QVERIFY(f.open(QIODevice::ReadOnly));
    std::unique_ptr<QWidget> form(loader.load(&f));
    QVERIFY2(form, qPrintable(loader.errorString()));
    form->resize(560, 420);
    form->show();
    QVERIFY(QTest::qWaitForWindowExposed(form.get()));
    QWidget *tags = form->findChild<QWidget *>(u"tags"_s);
    QVERIFY(tags && tags->inherits("fm::ui::FlowBox"));
    const QStringList order = tags->property("itemOrder").toStringList();
    QCOMPARE(order.mid(0, 3), (QStringList{u"docsChip"_s, u"imagesChip"_s, u"codeChip"_s}));
    QCOMPARE(order.size(), 5);
    auto *docs = form->findChild<QWidget *>(u"docsChip"_s);
    auto *images = form->findChild<QWidget *>(u"imagesChip"_s);
    QVERIFY(docs->x() < images->x());
    QWidget *find = form->findChild<QWidget *>(u"findEdit"_s);
    QWidget *chip = form->findChild<QWidget *>(u"targetChip"_s);
    QVERIFY2(find->width() > 2 * chip->width(), qPrintable(u"%1 · %2"_s.arg(find->width()).arg(chip->width())));
}

void TestDesigner::promotions()
{
#if !FM_TEST_DESIGNER_CORE
    QSKIP("Qt6::DesignerComponentsPrivate 없음");
#else
    // 실제 Designer 코어 — 플러그인 초기화가 메뉴 · 메뉴 막대를 승격 대상으로 등록하고, 배치 상자는 디자인 모드로 만든다
    QDesignerComponents::initializeResources();
    QObject owner;
    QDesignerFormEditorInterface *core = QDesignerComponents::createFormEditor(&owner);
    QVERIFY(core);
    for (QDesignerCustomWidgetInterface *w : std::as_const(m_widgets))
        w->initialize(core);
    QDesignerWidgetDataBaseInterface *db = core->widgetDataBase();
    QVERIFY(db);
    for (const auto &[cls, base] : {std::pair{u"fm::ui::MenuBar"_s, u"QMenuBar"_s}, std::pair{u"fm::ui::Menu"_s, u"QMenu"_s}}) {
        const int index = db->indexOfClassName(cls);
        QVERIFY2(index >= 0, qPrintable(cls));
        QDesignerWidgetDataBaseItemInterface *item = db->item(index);
        QVERIFY(item->isPromoted());
        QCOMPARE(item->extends(), base);
        QCOMPARE(item->includeFile(), u"fmwidgets/Menu.h"_s);
    }
    const auto flow = std::find_if(m_widgets.cbegin(), m_widgets.cend(), [](auto *w) { return w->name() == u"fm::ui::FlowBox"; });
    QVERIFY(flow != m_widgets.cend());
    std::unique_ptr<QWidget> box((*flow)->createWidget(nullptr));
    QVERIFY(box->property("designMode").toBool());
#endif
}

QTEST_MAIN(TestDesigner)
#include "tst_designer.moc"
