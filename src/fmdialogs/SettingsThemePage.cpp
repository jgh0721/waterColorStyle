// 설정 › 테마 색상(docs/specs/04 §2.2) — 도구 줄 · 두 보기 · 편집기 · 미리보기 골격은 .ui, 목록 · 트리 · 파생 행 ·
// 대비 검사는 코드. 색은 늘 보류 구성표로 계산하고(deriveTheme), 미리보기에는 ThemeScope로 입힌다.
// 행 상태 미리보기는 메인 창과 같은 Qtitan FileListView를 PreviewStateRole로 돌린다.

#include "SettingsPages_p.h"

#include "ui_SettingsThemePage.h"

#include <fmfilelist/FileListView.h>
#include <fmfilelist/FileRoles.h>
#include <fmsettings/SettingsStore.h>
#include <fmstyle/ColorScheme.h>
#include <fmstyle/Glyphs.h>
#include <fmstyle/StylePaint.h>
#include <fmstyle/StyleProps.h>
#include <fmstyle/ThemeManager.h>
#include <fmwidgets/Button.h>
#include <fmwidgets/ProgressBar.h>
#include <fmwidgets/SettingsWidgets.h>
#include <fmwidgets/Switch.h>
#include <fmwidgets/Tag.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHelpEvent>
#include <QInputDialog>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QToolTip>

using namespace Qt::StringLiterals;

namespace fm::dialogs {

namespace {

namespace fl = fm::filelist;
namespace fs = fm::style;
namespace st = fm::settings;
using T = fs::Token;
using K = fm::ui::ChipKind;

enum ItemRole { ColorRole = Qt::UserRole + 1, BaseColorRole, CountRole, HexRole, ChipRole, ChipKindRole, KindRole, TokenRole, GroupRole };
enum TreeKind { GroupRow, TokenRow };

qreal radiusFor(const fs::ThemeColors &tc, qreal r)
{
    return tc.isWatercolor() ? 0 : r;
}

QString groupLabel(fs::TokenGroup g)
{
    switch (g) {
    case fs::TokenGroup::Surfaces: return QObject::tr("바탕 · 면");
    case fs::TokenGroup::Text: return QObject::tr("글자");
    case fs::TokenGroup::AccentSelection: return QObject::tr("강조 · 선택");
    case fs::TokenGroup::Inverse: return QObject::tr("역상 표시");
    case fs::TokenGroup::RecordTint: return QObject::tr("레코드 틴트");
    case fs::TokenGroup::Status: return QObject::tr("상태");
    case fs::TokenGroup::Elevation: return QObject::tr("권한");
    case fs::TokenGroup::FileIcons: return QObject::tr("파일 아이콘");
    case fs::TokenGroup::Effects: return QObject::tr("효과");
    }
    return {};
}

/// 반투명 토큰은 기준 토큰 위에 합성해 보인다(--tint-sel 위 --sel, --tint-inv 위 --inv-cur), --shadow는 그림자 견본.
QColor compositeBase(T token, const fs::ThemeColors &c)
{
    switch (token) {
    case T::TintSel: return c[T::Sel];
    case T::TintInv: return c[T::InvCur];
    case T::Tint: return c[T::Surface];
    default: return QColor();
    }
}

void paintTokenSwatch(QPainter *p, const QRectF &r, T token, const QColor &color, const QColor &base, const fs::ThemeColors &tc,
                      qreal radius)
{
    if (token == T::Shadow)
        fm::ui::paintSwatch(p, r, tc[T::Surface], tc, radius, QColor(), true);
    else
        fm::ui::paintSwatch(p, r, color, tc, radius, base);
}

// ------------------------------------------------------------------------------------- 기준 색 목록

/// 기준 색 행(04 §2.2.3) — 높이 38, `20 | 1fr | 36 | 64 | 84`, 선택 = --accent-soft + 왼쪽 막대 3 × 18.
class SeedDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &o, const QModelIndex &i) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(o, i).width(), 38);
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(option.widget);
        const QRect r = option.rect;
        const bool selected = option.state & QStyle::State_Selected;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (index.row() > 0)
            p->fillRect(QRect(r.left(), r.top(), r.width(), 1), tc[T::Grid]);
        if (selected) {
            p->fillRect(r.adjusted(0, index.row() > 0 ? 1 : 0, 0, 0), tc[T::AccentSoft]);
            p->setPen(Qt::NoPen);
            p->setBrush(tc[T::Accent]);
            p->drawRoundedRect(QRectF(r.left(), r.top() + 10, 3, 18), radiusFor(tc, 2), radiusFor(tc, 2));
        }
        qreal x = r.left() + 12;
        const qreal cy = r.center().y() + 0.5;
        fm::ui::paintSwatch(p, QRectF(x, cy - 10, 20, 20), index.data(ColorRole).value<QColor>(), tc, radiusFor(tc, 4));
        x += 20 + 10;
        const qreal right = r.right() - 12;
        const qreal chipW = 84, hexW = 64, countW = 36;
        const qreal nameW = right - x - chipW - hexW - countW - 30;
        const QFont nameFont = fs::pixelFont(option.font, 13, selected ? QFont::DemiBold : QFont::Normal);
        p->setFont(nameFont);
        p->setPen(tc[T::Fg]);
        p->drawText(QRectF(x, r.top(), nameW, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(nameFont).elidedText(index.data().toString(), Qt::ElideRight, int(nameW)));
        x += nameW + 10;
        p->setFont(fs::withTabularNumbers(fs::pixelFont(option.font, 12)));
        p->setPen(tc[T::Fg3]);
        p->drawText(QRectF(x, r.top(), countW, r.height()), Qt::AlignRight | Qt::AlignVCenter, tr("%1개").arg(index.data(CountRole).toInt()));
        x += countW + 10;
        p->setFont(fs::monoFont(12));
        p->setPen(tc[T::Fg2]);
        p->drawText(QRectF(x, r.top(), hexW, r.height()), Qt::AlignLeft | Qt::AlignVCenter, index.data(HexRole).toString());
        const QString chip = index.data(ChipRole).toString();
        const QSize cs = fm::ui::chipSize(chip);
        fm::ui::paintChip(p, QRectF(right - cs.width(), cy - 10, cs.width(), cs.height()), chip, K(index.data(ChipKindRole).toInt()), tc,
                          radiusFor(tc, 10));
        p->restore();
    }
};

/// 개별 색 행(04 §2.2.3) — 최소 높이 52, 이름 · 설명 · 16 px 견본들 · 셰브런. 누르면 모든 토큰 보기로.
class IndividualRow final : public QAbstractButton
{
public:
    IndividualRow(const QString &title, const QString &description, QWidget *parent)
        : QAbstractButton(parent)
        , m_description(description)
    {
        setText(title);
        setAccessibleName(title);
        setAccessibleDescription(description);
        setFocusPolicy(Qt::StrongFocus);
        setMinimumHeight(52);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setSwatches(const QList<std::pair<QColor, bool>> &swatches)  // (색, 그림자 견본)
    {
        m_swatches = swatches;
        update();
    }
    bool m_first = false;

    QSize sizeHint() const override { return QSize(400, 52); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (!m_first)
            p.fillRect(QRect(0, 0, width(), 1), tc[T::Grid]);
        if (underMouse())
            p.fillRect(rect().adjusted(0, m_first ? 0 : 1, 0, 0), tc[T::Alt]);
        qreal right = width() - 12;
        fs::paintGlyph(&p, fs::Glyph::ChevronRight, QRectF(right - 12, (height() - 12) / 2.0, 12, 12), tc[T::Fg3]);
        right -= 12 + 12;
        for (int i = int(m_swatches.size()) - 1; i >= 0; --i) {
            const QRectF r(right - 16, (height() - 16) / 2.0, 16, 16);
            if (m_swatches[i].second)
                fm::ui::paintSwatch(&p, r, tc[T::Surface], tc, radiusFor(tc, 3), QColor(), true);
            else
                fm::ui::paintSwatch(&p, r, m_swatches[i].first, tc, radiusFor(tc, 3));
            right -= 16 + 4;
        }
        const qreal textW = right - 14 - 8;
        p.setFont(fs::pixelFont(font(), 13));
        p.setPen(tc[T::Fg]);
        p.drawText(QRectF(14, 8, textW, 18), Qt::AlignLeft | Qt::AlignVCenter, text());
        const QFont small = fs::pixelFont(font(), 12);
        p.setFont(small);
        p.setPen(tc[T::Fg3]);
        p.drawText(QRectF(14, 28, textW, 16), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(small).elidedText(m_description, Qt::ElideRight, int(textW)));
        if (hasFocus()) {
            p.setPen(QPen(tc[T::Focus], 1.0, Qt::DotLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(rect()).adjusted(2.5, 2.5, -2.5, -2.5));
        }
    }
    void enterEvent(QEnterEvent *e) override
    {
        update();
        QAbstractButton::enterEvent(e);
    }
    void leaveEvent(QEvent *e) override
    {
        update();
        QAbstractButton::leaveEvent(e);
    }

private:
    QString m_description;
    QList<std::pair<QColor, bool>> m_swatches;
};

// ------------------------------------------------------------------------------------- 편집기 부품

/// 큰 견본(34 × 34, 모서리 6).
class SwatchLabel final : public QWidget
{
public:
    explicit SwatchLabel(QWidget *parent)
        : QWidget(parent)
    {
        setFixedSize(34, 34);
    }
    void setSwatch(T token, const QColor &color, const QColor &base)
    {
        m_token = token;
        m_color = color;
        m_base = base;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        paintTokenSwatch(&p, QRectF(rect()), m_token, m_color, m_base, tc, radiusFor(tc, 6));
    }

private:
    T m_token = T::Accent;
    QColor m_color, m_base;
};

class ChipWidget final : public QWidget
{
public:
    using QWidget::QWidget;
    void setChip(const QString &text, K kind)
    {
        m_text = text;
        m_kind = kind;
        setAccessibleName(text);
        updateGeometry();
        update();
    }
    QSize sizeHint() const override { return fm::ui::chipSize(m_text); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QSize s = sizeHint();
        fm::ui::paintChip(&p, QRectF(0, (height() - s.height()) / 2.0, s.width(), s.height()), m_text, m_kind, tc, radiusFor(tc, 10));
    }

private:
    QString m_text;
    K m_kind = K::Base;
};

/// 위 · 아래 1 px --grid를 긋는 줄(기준 색 줄 · 연동 안내 · 단추 줄).
class RuledRow final : public QWidget
{
public:
    RuledRow(bool top, bool bottom, QWidget *parent)
        : QWidget(parent)
        , m_top(top)
        , m_bottom(bottom)
    {
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        if (m_top)
            p.fillRect(QRect(0, 0, width(), 1), tc[T::Grid]);
        if (m_bottom)
            p.fillRect(QRect(0, height() - 1, width(), 1), tc[T::Grid]);
    }

private:
    bool m_top, m_bottom;
};

/// 파생 행(04 §2.2.4) — 높이 24, `16 | 128 | 60 | 1fr`, 간격 8, 12.5 px. 규칙은 말줄임 + 도구 설명 전문, 태그(직접 지정 · 보정됨).
class DerivedRows final : public QWidget
{
public:
    struct Row
    {
        T token;
        QColor color;
        QColor base;
        QString value;
        QString rule;
        int tag = 0;  // 1 직접 지정 · 2 보정됨
    };
    using QWidget::QWidget;

    void setRows(const QList<Row> &rows)
    {
        m_rows = rows;
        setFixedHeight(24 * int(rows.size()));
        update();
    }

protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::ToolTip) {
            auto *help = static_cast<QHelpEvent *>(e);
            const int i = help->pos().y() / 24;
            if (i >= 0 && i < m_rows.size())
                QToolTip::showText(help->globalPos(), u"%1 — %2"_s.arg(fs::tokenCssName(m_rows[i].token), m_rows[i].rule), this);
            return true;
        }
        return QWidget::event(e);
    }
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QFont body = fs::pixelFont(font(), 12.5);
        const QFont mono = fs::monoFont(12);
        const QFont rule = fs::pixelFont(font(), 12);
        for (int i = 0; i < m_rows.size(); ++i) {
            const Row &r = m_rows[i];
            const qreal y = i * 24.0;
            qreal x = 0;
            paintTokenSwatch(&p, QRectF(x, y + 4, 16, 16), r.token, r.color, r.base, tc, radiusFor(tc, 3));
            x += 16 + 8;
            p.setFont(body);
            p.setPen(tc[T::Fg]);
            p.drawText(QRectF(x, y, 128, 24), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(body).elidedText(fs::tokenLabel(r.token), Qt::ElideRight, 128));
            x += 128 + 8;
            p.setFont(mono);
            p.setPen(tc[T::Fg2]);
            p.drawText(QRectF(x, y, 60, 24), Qt::AlignLeft | Qt::AlignVCenter, r.value);
            x += 60 + 8;
            qreal right = width();
            if (r.tag) {
                const QString tag = r.tag == 1 ? tr("직접 지정") : tr("보정됨");
                const QSize s = fm::ui::chipSize(tag, 10.5, 6, 18);
                right -= s.width();
                fm::ui::paintChip(&p, QRectF(right, y + 3, s.width(), s.height()), tag, r.tag == 1 ? K::Override : K::Fixed, tc,
                                  radiusFor(tc, 9), 10.5);
                right -= 8;
            }
            p.setFont(rule);
            p.setPen(tc[T::Fg3]);
            p.drawText(QRectF(x, y, right - x, 24), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(rule).elidedText(r.rule, Qt::ElideRight, int(right - x)));
        }
    }

private:
    QList<Row> m_rows;
};

// ------------------------------------------------------------------------------------- 토큰 트리

/// 그룹 머리 32(--head, 셰브런 · 이름 · 개수 · "바뀜 N") · 토큰 행 40(들여쓰기 32, 견본 · 두 줄 이름 · 값 · 칩).
class TokenTreeDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &o, const QModelIndex &i) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(o, i).width(), i.data(KindRole).toInt() == GroupRow ? 32 : 40);
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(option.widget);
        const auto *view = qobject_cast<const QTreeView *>(option.widget);
        const QRect r = option.rect;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (index.data(KindRole).toInt() == GroupRow) {
            p->fillRect(r, tc[T::Head]);
            if (index.row() > 0)
                p->fillRect(QRect(r.left(), r.top(), r.width(), 1), tc[T::Line]);
            const bool open = view && view->isExpanded(index);
            qreal x = r.left() + 10;
            fs::paintGlyph(p, open ? fs::Glyph::ChevronDown : fs::Glyph::ChevronRight, QRectF(x, r.center().y() - 5.5, 12, 12), tc[T::Fg3]);
            x += 12 + 8;
            const QFont name = fs::pixelFont(option.font, 12.5, QFont::DemiBold);
            p->setFont(name);
            p->setPen(tc[T::Fg]);
            const QString label = index.data().toString();
            p->drawText(QRectF(x, r.top(), r.width(), r.height()), Qt::AlignLeft | Qt::AlignVCenter, label);
            x += QFontMetrics(name).horizontalAdvance(label) + 8;
            p->setFont(fs::withTabularNumbers(fs::pixelFont(option.font, 12)));
            p->setPen(tc[T::Fg3]);
            p->drawText(QRectF(x, r.top(), 60, r.height()), Qt::AlignLeft | Qt::AlignVCenter, index.data(CountRole).toString());
            const QString chip = index.data(ChipRole).toString();
            if (!chip.isEmpty()) {
                const QSize s = fm::ui::chipSize(chip);
                fm::ui::paintChip(p, QRectF(r.right() - 12 - s.width(), r.center().y() - 9.5, s.width(), s.height()), chip, K::Changed, tc,
                                  radiusFor(tc, 10));
            }
            p->restore();
            return;
        }
        p->fillRect(QRect(r.left(), r.top(), r.width(), 1), tc[T::Grid]);
        if (option.state & QStyle::State_Selected) {
            p->fillRect(r.adjusted(0, 1, 0, 0), tc[T::AccentSoft]);
            p->setPen(Qt::NoPen);
            p->setBrush(tc[T::Accent]);
            p->drawRoundedRect(QRectF(r.left(), r.top() + 11, 3, 18), radiusFor(tc, 2), radiusFor(tc, 2));
        }
        const auto token = T(index.data(TokenRole).toInt());
        qreal x = r.left() + 32;
        paintTokenSwatch(p, QRectF(x, r.center().y() - 9.5, 20, 20), token, index.data(ColorRole).value<QColor>(),
                         index.data(BaseColorRole).value<QColor>(), tc, radiusFor(tc, 4));
        x += 20 + 10;
        const qreal right = r.right() - 12;
        const qreal chipW = 108, valueW = 76;
        const qreal nameW = right - x - chipW - valueW - 20;
        const QFont name = fs::pixelFont(option.font, 13);
        p->setFont(name);
        p->setPen(tc[T::Fg]);
        p->drawText(QRectF(x, r.top() + 3, nameW, 17), Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(name).elidedText(index.data().toString(), Qt::ElideRight, int(nameW)));
        p->setFont(fs::monoFont(11));
        p->setPen(tc[T::Fg3]);
        p->drawText(QRectF(x, r.top() + 20, nameW, 15), Qt::AlignLeft | Qt::AlignVCenter, fs::tokenCssName(token));
        x += nameW + 10;
        p->setFont(fs::monoFont(12));
        p->setPen(tc[T::Fg2]);
        p->drawText(QRectF(x, r.top(), valueW, r.height()), Qt::AlignLeft | Qt::AlignVCenter, index.data(HexRole).toString());
        const QString chip = index.data(ChipRole).toString();
        const QFont chipFont = fs::pixelFont(option.font, 11.5, QFont::DemiBold);
        const QString shown = QFontMetrics(chipFont).elidedText(chip, Qt::ElideRight, int(chipW) - 16);
        const QSize s = fm::ui::chipSize(shown);
        fm::ui::paintChip(p, QRectF(right - s.width(), r.center().y() - 9.5, s.width(), s.height()), shown, K(index.data(ChipKindRole).toInt()),
                          tc, radiusFor(tc, 10));
        p->restore();
    }
};

// ------------------------------------------------------------------------------------- 대비 검사

/// 대비 검사 격자(04 §2.2.7 (4)) — 3열, 항목 20: 통과 체크 --ok / 실패 느낌표 --danger · 이름 · 비율(실패면 --danger 600).
class ContrastGrid final : public QWidget
{
public:
    using QWidget::QWidget;
    void setResults(const QList<fs::ContrastResult> &results, const std::bitset<fs::kTokenCount> &adjusted)
    {
        m_results = results;
        m_adjusted = adjusted;
        const int rows = (int(results.size()) + 2) / 3;
        setFixedHeight(rows * 20 + (rows - 1) * 4);
        QStringList a11y;
        for (const fs::ContrastResult &r : results)
            a11y.append(u"%1 %2:1%3"_s.arg(r.label).arg(r.ratio, 0, 'f', 1).arg(r.passes ? QString() : tr(" 낮음")));
        setAccessibleDescription(a11y.join(u", "_s));
        update();
    }

protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::ToolTip) {
            const int i = hit(static_cast<QHelpEvent *>(e)->pos());
            if (i >= 0) {
                const fs::ContrastResult &r = m_results[i];
                QToolTip::showText(static_cast<QHelpEvent *>(e)->globalPos(),
                                   u"%1 / %2 = %3 : 1"_s.arg(fs::tokenLabel(r.foreground), fs::tokenLabel(r.background)).arg(r.ratio, 0, 'f', 2),
                                   this);
            }
            return true;
        }
        return QWidget::event(e);
    }
    void paintEvent(QPaintEvent *) override
    {
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal colW = (width() - 24) / 3.0;
        const QFont name = fs::pixelFont(font(), 12);
        for (int i = 0; i < m_results.size(); ++i) {
            const fs::ContrastResult &r = m_results[i];
            const qreal x = (i % 3) * (colW + 12);
            const qreal y = (i / 3) * 24.0;
            const QRectF icon(x, y + 4, 12, 12);
            if (r.passes) {
                fs::paintGlyph(&p, fs::Glyph::Check, icon, tc[T::Ok]);
            } else {
                p.setPen(QPen(tc[T::Danger], 1.6, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(QPointF(icon.center().x(), icon.top() + 2.5), QPointF(icon.center().x(), icon.top() + 6.7));
                p.drawLine(QPointF(icon.center().x(), icon.top() + 9.2), QPointF(icon.center().x(), icon.top() + 9.5));
            }
            const QString ratio = QString::number(std::floor(r.ratio * 10) / 10, 'f', 1)
                                + (m_adjusted.test(std::size_t(r.foreground)) ? tr(" 보정") : QString());
            const QFont mono = fs::monoFont(11.5, r.passes ? QFont::Normal : QFont::DemiBold);
            const qreal ratioW = QFontMetrics(mono).horizontalAdvance(ratio);
            p.setFont(mono);
            p.setPen(r.passes ? tc[T::Fg2] : tc[T::Danger]);
            p.drawText(QRectF(x + colW - ratioW, y, ratioW, 20), Qt::AlignRight | Qt::AlignVCenter, ratio);
            p.setFont(name);
            p.setPen(tc[T::Fg2]);
            const qreal nameW = colW - 12 - 6 - ratioW - 6;
            p.drawText(QRectF(x + 18, y, nameW, 20), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(name).elidedText(r.label, Qt::ElideRight, int(nameW)));
        }
    }

private:
    int hit(const QPoint &pos) const
    {
        const qreal colW = (width() - 24) / 3.0;
        const int col = int(pos.x() / (colW + 12));
        const int row = pos.y() / 24;
        const int i = row * 3 + col;
        return col >= 0 && col < 3 && i >= 0 && i < m_results.size() ? i : -1;
    }
    QList<fs::ContrastResult> m_results;
    std::bitset<fs::kTokenCount> m_adjusted;
};

/// 행 상태 미리보기 열 — 아이콘 · 이름("미리보기 · 적용 전, 이 변형만") · 상태 글자("행 상태", 112, 오른쪽).
fl::ListColumnLayout rowStateLayout()
{
    using R = fl::ListColumn::Role;
    using L = fl::ListColumn::TwoLine;
    fl::ListColumnLayout layout;
    layout.columns = {
        {0, R::Icon, QString(), 16, 36, Qt::AlignCenter, false, L::Row0Full},
        {1, R::Name, QObject::tr("미리보기 · 적용 전, 이 변형만"), 0, 0, Qt::AlignLeft, true, L::Row0Full},
        {2, R::Meta, QObject::tr("행 상태"), 112, -1, Qt::AlignRight, true, L::Row1},
    };
    return layout;
}

// ------------------------------------------------------------------------------------- 페이지

class ThemePage final : public SettingsPage
{
public:
    ThemePage(SettingsSession *session, QWidget *parent)
        : SettingsPage(session, parent)
        , ui(std::make_unique<Ui::SettingsThemePage>())
    {
        ui->setupUi(this);
        m_editDark = fs::isDarkVariant(fs::ThemeManager::instance().effectiveVariant());
        buildHeader();
        buildSeedList();
        buildIndividual();
        buildSeedEditor();
        buildTokenTree();
        buildTokenEditor();
        buildPreview();
        bindToolbar();
        m_saved = st::SettingsStore::instance().savedSchemes();
        addCustomItem(this, [](const AppSettings &a, const AppSettings &b) {
                          return a.theme.schemeId != b.theme.schemeId || !a.theme.scheme.sameColors(b.theme.scheme);
                      },
                      [](AppSettings &p, const AppSettings &d) {
                          p.theme.schemeId = d.theme.schemeId;
                          p.theme.scheme = d.theme.scheme;
                      },
                      Section::Theme);
    }

    QString pageId() const override { return u"theme"_s; }
    QString title() const override { return tr("테마 색상"); }
    QString description() const override
    {
        return tr("기준 색 11개를 고르면 나머지 토큰은 규칙에 따라 맞춰집니다. 라이트와 다크는 따로 저장됩니다.");
    }
    QString footerPath() const override { return st::SettingsStore::displayPath(true); }
    QWidget *headerTrailing() override { return m_header; }
    fm::settings::Sections sections() const override { return Section::Theme; }
    QStringList searchKeywords() const override
    {
        QStringList words = SettingsPage::searchKeywords();
        words << tr("색 구성표") << tr("기준 색") << tr("개별 색") << tr("모든 토큰") << tr("대비 검사") << tr("낮으면 자동 보정");
        for (const fs::SeedRoleInfo &r : fs::seedRoles())
            words.append(QString::fromUtf8(r.label));
        for (const fs::TokenInfo &t : fs::allTokens())
            words << QString::fromUtf8(t.label) << QString::fromLatin1(t.cssName);
        return words;
    }

    /// 테마 색상의 되돌리기: 확인 후 지금 디자인의 기준 색 · 직접 지정을 모두 지우고 "기본 (내장)"으로(04 §1.7).
    void resetToDefaults() override
    {
        if (QMessageBox::question(this, tr("기본값으로 되돌리기"),
                                  tr("지금 디자인의 라이트 · 다크 기준 색과 직접 지정을 모두 지우고 \"기본 (내장)\"으로 되돌릴까요?"))
            != QMessageBox::Yes)
            return;
        const auto d = std::size_t(design());
        session()->edit(Section::Theme, [&](AppSettings &p) {
            fs::ColorScheme &s = p.theme.scheme;
            s.seeds.accent.reset();
            s.seeds.fixContrast = true;
            for (fs::VariantSeeds &vs : s.seeds.roles[d])
                vs = fs::VariantSeeds();
            for (fs::TokenOverrides &o : s.overrides[d])
                o = fs::TokenOverrides();
            p.theme.schemeId = u"builtin"_s;
            s.id = u"builtin"_s;
            s.name.clear();
        });
    }

    void syncFromPending() override
    {
        SettingsPage::syncFromPending();
        const AppSettings &p = pending();
        const fs::Variant v = editVariant();
        m_derived = fs::deriveTheme(v, p.theme.scheme.seeds, p.theme.scheme.overridesFor(design(), v), design());
        m_changed.reset();
        const fs::TokenOverrides &ov = overrides();
        for (const fs::TokenInfo &t : fs::allTokens()) {
            const auto i = std::size_t(t.token);
            if (ov[i] || m_derived.colors[t.token].rgba() != fs::builtinColor(t.token, v, design()))
                m_changed.set(i);
        }
        m_contrastFail.reset();
        for (const fs::ContrastResult &r : fs::checkContrast(m_derived.colors)) {
            if (!r.passes) {
                m_contrastFail.set(std::size_t(r.foreground));
                m_contrastFail.set(std::size_t(r.background));
            }
        }
        {
            const QSignalBlocker b1(m_variantSegment);
            m_variantSegment->setCurrentIndex(m_editDark ? 1 : 0);
            const QSignalBlocker b2(ui->viewSegment);
            const bool advanced = p.dialog.themeView == u"adv";
            ui->viewSegment->setCurrentIndex(advanced ? 1 : 0);
            ui->leftStack->setCurrentIndex(advanced ? 1 : 0);
            ui->editorStack->setCurrentIndex(advanced ? 1 : 0);
        }
        syncSchemeCombo();
        syncSeedList();
        syncIndividual();
        syncSeedEditor();
        syncTokenTree();
        syncTokenEditor();
        syncPreview();
    }

    /// 목업 보드: 기준 색 보기, 강조색 선택.
    void showBoardState() override
    {
        m_role = fs::SeedRole::Accent;
        m_token = T::AccentFg;
        syncFromPending();
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        m_saved = st::SettingsStore::instance().savedSchemes();
        syncSchemeCombo();
        SettingsPage::showEvent(event);
    }

private:
    // ---------------------------------------------------------------- 상태 도우미

    fs::Design design() const { return pending().appearance.design; }
    /// 편집 변형 — 다크는 워터컬러 남색 색조면 남색 칸(시안1의 남색은 다크 칸을 쓴다).
    fs::Variant editVariant() const
    {
        if (!m_editDark)
            return fs::Variant::Light;
        return design() == fs::Design::Watercolor && pending().appearance.darkTone == st::DarkTone::Navy ? fs::Variant::Navy
                                                                                                          : fs::Variant::Dark;
    }
    const fs::ThemeSeeds &seeds() const { return pending().theme.scheme.seeds; }
    const fs::VariantSeeds &variantSeeds() const { return seeds().variantSeeds(design(), editVariant()); }
    const fs::TokenOverrides &overrides() const { return pending().theme.scheme.overridesFor(design(), editVariant()); }
    bool isOverridden(T t) const { return overrides()[std::size_t(t)].has_value(); }

    void editScheme(const std::function<void(fs::ColorScheme &, fs::VariantSeeds &, fs::TokenOverrides &)> &mutate)
    {
        const fs::Design d = design();
        const fs::Variant v = editVariant();
        session()->edit(Section::Theme, [&](AppSettings &p) {
            mutate(p.theme.scheme, p.theme.scheme.seeds.variantSeeds(d, v), p.theme.scheme.overridesFor(d, v));
        });
    }

    bool roleDirty(fs::SeedRole role) const
    {
        const fs::SeedRoleInfo &info = fs::seedRoleInfo(role);
        const fs::VariantSeeds &vs = variantSeeds();
        if (vs.seed[std::size_t(role)])
            return true;
        if (role == fs::SeedRole::Accent && seeds().accent)
            return true;
        if ((role == fs::SeedRole::Sel && !vs.selFollowsAccent) || (role == fs::SeedRole::InvSel && !vs.invSelFollowsAccent))
            return true;
        for (int i = 0; i < info.tokenCount; ++i) {
            if (isOverridden(info.tokens[std::size_t(i)]))
                return true;
        }
        return false;
    }

    void setRoleSeed(fs::SeedRole role, const std::optional<QColor> &color)
    {
        const bool both = pending().theme.editBothVariants;
        const auto d = std::size_t(design());
        editScheme([&](fs::ColorScheme &s, fs::VariantSeeds &vs, fs::TokenOverrides &) {
            const auto r = std::size_t(role);
            if (role == fs::SeedRole::Accent && both) {
                s.seeds.accent = color;
                for (fs::VariantSeeds &x : s.seeds.roles[d])
                    x.seed[r].reset();
                return;
            }
            vs.seed[r] = color;
            if (role == fs::SeedRole::Sel)
                vs.selFollowsAccent = false;
            if (role == fs::SeedRole::InvSel)
                vs.invSelFollowsAccent = false;
        });
    }

    void resetRole(fs::SeedRole role)
    {
        const bool both = pending().theme.editBothVariants;
        const auto d = std::size_t(design());
        const fs::SeedRoleInfo &info = fs::seedRoleInfo(role);
        editScheme([&](fs::ColorScheme &s, fs::VariantSeeds &vs, fs::TokenOverrides &ov) {
            vs.seed[std::size_t(role)].reset();
            if (role == fs::SeedRole::Accent && both) {
                s.seeds.accent.reset();
                for (fs::VariantSeeds &x : s.seeds.roles[d])
                    x.seed[std::size_t(role)].reset();
            }
            if (role == fs::SeedRole::Sel)
                vs.selFollowsAccent = true;
            if (role == fs::SeedRole::InvSel)
                vs.invSelFollowsAccent = true;
            for (int i = 0; i < info.tokenCount; ++i)
                ov[std::size_t(info.tokens[std::size_t(i)])].reset();
        });
    }

    void setOverride(T token, const std::optional<QColor> &color)
    {
        editScheme([&](fs::ColorScheme &, fs::VariantSeeds &, fs::TokenOverrides &ov) { ov[std::size_t(token)] = color; });
    }

    /// 토큰 값 바꾸기(모든 토큰 보기) — 시드 토큰은 기준 색, 나머지는 직접 지정.
    void setTokenValue(T token, const QColor &color)
    {
        const auto role = fs::roleOfToken(token);
        if (role && fs::seedRoleInfo(*role).seedToken() == token && !isOverridden(token))
            setRoleSeed(*role, color);
        else
            setOverride(token, color);
    }

    void setView(bool advanced)
    {
        session()->edit(Section::Dialog, [&](AppSettings &p) { p.dialog.themeView = advanced ? u"adv"_s : u"basic"_s; });
    }

    std::optional<QColor> pickColor(const QColor &initial, const QString &title)
    {
        const QColor c = QColorDialog::getColor(initial, this, title);
        return c.isValid() ? std::optional<QColor>(c) : std::nullopt;
    }

    /// 숨은 스택 페이지는 LayoutRequest를 처리하지 않아, 자식을 숨겨도 최소 폭이 옛 값(모든 컨트롤이 보일 때)으로 남는다
    /// — 그러면 스택 · 페이지가 넓어져 오른쪽이 잘린다. 부모 레이아웃이 들고 있는 자식 크기 캐시(QWidgetItemV2)를 직접 버린다.
    void refreshStackGeometry(QWidget *page)
    {
        for (QLayout *l : page->findChildren<QLayout *>())
            l->invalidate();
        for (QWidget *w : page->findChildren<QWidget *>())
            w->updateGeometry();
        page->updateGeometry();
        ui->editorStack->updateGeometry();
    }

    // ---------------------------------------------------------------- 머리 · 도구 줄

    void buildHeader()
    {
        m_header = new QWidget(this);
        auto *layout = new QHBoxLayout(m_header);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(16);
        auto *label = new fm::ui::Label(tr("편집 중"), fm::ui::Label::Summary, m_header);
        m_variantSegment = new fm::ui::SegmentedControl(m_header);
        m_variantSegment->setItems({tr("라이트"), tr("다크")});
        m_variantSegment->setAccessibleName(tr("편집할 변형"));
        layout->addWidget(label, 0, Qt::AlignBottom);
        layout->addWidget(m_variantSegment, 0, Qt::AlignBottom);
        m_header->hide();
        connect(m_variantSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, [this](int i) {
            m_editDark = i == 1;
            syncFromPending();
        });
        connect(ui->viewSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, [this](int i) { setView(i == 1); });
    }

    void bindToolbar()
    {
        connect(ui->schemeCombo, &QComboBox::activated, this, [this](int index) {
            const QString id = ui->schemeCombo->itemData(index).toString();
            if (id == pending().theme.schemeId && !isSchemeModified())
                return;
            fs::ColorScheme chosen;
            for (const fs::ColorScheme &s : std::as_const(m_saved)) {
                if (s.id == id)
                    chosen = s;
            }
            session()->edit(Section::Theme, [&](AppSettings &p) {
                p.theme.schemeId = id;
                p.theme.scheme = chosen;
            });
        });
        connect(ui->saveAsButton, &QPushButton::clicked, this, [this] {
            bool ok = false;
            const QString name = QInputDialog::getText(this, tr("다른 이름으로 저장"), tr("색 구성표 이름"), QLineEdit::Normal,
                                                       pending().theme.scheme.name, &ok)
                                     .trimmed();
            if (!ok || name.isEmpty())
                return;
            QString id = name;
            static const QRegularExpression invalid(u"[\\\\/:*?\"<>|\\s]+"_s);
            id.replace(invalid, u"-"_s);
            fs::ColorScheme scheme = pending().theme.scheme;
            scheme.id = id;
            scheme.name = name;
            QString error;
            if (!st::SettingsStore::instance().saveScheme(scheme, &error)) {
                QMessageBox::warning(this, tr("다른 이름으로 저장"), tr("저장하지 못했습니다.\n%1").arg(error));
                return;
            }
            m_saved = st::SettingsStore::instance().savedSchemes();
            session()->edit(Section::Theme, [&](AppSettings &p) {
                p.theme.schemeId = id;
                p.theme.scheme = scheme;
            });
        });
        connect(ui->importButton, &QPushButton::clicked, this, [this] {
            const QString path = QFileDialog::getOpenFileName(this, tr("색 구성표 가져오기"), QString(), tr("색 구성표 (*.json)"));
            if (path.isEmpty())
                return;
            QFile file(path);
            QString error;
            std::optional<fs::ColorScheme> scheme;
            if (file.open(QIODevice::ReadOnly))
                scheme = fs::ColorScheme::fromJson(QJsonDocument::fromJson(file.readAll()).object(), &error);
            else
                error = file.errorString();
            if (!scheme || !st::SettingsStore::instance().saveScheme(*scheme, &error)) {
                QMessageBox::warning(this, tr("색 구성표 가져오기"), tr("가져오지 못했습니다.\n%1").arg(error));
                return;
            }
            m_saved = st::SettingsStore::instance().savedSchemes();
            session()->edit(Section::Theme, [&](AppSettings &p) {
                p.theme.schemeId = scheme->id;
                p.theme.scheme = *scheme;
            });
        });
        connect(ui->exportButton, &QPushButton::clicked, this, [this] {
            const fs::ColorScheme &scheme = pending().theme.scheme;
            const QString suggested = (scheme.name.isEmpty() ? u"fm-theme"_s : scheme.name) + u".fmtheme.json"_s;
            const QString path = QFileDialog::getSaveFileName(this, tr("색 구성표 내보내기"), suggested, tr("색 구성표 (*.json)"));
            if (path.isEmpty())
                return;
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
                || file.write(QJsonDocument(scheme.toJson()).toJson(QJsonDocument::Indented)) < 0)
                QMessageBox::warning(this, tr("색 구성표 내보내기"), tr("저장하지 못했습니다.\n%1").arg(file.errorString()));
        });
    }

    bool isSchemeModified() const
    {
        const st::ThemeSettings &t = pending().theme;
        if (t.schemeId == u"builtin")
            return !t.scheme.isPristine();
        for (const fs::ColorScheme &s : m_saved) {
            if (s.id == t.schemeId)
                return !t.scheme.sameColors(s);
        }
        return false;
    }

    void syncSchemeCombo()
    {
        const st::ThemeSettings &t = pending().theme;
        const bool modified = isSchemeModified();
        const QSignalBlocker block(ui->schemeCombo);
        ui->schemeCombo->clear();
        auto add = [&](const QString &name, const QString &id) {
            ui->schemeCombo->addItem(id == t.schemeId && modified ? tr("%1 — 수정됨").arg(name) : name, id);
        };
        add(tr("기본 (내장)"), u"builtin"_s);
        bool found = t.schemeId == u"builtin";
        for (const fs::ColorScheme &s : std::as_const(m_saved)) {
            add(s.name.isEmpty() ? s.id : s.name, s.id);
            found = found || s.id == t.schemeId;
        }
        if (!found)
            add(t.scheme.name.isEmpty() ? t.schemeId : t.scheme.name, t.schemeId);
        ui->schemeCombo->setCurrentIndex(std::max(0, ui->schemeCombo->findData(t.schemeId)));
    }

    // ---------------------------------------------------------------- 기준 색 보기 — 왼쪽

    void buildSeedList()
    {
        m_seedModel = new QStandardItemModel(this);
        for (const fs::SeedRoleInfo &r : fs::seedRoles()) {
            auto *item = new QStandardItem(QString::fromUtf8(r.label));
            item->setData(r.tokenCount, CountRole);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_seedModel->appendRow(item);
        }
        ui->seedList->setModel(m_seedModel);
        ui->seedList->setItemDelegate(new SeedDelegate(ui->seedList));
        ui->seedList->setUniformItemSizes(true);
        ui->seedList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->seedList->setFixedHeight(38 * int(fs::kSeedRoleCount) + 2);
        connect(ui->seedList->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &index) {
            if (m_syncing || !index.isValid())
                return;
            m_role = fs::SeedRole(index.row());
            syncSeedEditor();
        });
    }

    /// 목록 칩(우선순위): 직접 지정 있음 · 바뀜 · 강조 연동 · 기본.
    std::pair<QString, K> roleChip(fs::SeedRole role) const
    {
        const fs::SeedRoleInfo &info = fs::seedRoleInfo(role);
        for (int i = 0; i < info.tokenCount; ++i) {
            if (isOverridden(info.tokens[std::size_t(i)]))
                return {tr("직접 지정 있음"), K::Override};
        }
        const fs::VariantSeeds &vs = variantSeeds();
        const bool seeded = vs.seed[std::size_t(role)].has_value() || (role == fs::SeedRole::Accent && seeds().accent.has_value())
                          || (role == fs::SeedRole::Sel && !vs.selFollowsAccent) || (role == fs::SeedRole::InvSel && !vs.invSelFollowsAccent);
        if (seeded)
            return {tr("바뀜"), K::Changed};
        if (info.followsAccent)
            return {tr("강조 연동"), K::Link};
        return {tr("기본"), K::Base};
    }

    void syncSeedList()
    {
        m_syncing = true;
        for (const fs::SeedRoleInfo &r : fs::seedRoles()) {
            QStandardItem *item = m_seedModel->item(int(r.role));
            const QColor c = fs::seedColor(r.role, m_derived.colors, seeds());
            const auto [chip, kind] = roleChip(r.role);
            item->setData(c, ColorRole);
            item->setData(fs::colorHex(c), HexRole);
            item->setData(chip, ChipRole);
            item->setData(int(kind), ChipKindRole);
            item->setData(u"%1, %2, %3, 토큰 %4개"_s.arg(QString::fromUtf8(r.label), fs::colorHex(c), chip).arg(r.tokenCount), Qt::AccessibleTextRole);
        }
        ui->seedList->selectionModel()->setCurrentIndex(m_seedModel->index(int(m_role), 0), QItemSelectionModel::ClearAndSelect);
        m_syncing = false;
    }

    void buildIndividual()
    {
        m_iconsRow = new IndividualRow(tr("파일 아이콘"), tr("폴더와 종류별 아이콘 8개"), ui->individualCard);
        m_iconsRow->m_first = true;
        m_miscRow = new IndividualRow(tr("진행 · 권한 · 그림자"), tr("일시 정지 막대, 방패 두 색, 팝업 그림자"), ui->individualCard);
        ui->individualLayout->addWidget(m_iconsRow);
        ui->individualLayout->addWidget(m_miscRow);
        auto jump = [this](fs::TokenGroup group, T token) {
            m_filter = 0;
            ui->tokenSearch->clear();
            m_expanded.insert(int(group));
            m_token = token;
            setView(true);
        };
        connect(m_iconsRow, &QAbstractButton::clicked, this, [jump] { jump(fs::TokenGroup::FileIcons, T::Folder); });
        connect(m_miscRow, &QAbstractButton::clicked, this, [this, jump] {
            m_expanded.insert(int(fs::TokenGroup::Elevation));
            m_expanded.insert(int(fs::TokenGroup::Effects));
            jump(fs::TokenGroup::Status, T::Paused);
        });
    }

    void syncIndividual()
    {
        const fs::ThemeColors &c = m_derived.colors;
        QList<std::pair<QColor, bool>> icons;
        for (T t : {T::Folder, T::KExe, T::KPdf, T::KImg, T::KZip, T::KCode, T::KDoc, T::KSys})
            icons.append({c[t], false});
        m_iconsRow->setSwatches(icons);
        m_miscRow->setSwatches({{c[T::Paused], false}, {c[T::Shield], false}, {c[T::Shield2], false}, {c[T::Shadow], true}});
    }

    // ---------------------------------------------------------------- 기준 색 편집기

    void buildSeedEditor()
    {
        QVBoxLayout *layout = ui->seedEditorLayout;
        // 머리: 견본 34 · 이름 15/600 · 설명 · 기본값으로
        auto *head = new QHBoxLayout();
        head->setSpacing(12);
        m_seedSwatch = new SwatchLabel(ui->seedEditor);
        auto *names = new QVBoxLayout();
        names->setSpacing(0);
        m_seedName = new fm::ui::Label(QString(), fm::ui::Label::Heading, ui->seedEditor);
        m_seedDescription = new fm::ui::Label(QString(), fm::ui::Label::Minor, ui->seedEditor);
        m_seedDescription->setElideMode(Qt::ElideRight);
        names->addWidget(m_seedName);
        names->addWidget(m_seedDescription);
        m_roleReset = new fm::ui::Button(tr("기본값으로"), ui->seedEditor);
        m_roleReset->setCompact(true);
        m_roleReset->setAutoDefault(false);
        head->addWidget(m_seedSwatch);
        head->addLayout(names, 1);
        head->addWidget(m_roleReset);
        layout->addLayout(head);

        // 기준 색 줄
        auto *line = new RuledRow(true, true, ui->seedEditor);
        auto *ll = new QHBoxLayout(line);
        ll->setContentsMargins(0, 6, 0, 6);
        ll->setSpacing(10);
        ll->addWidget(new fm::ui::Label(tr("기준 색"), fm::ui::Label::Meta, line));
        m_accentPicker = new fm::ui::AccentPicker(line);
        m_accentPicker->setAccessibleName(tr("강조색"));
        m_followCheck = new QCheckBox(tr("강조색 따라가기(&W)"), line);
        m_pickSeed = new fm::ui::Button(tr("색 선택…(&C)"), line);
        m_pickSeed->setCompact(true);
        m_pickSeed->setAutoDefault(false);
        m_seedHex = new fm::ui::HexColorEdit(line);
        m_seedHex->setFixedWidth(96);
        m_seedHex->setAccessibleName(tr("기준 색 16진수 값"));
        ll->addWidget(m_accentPicker);
        ll->addWidget(m_followCheck);
        ll->addStretch(1);
        ll->addWidget(m_pickSeed);
        ll->addWidget(m_seedHex);
        layout->addWidget(line);

        m_derivedRows = new DerivedRows(ui->seedEditor);
        layout->addWidget(m_derivedRows);

        // 연동 안내(강조색만)
        m_also = new RuledRow(true, false, ui->seedEditor);
        auto *al = new QHBoxLayout(m_also);
        al->setContentsMargins(0, 6, 0, 0);
        al->setSpacing(6);
        auto *arrow = new QLabel(m_also);
        arrow->setFixedSize(12, 12);
        m_alsoArrow = arrow;
        al->addWidget(arrow);
        al->addWidget(new fm::ui::Label(tr("선택 · 역상 선택 · 역상 커서 글자에도 반영"), fm::ui::Label::Meta, m_also));
        al->addStretch(1);
        m_bothCheck = new QCheckBox(tr("라이트 · 다크 함께(&B)"), m_also);
        al->addWidget(m_bothCheck);
        layout->addWidget(m_also);

        connect(m_roleReset, &QPushButton::clicked, this, [this] { resetRole(m_role); });
        connect(m_accentPicker, &fm::ui::AccentPicker::accentChosen, this,
                [this](const std::optional<QColor> &c) { setRoleSeed(fs::SeedRole::Accent, c); });
        connect(m_accentPicker, &fm::ui::AccentPicker::customRequested, this, [this] {
            if (const auto c = pickColor(fs::seedColor(fs::SeedRole::Accent, m_derived.colors, seeds()), tr("강조색")))
                setRoleSeed(fs::SeedRole::Accent, c);
        });
        connect(m_followCheck, &QCheckBox::toggled, this, [this](bool on) {
            const fs::SeedRole role = m_role;
            const QColor current = m_derived.colors[fs::seedRoleInfo(role).seedToken()];
            editScheme([&](fs::ColorScheme &, fs::VariantSeeds &vs, fs::TokenOverrides &) {
                bool &follows = role == fs::SeedRole::Sel ? vs.selFollowsAccent : vs.invSelFollowsAccent;
                follows = on;
                if (on)
                    vs.seed[std::size_t(role)].reset();
                else
                    vs.seed[std::size_t(role)] = current;  // 끄면 지금 색에서 독립 시드로 시작
            });
        });
        connect(m_pickSeed, &QPushButton::clicked, this, [this] {
            if (const auto c = pickColor(fs::seedColor(m_role, m_derived.colors, seeds()), QString::fromUtf8(fs::seedRoleInfo(m_role).label)))
                setRoleSeed(m_role, c);
        });
        connect(m_seedHex, &fm::ui::HexColorEdit::colorEdited, this, [this](const QColor &c) { setRoleSeed(m_role, c); });
        connect(m_bothCheck, &QCheckBox::toggled, this, [this](bool on) {
            session()->edit(Section::Theme, [&](AppSettings &p) { p.theme.editBothVariants = on; });
        });
    }

    void syncSeedEditor()
    {
        const fs::SeedRoleInfo &info = fs::seedRoleInfo(m_role);
        const fs::ThemeColors &c = m_derived.colors;
        const QColor seed = fs::seedColor(m_role, c, seeds());
        const bool isAccent = m_role == fs::SeedRole::Accent;
        const bool follows = info.followsAccent;
        const fs::VariantSeeds &vs = variantSeeds();
        const bool following = (m_role == fs::SeedRole::Sel && vs.selFollowsAccent) || (m_role == fs::SeedRole::InvSel && vs.invSelFollowsAccent);
        m_seedSwatch->setSwatch(info.seedToken(), seed, QColor());
        m_seedName->setText(QString::fromUtf8(info.label));
        m_seedDescription->setText(tr("%1 · 토큰 %2개").arg(QString::fromUtf8(info.description)).arg(info.tokenCount));
        m_roleReset->setEnabled(roleDirty(m_role));
        m_accentPicker->setVisible(isAccent);
        {
            const QSignalBlocker block(m_accentPicker);
            const auto &variantAccent = vs.seed[std::size_t(fs::SeedRole::Accent)];
            m_accentPicker->setCurrent(variantAccent ? variantAccent : seeds().accent);
        }
        m_followCheck->setVisible(follows);
        {
            const QSignalBlocker block(m_followCheck);
            m_followCheck->setChecked(following);
        }
        m_pickSeed->setVisible(!isAccent);
        m_pickSeed->setEnabled(!following);
        m_seedHex->setEnabled(!following);
        {
            const QSignalBlocker block(m_seedHex);
            m_seedHex->setColor(seed);
        }
        QList<DerivedRows::Row> rows;
        const fs::Variant v = editVariant();
        for (int i = 0; i < info.tokenCount; ++i) {
            const T t = info.tokens[std::size_t(i)];
            const bool ovr = isOverridden(t);
            DerivedRows::Row row;
            row.token = t;
            row.color = c[t];
            row.base = compositeBase(t, c);
            row.value = fs::tokenDisplayValue(t, c[t]);
            row.rule = ovr ? tr("직접 지정한 값 유지") : fs::tokenSource(t, design()).rule(v);
            row.tag = ovr ? 1 : m_derived.adjusted.test(std::size_t(t)) ? 2 : 0;
            rows.append(row);
        }
        m_derivedRows->setRows(rows);
        m_also->setVisible(isAccent);
        {
            const QSignalBlocker block(m_bothCheck);
            m_bothCheck->setChecked(pending().theme.editBothVariants);
        }
        const fs::ThemeColors &tc = fs::themeColorsFor(this);
        m_alsoArrow->setPixmap(fs::glyphIcon(fs::Glyph::ArrowRight, tc[T::AccentFg], 12).pixmap(12, 12));
        refreshStackGeometry(ui->seedEditor);
    }

    // ---------------------------------------------------------------- 모든 토큰 보기

    void buildTokenTree()
    {
        m_treeModel = new QStandardItemModel(this);
        for (fs::TokenGroup g : {fs::TokenGroup::Surfaces, fs::TokenGroup::Text, fs::TokenGroup::AccentSelection, fs::TokenGroup::Inverse,
                                 fs::TokenGroup::RecordTint, fs::TokenGroup::Status, fs::TokenGroup::Elevation, fs::TokenGroup::FileIcons,
                                 fs::TokenGroup::Effects}) {
            auto *group = new QStandardItem(groupLabel(g));
            group->setData(GroupRow, KindRole);
            group->setData(int(g), GroupRole);
            group->setFlags(Qt::ItemIsEnabled);
            int count = 0;
            for (const fs::TokenInfo &t : fs::allTokens()) {
                if (t.group != g)
                    continue;
                auto *item = new QStandardItem(QString::fromUtf8(t.label));
                item->setData(TokenRow, KindRole);
                item->setData(int(t.token), TokenRole);
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                group->appendRow(item);
                ++count;
            }
            group->setData(QString::number(count), CountRole);
            m_treeModel->appendRow(group);
        }
        QTreeView *tree = ui->tokenTree;
        tree->setModel(m_treeModel);
        tree->setItemDelegate(new TokenTreeDelegate(tree));
        tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tree->setSelectionMode(QAbstractItemView::SingleSelection);
        tree->setExpandsOnDoubleClick(false);
        tree->setMinimumHeight(560);
        m_expanded.insert(int(fs::TokenGroup::AccentSelection));
        connect(tree, &QTreeView::clicked, this, [this](const QModelIndex &index) {
            if (index.data(KindRole).toInt() != GroupRow)
                return;
            const int g = index.data(GroupRole).toInt();
            if (m_expanded.contains(g))
                m_expanded.remove(g);
            else
                m_expanded.insert(g);
            applyTreeFilter();
        });
        connect(tree->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &index) {
            if (m_syncing || index.data(KindRole).toInt() != TokenRow)
                return;
            m_token = T(index.data(TokenRole).toInt());
            syncTokenEditor();
        });
        connect(ui->tokenSearch, &QLineEdit::textChanged, this, [this] { applyTreeFilter(); });
        connect(ui->filterSegment, &fm::ui::SegmentedControl::currentIndexChanged, this, [this](int i) {
            m_filter = i;
            applyTreeFilter();
        });
    }

    /// 토큰 칩: 직접 지정 · 개별 · 강조 연동(선택 · 역상 선택 시드) · 기준 · {역할} · 자동 · {역할}.
    std::pair<QString, K> tokenChip(T t) const
    {
        if (isOverridden(t))
            return {tr("직접 지정"), K::Override};
        const fs::TokenSource src = fs::tokenSource(t, design());
        if (!src.role)
            return {tr("개별"), K::Base};
        const fs::SeedRoleInfo &info = fs::seedRoleInfo(*src.role);
        const QString role = QString::fromUtf8(info.label);
        if (src.isSeed && info.followsAccent)
            return {tr("강조 연동"), K::Link};
        if (src.isSeed)
            return {tr("기준 · %1").arg(role), K::Base};
        return {tr("자동 · %1").arg(role), K::Link};
    }

    void syncTokenTree()
    {
        const fs::ThemeColors &c = m_derived.colors;
        m_syncing = true;
        for (int g = 0; g < m_treeModel->rowCount(); ++g) {
            QStandardItem *group = m_treeModel->item(g);
            int changed = 0;
            for (int r = 0; r < group->rowCount(); ++r) {
                QStandardItem *item = group->child(r);
                const T t = T(item->data(TokenRole).toInt());
                const auto [chip, kind] = tokenChip(t);
                item->setData(c[t], ColorRole);
                item->setData(compositeBase(t, c), BaseColorRole);
                item->setData(fs::tokenDisplayValue(t, c[t]), HexRole);
                item->setData(chip, ChipRole);
                item->setData(int(kind), ChipKindRole);
                item->setData(u"%1 %2, %3, %4"_s.arg(item->text(), fs::tokenCssName(t), fs::tokenDisplayValue(t, c[t]), chip),
                              Qt::AccessibleTextRole);
                if (m_changed.test(std::size_t(t)))
                    ++changed;
            }
            group->setData(changed > 0 ? tr("바뀜 %1").arg(changed) : QString(), ChipRole);
        }
        m_syncing = false;
        // 거르기 개수는 세그먼트 글자에 실시간 반영
        const QStringList items{tr("전체 %1").arg(fs::kTokenCount), tr("바뀜 %1").arg(m_changed.count()),
                                tr("대비 경고 %1").arg(m_contrastFail.count())};
        if (ui->filterSegment->items() != items) {
            const QSignalBlocker block(ui->filterSegment);
            ui->filterSegment->setItems(items);
            ui->filterSegment->setCurrentIndex(m_filter);
        }
        applyTreeFilter();
    }

    void applyTreeFilter()
    {
        const QString query = ui->tokenSearch->text().trimmed().toLower();
        const bool filtering = m_filter != 0 || !query.isEmpty();
        QTreeView *tree = ui->tokenTree;
        m_syncing = true;
        for (int g = 0; g < m_treeModel->rowCount(); ++g) {
            QStandardItem *group = m_treeModel->item(g);
            int visible = 0;
            for (int r = 0; r < group->rowCount(); ++r) {
                const T t = T(group->child(r)->data(TokenRole).toInt());
                bool show = true;
                if (m_filter == 1)
                    show = m_changed.test(std::size_t(t));
                else if (m_filter == 2)
                    show = m_contrastFail.test(std::size_t(t));
                if (show && !query.isEmpty())
                    show = fs::tokenLabel(t).toLower().contains(query) || fs::tokenCssName(t).toLower().contains(query)
                         || fs::tokenUsage(t).toLower().contains(query);
                tree->setRowHidden(r, group->index(), !show);
                visible += show ? 1 : 0;
            }
            tree->setRowHidden(g, QModelIndex(), filtering && visible == 0);
            const bool open = filtering || m_expanded.contains(group->data(GroupRole).toInt());
            tree->setExpanded(group->index(), open);
        }
        // 선택 토큰
        for (int g = 0; g < m_treeModel->rowCount(); ++g) {
            QStandardItem *group = m_treeModel->item(g);
            for (int r = 0; r < group->rowCount(); ++r) {
                if (T(group->child(r)->data(TokenRole).toInt()) == m_token) {
                    tree->selectionModel()->setCurrentIndex(group->child(r)->index(), QItemSelectionModel::ClearAndSelect);
                    if (tree->isVisible())
                        tree->scrollTo(group->child(r)->index());
                }
            }
        }
        m_syncing = false;
        tree->viewport()->update();
    }

    // ---------------------------------------------------------------- 토큰 편집기

    void buildTokenEditor()
    {
        QVBoxLayout *layout = ui->tokenEditorLayout;
        auto *head = new QHBoxLayout();
        head->setSpacing(12);
        m_tokenSwatch = new SwatchLabel(ui->tokenEditor);
        auto *names = new QVBoxLayout();
        names->setSpacing(0);
        m_tokenName = new fm::ui::Label(QString(), fm::ui::Label::Heading, ui->tokenEditor);
        m_tokenCss = new fm::ui::Label(QString(), fm::ui::Label::PathMeta, ui->tokenEditor);
        names->addWidget(m_tokenName);
        names->addWidget(m_tokenCss);
        m_tokenChip = new ChipWidget(ui->tokenEditor);
        head->addWidget(m_tokenSwatch);
        head->addLayout(names, 1);
        head->addWidget(m_tokenChip);
        layout->addLayout(head);

        auto *kv = new RuledRow(true, false, ui->tokenEditor);
        auto *grid = new QGridLayout(kv);
        grid->setContentsMargins(0, 8, 0, 0);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(6);
        grid->setColumnMinimumWidth(0, 64);
        grid->setColumnStretch(1, 1);
        auto addKey = [&](int row, const QString &text) {
            auto *label = new fm::ui::Label(text, fm::ui::Label::Meta, kv);
            label->setMinimumHeight(30);
            grid->addWidget(label, row, 0, Qt::AlignTop);
        };
        addKey(0, tr("값"));
        auto *valueRow = new QHBoxLayout();
        valueRow->setSpacing(8);
        m_tokenHex = new fm::ui::HexColorEdit(kv);
        m_tokenHex->setFixedWidth(96);
        m_tokenHex->setAccessibleName(tr("토큰 16진수 값"));
        m_pickToken = new fm::ui::Button(tr("색 선택…(&C)"), kv);
        m_pickToken->setCompact(true);
        m_pickToken->setAutoDefault(false);
        valueRow->addWidget(m_tokenHex);
        valueRow->addWidget(m_pickToken);
        valueRow->addStretch(1);
        grid->addLayout(valueRow, 0, 1);
        addKey(1, tr("Qt 역할"));
        m_tokenQtRole = new fm::ui::Label(QString(), fm::ui::Label::PathMeta, kv);
        grid->addWidget(m_tokenQtRole, 1, 1, Qt::AlignVCenter);
        addKey(2, tr("쓰는 곳"));
        m_tokenUsage = new fm::ui::Label(QString(), fm::ui::Label::Help, kv);
        grid->addWidget(m_tokenUsage, 2, 1, Qt::AlignVCenter);
        addKey(3, tr("규칙"));
        m_tokenRule = new fm::ui::Label(QString(), fm::ui::Label::Help, kv);
        grid->addWidget(m_tokenRule, 3, 1, Qt::AlignVCenter);
        layout->addWidget(kv);

        auto *buttons = new RuledRow(true, false, ui->tokenEditor);
        auto *bl = new QHBoxLayout(buttons);
        bl->setContentsMargins(0, 8, 0, 0);
        bl->setSpacing(8);
        m_overrideButton = new fm::ui::Button(tr("직접 지정(&D)"), buttons);
        m_autoButton = new fm::ui::Button(tr("자동으로 되돌리기(&U)"), buttons);
        m_editRoleButton = new fm::ui::Button(QString(), buttons);
        for (fm::ui::Button *b : {m_overrideButton, m_autoButton, m_editRoleButton}) {
            b->setCompact(true);
            b->setAutoDefault(false);
            bl->addWidget(b);
        }
        bl->addStretch(1);
        m_variantOnly = new fm::ui::Label(QString(), fm::ui::Label::Minor, buttons);
        bl->addWidget(m_variantOnly);
        layout->addWidget(buttons);

        connect(m_tokenHex, &fm::ui::HexColorEdit::colorEdited, this, [this](const QColor &c) { setTokenValue(m_token, c); });
        connect(m_pickToken, &QPushButton::clicked, this, [this] {
            if (const auto c = pickColor(m_derived.colors[m_token], fs::tokenLabel(m_token)))
                setTokenValue(m_token, *c);
        });
        connect(m_overrideButton, &QPushButton::clicked, this, [this] { setOverride(m_token, m_derived.colors[m_token]); });
        connect(m_autoButton, &QPushButton::clicked, this, [this] { setOverride(m_token, std::nullopt); });
        connect(m_editRoleButton, &QPushButton::clicked, this, [this] {
            if (const auto role = fs::roleOfToken(m_token)) {
                m_role = *role;
                setView(false);
            }
        });
    }

    void syncTokenEditor()
    {
        const T t = m_token;
        const fs::ThemeColors &c = m_derived.colors;
        const fs::TokenInfo &info = fs::tokenInfo(t);
        const fs::TokenSource src = fs::tokenSource(t, design());
        const bool ovr = isOverridden(t);
        m_tokenSwatch->setSwatch(t, c[t], compositeBase(t, c));
        m_tokenName->setText(fs::tokenLabel(t));
        m_tokenCss->setText(fs::tokenCssName(t));
        const auto [chip, kind] = tokenChip(t);
        m_tokenChip->setChip(chip, kind);
        {
            const QSignalBlocker block(m_tokenHex);
            m_tokenHex->setColor(c[t]);
        }
        m_tokenQtRole->setText(info.qtRole ? u"QPalette::"_s + QString::fromLatin1(info.qtRole) : u"—"_s);
        m_tokenUsage->setText(fs::tokenUsage(t));
        QString rule;
        if (ovr)
            rule = tr("직접 지정 — 기준 색을 바꿔도 이 값은 유지");
        else if (!src.role)
            rule = tr("개별 지정 — 다른 색에 영향을 주지도 받지도 않음");
        else if (src.isSeed)
            rule = tr("기준 색 ‘%1’ 그 자체").arg(QString::fromUtf8(fs::seedRoleInfo(*src.role).label));
        else
            rule = tr("‘%1’에서 자동 — %2").arg(QString::fromUtf8(fs::seedRoleInfo(*src.role).label), src.rule(editVariant()));
        m_tokenRule->setText(rule);
        m_overrideButton->setVisible(src.role && !src.isSeed && !ovr);
        m_autoButton->setVisible(ovr);
        m_editRoleButton->setVisible(src.role.has_value());
        if (src.role)
            m_editRoleButton->setText(tr("기준 색 ‘%1’ 편집").arg(QString::fromUtf8(fs::seedRoleInfo(*src.role).label)));
        m_variantOnly->setText(m_editDark ? tr("다크만") : tr("라이트만"));
        refreshStackGeometry(ui->tokenEditor);
    }

    // ---------------------------------------------------------------- 미리보기

    void buildPreview()
    {
        m_previewBox = new QWidget(ui->previewCard);
        auto *layout = new QVBoxLayout(m_previewBox);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        ui->previewLayout->addWidget(m_previewBox);

        // (1) 행 상태 — 보통 · 선택 · 커서 · 역상 커서 · 선택 + 역상 커서 · 역상 선택
        struct Sample
        {
            const char *stem;
            const char *ext;
            fl::Kind kind;
            int state;
            const char *label;
        };
        const Sample samples[] = {
            {"Projects", "", fl::Kind::Folder, 0, "보통"},
            {"qt-online-installer-6.11", "exe", fl::Kind::Exe, fl::PreviewMarked, "선택"},
            {"release-notes", "md", fl::Kind::Doc, fl::PreviewCursor, "커서"},
            {"IMG_2041", "heic", fl::Kind::Img, fl::PreviewCursor | fl::PreviewInvertCursor, "역상 커서"},
            {"backup-0927", "zip", fl::Kind::Zip, fl::PreviewMarked | fl::PreviewCursor | fl::PreviewInvertCursor, "선택 + 역상 커서"},
            {"MainWindow", "cpp", fl::Kind::Code, fl::PreviewMarked | fl::PreviewInvertSelection, "역상 선택"},
        };
        auto *model = new QStandardItemModel(int(std::size(samples)), 3, this);
        for (int r = 0; r < int(std::size(samples)); ++r) {
            const Sample &s = samples[r];
            const QString stem = QString::fromUtf8(s.stem), ext = QString::fromUtf8(s.ext);
            for (int col = 0; col < 3; ++col) {
                auto *item = new QStandardItem();
                item->setData(stem, fl::StemRole);
                item->setData(ext, fl::ExtRole);
                item->setData(ext.isEmpty() ? stem : stem + u'.' + ext, fl::FullNameRole);
                item->setData(int(s.kind), fl::KindRole);
                item->setData(s.kind == fl::Kind::Folder, fl::IsDirRole);
                item->setData(false, fl::IsUpRole);
                item->setData(false, fl::HiddenRole);
                item->setData(bool(s.state & fl::PreviewMarked), fl::MarkedRole);
                item->setData(s.state, fl::PreviewStateRole);
                if (col == 1)
                    item->setText(ext.isEmpty() ? stem : stem + u'.' + ext);
                if (col == 2)
                    item->setText(QString::fromUtf8(s.label));
                model->setItem(r, col, item);
            }
        }
        m_rowStates = new fl::FileListView(m_previewBox);
        m_rowStates->setObjectName(u"themeRowStates"_s);
        m_rowStates->setPreviewMode(true);
        m_rowStates->setColumnLayout(rowStateLayout());
        m_rowStates->setModel(model);
        m_rowStates->setViewMode(fl::ViewMode::OneLine);
        m_rowStates->setPaneActive(true);
        m_rowStates->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_rowStates->setFixedHeight(m_rowStates->preferredHeight(model->rowCount()));
        layout->addWidget(m_rowStates);

        // (2) 단추 줄
        auto *buttons = new QWidget(m_previewBox);
        auto *bl = new QHBoxLayout(buttons);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(8);
        auto *primary = new fm::ui::Button(tr("기본 버튼"), buttons);
        primary->setRole(fm::ui::Button::Primary);
        auto *secondary = new fm::ui::Button(tr("보조"), buttons);
        auto *danger = new fm::ui::Button(tr("영구 삭제"), buttons);
        danger->setRole(fm::ui::Button::Danger);
        for (fm::ui::Button *b : {primary, secondary, danger}) {
            b->setCompact(true);
            b->setAutoDefault(false);
            b->setFocusPolicy(Qt::NoFocus);
            bl->addWidget(b);
        }
        auto *link = new fm::ui::Label(tr("강조 글자 · 링크"), fm::ui::Label::Summary, buttons);
        link->setTone(fm::ui::Label::Accent);
        QFont lf = link->font();
        lf.setUnderline(true);
        link->setFont(lf);
        bl->addWidget(link);
        bl->addStretch(1);
        buttons->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(buttons);

        // (3) 상태 줄 — 진행 막대 2개 · 태그 3개
        auto *status = new QWidget(m_previewBox);
        auto *sl = new QHBoxLayout(status);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->setSpacing(8);
        auto *running = new fm::ui::ProgressBar(status);
        running->setValue(62);
        auto *paused = new fm::ui::ProgressBar(status);
        paused->setValue(38);
        paused->setState(fm::ui::ProgressBar::Paused);
        for (fm::ui::ProgressBar *bar : {running, paused}) {
            bar->setThickness(fm::ui::ProgressBar::ThinBar);
            bar->setMinimumWidth(40);
            sl->addWidget(bar, 1);
        }
        for (auto [text, tone] : {std::pair{tr("정상"), fm::ui::Tag::Ok}, std::pair{tr("권한 필요"), fm::ui::Tag::Warn},
                                  std::pair{tr("충돌"), fm::ui::Tag::Danger}}) {
            auto *tag = new fm::ui::Tag(text, tone, status);
            tag->setCompact(true);
            sl->addWidget(tag);
        }
        status->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(status);

        // (4) 대비 검사
        auto *contrast = new RuledRow(true, false, m_previewBox);
        auto *cl = new QVBoxLayout(contrast);
        cl->setContentsMargins(0, 8, 0, 0);
        cl->setSpacing(4);
        auto *ch = new QHBoxLayout();
        ch->setSpacing(8);
        ch->addWidget(new fm::ui::Label(tr("대비 검사"), fm::ui::Label::SectionTitle, contrast));
        ch->addWidget(new fm::ui::Label(tr("본문 글자 기준 4.5 : 1"), fm::ui::Label::Minor, contrast));
        ch->addStretch(1);
        m_fixSwitch = new fm::ui::Switch(contrast);
        m_fixSwitch->setText(tr("낮으면 자동 보정"));
        m_fixSwitch->setOnText(tr("낮으면 자동 보정"));
        m_fixSwitch->setOffText(tr("낮으면 자동 보정"));
        ch->addWidget(m_fixSwitch);
        cl->addLayout(ch);
        m_contrast = new ContrastGrid(contrast);
        cl->addWidget(m_contrast);
        layout->addWidget(contrast);

        bindCheck(m_fixSwitch, Section::Theme, [](const AppSettings &p) { return p.theme.scheme.seeds.fixContrast; },
                  [](AppSettings &p, bool on) { p.theme.scheme.seeds.fixContrast = on; });
    }

    void syncPreview()
    {
        fs::ThemeScope::set(m_previewBox, m_derived.colors);
        m_contrast->setResults(fs::checkContrast(m_derived.colors), m_derived.adjusted);
    }

    std::unique_ptr<Ui::SettingsThemePage> ui;
    QWidget *m_header = nullptr;
    fm::ui::SegmentedControl *m_variantSegment = nullptr;
    QList<fs::ColorScheme> m_saved;
    fs::DerivedTheme m_derived;
    std::bitset<fs::kTokenCount> m_changed;
    std::bitset<fs::kTokenCount> m_contrastFail;
    bool m_editDark = false;
    bool m_syncing = false;
    fs::SeedRole m_role = fs::SeedRole::Accent;
    T m_token = T::AccentFg;
    int m_filter = 0;
    QSet<int> m_expanded;

    QStandardItemModel *m_seedModel = nullptr;
    IndividualRow *m_iconsRow = nullptr;
    IndividualRow *m_miscRow = nullptr;

    SwatchLabel *m_seedSwatch = nullptr;
    fm::ui::Label *m_seedName = nullptr;
    fm::ui::Label *m_seedDescription = nullptr;
    fm::ui::Button *m_roleReset = nullptr;
    fm::ui::AccentPicker *m_accentPicker = nullptr;
    QCheckBox *m_followCheck = nullptr;
    fm::ui::Button *m_pickSeed = nullptr;
    fm::ui::HexColorEdit *m_seedHex = nullptr;
    DerivedRows *m_derivedRows = nullptr;
    RuledRow *m_also = nullptr;
    QLabel *m_alsoArrow = nullptr;
    QCheckBox *m_bothCheck = nullptr;

    QStandardItemModel *m_treeModel = nullptr;
    SwatchLabel *m_tokenSwatch = nullptr;
    fm::ui::Label *m_tokenName = nullptr;
    fm::ui::Label *m_tokenCss = nullptr;
    ChipWidget *m_tokenChip = nullptr;
    fm::ui::HexColorEdit *m_tokenHex = nullptr;
    fm::ui::Button *m_pickToken = nullptr;
    fm::ui::Label *m_tokenQtRole = nullptr;
    fm::ui::Label *m_tokenUsage = nullptr;
    fm::ui::Label *m_tokenRule = nullptr;
    fm::ui::Button *m_overrideButton = nullptr;
    fm::ui::Button *m_autoButton = nullptr;
    fm::ui::Button *m_editRoleButton = nullptr;
    fm::ui::Label *m_variantOnly = nullptr;

    QWidget *m_previewBox = nullptr;
    fl::FileListView *m_rowStates = nullptr;
    fm::ui::Switch *m_fixSwitch = nullptr;
    ContrastGrid *m_contrast = nullptr;
};

} // namespace

SettingsPage *createThemePage(SettingsSession *session, QWidget *parent)
{
    return new ThemePage(session, parent);
}

} // namespace fm::dialogs
