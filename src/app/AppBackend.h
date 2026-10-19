// SPDX-License-Identifier: MPL-2.0

#ifndef APPBACKEND_H
#define APPBACKEND_H

#include "core/PluginManager.h"
#include "core/UserConfig.h"

#include <QAbstractItemModel>

class FilePathListModel;

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class ArchivePluginInterface;
class DestinationPluginInterface;
class SendPluginInterface;

/**
 * Everything the Kirigami/QML frontend needs, exposed as one object.
 *
 * The user interface is deliberately thin: it renders plugin settings from the
 * ConfigSchema the plugins declare, and asks this backend to run the archive,
 * transfer, and delivery steps. Keeping that logic here means the QML shell and
 * the classic widget dialog behave identically.
 */
class AppBackend : public QObject
{
    Q_OBJECT

public:
    explicit AppBackend(QObject* parent = nullptr);
    ~AppBackend() override;

    // Input files
    Q_PROPERTY(QAbstractItemModel* inputModel READ inputModel CONSTANT)
    Q_PROPERTY(int inputCount READ inputCount NOTIFY inputChanged)
    Q_PROPERTY(QString inputSummary READ inputSummary NOTIFY inputChanged)
    Q_PROPERTY(QString totalSizeText READ totalSizeText NOTIFY inputChanged)

    // Archive options
    Q_PROPERTY(QVariantList formats READ formats NOTIFY formatChanged)
    Q_PROPERTY(int formatIndex READ formatIndex WRITE setFormatIndex NOTIFY formatChanged)
    Q_PROPERTY(bool formatSupportsEncryption READ formatSupportsEncryption NOTIFY formatChanged)
    Q_PROPERTY(QStringList encryptionMethods READ encryptionMethods NOTIFY formatChanged)
    Q_PROPERTY(int encryptionMethodIndex READ encryptionMethodIndex
                   WRITE setEncryptionMethodIndex NOTIFY formatChanged)
    Q_PROPERTY(bool encrypt READ encrypt WRITE setEncrypt NOTIFY encryptChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(QString passwordVisibleText READ passwordVisibleText NOTIFY passwordChanged)

    // Output location
    Q_PROPERTY(QString destinationFolder READ destinationFolder
                   WRITE setDestinationFolder NOTIFY outputChanged)
    Q_PROPERTY(QString defaultArchiveName READ defaultArchiveName NOTIFY formatChanged)
    Q_PROPERTY(QString outputFileName READ outputFileName
                   WRITE setOutputFileName NOTIFY outputChanged)

    // Destination plugin, profile and target
    Q_PROPERTY(QVariantList destinations READ destinations NOTIFY destinationChanged)
    Q_PROPERTY(int destinationIndex READ destinationIndex
                   WRITE setDestinationIndex NOTIFY destinationChanged)
    Q_PROPERTY(QString destinationName READ destinationName NOTIFY destinationChanged)
    Q_PROPERTY(QStringList destinationProfiles READ destinationProfiles NOTIFY destinationChanged)
    Q_PROPERTY(QString destinationProfile READ destinationProfile
                   WRITE setDestinationProfile NOTIFY destinationChanged)
    Q_PROPERTY(bool destinationHasTarget READ destinationHasTarget NOTIFY destinationChanged)
    Q_PROPERTY(QString destinationTargetLabel READ destinationTargetLabel NOTIFY destinationChanged)
    Q_PROPERTY(QString destinationTargetPlaceholder READ destinationTargetPlaceholder
                   NOTIFY destinationChanged)
    Q_PROPERTY(QString destinationTarget READ destinationTarget
                   WRITE setDestinationTarget NOTIFY destinationChanged)

    // Password delivery channel, profile and recipient
    Q_PROPERTY(QVariantList channels READ channels NOTIFY channelChanged)
    Q_PROPERTY(int channelIndex READ channelIndex WRITE setChannelIndex NOTIFY channelChanged)
    Q_PROPERTY(QString channelName READ channelName NOTIFY channelChanged)
    Q_PROPERTY(QStringList channelProfiles READ channelProfiles NOTIFY channelChanged)
    Q_PROPERTY(QString channelProfile READ channelProfile
                   WRITE setChannelProfile NOTIFY channelChanged)
    Q_PROPERTY(bool channelHasRecipient READ channelHasRecipient NOTIFY channelChanged)
    Q_PROPERTY(QString channelRecipientLabel READ channelRecipientLabel NOTIFY channelChanged)
    Q_PROPERTY(QString channelRecipientPlaceholder READ channelRecipientPlaceholder
                   NOTIFY channelChanged)
    Q_PROPERTY(QString channelRecipient READ channelRecipient
                   WRITE setChannelRecipient NOTIFY channelChanged)

    // Progress and result
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool resultVisible READ resultVisible NOTIFY resultChanged)
    Q_PROPERTY(QString resultTitle READ resultTitle NOTIFY resultChanged)
    Q_PROPERTY(QString resultDetails READ resultDetails NOTIFY resultChanged)
    Q_PROPERTY(QString resultProblems READ resultProblems NOTIFY resultChanged)
    Q_PROPERTY(QString resultShareLink READ resultShareLink NOTIFY resultChanged)
    Q_PROPERTY(bool resultHasPassword READ resultHasPassword NOTIFY resultChanged)

    // Appearance
    Q_PROPERTY(QString applicationVersion READ applicationVersion CONSTANT)
    Q_PROPERTY(QStringList languages READ languages CONSTANT)
    Q_PROPERTY(int languageIndex READ languageIndex WRITE setLanguageIndex NOTIFY languageChanged)

    QAbstractItemModel* inputModel();
    int inputCount() const;
    QString inputSummary() const;
    QString totalSizeText() const;

    QVariantList formats() const;
    int formatIndex() const;
    void setFormatIndex(int index);
    bool formatSupportsEncryption() const;
    QStringList encryptionMethods() const;
    int encryptionMethodIndex() const;
    void setEncryptionMethodIndex(int index);
    bool encrypt() const;
    void setEncrypt(bool enabled);
    QString password() const;
    void setPassword(const QString& password);
    QString passwordVisibleText() const;

    QString destinationFolder() const;
    void setDestinationFolder(const QString& folder);
    QString defaultArchiveName() const;
    QString outputFileName() const;
    void setOutputFileName(const QString& name);

    QVariantList destinations() const;
    int destinationIndex() const;
    void setDestinationIndex(int index);
    QString destinationName() const;
    QStringList destinationProfiles() const;
    QString destinationProfile() const;
    void setDestinationProfile(const QString& name);
    bool destinationHasTarget() const;
    QString destinationTargetLabel() const;
    QString destinationTargetPlaceholder() const;
    QString destinationTarget() const;
    void setDestinationTarget(const QString& value);

    QVariantList channels() const;
    int channelIndex() const;
    void setChannelIndex(int index);
    QString channelName() const;
    QStringList channelProfiles() const;
    QString channelProfile() const;
    void setChannelProfile(const QString& name);
    bool channelHasRecipient() const;
    QString channelRecipientLabel() const;
    QString channelRecipientPlaceholder() const;
    QString channelRecipient() const;
    void setChannelRecipient(const QString& value);

    bool busy() const;
    int progress() const;
    QString statusText() const;
    bool resultVisible() const;
    QString resultTitle() const;
    QString resultDetails() const;
    QString resultProblems() const;
    QString resultShareLink() const;
    bool resultHasPassword() const;

    QString applicationVersion() const;
    QStringList languages() const;
    int languageIndex() const;
    void setLanguageIndex(int index);

    Q_INVOKABLE void addFiles(const QStringList& paths);
    Q_INVOKABLE void removeFile(int row);
    Q_INVOKABLE void clearFiles();
    Q_INVOKABLE void generatePassword();
    Q_INVOKABLE void copyPassword();
    Q_INVOKABLE void copyShareLink();
    Q_INVOKABLE void dismissResult();
    Q_INVOKABLE void openArchiveBrowser();
    Q_INVOKABLE void chooseArchiveFormat(int index);
    Q_INVOKABLE bool startArchive();

    /// Plugin settings for the schema driven settings pages.
    Q_INVOKABLE QVariantList pluginSettings(bool sendMode);
    Q_INVOKABLE QVariantMap profileValues(bool sendMode, const QString& key, const QString& profile);
    /// Returns an empty string on success, or a translated error message.
    Q_INVOKABLE QString saveProfileValues(bool sendMode, const QString& key, const QString& profile,
                                          const QVariantMap& values);
    Q_INVOKABLE QString createProfile(bool sendMode, const QString& key, const QString& name,
                                      const QString& copyFrom);
    Q_INVOKABLE QString renameProfile(bool sendMode, const QString& key, const QString& from,
                                      const QString& to);
    Q_INVOKABLE QString deleteProfile(bool sendMode, const QString& key, const QString& name);
    Q_INVOKABLE void selectProfile(bool sendMode, const QString& key, const QString& name);

signals:
    void inputChanged();
    void formatChanged();
    void encryptChanged();
    void passwordChanged();
    void outputChanged();
    void destinationChanged();
    void channelChanged();
    void busyChanged();
    void progressChanged();
    void statusChanged();
    void resultChanged();
    void languageChanged();

private:
    using ConfigKind = UserConfig::ConfigKind;

    ArchivePluginInterface* currentArchivePlugin() const;
    DestinationPluginInterface* currentDestination() const;
    SendPluginInterface* currentChannel() const;
    QStringList files() const;

    void reloadDestinationPlugins();
    void reloadChannels();
    void refreshDestinationProfiles();
    void refreshChannelProfiles();
    QString storedProfileName(ConfigKind kind, const QString& key) const;
    QVariantMap mergedConfig(ConfigKind kind, const QString& key, const QString& profile) const;
    void persistRecipient(ConfigKind kind, const QString& key, const QString& fieldKey,
                          const QString& value);
    void setStatus(const QString& text);
    void setProgress(int value);
    void setBusy(bool busy);
    void buildResult(bool archiveCreated, bool transferSucceeded, const QString& transferError,
                     bool deliverySucceeded, const QString& deliveryError, const QString& shareLink,
                     const QString& archivePath);
    QString sanitizedBaseName() const;

    PluginManager m_pluginManager;
    FilePathListModel* m_inputModel = nullptr;

    QVector<ArchivePluginInterface*> m_archivePlugins;
    QVector<DestinationPluginInterface*> m_destinationPlugins;
    QVector<SendPluginInterface*> m_sendPlugins;

    int m_formatIndex = 0;
    int m_encryptionMethodIndex = 0;
    bool m_encrypt = false;
    QString m_password;
    QString m_destinationFolder;
    QString m_outputFileName;
    QString m_generatedOutputFileName;
    QString m_destinationKey;
    QString m_destinationProfile;
    QString m_destinationTargetOverride;
    int m_channelIndex = -1;
    QString m_channelProfile;
    QString m_channelRecipientOverride;
    QString m_lastShareLink;
    bool m_trackedOutputName = true;

    bool m_busy = false;
    int m_progress = 0;
    QString m_statusText;

    bool m_resultVisible = false;
    QString m_resultTitle;
    QString m_resultDetails;
    QString m_resultProblems;
    QString m_resultShareLink;
    bool m_resultHasPassword = false;

    QVariantMap m_lastDestinationConfig;
    QVariantMap m_lastChannelConfig;
};

#endif // APPBACKEND_H