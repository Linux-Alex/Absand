// SPDX-License-Identifier: MPL-2.0

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QTranslator>

#include <cstdio>

#include "core/UserConfig.h"

#ifdef ABSAND_KIRIGAMI_FRONTEND
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QLibraryInfo>
#include <QQuickStyle>

#include "app/AppBackend.h"
#include "app/ArchiveBrowser.h"
#else
#include <QWidget>

#include "ui/MainDialog.h"
#include "TestHarness.h"
#endif

namespace {

#ifdef Q_OS_WIN
QFile* g_logFile = nullptr;

// A Windows GUI application has no console, so every qDebug()/qWarning() call in
// Absand would otherwise be lost. Mirroring them into a log file keeps problems
// on end-user machines diagnosable.
void logMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QByteArray line = qFormatLogMessage(type, context, message).toUtf8() + '\n';
    std::fwrite(line.constData(), 1, static_cast<std::size_t>(line.size()), stderr);
    if (g_logFile && g_logFile->isOpen()) {
        g_logFile->write(line);
        g_logFile->flush();
    }
}

void installFileLogger()
{
    const QString logFolder = UserConfig::appConfigRoot() + QStringLiteral("/logs");
    if (!QDir().mkpath(logFolder)) {
        return;
    }

    const QString logPath = QDir(logFolder).filePath(QStringLiteral("absand.log"));
    if (QFileInfo(logPath).size() > 1024 * 1024) {
        QFile::remove(logPath);
    }

    auto* file = new QFile(logPath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete file;
        return;
    }

    g_logFile = file;
    qSetMessagePattern(QStringLiteral("%{time yyyy-MM-dd HH:mm:ss.zzz} [%{type}] %{message}"));
    qInstallMessageHandler(&logMessageHandler);
}
#else
void installFileLogger() {}
#endif

bool loadLanguage(QApplication& app, QTranslator& translator, const QString& locale)
{
    app.removeTranslator(&translator);
    if (locale == QStringLiteral("en_US")) return true;
    const QString fileName = QStringLiteral("absand_%1.qm").arg(locale);
    const QStringList roots{
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../share/absand/translations"))
    };
    for (const QString& root : roots) {
        if (translator.load(fileName, root)) {
            app.installTranslator(&translator);
            return true;
        }
    }
    qWarning() << "Could not load translation" << fileName;
    return false;
}

#ifdef ABSAND_KIRIGAMI_FRONTEND
/// True when a QML module (directory form, e.g. "org/kde/desktop") exists in one
/// of the import directories Qt uses, including the deployment directory.
bool qmlModuleAvailable(const QString& modulePath)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList roots = QStringList{
        QLibraryInfo::path(QLibraryInfo::QmlImportsPath),
        QDir(appDir).filePath(QStringLiteral("qml")),
        appDir,
    };
    for (const QString& root : roots) {
        if (QFileInfo::exists(QDir(root).filePath(modulePath))) {
            return true;
        }
    }
    return false;
}
#endif // ABSAND_KIRIGAMI_FRONTEND

QStringList fileArguments(const QStringList& arguments)
{
    QStringList inputPaths;
    for (int i = 1; i < arguments.size(); ++i) {
        const QString path = arguments.at(i);
        if (path == QStringLiteral("--test") || path == QStringLiteral("-t")) {
            continue;
        }
        if (QFileInfo::exists(path)) {
            inputPaths << path;
        }
    }
    return inputPaths;
}

} // namespace

#ifdef ABSAND_KIRIGAMI_FRONTEND

// Kirigami/QML front end: the window is described in src/qml, and AppBackend
// supplies the logic plus the plugin schemas the settings pages render.
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("Absand"));
    app.setApplicationVersion(QStringLiteral(ABSAND_VERSION));
    app.setOrganizationName(QStringLiteral("Absand"));
    app.setDesktopFileName(QStringLiteral("Absand"));

    installFileLogger();
    qInfo() << "Absand" << QCoreApplication::applicationVersion() << "starting (Kirigami frontend)";

    QTranslator translator;
    loadLanguage(app, translator, UserConfig::language());

    // Prefer the Breeze style (org.kde.desktop from qqc2-desktop-style), but
    // only when its QML module is installed: requesting a style that is missing
    // makes every Qt Quick control fail to load. Fusion is the safe default and
    // also the fallback style.
    QQuickStyle::setFallbackStyle(QStringLiteral("Fusion"));
    QQuickStyle::setStyle(qmlModuleAvailable(QStringLiteral("org/kde/desktop"))
                              ? QStringLiteral("org.kde.desktop")
                              : QStringLiteral("Fusion"));

    AppBackend backend;
    ArchiveBrowser browser;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Backend"), &backend);
    engine.rootContext()->setContextProperty(QStringLiteral("Browser"), &browser);

    QObject::connect(&backend, &AppBackend::languageChanged, &app, [&app, &translator, &engine]() {
        loadLanguage(app, translator, UserConfig::language());
        engine.retranslate();
    });

    const QStringList inputPaths = fileArguments(app.arguments());
    if (!inputPaths.isEmpty()) {
        backend.addFiles(inputPaths);
    }

    engine.loadFromModule("Absand", "Main");
    if (engine.rootObjects().isEmpty()) {
        qWarning() << "The QML front end could not be loaded.";
        return 1;
    }

    // Headless smoke test used by CI: instantiate every QML page once and exit.
    if (app.arguments().contains(QStringLiteral("--qml-selftest"))) {
        bool ok = true;
        const QStringList pages{
            QStringLiteral("qrc:/qt/qml/Absand/ArchiveBrowserPage.qml"),
        };
        for (const QString& pageUrl : pages) {
            QQmlComponent component(&engine, QUrl(pageUrl));
            QObject* page = component.create();
            if (!page) {
                qWarning().noquote() << pageUrl << "failed:" << component.errorString();
                ok = false;
                continue;
            }
            delete page;
        }
        for (const bool sendMode : { true, false }) {
            QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qt/qml/Absand/SettingsPage.qml")));
            QObject* page = component.createWithInitialProperties({ { QStringLiteral("sendMode"), sendMode } });
            if (!page) {
                qWarning().noquote() << "SettingsPage failed:" << component.errorString();
                ok = false;
                continue;
            }
            delete page;
        }
        qInfo() << "QML self test" << (ok ? "passed" : "failed");
        return ok ? 0 : 1;
    }

    return app.exec();
}

#else

// Classic widget front end.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Absand");
    app.setApplicationVersion(QStringLiteral(ABSAND_VERSION));
    app.setOrganizationName("Absand");

    installFileLogger();
    qInfo() << "Absand" << QCoreApplication::applicationVersion() << "starting";

    QTranslator translator;
    loadLanguage(app, translator, UserConfig::language());

    QStringList arguments = app.arguments();
    // "-h" is left out on purpose: users type it expecting help, and it used to
    // open the developer test harness window.
    const bool showTestHarness = arguments.contains(QStringLiteral("--harness"));
    if (showTestHarness) {
        TestHarness harness;
        harness.exec();
        return 0;
    }

    const QStringList inputPaths = fileArguments(arguments);

    qDebug() << "Absand starting with" << inputPaths.size() << "input path(s)";
    MainDialog dialog(inputPaths);
    QObject::connect(&dialog, &MainDialog::languageChangeRequested, &app,
                     [&app, &translator](const QString& locale) {
        loadLanguage(app, translator, locale);
        const auto widgets = app.topLevelWidgets();
        for (QWidget* widget : widgets) {
            QEvent event(QEvent::LanguageChange);
            QCoreApplication::sendEvent(widget, &event);
        }
    });
    return dialog.exec();
}

#endif