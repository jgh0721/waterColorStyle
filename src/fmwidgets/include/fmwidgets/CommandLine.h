#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;

namespace fm::ui {

/// 명령줄(01 §1.3 A5 · 06 §4.15) — 프롬프트 "{경로}>"(고정폭) + 입력. Ctrl+↓ = 명령 기록.
/// 시안1: 34 · 위 1 px --line · --surface · 입력 틀 없음. 시안2: 33 · 선 없음 · --win · 들어간 입력 25.
class CommandLine : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString prompt READ prompt WRITE setPrompt)

public:
    explicit CommandLine(QWidget *parent = nullptr);

    QString prompt() const;
    void setPrompt(const QString &path);
    QLineEdit *lineEdit() const noexcept { return m_edit; }
    QStringList history() const { return m_history; }

    QSize sizeHint() const override;

Q_SIGNALS:
    void commandEntered(const QString &command);

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyMetrics();
    void applyFonts();
    void showHistory();

    QLabel *m_prompt = nullptr;
    QLineEdit *m_edit = nullptr;
    QStringList m_history;
};

} // namespace fm::ui
