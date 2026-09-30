#pragma once

// 관리자 권한 대화상자(docs/specs/03 §3.3) — PromptSpec을 받아 코드로 조립한다(8종이 섹션 · 단추 구성만 다르다).
// 작업 스레드와는 ask()(open() + 결과 콜백)로 잇는다 — exec()의 중첩 이벤트 루프를 피한다.

#include "fmdialogs/ElevationPrompt.h"

#include <QDialog>

#include <functional>

class QAbstractButton;
class QButtonGroup;
class QCheckBox;

namespace fm::ui {
class Button;
class DialogHeader;
}

namespace fm::dialogs {

class ElevationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ElevationDialog(const elev::PromptSpec &spec, QWidget *parent = nullptr);
    ~ElevationDialog() override;

    const elev::PromptSpec &spec() const noexcept { return m_spec; }
    elev::Result result() const noexcept { return m_result; }

    /// 라디오 선택지(없으면 -1).
    int selectedOption() const;
    void selectOption(int index);

    /// 이 결과를 내는 단추(없으면 nullptr). followsOption 단추는 지금 선택지의 결과로 찾는다.
    fm::ui::Button *button(elev::Choice choice) const;
    fm::ui::Button *defaultButton() const noexcept { return m_default; }
    QCheckBox *checkBox() const noexcept { return m_check; }

    /// 사용자가 고른 것처럼 끝낸다(흐름 자동화 · 테스트). 선택지의 결과면 그 선택지를 고르고 끝낸다.
    void choose(elev::Choice choice);

    /// 창 모달로 열고 결정되면 done을 부른다(대화상자는 닫힐 때 지워진다).
    static ElevationDialog *ask(QWidget *parent, const elev::PromptSpec &spec, std::function<void(elev::Result)> done);

Q_SIGNALS:
    void decided(const fm::dialogs::elev::Result &result);

public Q_SLOTS:
    void reject() override;

protected:
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void finish(elev::Choice choice);
    void syncPrimary();
    void fitHeight();

    elev::PromptSpec m_spec;
    elev::Result m_result;
    QButtonGroup *m_options = nullptr;
    QCheckBox *m_check = nullptr;
    fm::ui::Button *m_follow = nullptr;
    fm::ui::Button *m_default = nullptr;
    QList<QPair<fm::ui::Button *, elev::ButtonSpec>> m_buttons;
    bool m_finished = false;
};

} // namespace fm::dialogs
