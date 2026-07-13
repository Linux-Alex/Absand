// SPDX-License-Identifier: MPL-2.0

#include "ZipPlugin.h"
#include <zip.h>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QDebug>
#include <QByteArray>

#ifndef ZIP_EM_AES_256
#define ZIP_EM_AES_256 0x6601
#endif

#ifndef ZIP_EM_TRAD_PKWARE
#define ZIP_EM_TRAD_PKWARE 0x0001
#endif

ZipPlugin::ZipPlugin(QObject* parent) : QObject(parent) {}
ZipPlugin::~ZipPlugin() = default;

int ZipPlugin::getEncryptionMethod(const QString& method) const
{
    if (method == "AES-256") return ZIP_EM_AES_256;
    if (method == "Traditional PKWARE") return ZIP_EM_TRAD_PKWARE;
    return 0;
}

bool ZipPlugin::createArchive(const QStringList& files, 
                              const QString& outputPath, 
                              const QString& password,
                              std::function<void(int)> progressCallback)
{
    if (files.isEmpty()) {
        qWarning() << "No input files supplied";
        return false;
    }
    for (const QString& path : files) {
        if (!QFileInfo::exists(path)) {
            qWarning() << "Input path does not exist:" << path;
            return false;
        }
    }

    int error = 0;
    zip_t* archive = zip_open(outputPath.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    
    if (!archive) {
        qWarning() << "Cannot create archive:" << outputPath;
        return false;
    }
    
    bool useEncryption = !password.isEmpty();
    
    if (useEncryption) {
        if (zip_set_default_password(archive, password.toUtf8().constData()) != 0) {
            zip_close(archive);
            return false;
        }
    }
    
    QList<QByteArray> fileDataBuffers;
    bool success = true;
    int totalFiles = files.size();
    int currentFile = 0;
    int encryptionMethod = useEncryption ? ZIP_EM_AES_256 : 0;
    
    for (const QString& path : files) {
        QFileInfo info(path);
        QString relativeName = info.fileName();
        
        if (progressCallback) {
            progressCallback((currentFile * 100) / totalFiles);
        }
        
        if (info.isFile()) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                success = false;
                break;
            }
            
            QByteArray data = file.readAll();
            file.close();
            
            fileDataBuffers.append(data);
            QByteArray& storedData = fileDataBuffers.last();
            
            zip_source_t* source = zip_source_buffer(archive, storedData.data(), storedData.size(), 0);
            if (!source) {
                success = false;
                break;
            }
            
            zip_int64_t index = zip_file_add(archive, relativeName.toUtf8().constData(), source, ZIP_FL_OVERWRITE);
            if (index < 0) {
                zip_source_free(source);
                success = false;
                break;
            }
            
            if (useEncryption && encryptionMethod > 0) {
                if (zip_file_set_encryption(archive, index, encryptionMethod,
                                            password.toUtf8().constData()) != 0) {
                    success = false;
                    break;
                }
            }
            
        } else if (info.isDir()) {
            QString dirPath = relativeName + "/";
            zip_dir_add(archive, dirPath.toUtf8().constData(), ZIP_FL_ENC_UTF_8);
            
            QDirIterator it(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QString entryPath = it.filePath();
                QString entryRelativePath = relativeName + "/" + it.filePath().remove(0, path.length() + 1);
                
                if (it.fileInfo().isFile()) {
                    QFile file(entryPath);
                    if (!file.open(QIODevice::ReadOnly)) {
                        success = false;
                        break;
                    }
                    
                    QByteArray data = file.readAll();
                    file.close();
                    
                    fileDataBuffers.append(data);
                    QByteArray& storedData = fileDataBuffers.last();
                    
                    zip_source_t* source = zip_source_buffer(archive, storedData.data(), storedData.size(), 0);
                    if (!source) {
                        success = false;
                        break;
                    }
                    
                    zip_int64_t fileIndex = zip_file_add(archive, entryRelativePath.toUtf8().constData(), source, ZIP_FL_OVERWRITE);
                    if (fileIndex < 0) {
                        zip_source_free(source);
                        success = false;
                        break;
                    }
                    if (useEncryption && encryptionMethod > 0 &&
                        zip_file_set_encryption(archive, fileIndex, encryptionMethod,
                                                password.toUtf8().constData()) != 0) {
                        success = false;
                        break;
                    }
                }
            }
        }
        
        currentFile++;
    }
    
    if (zip_close(archive) != 0) {
        success = false;
    }
    
    if (!success) {
        QFile::remove(outputPath);
    }
    
    if (progressCallback) {
        progressCallback(100);
    }
    
    return success;
}
