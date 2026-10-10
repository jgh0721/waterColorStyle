#pragma once

#include <QDialog>
#include <QPointer>

class QLabel;
class QPushButton;
class QVariantAnimation;
class QVBoxLayout;

namespace fm::ui {

/// 내용 대화상자 — Windows 11 ContentDialog · ElaContentDialog 형식. 제목 · 내용(글 또는 위젯)과 단추 셋
/// (기본 · 보조 · 닫기). 글이 빈 단추는 숨는다. exec()는 Result를 돌려준다(Esc · 닫기 = None).
///
/// - 시안1(Windows 11): 부모 창을 어둡게 덮는 층(smoke) 위에 둥근 카드(모서리 8 · 그림자), 위는 Surface에
///   20 px 제목 · 내용(여백 24), 아래 단추 줄은 Win 바탕 · 위 1 px 선 · 같은 폭 단추(간격 8). 기본 단추만 강조색.
///   열 때 167 ms · 닫을 때 120 ms 흐려짐.
/// - 시안2(XP): 스타일이 그리는 파란 제목 표시줄(제목 · 닫기 단추, 끌어 옮기기)과 창 틀, Win 바탕 내용,
///   오른쪽에 붙은 단추(최소 폭 75, 기본 단추는 검은 테두리). 덮는 층은 옅게.
class ContentDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QString primaryButtonText READ primaryButtonText WRITE setPrimaryButtonText)
    Q_PROPERTY(QString secondaryButtonText READ secondaryButtonText WRITE setSecondaryButtonText)
    Q_PROPERTY(QString closeButtonText READ closeButtonText WRITE setCloseButtonText)
    Q_PROPERTY(Button defaultButton READ defaultButton WRITE setDefaultButton)
    Q_PROPERTY(bool smokeVisible READ isSmokeVisible WRITE setSmokeVisible)

public:
    // Qt Designer · uic 호환을 위해 범위 없는 enum.
    /// exec()의 값 — None = 닫기 단추 · Esc · 제목 표시줄 닫기. Primary는 QDialog::Accepted와 같다.
    enum Result { None = 0, Primary = 1, Secondary = 2 };
    Q_ENUM(Result)
    enum Button { NoButton, PrimaryButton, SecondaryButton, CloseButton };
    Q_ENUM(Button)

    explicit ContentDialog(QWidget *parent = nullptr);
    ~ContentDialog() override;

    /// 한 번에 묻는다. 닫기 단추 글이 비면 "취소".
    static Result ask(QWidget *parent, const QString &title, const QString &text, const QString &primaryText,
                      const QString &secondaryText = {}, const QString &closeText = {},
                      Button defaultButton = PrimaryButton);

    QString title() const { return m_title; }
    void setTitle(const QString &title);
    QString text() const;
    void setText(const QString &text);
    /// 글 대신 보일 내용 위젯(소유권을 가져간다). nullptr이면 글로 돌아간다.
    void setContentWidget(QWidget *widget);
    QWidget *contentWidget() const { return m_content; }

    QString primaryButtonText() const;
    void setPrimaryButtonText(const QString &text);
    QString secondaryButtonText() const;
    void setSecondaryButtonText(const QString &text);
    QString closeButtonText() const;
    void setCloseButtonText(const QString &text);
    Button defaultButton() const noexcept { return m_default; }
    void setDefaultButton(Button button);
    QPushButton *button(Button which) const;

    bool isSmokeVisible() const noexcept { return m_smokeVisible; }
    void setSmokeVisible(bool visible);
    /// 마지막으로 닫힌 결과.
    Result contentResult() const noexcept { return Result(result()); }

    /// 카드(그림자 · 틀 밖 여백을 뺀) 영역 — 위젯 좌표.
    QRect cardRect() const;
    /// 시안2 제목 표시줄 영역(시안1은 빈 사각형).
    QRect captionRect() const;

    void done(int result) override;

Q_SIGNALS:
    void primaryButtonClicked();
    void secondaryButtonClicked();
    void closeButtonClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyDesign();
    void updateButtons();
    QMargins chromeMargins() const;
    QRect closeButtonRect() const;
    void showSmoke(bool show);

    QString m_title;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_textLabel = nullptr;
    QPointer<QWidget> m_content;
    QWidget *m_body = nullptr;
    QVBoxLayout *m_bodyLayout = nullptr;
    QWidget *m_footer = nullptr;
    QPushButton *m_primary = nullptr;
    QPushButton *m_secondary = nullptr;
    QPushButton *m_close = nullptr;
    Button m_default = PrimaryButton;
    bool m_smokeVisible = true;
    QPointer<QWidget> m_smoke;
    QVariantAnimation *m_fade = nullptr;
    bool m_closing = false;
    bool m_closeHot = false;
    bool m_closeDown = false;
    QPoint m_dragOffset;
    bool m_dragging = false;
};

} // namespace fm::ui
