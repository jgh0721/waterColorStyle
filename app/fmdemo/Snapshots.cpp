#include "Snapshots.h"

#include "MainWindow.h"

#include <fmdialogs/DialogCatalog.h>
#include <fmdialogs/ProgressDialog.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/ThemeManager.h>
#include <fmstyle/WatercolorChrome.h>
#include <fmstyle/WatercolorStyle.h>

#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QStyleOptionTitleBar>
#include <QTimer>

#include <cstdio>
#include <memory>

using namespace Qt::StringLiterals;

namespace fm::app {

namespace {

namespace fs = fm::style;
using TM = fs::ThemeManager;

struct Theme
{
    const char *id;
    const char *label;
    fs::Design design;
    TM::Scheme scheme;
    TM::DarkTone tone;
};

constexpr Theme kThemes[] = {
    {"std-light", "시안1 라이트", fs::Design::Standard, TM::Scheme::Light, TM::DarkTone::Gray},
    {"std-dark", "시안1 다크", fs::Design::Standard, TM::Scheme::Dark, TM::DarkTone::Gray},
    {"wc-light", "시안2 라이트", fs::Design::Watercolor, TM::Scheme::Light, TM::DarkTone::Gray},
    {"wc-dark", "시안2 다크", fs::Design::Watercolor, TM::Scheme::Dark, TM::DarkTone::Gray},
    {"wc-navy", "시안2 남색", fs::Design::Watercolor, TM::Scheme::Dark, TM::DarkTone::Navy},
};

struct Screen
{
    QString id;
    QString group;
    QString label;
    bool fromMockup = true;
    QSize mockup;
    QSize actual;
};

void settle(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

/// 화면 배율과 상관없이 논리 크기 그대로(1배율) — 목업 보드의 CSS 픽셀과 맞춰 비교한다.
QImage renderClient(QWidget *w)
{
    QImage image(w->size(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(1.0);
    image.fill(Qt::transparent);
    w->render(&image);
    return image;
}

QWidget *createScreen(const QString &id)
{
    if (id == u"main" || id == u"main.docks") {
        auto *w = new MainWindow;
        w->loadBoardState();
        w->resize(1440, 900);
        if (id == u"main.docks") {
            // 도크 넷(07 §6) + 작업 대기열 두 줄 — 일시 정지한 복사(목업 정적 값) · 그 뒤에 대기열에 넣은 이동
            w->openAllDocks();
            const fm::dialogs::ProgressDialog::Operation board = fm::dialogs::ProgressDialog::boardCopy();
            fm::dialogs::ProgressDialog *copy = w->startJob(fm::dialogs::ProgressDialog::Copy, board.source, board.target,
                                                            board.fileNames, board.fileSizes);
            copy->hide();
            copy->applyVariant(u"progress.copy.detail.paused"_s);
            fm::dialogs::ProgressDialog *move = w->startJob(fm::dialogs::ProgressDialog::Move, u"D:\\Work\\fm-core\\build"_s,
                                                            u"E:\\Archive\\build-2026-09"_s, {u"build"_s}, {qint64(1'900'000'000)},
                                                            {}, true);
            move->hide();
        }
        return w;
    }
    return fm::dialogs::createDialog(id);
}

bool selected(const QString &id, const QStringList &only)
{
    if (only.isEmpty())
        return true;
    return std::any_of(only.begin(), only.end(), [&](const QString &prefix) { return id.startsWith(prefix); });
}

void drawCaptionGlyph(QPainter &p, const QRectF &button, int kind, const QColor &color)
{
    // 0 최소화 · 1 최대화 · 2 닫기 — 10 × 10, 선 1 px(Win11)
    const QPointF c = button.center();
    p.save();
    p.setRenderHint(QPainter::Antialiasing, kind == 2);
    p.setPen(QPen(color, 1.0));
    p.setBrush(Qt::NoBrush);
    switch (kind) {
    case 0: p.drawLine(QPointF(c.x() - 5, c.y()), QPointF(c.x() + 5, c.y())); break;
    case 1: p.drawRect(QRectF(c.x() - 5, c.y() - 5, 10, 10)); break;
    default:
        p.drawLine(QPointF(c.x() - 5, c.y() - 5), QPointF(c.x() + 5, c.y() + 5));
        p.drawLine(QPointF(c.x() + 5, c.y() - 5), QPointF(c.x() - 5, c.y() + 5));
        break;
    }
    p.restore();
}

QString html(const QString &text)
{
    return text.toHtmlEscaped();
}

bool writeIndex(const QString &dir, const QList<Screen> &screens, const QList<const Theme *> &themes)
{
    QString out;
    out += uR"(<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>fmdemo 스냅숏</title>
<style>
:root { color-scheme: light dark; --bg:#f4f5f7; --fg:#16181c; --fg2:#5d636d; --line:#d5d9df; --card:#fff; }
@media (prefers-color-scheme: dark) { :root { --bg:#16181b; --fg:#e6e8eb; --fg2:#9aa0a8; --line:#34383f; --card:#1f2125; } }
body { margin:0; padding:16px 24px; background:var(--bg); color:var(--fg); font:13px/1.45 "Segoe UI Variable Text","Segoe UI","Malgun Gothic",sans-serif; }
h1 { font-size:20px; margin:0 0 4px; } p.meta { color:var(--fg2); margin:0 0 12px; }
.bar { position:sticky; top:0; background:var(--bg); padding:8px 0; z-index:1; display:flex; gap:16px; align-items:center; border-bottom:1px solid var(--line); }
table { border-collapse:collapse; margin-top:8px; }
th, td { border-bottom:1px solid var(--line); padding:8px; vertical-align:top; text-align:left; }
th { position:sticky; top:41px; background:var(--bg); font-weight:600; }
tr.group td { font-weight:600; font-size:14px; padding-top:20px; }
td.info { min-width:220px; max-width:260px; } code { font:12px "Cascadia Mono",Consolas,monospace; color:var(--fg2); }
.tag { display:inline-block; padding:0 6px; border-radius:9px; font-size:11px; background:var(--line); }
img { width:var(--w,260px); height:auto; display:block; background:var(--card); }
.diff { color:#c0392b; }
</style></head><body>
<h1>fmdemo 스냅숏</h1>
)"_s;
    out += u"<p class=\"meta\">%1 · 화면 %2개 × 테마 %3 · 1배율 렌더. 그림을 누르면 원본 크기.</p>\n"_s
               .arg(QDateTime::currentDateTime().toString(u"yyyy-MM-dd HH:mm"_s))
               .arg(screens.size())
               .arg(themes.size());
    out += uR"(<div class="bar"><label><input type="checkbox" id="framed" checked> 목업식 제목 표시줄 틀</label>
<label>미리보기 폭 <input type="range" id="width" min="160" max="720" value="260"></label></div>
<table><thead><tr><th>화면</th>)"_s;
    for (const Theme *t : themes)
        out += u"<th>%1</th>"_s.arg(html(QString::fromUtf8(t->label)));
    out += u"</tr></thead><tbody>\n"_s;
    QString group;
    for (const Screen &s : screens) {
        if (s.group != group) {
            group = s.group;
            out += u"<tr class=\"group\"><td colspan=\"%1\">%2</td></tr>\n"_s.arg(themes.size() + 1).arg(html(group));
        }
        QString size = u"구현 %1 × %2"_s.arg(s.actual.width()).arg(s.actual.height());
        if (s.mockup.width() > 0) {
            const bool same = s.mockup.height() > 0 ? s.mockup == s.actual : s.mockup.width() == s.actual.width();
            const QString mock = s.mockup.height() > 0 ? u"%1 × %2"_s.arg(s.mockup.width()).arg(s.mockup.height())
                                                       : u"폭 %1"_s.arg(s.mockup.width());
            size += same ? u" · 목업 "_s + mock : u" · <span class=\"diff\">목업 "_s + mock + u"</span>"_s;
        }
        out += u"<tr><td class=\"info\"><b>%1</b><br><code>%2</code><br><span class=\"tag\">%3</span> %4</td>"_s.arg(
            html(s.label), html(s.id), s.fromMockup ? u"목업"_s : u"제안"_s, size);
        for (const Theme *t : themes) {
            const QString plain = u"%1/%2.png"_s.arg(QString::fromLatin1(t->id), s.id);
            const QString framed = u"%1/framed/%2.png"_s.arg(QString::fromLatin1(t->id), s.id);
            out += u"<td><a href=\"%1\" data-plain=\"%1\" data-framed=\"%2\"><img loading=\"lazy\" src=\"%2\" data-plain=\"%1\" data-framed=\"%2\" alt=\"%3\"></a></td>"_s
                       .arg(plain, framed, html(s.id));
        }
        out += u"</tr>\n"_s;
    }
    out += uR"(</tbody></table>
<script>
const framed = document.getElementById('framed');
framed.addEventListener('change', () => {
  for (const el of document.querySelectorAll('[data-framed]')) {
    const src = framed.checked ? el.dataset.framed : el.dataset.plain;
    if (el.tagName === 'IMG') el.src = src; else el.href = src;
  }
});
document.getElementById('width').addEventListener('input', e => document.body.style.setProperty('--w', e.target.value + 'px'));
</script></body></html>
)"_s;
    QFile file(dir + u"/index.html"_s);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(out.toUtf8()) > 0;
}

} // namespace

QImage mockFrame(const QImage &client, const QString &title, const QIcon &icon, bool mainWindow)
{
    const TM &tm = TM::instance();
    const fs::ThemeColors &tc = tm.colors();
    if (tm.design() == fs::Design::Watercolor) {
        // 27 px 그라데이션 제목 + 3 px 틀(06 §4.3). 제목은 WatercolorStyle의 CC_TitleBar로 그린다(MDI와 같은 그림).
        static QStyle *style = fs::createStyle(fs::Design::Watercolor);
        const fs::WatercolorChrome &x = fs::WatercolorStyle::chromeFor(nullptr);
        constexpr int frame = 3, titleH = 27;
        QImage image(client.width() + 2 * frame, client.height() + titleH + frame, QImage::Format_ARGB32_Premultiplied);
        image.fill(x.frame);
        QPainter p(&image);
        p.setPen(x.frameOuter);
        p.drawRect(image.rect().adjusted(0, 0, -1, -1));
        QStyleOptionTitleBar opt;
        opt.rect = QRect(0, 0, image.width(), titleH);
        opt.text = title;
        opt.icon = icon;
        opt.titleBarFlags = Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                          | (mainWindow ? Qt::WindowMinMaxButtonsHint : Qt::WindowFlags());
        opt.titleBarState = 0;
        opt.state = QStyle::State_Active | QStyle::State_Enabled;
        opt.subControls = QStyle::SC_All;
        opt.activeSubControls = QStyle::SC_None;
        opt.palette = tc.toPalette();
        opt.fontMetrics = QFontMetrics(QApplication::font());
        style->drawComplexControl(QStyle::CC_TitleBar, &opt, &p, nullptr);
        p.drawImage(frame, titleH, client);
        return image;
    }
    // 시안1: --win 제목(대화상자 36 · 메인 32), 아이콘 16 · 간격 10 · 12 px 제목, 캡션 46 폭, 모서리 8, 1 px --line
    const int titleH = mainWindow ? 32 : 36;
    QImage image(client.width() + 2, client.height() + titleH + 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(image.rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    p.fillPath(shape, tc[fs::Token::Win]);
    p.save();
    p.setClipPath(shape);
    p.drawImage(1, titleH + 1, client);
    const int left = mainWindow ? 12 : 14;
    icon.paint(&p, QRect(1 + left, 1 + (titleH - 16) / 2, 16, 16));
    p.setFont(fs::pixelFont(QApplication::font(), 12));
    p.setPen(tc[fs::Token::Fg]);
    const int textLeft = 1 + left + 16 + 10;
    const int buttons = mainWindow ? 3 : 1;
    const QRect textRect(textLeft, 1, image.width() - textLeft - buttons * 46 - 8, titleH);
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(title, Qt::ElideRight, textRect.width()));
    for (int i = 0; i < buttons; ++i) {
        const QRectF button(image.width() - 1 - (buttons - i) * 46, 1, 46, titleH);
        drawCaptionGlyph(p, button, mainWindow ? i : 2, tc[fs::Token::Fg]);
    }
    p.restore();
    p.setPen(QPen(tc[fs::Token::Line], 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(shape);
    return image;
}

int runSnapshots(const SnapshotOptions &options)
{
    QDir root(options.dir);
    if (!root.mkpath(u"."_s)) {
        std::fprintf(stderr, "cannot create %s\n", qPrintable(options.dir));
        return 2;
    }

    QList<const Theme *> themes;
    for (const Theme &t : kThemes) {
        if (options.themes.isEmpty() || options.themes.contains(QString::fromLatin1(t.id)))
            themes.append(&t);
    }
    QList<Screen> screens;
    if (selected(u"main"_s, options.only))
        screens.append({u"main"_s, u"메인 창"_s, u"Main 보드 기본 상태"_s, true, QSize(1440, 900)});
    if (selected(u"main.docks"_s, options.only))
        screens.append({u"main.docks"_s, u"메인 창"_s, u"도크 넷 — 폴더 트리 · 미리보기/속성 · 작업 대기열"_s, false, QSize()});
    for (const fm::dialogs::DialogVariant &v : fm::dialogs::dialogVariants()) {
        if (selected(v.id, options.only))
            screens.append({v.id, fm::dialogs::dialogGroupLabel(v.dialog), v.label, v.fromMockup, v.client});
    }
    if (themes.isEmpty() || screens.isEmpty()) {
        std::fprintf(stderr, "nothing to shoot (check --only / --themes)\n");
        return 2;
    }

    // 최근 대상 메뉴처럼 QMenu::exec()로 여는 변형이 있다 — 열린 팝업을 닫아 중첩 루프를 끝낸다(팝업은 찍히지 않는다)
    QTimer popupCloser;
    popupCloser.setInterval(100);
    QObject::connect(&popupCloser, &QTimer::timeout, [] {
        if (QWidget *popup = QApplication::activePopupWidget())
            popup->close();
    });
    popupCloser.start();

    auto &tm = TM::instance();
    int saved = 0;
    for (const Theme *t : themes) {
        tm.setDesign(t->design);
        tm.setDarkTone(t->tone);
        tm.setScheme(t->scheme);
        settle(80);
        const QString themeDir = QString::fromLatin1(t->id);
        root.mkpath(themeDir + u"/framed"_s);
        for (Screen &s : screens) {
            QPointer<QWidget> w = createScreen(s.id);
            if (!w) {
                std::fprintf(stderr, "skip %s\n", qPrintable(s.id));
                continue;
            }
            w->setAttribute(Qt::WA_DontShowOnScreen);  // 화면에 띄우지 않고 배치 · 그리기만
            w->show();
            const bool mainWindow = s.id == u"main" || s.id.startsWith(u"main.");
            settle(s.id.startsWith(u"settings") || mainWindow ? options.settleMs * 2 : options.settleMs);
            if (!w)
                continue;
            const QImage client = renderClient(w);
            if (t == themes.first())
                s.actual = w->size();
            const QString base = root.filePath(themeDir + u'/' + s.id);
            client.save(base + u".png"_s);
            mockFrame(client, w->windowTitle(), w->windowIcon(), mainWindow).save(root.filePath(themeDir + u"/framed/"_s + s.id + u".png"_s));
            ++saved;
            w->close();
            settle(0);
            delete w.data();
            settle(0);
        }
        std::printf("%s: %d\n", t->id, int(screens.size()));
        std::fflush(stdout);
    }
    if (!writeIndex(root.absolutePath(), screens, themes)) {
        std::fprintf(stderr, "cannot write index.html\n");
        return 2;
    }
    std::printf("saved %d images + index.html in %s\n", saved * 2, qPrintable(QDir::toNativeSeparators(root.absolutePath())));
    return 0;
}

} // namespace fm::app
