// SPDX-License-Identifier: MPL-2.0

#include "PluginManager.h"
#include "../interfaces/PluginInterface.h"
#include "../interfaces/ArchivePluginInterface.h"
#include "../interfaces/SendPluginInterface.h"
#include "../interfaces/DestinationPluginInterface.h"
#include <QCoreApplication>
#include <QDebug>
#include <QLibrary>
#include <QFileInfo>
#include <QPluginLoader>
#include <QJsonDocument>
#include <QJsonObject>

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
{
}

PluginManager::~PluginManager()
{
    // QPluginLoader inherits QObject, so setting parent in constructor
    // means they get deleted automatically when PluginManager is destroyed
}

bool PluginManager::loadAllPlugins()
{
    bool anyLoaded = false;
    
    for (const QString& dirPath : findPluginDirectories()) {
        QDir pluginDir(dirPath);
        if (!pluginDir.exists()) {
            qDebug() << "Plugin directory does not exist:" << dirPath;
            continue;
        }
        
        qDebug() << "Scanning for plugins in:" << dirPath;
        
        // Also check subdirectories
        QStringList subDirs = pluginDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QStringList searchDirs;
        searchDirs << dirPath;
        for (const QString& subDir : subDirs) {
            searchDirs << dirPath + "/" + subDir;
            qDebug() << "  Adding subdir:" << dirPath + "/" + subDir;
        }
        
        for (const QString& searchDir : searchDirs) {
            QDir dir(searchDir);
            const QStringList entries = dir.entryList(QDir::Files);
            qDebug() << "  Scanning directory:" << searchDir << "found" << entries.size() << "files";
            for (const QString& fileName : entries) {
                qDebug() << "    File:" << fileName;
                if (QLibrary::isLibrary(fileName)) {
                    QString fullPath = dir.absoluteFilePath(fileName);
                    qDebug() << "      -> Loading plugin:" << fullPath;
                    if (loadPlugin(fullPath)) {
                        anyLoaded = true;
                    }
                } else {
                    qDebug() << "      -> Not a library";
                }
            }
        }
    }
    
    qDebug() << "Loaded" << m_plugins.size() << "basic plugins,"
             << m_archivePlugins.size() << "archive plugins,"
             << m_sendPlugins.size() << "send plugins,"
             << m_destinationPlugins.size() << "destination plugins";
    return anyLoaded;
}

bool PluginManager::loadPlugin(const QString& filePath)
{
    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath();
    for (QPluginLoader* existingLoader : m_loaders) {
        if (QFileInfo(existingLoader->fileName()).canonicalFilePath() == canonicalPath) {
            return false;
        }
    }

    QPluginLoader* loader = new QPluginLoader(filePath, this);
    
    QObject* instance = loader->instance();
    if (!instance) {
        emit pluginError(filePath, loader->errorString());
        qWarning() << "Failed to load plugin:" << filePath << loader->errorString();
        delete loader;
        return false;
    }
    
    // Try as ArchivePluginInterface
    ArchivePluginInterface* archivePlugin = qobject_cast<ArchivePluginInterface*>(instance);
    if (archivePlugin) {
        m_archivePlugins.append(archivePlugin);
        m_loaders.append(loader);
        qDebug() << "Loaded archive plugin:" << archivePlugin->name();
        emit pluginLoaded(archivePlugin->name());
        return true;
    }
    
    // Try as SendPluginInterface
    SendPluginInterface* sendPlugin = qobject_cast<SendPluginInterface*>(instance);
    if (sendPlugin) {
        m_sendPlugins.append(sendPlugin);
        m_loaders.append(loader);
        qDebug() << "Loaded send plugin:" << sendPlugin->name();
        emit pluginLoaded(sendPlugin->name());
        return true;
    }

    DestinationPluginInterface* destinationPlugin = qobject_cast<DestinationPluginInterface*>(instance);
    if (destinationPlugin) {
        m_destinationPlugins.append(destinationPlugin);
        m_loaders.append(loader);
        qDebug() << "Loaded destination plugin:" << destinationPlugin->name();
        emit pluginLoaded(destinationPlugin->name());
        return true;
    }
    
    // Try as old PluginInterface
    PluginInterface* plugin = qobject_cast<PluginInterface*>(instance);
    if (plugin) {
        m_plugins.append(plugin);
        m_loaders.append(loader);
        qDebug() << "Loaded plugin:" << plugin->name();
        emit pluginLoaded(plugin->name());
        return true;
    }
    
    emit pluginError(filePath, "Does not implement any plugin interface");
    qWarning() << "Plugin doesn't implement any interface:" << filePath;
    delete loader;
    return false;
}

QVariantMap PluginManager::readPluginMetadata(const QString& filePath)
{
    QVariantMap result;
    
    QPluginLoader loader(filePath);
    QJsonObject metadata = loader.metaData();
    
    if (!metadata.isEmpty()) {
        result["IID"] = metadata["IID"].toString();
        result["className"] = metadata["className"].toString();
        
        if (metadata.contains("MetaData")) {
            QJsonObject customMeta = metadata["MetaData"].toObject();
            for (auto it = customMeta.begin(); it != customMeta.end(); ++it) {
                result[it.key()] = it.value().toVariant();
            }
        }
    }
    
    return result;
}

QVariantMap PluginManager::getPluginMetadata(PluginInterface* plugin) const
{
    if (!plugin) {
        return QVariantMap();
    }
    
    int index = m_plugins.indexOf(plugin);
    if (index == -1 || index >= m_loaders.size()) {
        return QVariantMap();
    }
    
    QPluginLoader* loader = m_loaders[index];
    if (!loader) {
        return QVariantMap();
    }
    
    QJsonObject metadata = loader->metaData();
    QVariantMap result;
    
    result["IID"] = metadata["IID"].toString();
    result["className"] = metadata["className"].toString();
    
    if (metadata.contains("MetaData")) {
        QJsonObject customMeta = metadata["MetaData"].toObject();
        for (auto it = customMeta.begin(); it != customMeta.end(); ++it) {
            result[it.key()] = it.value().toVariant();
        }
    }
    
    QVariantMap interfaceMeta = plugin->metadata();
    for (auto it = interfaceMeta.begin(); it != interfaceMeta.end(); ++it) {
        result[it.key()] = it.value();
    }
    
    return result;
}

QStringList PluginManager::findPluginDirectories() const
{
    QStringList dirs;
    
    dirs.append(QCoreApplication::applicationDirPath() + "/plugins");
    
    #ifdef Q_OS_MAC
        dirs.append(QCoreApplication::applicationDirPath() + "/../PlugIns");
        dirs.append(QCoreApplication::applicationDirPath() + "/../../../PlugIns");
    #endif
    
    #ifdef Q_OS_WIN
        dirs.append(QCoreApplication::applicationDirPath() + "/../plugins");
        dirs.append("C:/Program Files/Absand/plugins");
    #elif defined(Q_OS_LINUX)
        dirs.append("/usr/lib/absand/plugins");
        dirs.append("/usr/local/lib/absand/plugins");
        dirs.append(QDir::homePath() + "/.local/lib/absand/plugins");
    #endif
    
    dirs.removeDuplicates();
    
    return dirs;
}

QStringList PluginManager::pluginNames() const
{
    QStringList names;
    for (PluginInterface* plugin : m_plugins) {
        names.append(plugin->name());
    }
    return names;
}

ArchivePluginInterface* PluginManager::archivePluginByName(const QString& name) const
{
    for (ArchivePluginInterface* plugin : m_archivePlugins) {
        if (plugin->name().compare(name, Qt::CaseInsensitive) == 0) {
            return plugin;
        }
    }
    return nullptr;
}

QStringList PluginManager::availableArchiveFormats() const
{
    QStringList formats;
    for (ArchivePluginInterface* plugin : m_archivePlugins) {
        formats << plugin->supportedExtensions();
    }
    return formats;
}

SendPluginInterface* PluginManager::sendPluginByName(const QString& name) const
{
    for (SendPluginInterface* plugin : m_sendPlugins) {
        if (plugin->name().compare(name, Qt::CaseInsensitive) == 0) {
            return plugin;
        }
    }
    return nullptr;
}

QStringList PluginManager::availableSendChannels() const
{
    QStringList channels;
    for (SendPluginInterface* plugin : m_sendPlugins) {
        channels << plugin->channelName();
    }
    return channels;
}

DestinationPluginInterface* PluginManager::destinationPluginByName(const QString& name) const
{
    for (DestinationPluginInterface* plugin : m_destinationPlugins) {
        if (plugin->name().compare(name, Qt::CaseInsensitive) == 0) {
            return plugin;
        }
    }
    return nullptr;
}

QStringList PluginManager::availableDestinationTargets() const
{
    QStringList targets;
    for (DestinationPluginInterface* plugin : m_destinationPlugins) {
        targets << plugin->channelName();
    }
    return targets;
}
