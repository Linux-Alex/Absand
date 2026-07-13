// SPDX-License-Identifier: MPL-2.0

#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QVector>
#include <memory>

#include "core/PluginManager.h"
#include "models/FilePathListModel.h"
#include "models/ArchiveEntryModel.h"

#include <bit7z/bit7z.hpp>
#include <bit7z/bitarchivereader.hpp>

namespace bit7z {
class Bit7zLibrary;
class BitArchiveReader;
class BitInOutFormat;
}

class DestinationPluginInterface;
class ArchivePluginInterface;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(FilePathListModel* inputFilesModel READ inputFilesModel CONSTANT)
    Q_PROPERTY(ArchiveEntryModel* archiveEntriesModel READ archiveEntriesModel CONSTANT)
    Q_PROPERTY(QStringList archiveFormats READ archiveFormats NOTIFY pluginDataChanged)
    Q_PROPERTY(QStringList destinationTargets READ destinationTargets NOTIFY pluginDataChanged)
    Q_PROPERTY(QStringList destinationTargetKeys READ destinationTargetKeys NOTIFY pluginDataChanged)
    Q_PROPERTY(int archiveFormatIndex READ archiveFormatIndex WRITE setArchiveFormatIndex NOTIFY pluginDataChanged)
    Q_PROPERTY(int destinationTargetIndex READ destinationTargetIndex WRITE setDestinationTargetIndex NOTIFY pluginDataChanged)
    Q_PROPERTY(bool encryptArchive READ encryptArchive WRITE setEncryptArchive NOTIFY archiveOptionsChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY archiveOptionsChanged)
    Q_PROPERTY(QString destinationPath READ destinationPath WRITE setDestinationPath NOTIFY destinationChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString browserStatusMessage READ browserStatusMessage NOTIFY browserStatusMessageChanged)
    Q_PROPERTY(bool archiveLoaded READ archiveLoaded NOTIFY browserStateChanged)
    Q_PROPERTY(QString currentArchivePath READ currentArchivePath NOTIFY browserStateChanged)
    Q_PROPERTY(int browserViewMode READ browserViewMode WRITE setBrowserViewMode NOTIFY browserStateChanged)

public:
    explicit AppController(QObject* parent = nullptr);

    FilePathListModel* inputFilesModel() const;
    ArchiveEntryModel* archiveEntriesModel() const;
    QStringList archiveFormats() const;
    QStringList destinationTargets() const;
    QStringList destinationTargetKeys() const;
    int archiveFormatIndex() const;
    void setArchiveFormatIndex(int index);
    int destinationTargetIndex() const;
    void setDestinationTargetIndex(int index);
    bool encryptArchive() const;
    void setEncryptArchive(bool enabled);
    QString password() const;
    void setPassword(const QString& password);
    QString destinationPath() const;
    void setDestinationPath(const QString& path);
    QString statusMessage() const;
    QString browserStatusMessage() const;
    bool archiveLoaded() const;
    QString currentArchivePath() const;
    int browserViewMode() const;
    void setBrowserViewMode(int mode);

    Q_INVOKABLE void addFiles();
    Q_INVOKABLE void removeInputAt(int index);
    Q_INVOKABLE void clearInputs();
    Q_INVOKABLE QString chooseDirectory(const QString& title, const QString& startDir = QString()) const;
    Q_INVOKABLE void openDestinationFolderPicker();
    Q_INVOKABLE void configureDestinations();
    Q_INVOKABLE QVariantMap destinationConfigForIndex(int index) const;
    Q_INVOKABLE void saveDestinationConfigForIndex(int index, const QVariantMap& config);
    Q_INVOKABLE void createArchive();

    Q_INVOKABLE void openArchive();
    Q_INVOKABLE void addFilesToArchive();
    Q_INVOKABLE void refreshArchive();
    Q_INVOKABLE void browserSetCurrentIndex(int index);
    Q_INVOKABLE void browserOpenCurrent();
    Q_INVOKABLE void browserOpenCurrentWith();
    Q_INVOKABLE void browserExtractCurrent();
    Q_INVOKABLE void browserCopyCurrentPath();

signals:
    void pluginDataChanged();
    void archiveOptionsChanged();
    void destinationChanged();
    void statusMessageChanged();
    void browserStatusMessageChanged();
    void browserStateChanged();
    void showBrowserRequested();
    void showHomeRequested();
    void showDestinationSettingsRequested();

private:
    QString getDefaultFileName() const;
    QString currentArchiveExtension() const;
    qint64 calculateTotalSize() const;
    void updateDestinationPath();
    void loadPlugins();
    void syncDefaultsFromPlugins();
    ArchivePluginInterface* currentArchivePlugin() const;
    DestinationPluginInterface* currentDestinationPlugin() const;
    QVariantMap currentDestinationConfig() const;
    void loadArchiveEntries();
    bool loadArchiveFile(const QString& archivePath, const QString& password);
    QString promptPassword() const;
    QString promptDestinationDirectory(const QString& title, const QString& startDir) const;
    QString promptProgramPath() const;
    void showInfo(const QString& title, const QString& text) const;
    void showWarning(const QString& title, const QString& text) const;
    void showError(const QString& title, const QString& text) const;
    void transferArchiveIfNeeded(const QString& archivePath);

    PluginManager m_pluginManager;
    std::unique_ptr<FilePathListModel> m_inputFiles;
    std::unique_ptr<ArchiveEntryModel> m_archiveEntries;
    QStringList m_archiveFormatNames;
    QStringList m_destinationTargets;
    QVector<ArchivePluginInterface*> m_archivePlugins;
    QVector<DestinationPluginInterface*> m_destinationPlugins;
    int m_archiveFormatIndex = 0;
    int m_destinationTargetIndex = 0;
    bool m_encryptArchive = false;
    QString m_password;
    QString m_destinationPath;
    QString m_statusMessage;
    QString m_browserStatusMessage;
    bool m_archiveLoaded = false;
    QString m_currentArchivePath;
    QString m_currentArchivePassword;
    int m_browserViewMode = 0;
    int m_browserCurrentIndex = -1;
    std::unique_ptr<bit7z::Bit7zLibrary> m_library;
    std::unique_ptr<bit7z::BitArchiveReader> m_reader;
    const bit7z::BitInOutFormat* m_format = nullptr;
};

#endif // APPCONTROLLER_H
