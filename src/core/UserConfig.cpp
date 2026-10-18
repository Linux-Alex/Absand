// SPDX-License-Identifier: MPL-2.0

#include "UserConfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QString g_configRootOverride;

QString ensureConfigRoot()
{
    if (!g_configRootOverride.isEmpty()) {
        QDir dir(g_configRootOverride);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        return dir.absolutePath();
    }

    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (baseDir.isEmpty()) {
        baseDir = QDir::homePath() + "/.config/Absand";
    }

    QDir dir(baseDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.absolutePath();
}

QVariantMap loadMap(const QString& path)
{
    QVariantMap result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return result;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        result.insert(it.key(), it.value().toVariant());
    }
    return result;
}

void saveMap(const QString& path, const QVariantMap& config)
{
    const QDir parent = QFileInfo(path).absoluteDir();
    if (!parent.exists()) {
        QDir().mkpath(parent.absolutePath());
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    QJsonObject object;
    for (auto it = config.constBegin(); it != config.constEnd(); ++it) {
        object.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    file.commit();
}

struct ProfileStore
{
    QString activeProfile;
    QMap<QString, QVariantMap> profiles;
    bool hadLegacyFormat = false;
};

ProfileStore readStore(const QString& path)
{
    ProfileStore store;
    const QVariantMap raw = loadMap(path);

    const QVariant profilesValue = raw.value(QStringLiteral("profiles"));
    if (profilesValue.canConvert<QVariantMap>() && !profilesValue.toMap().isEmpty()) {
        const QVariantMap profiles = profilesValue.toMap();
        for (auto it = profiles.constBegin(); it != profiles.constEnd(); ++it) {
            store.profiles.insert(it.key(), it.value().toMap());
        }
        store.activeProfile = raw.value(QStringLiteral("activeProfile")).toString();
    } else if (!raw.isEmpty()) {
        // Legacy layout: the file itself is one flat configuration object.
        store.profiles.insert(UserConfig::defaultProfileName(), raw);
        store.hadLegacyFormat = true;
    }

    // A plugin that has never been configured still has one usable profile, so
    // callers never have to special-case "no profiles yet".
    if (store.profiles.isEmpty()) {
        store.profiles.insert(UserConfig::defaultProfileName(), QVariantMap());
    }
    if (store.activeProfile.isEmpty() || !store.profiles.contains(store.activeProfile)) {
        store.activeProfile = store.profiles.firstKey();
    }
    return store;
}

void writeStore(const QString& path, const ProfileStore& store)
{
    QVariantMap profiles;
    for (auto it = store.profiles.constBegin(); it != store.profiles.constEnd(); ++it) {
        profiles.insert(it.key(), it.value());
    }

    QVariantMap document;
    document.insert(QStringLiteral("version"), 2);
    document.insert(QStringLiteral("activeProfile"), store.activeProfile);
    document.insert(QStringLiteral("profiles"), profiles);
    saveMap(path, document);
}

QString profileDirectory(UserConfig::ConfigKind kind)
{
    return ensureConfigRoot()
        + (kind == UserConfig::ConfigKind::Send ? QStringLiteral("/send")
                                                : QStringLiteral("/destinations"));
}

QString storePath(UserConfig::ConfigKind kind, const QString& pluginKey)
{
    QDir dir(profileDirectory(kind));
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath(pluginKey + QStringLiteral(".json"));
}

} // namespace

QString UserConfig::defaultProfileName()
{
    return QStringLiteral("Default");
}

QString UserConfig::appConfigRoot()
{
    return ensureConfigRoot();
}

void UserConfig::setConfigRootOverrideForTests(const QString& root)
{
    g_configRootOverride = root;
}

void UserConfig::clearConfigRootOverrideForTests()
{
    g_configRootOverride.clear();
}

QString UserConfig::configPath(ConfigKind kind, const QString& pluginKey)
{
    return storePath(kind, pluginKey);
}

QStringList UserConfig::profileNames(ConfigKind kind, const QString& pluginKey)
{
    return readStore(storePath(kind, pluginKey)).profiles.keys();
}

QString UserConfig::activeProfile(ConfigKind kind, const QString& pluginKey)
{
    return readStore(storePath(kind, pluginKey)).activeProfile;
}

void UserConfig::setActiveProfile(ConfigKind kind, const QString& pluginKey, const QString& profileName)
{
    const QString path = storePath(kind, pluginKey);
    ProfileStore store = readStore(path);
    if (!store.profiles.contains(profileName)) {
        return;
    }
    store.activeProfile = profileName;
    writeStore(path, store);
}

QVariantMap UserConfig::loadProfile(ConfigKind kind, const QString& pluginKey, const QString& profileName)
{
    const ProfileStore store = readStore(storePath(kind, pluginKey));
    const QString name = profileName.isEmpty() ? store.activeProfile : profileName;
    return store.profiles.value(name);
}

void UserConfig::saveProfile(ConfigKind kind, const QString& pluginKey,
                             const QString& profileName, const QVariantMap& config)
{
    const QString path = storePath(kind, pluginKey);
    ProfileStore store = readStore(path);
    const QString name = profileName.isEmpty() ? store.activeProfile : profileName;
    if (name.isEmpty()) {
        return;
    }

    // A plugin that was never configured starts with an empty "Default" profile.
    // Do not keep that placeholder around once the user names a real profile.
    const QString placeholder = defaultProfileName();
    if (name != placeholder && store.profiles.size() == 1
        && store.profiles.value(placeholder).isEmpty()) {
        store.profiles.remove(placeholder);
    }

    store.profiles.insert(name, config);
    store.activeProfile = name;
    writeStore(path, store);
}

void UserConfig::deleteProfile(ConfigKind kind, const QString& pluginKey, const QString& profileName)
{
    const QString path = storePath(kind, pluginKey);
    ProfileStore store = readStore(path);
    if (!store.profiles.contains(profileName) || store.profiles.size() <= 1) {
        return;
    }
    store.profiles.remove(profileName);
    if (store.activeProfile == profileName) {
        store.activeProfile = store.profiles.firstKey();
    }
    writeStore(path, store);
}

void UserConfig::renameProfile(ConfigKind kind, const QString& pluginKey,
                               const QString& from, const QString& to)
{
    const QString trimmed = to.trimmed();
    if (trimmed.isEmpty() || from == trimmed) {
        return;
    }

    const QString path = storePath(kind, pluginKey);
    ProfileStore store = readStore(path);
    if (!store.profiles.contains(from) || store.profiles.contains(trimmed)) {
        return;
    }

    store.profiles.insert(trimmed, store.profiles.take(from));
    if (store.activeProfile == from) {
        store.activeProfile = trimmed;
    }
    writeStore(path, store);
}

QString UserConfig::destinationConfigPath(const QString& pluginKey)
{
    return storePath(ConfigKind::Destination, pluginKey);
}

QVariantMap UserConfig::loadDestinationConfig(const QString& pluginKey)
{
    return loadProfile(ConfigKind::Destination, pluginKey);
}

void UserConfig::saveDestinationConfig(const QString& pluginKey, const QVariantMap& config)
{
    saveProfile(ConfigKind::Destination, pluginKey, activeProfile(ConfigKind::Destination, pluginKey), config);
}

QString UserConfig::sendConfigPath(const QString& pluginKey)
{
    return storePath(ConfigKind::Send, pluginKey);
}

QVariantMap UserConfig::loadSendConfig(const QString& pluginKey)
{
    return loadProfile(ConfigKind::Send, pluginKey);
}

void UserConfig::saveSendConfig(const QString& pluginKey, const QVariantMap& config)
{
    saveProfile(ConfigKind::Send, pluginKey, activeProfile(ConfigKind::Send, pluginKey), config);
}

QString UserConfig::language()
{
    return loadMap(QDir(appConfigRoot()).filePath("appearance.json"))
        .value("language", QStringLiteral("en_US")).toString();
}

void UserConfig::saveLanguage(const QString& locale)
{
    saveMap(QDir(appConfigRoot()).filePath("appearance.json"), {{"language", locale}});
}
