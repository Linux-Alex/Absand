// SPDX-License-Identifier: MPL-2.0

#ifndef USERCONFIG_H
#define USERCONFIG_H

#include <QString>
#include <QStringList>
#include <QVariantMap>

/**
 * Stores plugin settings as JSON below the current user's application-config
 * directory. Every plugin can keep several named profiles (for example
 * "marinic.eu SMTP" and "arcont.si SMTP"), each holding its own configuration
 * map, plus one that is currently active.
 *
 * Older releases stored a single flat configuration object per plugin; those
 * files are migrated on first read into a profile called "Default".
 */
class UserConfig
{
public:
    enum class ConfigKind {
        Send,
        Destination
    };

    static QString defaultProfileName();

    static QString appConfigRoot();
    static void setConfigRootOverrideForTests(const QString& root);
    static void clearConfigRootOverrideForTests();

    static QString configPath(ConfigKind kind, const QString& pluginKey);

    static QStringList profileNames(ConfigKind kind, const QString& pluginKey);
    static QString activeProfile(ConfigKind kind, const QString& pluginKey);
    static void setActiveProfile(ConfigKind kind, const QString& pluginKey, const QString& profileName);

    static QVariantMap loadProfile(ConfigKind kind, const QString& pluginKey,
                                   const QString& profileName = QString());
    static void saveProfile(ConfigKind kind, const QString& pluginKey,
                            const QString& profileName, const QVariantMap& config);
    static void deleteProfile(ConfigKind kind, const QString& pluginKey, const QString& profileName);
    static void renameProfile(ConfigKind kind, const QString& pluginKey,
                              const QString& from, const QString& to);

    // Single-configuration helpers kept for compatibility; they operate on the
    // profile that is currently active.
    static QString destinationConfigPath(const QString& pluginKey);
    static QVariantMap loadDestinationConfig(const QString& pluginKey);
    static void saveDestinationConfig(const QString& pluginKey, const QVariantMap& config);
    static QString sendConfigPath(const QString& pluginKey);
    static QVariantMap loadSendConfig(const QString& pluginKey);
    static void saveSendConfig(const QString& pluginKey, const QVariantMap& config);

    static QString language();
    static void saveLanguage(const QString& locale);
};

#endif // USERCONFIG_H
