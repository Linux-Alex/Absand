// SPDX-License-Identifier: MPL-2.0

#ifndef USERCONFIG_H
#define USERCONFIG_H

#include <QVariantMap>
#include <QString>

class UserConfig
{
public:
    static QString appConfigRoot();
    static void setConfigRootOverrideForTests(const QString& root);
    static void clearConfigRootOverrideForTests();
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
