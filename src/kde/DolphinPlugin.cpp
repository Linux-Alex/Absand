// SPDX-License-Identifier: MPL-2.0

#include "DolphinPlugin.h"
#include <KFileItemListProperties>
#include <KLocalizedString>
#include <QAction>
#include <QProcess>
#include <QDebug>
#include <QMenu>

#ifdef Q_OS_WIN
    #define ABSAND_EXE "Absand.exe"
#elif defined(Q_OS_MAC)
    #define ABSAND_EXE "Absand.app/Contents/MacOS/Absand"
#else
    #define ABSAND_EXE "Absand"
#endif

DolphinPlugin::DolphinPlugin(QObject* parent)
    : QObject(parent)
    , KFileItemActionPlugin()
{
}

DolphinPlugin::~DolphinPlugin() = default;

QList<QAction*> DolphinPlugin::actions(const KFileItemListProperties& properties) const
{
    QList<QAction*> actions;
    
    // Only show if there are selected items
    if (properties.items().isEmpty()) {
        return actions;
    }
    
    // Store URLs
    DolphinPlugin* nonConstThis = const_cast<DolphinPlugin*>(this);
    nonConstThis->m_currentUrls.clear();
    for (const KFileItem& item : properties.items()) {
        nonConstThis->m_currentUrls << item.url().toLocalFile();
    }
    
    // Main action
    QAction* action = new QAction(QIcon::fromTheme("archive-encrypted"), 
                                   i18n("Send with Absand"));
    connect(action, &QAction::triggered, nonConstThis, &DolphinPlugin::onTriggered);
    actions << action;
    
    // Submenu for additional options (optional)
    QAction* subMenuAction = new QAction(i18n("Absand"));
    QMenu* subMenu = new QMenu();
    
    QAction* compressOnly = new QAction(i18n("Compress only"));
    QAction* compressAndSend = new QAction(i18n("Compress and send"));
    QAction* settings = new QAction(i18n("Settings"));
    
    subMenu->addAction(compressOnly);
    subMenu->addAction(compressAndSend);
    subMenu->addSeparator();
    subMenu->addAction(settings);
    
    subMenuAction->setMenu(subMenu);
    actions << subMenuAction;
    
    return actions;
}

void DolphinPlugin::onTriggered()
{
    if (m_currentUrls.isEmpty()) {
        return;
    }
    
    // Find the Absand executable
    QString program = ABSAND_EXE;
    
    // Try multiple possible locations
    QStringList possiblePaths;
    possiblePaths << CMAKE_INSTALL_PREFIX "/bin/" ABSAND_EXE;
    possiblePaths << "/usr/local/bin/" ABSAND_EXE;
    possiblePaths << "/usr/bin/" ABSAND_EXE;
    possiblePaths << QDir::homePath() + "/.local/bin/" ABSAND_EXE;
    
    for (const QString& path : possiblePaths) {
        if (QFile::exists(path)) {
            program = path;
            break;
        }
    }
    
    QProcess* process = new QProcess(this);
    process->setProgram(program);
    process->setArguments(m_currentUrls);
    process->start();
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            process, &QProcess::deleteLater);
}