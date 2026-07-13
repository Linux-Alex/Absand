// SPDX-License-Identifier: MPL-2.0

#ifndef SEVENZIPPLUGIN_H
#define SEVENZIPPLUGIN_H

#include <QObject>
#include <QStringList>
#include <functional>
#include "interfaces/ArchivePluginInterface.h"

class SevenZipPlugin : public QObject, public ArchivePluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ArchivePluginInterface_IID FILE "sevenzip_plugin.json")
    Q_INTERFACES(ArchivePluginInterface)

public:
    explicit SevenZipPlugin(QObject* parent = nullptr);
    ~SevenZipPlugin();

    // PluginInterface
    QString name() const override { return "7z Archive"; }
    QString version() const override { return "1.0.0"; }
    QString description() const override { 
        return "7z archive format with AES-256 encryption support"; 
    }
    QVariantMap metadata() const override {
        return {{"author", "Absand Team"}, 
                {"formats", supportedExtensions()}};
    }
    
    // ArchivePluginInterface
    bool createArchive(const QStringList& files, 
                       const QString& outputPath, 
                       const QString& password,
                       std::function<void(int)> progressCallback = nullptr) override;
    
    QStringList supportedExtensions() const override { return {"7z"}; }
    bool supportsPassword() const override { return true; }
    bool supportsEncryptionMethod(const QString& method) const override { return method == "AES-256"; }
    QStringList availableEncryptionMethods() const override { return {"AES-256"}; }
    QString defaultExtension() const override { return "7z"; }
};

#endif // SEVENZIPPLUGIN_H
