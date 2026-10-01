#pragma once

// 설정 창 부품(docs/specs/04 §3.4 · 05 §3.4) — 두 디자인에서 치수는 같고 색 · 모서리는 테마를 따른다.
//   SettingRow       설정 행(이름 · 설명 · 기본값과 다름 점 · 오른쪽 컨트롤) — .ui에서 자식 위젯을 오른쪽 칸으로 옮겨 담는 컨테이너
//   SearchField      돋보기 검색 상자(Enter · Esc를 먹는다 — 대화상자 기본 단추로 새지 않게)
//   ThemeModeCard    테마 카드(시스템 · 라이트 · 다크 그림 + 라디오 + 이름)
//   ColorSwatchButton / AccentPicker   원형 강조색 견본 · 견본 묶음(＋ 직접 선택, 사용자 지정 견본)
//   HexColorEdit     16진수 색 입력(#RRGGBB · #AARRGGBB)
//   ToggleChip       켜고 끄는 작은 단추(글꼴 효과)
//   ColorPickButton  색 단추(견본 + 16진수 + ▾, 팝업: 추천 색 · 지정 안 함 · 사용자 지정…)
//   CheckListCombo   체크 목록 콤보(선택 항목을 " · "로 이어 표시)
//   KeyCaptureEdit   단축키 입력 칸(누른 키 칩 + 캡션)
// 그리기 도우미: paintChip(태그 · 배지 · 알약), paintKeyCap(키 칩), paintSwatch(사각 견본 — 반투명 합성).

#include <fmstyle/ThemeColors.h>

#include <QAbstractButton>
#include <QComboBox>
#include <QKeySequence>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

#include <optional>

class QButtonGroup;
class QHBoxLayout;
class QLabel;

namespace fm::ui {

class Label;

// ------------------------------------------------------------------------------------- 그리기 도우미

enum class ChipKind {
    Base,      // --grid 바탕 · --fg3
    Link,      // 투명 · --line 테두리 · --accent-fg
    Changed,   // --accent-soft · --accent 테두리 · --accent-fg
    Override,  // --warn-bg · --warn-line · --warn
    Fixed,     // --ok-bg · --ok
    Ok,        // --ok-bg · --ok (테두리 없음)
    Warn,      // --warn-bg · --warn
    Mute,      // --grid · --fg2
    Accent,    // --accent-soft · --accent-fg
    Dashed,    // 투명 · 점선 --line · --fg3 (2줄 위치 "숨김")
};
QSize chipSize(const QString &text, qreal px = 11.5, int padX = 8, int height = 20);
void paintChip(QPainter *p, const QRectF &rect, const QString &text, ChipKind kind, const fm::style::ThemeColors &tc,
               qreal radius = 10, qreal px = 11.5, bool bold = true);

/// 키 칩(kbd) — mono 11.5 px, 높이 20, 좌우 6, 테두리 1 + 아래 2. dim = 기본값 열, hot = 입력 중.
QSize keyCapSize(const QString &text);
void paintKeyCap(QPainter *p, const QRectF &rect, const QString &text, const fm::style::ThemeColors &tc, bool dim = false,
                 bool hot = false);

/// 사각 색 견본 — 테두리 --sw-line, 반투명 색은 base 위에 합성, shadow면 그림자 견본.
void paintSwatch(QPainter *p, const QRectF &rect, const QColor &color, const fm::style::ThemeColors &tc, qreal radius,
                 const QColor &base = QColor(), bool shadow = false);

// ------------------------------------------------------------------------------------- SettingRow

class SettingRow : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QString description READ description WRITE setDescription)
    Q_PROPERTY(bool modified READ isModified WRITE setModified)
    Q_PROPERTY(int rowHeight READ rowHeight WRITE setRowHeight)

public:
    explicit SettingRow(QWidget *parent = nullptr);
    SettingRow(const QString &title, const QString &description = QString(), QWidget *parent = nullptr);

    QString title() const;
    void setTitle(const QString &title);
    QString description() const;
    void setDescription(const QString &description);
    /// 기본값과 다름 — 이름 앞 6 px 강조색 점(도구 설명 "기본값과 다름").
    bool isModified() const noexcept { return m_modified; }
    void setModified(bool modified);
    /// 최소 높이(기본 52, 그룹 페이지 48).
    int rowHeight() const noexcept { return m_rowHeight; }
    void setRowHeight(int height);

    /// 오른쪽 컨트롤 칸에 넣는다(.ui에서 만든 자식은 처음 표시될 때 자동으로 옮겨진다).
    void addControl(QWidget *widget);
    QHBoxLayout *controlsLayout() const noexcept { return m_controls; }
    QLabel *titleLabel() const;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void adoptChildren();
    bool isFirstRow() const;

    Label *m_title = nullptr;
    Label *m_description = nullptr;
    QHBoxLayout *m_controls = nullptr;
    QWidget *m_textBox = nullptr;
    bool m_modified = false;
    int m_rowHeight = 52;
};

// ------------------------------------------------------------------------------------- SearchField

class SearchField : public QLineEdit
{
    Q_OBJECT
public:
    explicit SearchField(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void refreshIcon();
    QAction *m_icon = nullptr;
};

// ------------------------------------------------------------------------------------- ThemeModeCard

class ThemeModeCard : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent)

public:
    enum Mode { System, Light, Dark };
    Q_ENUM(Mode)

    explicit ThemeModeCard(QWidget *parent = nullptr);

    Mode mode() const noexcept { return m_mode; }
    void setMode(Mode mode);
    /// 그림 첫 줄의 강조색(보류 강조색).
    QColor accent() const { return m_accent; }
    void setAccent(const QColor &accent);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Mode m_mode = System;
    QColor m_accent = QColor(0x1F, 0x5F, 0xD1);
};

// ------------------------------------------------------------------------------------- 강조색 견본

class ColorSwatchButton : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)
    Q_PROPERTY(bool addButton READ isAddButton WRITE setAddButton)

public:
    explicit ColorSwatchButton(QWidget *parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    /// 점선 원 "＋" — 직접 선택….
    bool isAddButton() const noexcept { return m_add; }
    void setAddButton(bool add);

    QSize sizeHint() const override { return QSize(24, 24); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_color;
    bool m_add = false;
};

class AccentPicker : public QWidget
{
    Q_OBJECT
public:
    struct Swatch
    {
        QString name;   // "파랑"
        QColor color;
    };
    explicit AccentPicker(QWidget *parent = nullptr);

    /// 목업의 6색(파랑 · 청록 · 보라 · 자주 · 호박 · 회청). 첫 견본 = 기준 색 없음(내장 강조).
    static QList<Swatch> defaultSwatches();
    void setSwatches(const QList<Swatch> &swatches);
    void setAddButtonVisible(bool visible);

    /// nullopt = 첫 견본. 목록에 없는 색이면 사용자 지정 견본을 보여 준다.
    void setCurrent(const std::optional<QColor> &accent);
    std::optional<QColor> current() const { return m_current; }
    /// "파랑" · "사용자 지정 #RRGGBB"
    QString currentName() const;

Q_SIGNALS:
    void accentChosen(const std::optional<QColor> &accent);
    void customRequested();

private:
    void rebuild();

    QList<Swatch> m_swatches;
    QList<ColorSwatchButton *> m_buttons;
    ColorSwatchButton *m_custom = nullptr;
    ColorSwatchButton *m_add = nullptr;
    QButtonGroup *m_group = nullptr;
    QHBoxLayout *m_layout = nullptr;
    std::optional<QColor> m_current;
};

// ------------------------------------------------------------------------------------- HexColorEdit

class HexColorEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit HexColorEdit(QWidget *parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor &color);

Q_SIGNALS:
    void colorEdited(const QColor &color);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void commit();
    QColor m_color;
};

// ------------------------------------------------------------------------------------- ToggleChip

class ToggleChip : public QPushButton
{
    Q_OBJECT
public:
    explicit ToggleChip(QWidget *parent = nullptr);
    explicit ToggleChip(const QString &text, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;
};

// ------------------------------------------------------------------------------------- ColorPickButton

class ColorPickButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(bool allowNone READ allowNone WRITE setAllowNone)

public:
    explicit ColorPickButton(QWidget *parent = nullptr);

    std::optional<QColor> color() const { return m_color; }
    void setColor(const std::optional<QColor> &color);
    bool allowNone() const noexcept { return m_allowNone; }
    void setAllowNone(bool allow) { m_allowNone = allow; }
    /// 팝업의 추천 색.
    void setSuggestions(const QList<QColor> &colors) { m_suggestions = colors; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void colorChanged(const std::optional<QColor> &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void showPopup();
    void choose(const std::optional<QColor> &color);

    std::optional<QColor> m_color;
    bool m_allowNone = true;
    QList<QColor> m_suggestions;
};

// ------------------------------------------------------------------------------------- CheckListCombo

class CheckListCombo : public QComboBox
{
    Q_OBJECT
public:
    explicit CheckListCombo(QWidget *parent = nullptr);

    void setCheckItems(const QStringList &items);
    /// 비트 i = 항목 i가 켜짐.
    int checkedMask() const;
    void setCheckedMask(int mask);
    QString summaryText() const;

    void hidePopup() override;

Q_SIGNALS:
    void checkedMaskChanged(int mask);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_keepOpen = false;
};

// ------------------------------------------------------------------------------------- KeyCaptureEdit

class KeyCaptureEdit : public QWidget
{
    Q_OBJECT
public:
    explicit KeyCaptureEdit(QWidget *parent = nullptr);

    /// 입력 시작(키보드를 잡는다) · 끝.
    void startCapture();
    void stopCapture();
    bool isCapturing() const noexcept { return m_capturing; }
    QKeySequence pending() const { return m_pending; }
    /// 충돌 배너가 떠 있는 동안 누른 키를 보여 준다(입력은 멈춘 상태).
    void setPending(const QKeySequence &key);
    void setCaption(const QString &caption);

    QSize sizeHint() const override;

Q_SIGNALS:
    void captured(const QKeySequence &key);
    void cancelled();
    void cleared();

protected:
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    bool m_capturing = false;
    QKeySequence m_pending;
    Qt::KeyboardModifiers m_mods;
    QString m_caption;
};

} // namespace fm::ui
