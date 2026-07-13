// SPDX-License-Identifier: MPL-2.0

#ifndef ARCHIVEPLUGININTERFACE_H
#define ARCHIVEPLUGININTERFACE_H

#include "PluginInterface.h"
#include <QStringList>
#include <functional>

class ArchivePluginInterface : public PluginInterface
{
public:
    virtual ~ArchivePluginInterface() = default;
    
    // Core archive operations
    virtual bool createArchive(const QStringList& files, 
                               const QString& outputPath, 
                               const QString& password,
                               std::function<void(int)> progressCallback = nullptr) = 0;
    
    // Supported formats and features
    virtual QStringList supportedExtensions() const = 0;
    virtual bool supportsPassword() const = 0;
    virtual bool supportsEncryptionMethod(const QString& method) const = 0;
    virtual QStringList availableEncryptionMethods() const = 0;
    
    // Archive info
    virtual QString defaultExtension() const = 0;
};

#define ArchivePluginInterface_IID "com.absand.ArchivePluginInterface/1.0"
Q_DECLARE_INTERFACE(ArchivePluginInterface, ArchivePluginInterface_IID)

#endif // ARCHIVEPLUGININTERFACE_H