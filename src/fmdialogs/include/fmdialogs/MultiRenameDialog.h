#pragma once

// 다중 이름 변경(02 §9, Ctrl+M) — 규칙 영역 · 버튼 영역은 .ui, 미리보기는 Qtitan 밴드 뷰(RenamePreviewView).
// 데모에서는 실제 파일을 바꾸지 않고 목록 안에서만 이름을 바꾼다(되돌리기 기록 포함).

#include "fmdialogs/RenameEngine.h"

#include <QDialog>

#include <memory>

namespace Ui {
class MultiRenameDialog;
}

namespace fm::dialogs {

class RenamePreviewModel;
class RenamePreviewView;

struct RenamePreset
{
    QString name;
    RenameRules rules;
};

class MultiRenameDialog : public QDialog
{
    Q_OBJECT
public:
    struct Context
    {
        QList<RenameFile> files;
        QString folder;                    // "D:\\Photos\\2026-09 제주"
        QSet<QString> existing;            // 폴더에서 이번 목록에 들지 않은 이름
        RenameRules rules;
        QList<RenamePreset> presets;
        int preset = 0;
        /// 이전에 적용한 변경(되돌리기 기록) — 각 항목은 변경 전 목록.
        QList<QList<RenameFile>> history;
        /// 설정 "충돌 · 오류가 있으면 이름 바꾸기 막기"(끄면 오류 행을 건너뛴다).
        bool blockOnProblems = false;
        /// 설정 › 파일 작업 › 미리보기 레코드(0 1줄 · 1 2줄 · 2 자동)와 되돌리기 기록 수(0 = 기록하지 않음).
        int recordMode = 2;
        int undoDepth = 20;
    };

    explicit MultiRenameDialog(const Context &context, QWidget *parent = nullptr);
    ~MultiRenameDialog() override;

    RenameRules rules() const;
    void setRules(const RenameRules &rules);
    QList<RenamePreviewRow> previewRows() const;
    RenameSummary summary() const;
    RenamePreviewView *previewView() const;
    QList<RenameFile> files() const { return m_context.files; }
    bool canUndo() const { return !m_context.history.isEmpty(); }

    /// 이름 바꾸기 — 정상 행만 바꾸고 변경 전 목록을 되돌리기 기록에 넣는다.
    void applyRename();
    void undo();

    static QStringList variants();
    void applyVariant(const QString &id);
    /// 목업(02 §9.5) — 12개 사진 · 이미 있는 이름 1개 · 프리셋 3개 · 되돌리기 기록 1개.
    static Context boardContext();

Q_SIGNALS:
    /// 이름 바꾸기를 눌렀다(원래 이름 → 새 이름, 정상 행만).
    void renamed(const QList<RenamePreviewRow> &rows);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void updatePreview();
    void updateFlagsLayout();
    void updateInfo();
    void updateButtons();
    void insertToken(const QString &token);

    std::unique_ptr<Ui::MultiRenameDialog> ui;
    Context m_context;
    RenamePreviewModel *m_model = nullptr;
    bool m_loading = false;
};

} // namespace fm::dialogs
