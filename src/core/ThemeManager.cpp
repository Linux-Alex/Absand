// SPDX-License-Identifier: MPL-2.0

#include "ThemeManager.h"

#include <QApplication>
#include <QGuiApplication>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QWidget>

namespace {

QString g_systemStyleName;
QPalette g_systemPalette;
bool g_defaultsCaptured = false;

QString normalizedThemeName(const QString& name)
{
    return name.trimmed();
}

QStringList uniqueSorted(QStringList values)
{
    values.removeDuplicates();
    values.sort(Qt::CaseInsensitive);
    return values;
}

} // namespace

void ThemeManager::initialize()
{
    captureSystemDefaults();
    applyTheme(savedTheme());
}

QStringList ThemeManager::availableThemes()
{
    QStringList themes;
    themes << QStringLiteral("System");
    themes << uniqueSorted(QStyleFactory::keys());
    return uniqueSorted(themes);
}

QString ThemeManager::savedTheme()
{
    QSettings settings;
    return settings.value(QStringLiteral("ui/theme"), QStringLiteral("System")).toString();
}

void ThemeManager::saveTheme(const QString& themeName)
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/theme"), normalizedThemeName(themeName));
}

void ThemeManager::applyThemeByName(const QString& themeName)
{
    applyTheme(themeName);
}

void ThemeManager::applyTheme(const QString& themeName)
{
    captureSystemDefaults();

    auto* app = qobject_cast<QApplication*>(QGuiApplication::instance());
    if (!app) {
        return;
    }

    const QString name = normalizedThemeName(themeName);
    if (name.compare(QStringLiteral("System"), Qt::CaseInsensitive) == 0 || name.isEmpty()) {
        restoreSystemTheme();
        repolishApplication();
        return;
    }

    if (QStyle* style = QStyleFactory::create(name)) {
        app->setStyle(style);
        app->setPalette(style->standardPalette());
        repolishApplication();
        return;
    }

    restoreSystemTheme();
    repolishApplication();
}

void ThemeManager::captureSystemDefaults()
{
    if (g_defaultsCaptured) {
        return;
    }

    if (auto* app = qobject_cast<QApplication*>(QGuiApplication::instance())) {
        g_systemStyleName = app->style()->objectName();
        g_systemPalette = app->palette();
        g_defaultsCaptured = true;
    }
}

void ThemeManager::restoreSystemTheme()
{
    auto* app = qobject_cast<QApplication*>(QGuiApplication::instance());
    if (!app) {
        return;
    }

    if (!g_systemStyleName.isEmpty()) {
        if (QStyle* style = QStyleFactory::create(g_systemStyleName)) {
            app->setStyle(style);
        }
    }
    app->setPalette(g_systemPalette);
}

void ThemeManager::repolishApplication()
{
    auto* app = qobject_cast<QApplication*>(QGuiApplication::instance());
    if (!app) {
        return;
    }

    if (QStyle* style = app->style()) {
        for (QWidget* widget : app->allWidgets()) {
            style->unpolish(widget);
            style->polish(widget);
            widget->update();
        }
    }
}
