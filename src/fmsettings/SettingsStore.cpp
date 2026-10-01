#include "fmsettings/SettingsStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

using namespace Qt::StringLiterals;

namespace fm::settings {

SettingsStore &SettingsStore::instance()
{
    static SettingsStore store;
    return store;
}

void SettingsStore::setSettings(const AppSettings &settings)
{
    const Sections changedSections = differingSections(m_settings, settings);
    if (!changedSections)
        return;
    m_settings = settings;
    if (!m_path.isEmpty())
        save();
    Q_EMIT changed(changedSections);
}

bool SettingsStore::load(QString *error)
{
    if (m_path.isEmpty())
        return false;
    QFile file(m_path);
    if (!file.exists()) {
        m_settings = AppSettings();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (!doc.isObject()) {
        if (error)
            *error = parse.errorString();
        return false;
    }
    m_raw = doc.object();
    m_settings = AppSettings::fromJson(m_raw);
    return true;
}

bool SettingsStore::save(QString *error) const
{
    if (m_path.isEmpty())
        return false;
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    // 모르는 키(다른 버전)는 보존하고 아는 구역만 덮어쓴다
    QJsonObject root = m_raw;
    const QJsonObject known = m_settings.toJson();
    for (auto it = known.begin(); it != known.end(); ++it)
        root[it.key()] = it.value();
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QString SettingsStore::defaultFilePath()
{
    const QString appData = qEnvironmentVariable("APPDATA", QDir::homePath());
    return QDir(appData).filePath(u"FM Tools/settings.json"_s);
}

QString SettingsStore::themesDirectory()
{
    const QString appData = qEnvironmentVariable("APPDATA", QDir::homePath());
    return QDir(appData).filePath(u"FM Tools/themes"_s);
}

QString SettingsStore::displayPath(bool themes)
{
    return themes ? u"%APPDATA%\\FM Tools\\themes\\"_s : u"%APPDATA%\\FM Tools\\settings.json"_s;
}

QList<fm::style::ColorScheme> SettingsStore::savedSchemes() const
{
    QList<fm::style::ColorScheme> out;
    const QDir dir(themesDirectory());
    for (const QFileInfo &info : dir.entryInfoList({u"*.json"_s}, QDir::Files, QDir::Name)) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        if (auto scheme = fm::style::ColorScheme::fromJson(QJsonDocument::fromJson(file.readAll()).object())) {
            scheme->id = info.completeBaseName();
            if (scheme->name.isEmpty())
                scheme->name = scheme->id;
            out.append(*scheme);
        }
    }
    return out;
}

bool SettingsStore::saveScheme(const fm::style::ColorScheme &scheme, QString *error) const
{
    QDir().mkpath(themesDirectory());
    QSaveFile file(QDir(themesDirectory()).filePath(scheme.id + u".json"_s));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(scheme.toJson()).toJson(QJsonDocument::Indented));
    return file.commit();
}

void applyTheme(const AppSettings &s)
{
    auto &tm = fm::style::ThemeManager::instance();
    fm::style::ThemeManager::Batch batch(tm);
    tm.setDesign(s.appearance.design);
    tm.setDarkTone(s.appearance.darkTone);
    tm.setScheme(s.appearance.scheme);
    tm.setColorScheme(s.theme.scheme);
    tm.setDarkTitleBar(s.appearance.darkTitleBar);
    tm.setColoredTitleBar(s.appearance.coloredTitleBar);
    tm.setAlwaysShowMnemonics(s.keys.alwaysShowMnemonics);
}

void captureTheme(AppSettings &s)
{
    const auto &tm = fm::style::ThemeManager::instance();
    s.appearance.design = tm.design();
    s.appearance.darkTone = tm.darkTone();
    s.appearance.scheme = tm.scheme();
    s.appearance.darkTitleBar = tm.darkTitleBar();
    s.appearance.coloredTitleBar = tm.coloredTitleBar();
    const fm::style::ColorScheme scheme = tm.colorScheme();
    if (!s.theme.scheme.sameColors(scheme)) {
        s.theme.scheme.seeds = scheme.seeds;
        s.theme.scheme.overrides = scheme.overrides;
    }
    s.keys.alwaysShowMnemonics = tm.alwaysShowMnemonics();
}

} // namespace fm::settings
