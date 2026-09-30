#pragma once

// 파일 작업 대화상자(02 §2 ~ §6) — .ui(src/fmdialogs/ui) + 코드. 실행은 하지 않는다(읽기 전용 데모 — 결과 요청만 돌려준다).
// 변형 ID는 02 사양 표의 ID(예: "delete.permanent")이고, applyVariant()가 그 상태로 채운다(데모 · 스냅숏).

#include "fmdialogs/FileOpContext.h"
#include "fmdialogs/Planners.h"

#include <QDialog>

#include <memory>

namespace Ui {
class CopyDialog;
class MoveRenameDialog;
class DeleteDialog;
class NewFileDialog;
class NewFolderDialog;
}

namespace fm::dialogs {

// ---------------------------------------------------------------- 복사 (F5)

struct CopyRequest
{
    QString destination;
    int overwritePolicy = 0;   // 0 매번 묻기 · 1 덮어쓰기 · 2 더 새로우면 · 3 건너뛰기 · 4 번호 붙이기
    QStringList filters;
    bool keepAttributes = true;
    bool verify = false;
    bool acl = false;
    bool alternateStreams = true;
    bool symlinksAsLinks = true;
    bool emptyFolders = true;
    bool queued = false;
};

class CopyDialog : public QDialog
{
    Q_OBJECT
public:
    enum { Queued = 2 };   // done() 결과 — 대기열에 추가

    explicit CopyDialog(const FileOpContext &context, QWidget *parent = nullptr);
    ~CopyDialog() override;

    CopyRequest request() const;
    static QStringList variants();
    void applyVariant(const QString &id);

private:
    void updateDestination();

    std::unique_ptr<Ui::CopyDialog> ui;
    FileOpContext m_context;
};

// ---------------------------------------------------------------- 이름 변경 / 이동 (F2 · F6)

struct MoveRenameRequest
{
    QString target;          // 대상 전체 경로
    bool createDirectories = true;
};

class MoveRenameDialog : public QDialog
{
    Q_OBJECT
public:
    /// moveMode: F6으로 열었다 — 입력 처음 값이 "반대 패널 경로\원래 이름".
    MoveRenameDialog(const FileOpContext &context, bool moveMode, QWidget *parent = nullptr);
    ~MoveRenameDialog() override;

    MoveRenameRequest request() const;
    MoveRenamePlan plan() const { return m_plan; }
    static QStringList variants();
    void applyVariant(const QString &id);
    /// 입력을 바꾸고 이름 부분(확장자 제외)을 선택한다.
    void setTargetText(const QString &text);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void updatePlan();
    void refreshIcons();

    std::unique_ptr<Ui::MoveRenameDialog> ui;
    FileOpContext m_context;
    MoveRenamePlan m_plan;
};

// ---------------------------------------------------------------- 삭제 (F8 · Del · Shift+Del)

struct DeleteRequest
{
    bool permanent = false;
    bool forceReadOnly = false;
};

class DeleteDialog : public QDialog
{
    Q_OBJECT
public:
    DeleteDialog(const FileOpContext &context, bool permanent, QWidget *parent = nullptr);
    ~DeleteDialog() override;

    DeleteRequest request() const;
    bool isPermanent() const;
    void setPermanent(bool permanent);
    static QStringList variants();
    void applyVariant(const QString &id);

private:
    void updateMode();

    std::unique_ptr<Ui::DeleteDialog> ui;
    FileOpContext m_context;
};

// ---------------------------------------------------------------- 새 파일 (Shift+F4)

struct NewFileRequest
{
    QString name;
    int templateIndex = 4;
    bool openInEditor = true;
    bool utf8NoBom = true;
};

class NewFileDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewFileDialog(const FileOpContext &context, QWidget *parent = nullptr);
    ~NewFileDialog() override;

    NewFileRequest request() const;
    int templateIndex() const;
    void setTemplateIndex(int index);
    QString fileName() const;
    static QStringList variants();
    void applyVariant(const QString &id);

    /// 템플릿 8종 — 붙는 확장자(비면 없음) · 고정 이름.
    struct Template
    {
        QString extension;
        QString fixedName;
    };
    static const QList<Template> &templates();

private:
    void recompute();
    void validate();

    std::unique_ptr<Ui::NewFileDialog> ui;
    FileOpContext m_context;
    QString m_base;
    bool m_updating = false;
};

// ---------------------------------------------------------------- 새 폴더 (F7)

struct NewFolderRequest
{
    QString relativePath;
    bool enterLast = true;
};

class NewFolderDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewFolderDialog(const FileOpContext &context, QWidget *parent = nullptr);
    ~NewFolderDialog() override;

    NewFolderRequest request() const;
    FolderPlan plan() const { return m_plan; }
    static QStringList variants();
    void applyVariant(const QString &id);

private:
    void updatePlan();

    std::unique_ptr<Ui::NewFolderDialog> ui;
    FileOpContext m_context;
    FolderPlan m_plan;
};

} // namespace fm::dialogs
