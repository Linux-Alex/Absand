// SPDX-License-Identifier: MPL-2.0

#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QEvent>
#include <QTranslator>

#include "ui/MainDialog.h"
#include "TestHarness.h"
#include "core/UserConfig.h"

namespace {
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
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Absand");
    app.setApplicationVersion(QStringLiteral(ABSAND_VERSION));
    app.setOrganizationName("Absand");

    QTranslator translator;
    loadLanguage(app, translator, UserConfig::language());

    QStringList arguments = app.arguments();
    bool showTestHarness = arguments.contains("--harness") || arguments.contains("-h");
    if (showTestHarness) {
        TestHarness harness;
        harness.exec();
        return 0;
    }

    QStringList inputPaths;
    for (int i = 1; i < arguments.size(); ++i) {
        const QString path = arguments.at(i);
        if (path == "--test" || path == "-t") {
            continue;
        }
        if (QFileInfo::exists(path)) {
            inputPaths << path;
        }
    }

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
