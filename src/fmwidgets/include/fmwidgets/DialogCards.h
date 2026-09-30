#pragma once

// 권한 대화상자 등의 카드 부품(docs/specs/03 §1.5 · §1.6 · §1.8 · §1.11) — 직접 그리고, 색은 그릴 때 테마에서 읽는다.
//   KeyValueCard  키-값 카드(dl.card.kv) — 라벨 열 88, 행 20 · 간격 6, 값: 아이콘 · 이름(말줄임) · 보조 글자 또는 태그
//   ItemListCard  28 px 행 목록 카드 — 자물쇠 · 경로(고정폭) · 오른쪽 태그, 넘치면 "… 외 N개"
//   ActionCard    대안 동작 카드(a.alt) — 폴더 20 · 두 줄 글(굵은 제목 + 고정폭 경로) · 꺾쇠, 누르는 단추
//   OptionRadio   설명 있는 라디오 — 첫 줄 라디오 + 12 px 설명(들여쓰기 24), 설명을 눌러도 선택

#include "fmwidgets/Card.h"
#include "fmwidgets/Glyph.h"
#include "fmwidgets/Tag.h"

#include <fmstyle/ThemeTokens.h>

#include <QAbstractButton>
#include <QList>
#include <QWidget>

class QRadioButton;

namespace fm::ui {

class Label;

/// 작은 아이콘 — 글리프와 두 색 토큰(File은 둘째 색이 종류 띠, Shield는 방패 두 색을 쓴다).
struct IconSpec
{
    glyph::Glyph glyph = glyph::None;
    fm::style::Token primary = fm::style::Token::Fg3;
    fm::style::Token secondary = fm::style::Token::KDoc;

    static IconSpec file(fm::style::Token kind) { return {glyph::File, fm::style::Token::Fg3, kind}; }
    static IconSpec folder() { return {glyph::Folder, fm::style::Token::Folder, fm::style::Token::Folder}; }
    bool isNull() const noexcept { return glyph == glyph::None; }
    bool operator==(const IconSpec &) const = default;
};

class KeyValueCard : public Card
{
    Q_OBJECT
    Q_PROPERTY(int labelWidth READ labelWidth WRITE setLabelWidth)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    enum ValueStyle { Normal, Mono, Strong };
    Q_ENUM(ValueStyle)

    struct Row
    {
        QString label;                         // dt — --fg3
        QString value;                         // dd 이름(말줄임)
        ValueStyle style = Normal;             // Mono = 고정폭 12.5 px(경로), Strong = 600
        IconSpec icon;                         // 16 px, 이름 앞
        QString trailing;                      // 이름 바로 뒤 보조 글자 — --fg3, 숫자 폭 고정("24.4 MB")
        QString tagText;                       // 값 대신 태그(비어 있지 않으면 value 무시)
        Tag::Tone tagTone = Tag::Mute;
        Qt::TextElideMode elide = Qt::ElideRight;
    };

    explicit KeyValueCard(QWidget *parent = nullptr);

    void addRow(const Row &row);
    void addRow(const QString &label, const QString &value, ValueStyle style = Normal);
    void setRows(const QList<Row> &rows);
    QList<Row> rows() const { return m_rows; }
    void clear();

    int labelWidth() const noexcept { return m_labelWidth; }
    void setLabelWidth(int width);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    struct Placed
    {
        QRectF name;        // 이름이 놓인 칸(말줄임 판정 · 도구 설명)
        QString shown;
    };
    QList<Placed> place() const;
    int effectiveLabelWidth() const;
    void refreshAccessibility();

    QList<Row> m_rows;
    int m_labelWidth = 88;
};

class ItemListCard : public Card
{
    Q_OBJECT
    Q_PROPERTY(int maxVisibleRows READ maxVisibleRows WRITE setMaxVisibleRows)

public:
    struct Item
    {
        QString text;                          // 고정폭 12.5 px, 말줄임
        QString tagText;                       // 오른쪽 태그(비면 없음)
        Tag::Tone tagTone = Tag::Warn;
        glyph::Glyph glyph = glyph::Lock;      // 14 px, --fg3
    };

    explicit ItemListCard(QWidget *parent = nullptr);

    void addItem(const Item &item);
    void setItems(const QList<Item> &items);
    QList<Item> items() const { return m_items; }
    void clear();

    /// 이보다 많으면 (maxVisibleRows − 1)행 + "… 외 N개".
    int maxVisibleRows() const noexcept { return m_maxRows; }
    void setMaxVisibleRows(int rows);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int visibleRows() const;
    void refreshAccessibility();

    QList<Item> m_items;
    int m_maxRows = 5;
};

class ActionCard : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(QString detail READ detail WRITE setDetail)
    Q_PROPERTY(fm::ui::glyph::Glyph glyph READ glyph WRITE setGlyph)

public:
    explicit ActionCard(QWidget *parent = nullptr);

    /// 둘째 줄(고정폭 12 px, --fg3, 말줄임) — "%LOCALAPPDATA%\FM Tools\plugins".
    QString detail() const { return m_detail; }
    void setDetail(const QString &detail);

    /// 왼쪽 20 px 아이콘(기본 폴더).
    glyph::Glyph glyph() const noexcept { return m_icon.glyph; }
    void setGlyph(glyph::Glyph glyph);
    IconSpec icon() const { return m_icon; }
    void setIcon(const IconSpec &icon);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_detail;
    IconSpec m_icon = IconSpec::folder();
};

class OptionRadio : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QString description READ description WRITE setDescription)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled)

public:
    explicit OptionRadio(QWidget *parent = nullptr);
    OptionRadio(const QString &text, const QString &description, QWidget *parent = nullptr);

    QString text() const;
    void setText(const QString &text);
    QString description() const;
    void setDescription(const QString &description);
    bool isChecked() const;
    void setChecked(bool checked);

    QRadioButton *radio() const noexcept { return m_radio; }

Q_SIGNALS:
    void toggled(bool checked);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QRadioButton *m_radio = nullptr;
    Label *m_description = nullptr;
};

} // namespace fm::ui
