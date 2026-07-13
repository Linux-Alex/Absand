// SPDX-License-Identifier: MPL-2.0

#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QString>
#include <QStringList>

class ThemeManager
{
public:
    static void initialize();
    static QStringList availableThemes();
    static QString savedTheme();
    static void saveTheme(const QString& themeName);
    static void applyTheme(const QString& themeName);
    static void applyThemeByName(const QString& themeName);

private:
    static void captureSystemDefaults();
    static void restoreSystemTheme();
    static void repolishApplication();
};

#endif // THEMEMANAGER_H
