#pragma once

// 파일 작업 대화상자용 위젯(02 §10) — 경로 입력 · 칩 단추 · 최근 대상 · 선택 카드 · 파일 요약 목록 · 폴더 계획 · 토큰 단추.
// 색은 themeColorsFor(this), 시안2(워터컬러)는 모서리 0. Designer에서 .ui에 넣을 수 있게 범위 없는 enum을 쓴다.

#include "fmwidgets/Glyph.h"

#include <QPushButton>
#include <QRadioButton>
#include <QStringList>
#include <QWidget>

class QLineEdit;
class QHBoxLayout;

namespace fm::ui {

class Button;

/// 경로 입력(02 §2.2) — 편집기(늘어남, 고정폭 13 px) + [최근](Clock) + [찾아보기](More). Alt+↓ = 최근 대상 목록.
class PathEdit : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged USER true)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(QStringList history READ history WRITE setHistory)
    Q_PROPERTY(bool historyVisible READ isHistoryVisible WRITE setHistoryVisible)
    Q_PROPERTY(bool browseVisible READ isBrowseVisible WRITE setBrowseVisible)
    Q_PROPERTY(bool monospace READ isMonospace WRITE setMonospace)

public:
    explicit PathEdit(QWidget *parent = nullptr);

    QString path() const;
    void setPath(const QString &path);
    QString placeholderText() const;
    void setPlaceholderText(const QString &text);
    QStringList history() const { return m_history; }
    void setHistory(const QStringList &history);
    bool isHistoryVisible() const;
    void setHistoryVisible(bool visible);
    bool isBrowseVisible() const;
    void setBrowseVisible(bool visible);
    bool isMonospace() const noexcept { return m_monospace; }
    void setMonospace(bool on);

    QLineEdit *lineEdit() const noexcept { return m_edit; }
    void showHistory();

Q_SIGNALS:
    void pathChanged(const QString &path);
    /// 찾아보기 단추 — 받는 곳이 없으면 QFileDialog로 폴더를 고른다.
    void browseRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void browse();

    QLineEdit *m_edit = nullptr;
    Button *m_recent = nullptr;
    Button *m_browse = nullptr;
    QStringList m_history;
    bool m_monospace = true;
};

/// 칩 단추(02 §3.2) — [키 칩][폴더 14][경로(고정폭 12)]. 시안1 높이 28 · 좌 5 우 8, 시안2 입체 24.
class ChipButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(QString keyHint READ keyHint WRITE setKeyHint)
    Q_PROPERTY(fm::ui::glyph::Glyph glyph READ glyph WRITE setGlyph)
    Q_PROPERTY(bool monospace READ isMonospace WRITE setMonospace)

public:
    explicit ChipButton(QWidget *parent = nullptr);
    explicit ChipButton(const QString &text, QWidget *parent = nullptr);

    QString keyHint() const { return m_keyHint; }
    void setKeyHint(const QString &keys);
    glyph::Glyph glyph() const noexcept { return m_glyph; }
    void setGlyph(glyph::Glyph glyph);
    bool isMonospace() const noexcept { return m_monospace; }
    void setMonospace(bool on);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QFont labelFont() const;

    QString m_keyHint;
    glyph::Glyph m_glyph = glyph::Folder;
    bool m_monospace = true;
};

/// 최근 대상 줄(02 §3.2) — 칩 단추 n개, Alt+1 ~ n으로 고른다. 넘치면 잘린다.
class RecentTargetsBar : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList targets READ targets WRITE setTargets)

public:
    explicit RecentTargetsBar(QWidget *parent = nullptr);

    QStringList targets() const { return m_targets; }
    void setTargets(const QStringList &targets);
    QList<ChipButton *> chips() const { return m_chips; }

Q_SIGNALS:
    void targetActivated(int index, const QString &path);

private:
    QHBoxLayout *m_layout = nullptr;
    QStringList m_targets;
    QList<ChipButton *> m_chips;
    QList<QObject *> m_shortcuts;
};

/// 선택 카드(02 §5.2 새 파일 템플릿) — [라디오][파일 아이콘(종류 띠)][이름 · 말줄임][확장자 고정폭][키 칩].
/// 높이 40 · 좌우 12 · 모서리 6(시안2 0), 선택: 테두리 --accent · 바탕 --accent-soft.
/// Alt+1 ~ 8은 QAbstractButton::shortcut으로 준다(누르면 선택 · 포커스).
class ChoiceCard : public QRadioButton
{
    Q_OBJECT
    Q_PROPERTY(QString detail READ detail WRITE setDetail)
    Q_PROPERTY(QString keyHint READ keyHint WRITE setKeyHint)
    Q_PROPERTY(FileKind fileKind READ fileKind WRITE setFileKind)

public:
    enum FileKind { DocKind, CodeKind, ImageKind, ExeKind, PdfKind, ArchiveKind, SystemKind };
    Q_ENUM(FileKind)

    explicit ChoiceCard(QWidget *parent = nullptr);
    explicit ChoiceCard(const QString &text, QWidget *parent = nullptr);

    QString detail() const { return m_detail; }
    void setDetail(const QString &detail);
    QString keyHint() const { return m_keyHint; }
    void setKeyHint(const QString &keys);
    FileKind fileKind() const noexcept { return m_kind; }
    void setFileKind(FileKind kind);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool hitButton(const QPoint &pos) const override;

private:
    QString m_detail;
    QString m_keyHint;
    FileKind m_kind = DocKind;
};

/// 파일 요약 목록(02 §4.2 삭제) — 행 28 · 안쪽 좌우 12 · 간격 8: [파일 아이콘][이름 · 말줄임][크기 --fg2 오른쪽].
/// maxVisibleRows를 넘으면 마지막 행을 "… 외 N개"(--fg3)로.
class FileSummaryList : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int maxVisibleRows READ maxVisibleRows WRITE setMaxVisibleRows)

public:
    struct Item
    {
        QString name;
        QString size;          // 표시 문구("3.74 GB", 폴더는 "폴더")
        ChoiceCard::FileKind kind = ChoiceCard::DocKind;
        bool isDir = false;
        bool readOnly = false; // "읽기 전용" 태그
    };

    explicit FileSummaryList(QWidget *parent = nullptr);

    void setItems(const QList<Item> &items);
    QList<Item> items() const { return m_items; }
    int maxVisibleRows() const noexcept { return m_maxRows; }
    void setMaxVisibleRows(int rows);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int visibleRows() const;

    QList<Item> m_items;
    int m_maxRows = 5;
};

/// 폴더 계획(02 §6.2 새 폴더) — 한 줄 사슬: 현재 위치 → 이미 있음 → 새로 만듦, 행 24.
class FolderPlanView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int maxVisibleRows READ maxVisibleRows WRITE setMaxVisibleRows)

public:
    struct Node
    {
        enum State { Current, Existing, New, Invalid };
        QString name;
        State state = New;
    };

    explicit FolderPlanView(QWidget *parent = nullptr);

    void setPlan(const QList<Node> &nodes);
    QList<Node> plan() const { return m_nodes; }
    int maxVisibleRows() const noexcept { return m_maxRows; }
    void setMaxVisibleRows(int rows);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<Node> m_nodes;
    int m_maxRows = 6;
};

/// 토큰 단추(02 §9.2.1) — "[N] 이름": 코드(고정폭 500 --accent-fg) + 간격 5 + 이름(11.5 px --fg2), 높이 24 · 좌우 4, 평면.
class TokenButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(QString token READ token WRITE setToken)
    Q_PROPERTY(QString caption READ caption WRITE setCaption)

public:
    explicit TokenButton(QWidget *parent = nullptr);
    TokenButton(const QString &token, const QString &caption, QWidget *parent = nullptr);

    QString token() const { return m_token; }
    void setToken(const QString &token);
    QString caption() const { return m_caption; }
    void setCaption(const QString &caption);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_token;
    QString m_caption;
};

} // namespace fm::ui
