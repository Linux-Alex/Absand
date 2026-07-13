// SPDX-License-Identifier: MPL-2.0

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QObject>
#include <QDir>
#include <QVector>
#include <QStringList>
#include <QVariantMap>

class PluginInterface;
class ArchivePluginInterface;
class SendPluginInterface;
class DestinationPluginInterface;
class QPluginLoader;

class PluginManager : public QObject
{
    Q_OBJECT

public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override;
    
    // Load all plugins from standard locations
    bool loadAllPlugins();
    
    // Get loaded plugins
    QVector<PluginInterface*> plugins() const { return m_plugins; }
    
    // Plugin discovery
    QStringList pluginNames() const;
    
    // Read metadata from a plugin file WITHOUT loading it
    static QVariantMap readPluginMetadata(const QString& filePath);
    
    // Get metadata for loaded plugin
    QVariantMap getPluginMetadata(PluginInterface* plugin) const;

    // Get archive plugins
    QVector<ArchivePluginInterface*> archivePlugins() const { return m_archivePlugins; }
    ArchivePluginInterface* archivePluginByName(const QString& name) const;
    QStringList availableArchiveFormats() const;
    
    // Get send plugins
    QVector<SendPluginInterface*> sendPlugins() const { return m_sendPlugins; }
    SendPluginInterface* sendPluginByName(const QString& name) const;
    QStringList availableSendChannels() const;

    // Get destination plugins
    QVector<DestinationPluginInterface*> destinationPlugins() const { return m_destinationPlugins; }
    DestinationPluginInterface* destinationPluginByName(const QString& name) const;
    QStringList availableDestinationTargets() const;

signals:
    void pluginLoaded(const QString& name);
    void pluginError(const QString& file, const QString& error);

private:
    bool loadPlugin(const QString& filePath);
    QStringList findPluginDirectories() const;
    
    QVector<PluginInterface*> m_plugins;
    QVector<QPluginLoader*> m_loaders;
    QVector<ArchivePluginInterface*> m_archivePlugins;
    QVector<SendPluginInterface*> m_sendPlugins;
    QVector<DestinationPluginInterface*> m_destinationPlugins;
};

#endif // PLUGINMANAGER_H
