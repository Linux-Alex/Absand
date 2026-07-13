// SPDX-License-Identifier: MPL-2.0

#ifndef ZIPPLUGIN_H
#define ZIPPLUGIN_H

#include <QObject>
#include <QStringList>
#include <functional>
#include "interfaces/ArchivePluginInterface.h"

// Forward declare libzip
struct zip;

class ZipPlugin : public QObject, public ArchivePluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ArchivePluginInterface_IID FILE "zip_plugin.json")
    Q_INTERFACES(ArchivePluginInterface)

public:
    explicit ZipPlugin(QObject* parent = nullptr);
    ~ZipPlugin();
    
    // PluginInterface
    QString name() const override { return "ZIP Archive"; }
    QString version() const override { return "1.0.0"; }
    QString description() const override { 
        return "ZIP compression with AES-256 encryption support"; 
    }
    QVariantMap metadata() const override {
        QVariantMap meta;
        meta["name"] = name();
        meta["version"] = version();
        meta["description"] = description();
        meta["author"] = "Absand Team";
        meta["formats"] = supportedExtensions();
        meta["supportsPassword"] = supportsPassword();
        meta["encryptionMethods"] = availableEncryptionMethods();
        return meta;
    }
    
    // ArchivePluginInterface
    bool createArchive(const QStringList& files, 
                       const QString& outputPath, 
                       const QString& password,
                       std::function<void(int)> progressCallback = nullptr) override;
    
    QStringList supportedExtensions() const override { return {"zip"}; }
    bool supportsPassword() const override { return true; }
    bool supportsEncryptionMethod(const QString& method) const override {
        return method == "AES-256";
    }
    QStringList availableEncryptionMethods() const override { 
        return {"AES-256"};
    }
    QString defaultExtension() const override { return "zip"; }

private:
    int getEncryptionMethod(const QString& method) const;
};

#endif // ZIPPLUGIN_H
