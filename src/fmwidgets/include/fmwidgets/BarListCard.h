#pragma once

// 가로 누적 막대 목록 카드 — 줄마다 코드 · 이름 · 누적 막대 · 수 · 배지, 아래에 범례. 직접 그리고, 색은 그릴 때 테마에서 읽는다.
//   줄   코드(고정폭 11.5 px, --fg3) · 이름(12.5 px, 말줄임) · 막대(높이 12, 들어간 홈) · 수(숫자 폭 고정, --fg2) · 배지(경고색)
//   막대 구간의 길이는 가장 긴 줄(또는 maximum)에 견준 비율이다 — 줄끼리 크기를 견줄 수 있다.
//   범례 구간 이름 앞에 10 px 색 상자, 끝에 배지 설명(badgeLegend).

#include "fmwidgets/Card.h"

#include <fmstyle/ThemeColors.h>

#include <QColor>
#include <QList>
#include <QString>

namespace fm::ui {

class BarListCard : public Card
{
    Q_OBJECT
    Q_PROPERTY(int codeWidth READ codeWidth WRITE setCodeWidth)
    Q_PROPERTY(int labelWidth READ labelWidth WRITE setLabelWidth)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(bool legendVisible READ isLegendVisible WRITE setLegendVisible)
    Q_PROPERTY(QString badgeLegend READ badgeLegend WRITE setBadgeLegend)

public:
    /// 막대의 구간 하나 — 색은 테마 토큰(strength < 1 이면 바탕 쪽으로 옅게). color를 주면 그 색을 그대로 쓴다.
    struct Series
    {
        QString name;
        fm::style::Token token = fm::style::Token::Accent;
        qreal strength = 1.0;
        QColor color;
    };

    struct Row
    {
        QString code;           // 고정폭 · --fg3(비면 칸만 비운다)
        QString label;          // 말줄임 — 잘리면 도구 설명으로 전부
        QList<double> values;   // 구간 값 — series 순서. 모자라면 0
        QString countText;      // 비면 값의 합
        QString badgeText;      // 수 뒤 작은 배지(비면 없음)
        QString toolTip;        // 줄에 마우스를 올렸을 때(비면 이름 · 구간 값)
    };

    explicit BarListCard(QWidget *parent = nullptr);

    void setSeries(const QList<Series> &series);
    QList<Series> series() const { return m_series; }

    void addRow(const Row &row);
    void setRows(const QList<Row> &rows);
    QList<Row> rows() const { return m_rows; }
    void clear();

    /// 코드 칸 폭(기본 34) — 더 긴 코드가 있으면 그만큼 늘린다.
    int codeWidth() const noexcept { return m_codeWidth; }
    void setCodeWidth(int width);
    /// 이름 칸 폭(기본 132) — 넘치면 말줄임.
    int labelWidth() const noexcept { return m_labelWidth; }
    void setLabelWidth(int width);
    /// 막대 끝이 뜻하는 값. 0(기본)이면 가장 긴 줄의 합.
    double maximum() const noexcept { return m_maximum; }
    void setMaximum(double maximum);

    bool isLegendVisible() const noexcept { return m_legendVisible; }
    void setLegendVisible(bool visible);
    /// 범례 끝의 배지 설명 — "n" 배지 + 이 글자(비면 넣지 않는다).
    QString badgeLegend() const { return m_badgeLegend; }
    void setBadgeLegend(const QString &text);

    /// 줄 하나가 놓인 자리(막대 칸) — 자기검사 · 도구 설명용.
    QRectF barRect(int row) const;
    /// 구간 하나의 그린 폭(px).
    qreal segmentWidth(int row, int series) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    struct Columns
    {
        qreal code, label, bar, count, right;  // 각 칸의 왼쪽 x(right = 오른쪽 끝)
        qreal barWidth;
    };
    Columns columns() const;
    int effectiveCodeWidth() const;
    int countColumnWidth() const;
    double effectiveMaximum() const;
    double rowTotal(const Row &row) const;
    QString countOf(const Row &row) const;
    QColor seriesColor(int index, const fm::style::ThemeColors &tc) const;
    int legendHeight() const;
    void changed();
    void refreshAccessibility();

    QList<Series> m_series;
    QList<Row> m_rows;
    int m_codeWidth = 34;
    int m_labelWidth = 132;
    double m_maximum = 0.0;
    bool m_legendVisible = true;
    QString m_badgeLegend;
};

} // namespace fm::ui
