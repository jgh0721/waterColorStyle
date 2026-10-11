#pragma once

#include <QTimer>
#include <QTreeView>
#include <QTreeWidget>
#include <QWidget>

#include <functional>

class QFileSystemModel;
class QLabel;
class QPlainTextEdit;
class QStackedWidget;

namespace fm::dialogs {
class ProgressDialog;
}

namespace fm::app {

// 메인 창 도크의 내용(07 §6) — 폴더 트리 · 미리보기 · 속성 · 작업 대기열. 두 디자인이 같은 위젯 트리를 쓰고,
// 모양은 fm::style이 항목 보기 · 진행 막대 · 글자 토큰으로 그린다. 파일은 읽기만 한다.

/// 폴더 트리 — 이 PC의 드라이브와 폴더(QFileSystemModel, 폴더만 · 읽기 전용). 처음 보일 때 모델을 만든다.
/// 누르거나 Enter를 누르면 folderActivated(활성 패널이 그 폴더로 간다). 활성 패널이 실제 폴더면 그 자리를 따라간다.
class FolderTreePane : public QTreeView
{
    Q_OBJECT
public:
    explicit FolderTreePane(QWidget *parent = nullptr);

    /// 실제 폴더 경로(Windows 표기)를 펼쳐 고른다. 빈 문자열(샘플 데이터)이면 고른 것을 푼다.
    void follow(const QString &path);
    /// 모델을 만들었는지 — 숨은 도크는 드라이브를 읽지 않는다.
    bool hasModel() const noexcept { return m_model != nullptr; }

Q_SIGNALS:
    void folderActivated(const QString &path);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void ensureModel();
    void emitFolder(const QModelIndex &index);
    /// 열 폭 = max(창 폭, 보이는 이름의 폭) — 깊은 폴더는 줄이지 않고 가로로 스크롤한다(행 강조는 창 폭 전체).
    void fitColumn();

    QFileSystemModel *m_model = nullptr;
    QString m_pending;
};

/// 미리보기 — 커서 항목의 그림(실제 이미지 · 샘플의 가짜 섬네일 · 종류 아이콘) 또는 글 파일 앞부분, 이름과 정보 줄.
class PreviewPane : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewPane(QWidget *parent = nullptr);

    /// 프록시 인덱스(이름 열). 잘못된 인덱스면 빈 상태. local = 실제 폴더(파일을 열어 그림 · 글을 읽는다).
    void showItem(const QModelIndex &index, bool local);
    /// 지금 보이는 것 — "image" · "art" · "icon" · "text" · "empty"(테스트 · 스냅숏 확인용).
    QString content() const { return m_content; }

private:
    class Picture;

    Picture *m_picture = nullptr;
    QPlainTextEdit *m_text = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_name = nullptr;
    QLabel *m_info = nullptr;
    QString m_content;
    QString m_path;  // 같은 파일을 다시 읽지 않는다
};

/// 속성 — 커서 항목의 이름 · 종류 · 크기 · 날짜 · 속성 · 위치(항목 · 값 두 열).
class PropertiesPane : public QTreeWidget
{
    Q_OBJECT
public:
    explicit PropertiesPane(QWidget *parent = nullptr);

    void showItem(const QModelIndex &index, bool local);
    /// 항목 이름 → 값(테스트용).
    QString value(const QString &key) const;
};

/// 작업 대기열 — 진행 창(모덜리스)마다 한 줄: 작업 · 진행 막대 · 상태. 보이는 동안만 0.5초마다 갱신한다.
/// 두 번 누르면 그 진행 창을 앞으로 가져온다.
class JobsPane : public QWidget
{
    Q_OBJECT
public:
    using Provider = std::function<QList<fm::dialogs::ProgressDialog *>()>;
    explicit JobsPane(Provider provider, QWidget *parent = nullptr);

    void refresh();
    QTreeWidget *list() const noexcept { return m_list; }

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    Provider m_provider;
    QTreeWidget *m_list = nullptr;
    QLabel *m_empty = nullptr;
    QStackedWidget *m_stack = nullptr;
    QTimer m_timer;
};

} // namespace fm::app
