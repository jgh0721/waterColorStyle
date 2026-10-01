#pragma once

// 설정 창(docs/specs/04 §1 · 05 §1) — 왼쪽 탐색(검색 + 9개) · 페이지 머리 · 페이지 · 바닥(페이지별 왼쪽 단추 + 확인 · 취소 · 적용).
// 페이지는 보류(pending) 설정만 고치고, 적용 · 확인 때 SettingsStore에 한꺼번에 반영한다. 미리보기는 늘 보류 값을 그린다.

#include <fmsettings/AppSettings.h>

#include <QDialog>
#include <QPointer>

#include <functional>

class QAbstractButton;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListView;
class QSpinBox;
class QStackedWidget;
class QStandardItemModel;

namespace fm::ui {
class Button;
class Label;
class SearchField;
class SegmentedControl;
class SettingRow;
}

namespace fm::dialogs {

using fm::settings::AppSettings;
using fm::settings::Section;

class SettingsSession : public QObject
{
    Q_OBJECT
public:
    explicit SettingsSession(const AppSettings &applied, QObject *parent = nullptr);

    const AppSettings &applied() const noexcept { return m_applied; }
    const AppSettings &pending() const noexcept { return m_pending; }
    /// 보류 값을 고치고 알린다(같은 구역을 보는 페이지 · 미리보기 · 적용 단추).
    void edit(Section section, const std::function<void(AppSettings &)> &mutate);
    /// 창 상태(마지막 페이지 · 크기)는 적용 단추 판단에서 뺀다.
    bool isDirty() const;
    void apply();
    void revert();

Q_SIGNALS:
    void pendingChanged(fm::settings::Section section);
    void appliedChanged();

private:
    AppSettings m_applied;
    AppSettings m_pending;
};

/// 설정 페이지 — 위젯과 설정 값을 bind*()로 이어 두면 동기화 · 기본값과 다름 점 · 되돌리기 · 변경 개수를 기반 클래스가 맡는다.
class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(SettingsSession *session, QWidget *parent = nullptr);

    virtual QString pageId() const = 0;
    virtual QString title() const = 0;
    virtual QString description() const = 0;
    /// 탐색 아래 경로 — 기본 "%APPDATA%\FM Tools\settings.json".
    virtual QString footerPath() const;
    /// 페이지 머리 오른쪽(테마 색상의 "편집 중" 세그먼트).
    virtual QWidget *headerTrailing() { return nullptr; }
    /// 바닥 왼쪽 단추(기본: 기본값으로 되돌리기(R)).
    virtual QList<QAbstractButton *> footerButtons();
    virtual bool wantsScrollArea() const { return true; }
    /// 이 페이지가 고치는 구역(되돌리기 · 가져오기/내보내기 범위).
    virtual fm::settings::Sections sections() const = 0;

    /// 보류 값 → 위젯.
    virtual void syncFromPending();
    /// 이 페이지 범위만 기본값으로.
    virtual void resetToDefaults();
    /// 기본값과 다른 항목 수(바닥 상태 글자).
    virtual int modifiedCount() const;
    /// 설정 찾기 — 행 이름 · 설명 · 구역 제목 · 선택지.
    virtual QStringList searchKeywords() const;
    /// 찾은 글자가 들어 있는 첫 행(스크롤 · 강조 대상).
    virtual QWidget *searchTarget(const QString &query) const;
    /// 목업 보드의 한 순간(편집 중인 행 · 고른 항목 등)을 재현한다 — 대화상자 카탈로그 · 스크린샷용.
    virtual void showBoardState() {}

Q_SIGNALS:
    void navigateRequested(const QString &pageId);
    void modifiedCountChanged();

protected:
    SettingsSession *session() const noexcept { return m_session; }
    const AppSettings &pending() const { return m_session->pending(); }
    static const AppSettings &defaults();

    void bindCheck(QAbstractButton *widget, Section section, std::function<bool(const AppSettings &)> get,
                   std::function<void(AppSettings &, bool)> set);
    void bindCombo(QComboBox *widget, Section section, std::function<int(const AppSettings &)> get,
                   std::function<void(AppSettings &, int)> set);
    void bindSegment(fm::ui::SegmentedControl *widget, Section section, std::function<int(const AppSettings &)> get,
                     std::function<void(AppSettings &, int)> set);
    void bindSpin(QSpinBox *widget, Section section, std::function<int(const AppSettings &)> get,
                  std::function<void(AppSettings &, int)> set);
    void bindText(QLineEdit *widget, Section section, std::function<QString(const AppSettings &)> get,
                  std::function<void(AppSettings &, const QString &)> set);
    /// 바인딩이 없는 값(목록 등)의 기본값 비교 · 되돌리기.
    void addCustomItem(QWidget *rowWidget, std::function<bool(const AppSettings &, const AppSettings &)> differs,
                       std::function<void(AppSettings &, const AppSettings &)> reset, Section section);
    /// 이 행이 속한 SettingRow의 점을 갱신한다.
    void refreshModifiedMarks();
    fm::ui::Button *makeResetButton();
    fm::ui::Button *makeImportExportButton();

private:
    struct Binding
    {
        QWidget *widget = nullptr;
        Section section = Section::Appearance;
        std::function<void(const AppSettings &)> sync;
        std::function<bool(const AppSettings &, const AppSettings &)> differs;
        std::function<void(AppSettings &, const AppSettings &)> reset;
    };
    void addBinding(Binding binding);
    void importSections();
    void exportSections();

    SettingsSession *m_session;
    QList<Binding> m_bindings;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    /// 적용된 설정(SettingsStore)을 보류 값으로 열린다.
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

    QStringList pageIds() const;
    QString currentPageId() const;
    void setCurrentPage(const QString &pageId);
    SettingsPage *page(const QString &pageId) const;
    /// 지금 페이지를 목업 보드 상태로(SettingsPage::showBoardState).
    void showBoardState();
    SettingsSession *session() const noexcept { return m_session; }

    /// 설정 찾기(04 §1.9) — 적중 수를 탐색에 보이고, 적중 없는 항목은 흐리게.
    void setSearchText(const QString &text);
    int searchHits(const QString &pageId) const;

public Q_SLOTS:
    void accept() override;
    void reject() override;
    void applyPending();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void buildShell();
    void addPage(SettingsPage *page);
    void onPageChanged(int index);
    void refreshFooter();
    void runSearch();
    void jumpToFirstHit();
    void fitInitialSize();

    SettingsSession *m_session = nullptr;
    QList<SettingsPage *> m_pages;
    fm::ui::SearchField *m_search = nullptr;
    QListView *m_nav = nullptr;
    QStandardItemModel *m_navModel = nullptr;
    QLabel *m_navPath = nullptr;
    fm::ui::Label *m_pageTitle = nullptr;
    fm::ui::Label *m_pageDescription = nullptr;
    QHBoxLayout *m_headerTrailing = nullptr;
    QStackedWidget *m_stack = nullptr;
    QHBoxLayout *m_footerLeft = nullptr;
    QLabel *m_status = nullptr;
    fm::ui::Button *m_ok = nullptr;
    fm::ui::Button *m_cancel = nullptr;
    fm::ui::Button *m_apply = nullptr;
    QList<QAbstractButton *> m_shownFooterButtons;
    QPointer<QWidget> m_shownTrailing;
    QList<int> m_hits;
    bool m_sizedOnce = false;
};

} // namespace fm::dialogs
