#pragma once

#include <QFrame>

namespace fm::ui {

/// 카드 — 목록 바탕색과 구분선 테두리 (시안1 모서리 6 px, 시안2 네모, 시안3 모서리 8 px). 설정 화면의 항목 묶음에 쓴다.
/// setTitle()을 주면 카드 안 위쪽에 제목 띠(36 px)를 그리고 내용은 그 아래에 놓는다 — 시안3은 --alt → --surface 워시 띠.
/// setMeta()는 띠 오른쪽의 보조 글자("위에서부터 적용", "편집 중 · 라이트").
class Card : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QString meta READ meta WRITE setMeta)

public:
    explicit Card(QWidget *parent = nullptr);
    explicit Card(const QString &title, QWidget *parent = nullptr);

    QString title() const { return m_title; }
    void setTitle(const QString &title);
    QString meta() const { return m_meta; }
    void setMeta(const QString &meta);
    /// 띠 오른쪽 끝에 놓을 위젯(작은 단추 묶음 등). 띠 높이 안에서 세로 가운데.
    void setHeaderWidget(QWidget *widget);

    static constexpr int kHeaderHeight = 36;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void placeHeaderWidget();
    void applyTitleMargins();

    QString m_title;
    QString m_meta;
    QWidget *m_headerWidget = nullptr;
};

} // namespace fm::ui
