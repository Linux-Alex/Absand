// SPDX-License-Identifier: MPL-2.0

#include "UserConfig.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
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

} // namespace

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

QString UserConfig::destinationConfigPath(const QString& pluginKey)
{
    QDir dir(appConfigRoot() + "/destinations");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath(pluginKey + ".json");
}

QVariantMap UserConfig::loadDestinationConfig(const QString& pluginKey)
{
    return loadMap(destinationConfigPath(pluginKey));
}

void UserConfig::saveDestinationConfig(const QString& pluginKey, const QVariantMap& config)
{
    saveMap(destinationConfigPath(pluginKey), config);
}

QString UserConfig::sendConfigPath(const QString& pluginKey)
{
    QDir dir(appConfigRoot() + "/send");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath(pluginKey + ".json");
}

QVariantMap UserConfig::loadSendConfig(const QString& pluginKey)
{
    return loadMap(sendConfigPath(pluginKey));
}

void UserConfig::saveSendConfig(const QString& pluginKey, const QVariantMap& config)
{
    saveMap(sendConfigPath(pluginKey), config);
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
