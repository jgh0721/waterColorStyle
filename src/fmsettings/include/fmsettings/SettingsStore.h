#pragma once

// 적용된 설정 보관소 — 파일(settings.json)을 읽고 쓰고, 적용되면 바뀐 구역을 알린다.
// 경로를 주지 않으면 메모리에만 둔다(데모 스냅숏 · 테스트).

#include "fmsettings/AppSettings.h"

#include <QJsonObject>
#include <QObject>

namespace fm::settings {

class SettingsStore : public QObject
{
    Q_OBJECT
public:
    static SettingsStore &instance();

    const AppSettings &settings() const noexcept { return m_settings; }
    /// 적용 — 바뀐 구역이 있으면 저장(경로가 있을 때)하고 changed를 낸다.
    void setSettings(const AppSettings &settings);

    QString filePath() const { return m_path; }
    void setFilePath(const QString &path) { m_path = path; }
    /// 파일에서 읽는다(없으면 기본값). 모르는 키는 보존해 두었다가 저장 때 다시 쓴다.
    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr) const;

    /// %APPDATA%\FM Tools\settings.json · themes 폴더.
    static QString defaultFilePath();
    static QString themesDirectory();
    /// 화면 표시용 경로 — "%APPDATA%\FM Tools\settings.json".
    static QString displayPath(bool themes = false);

    /// themes 폴더의 색 구성표(이름순).
    QList<fm::style::ColorScheme> savedSchemes() const;
    bool saveScheme(const fm::style::ColorScheme &scheme, QString *error = nullptr) const;

Q_SIGNALS:
    void changed(fm::settings::Sections sections);

private:
    SettingsStore() = default;
    AppSettings m_settings;
    QString m_path;
    QJsonObject m_raw;
};

/// 테마 관련 설정을 ThemeManager에 한 번에 반영한다(디자인 · 색 구성표 · 다크 색조 · 기준 색 · 제목 표시줄 · 액세스 키 밑줄).
void applyTheme(const AppSettings &settings);
/// 지금 ThemeManager 상태를 설정에 옮긴다 — 도구 모음 등 설정 창 밖에서 바꾼 테마를 설정 창이 보게.
void captureTheme(AppSettings &settings);

} // namespace fm::settings
