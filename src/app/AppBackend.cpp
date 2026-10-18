// SPDX-License-Identifier: MPL-2.0

#include "AppBackend.h"

#include "models/FilePathListModel.h"
#include "ui/ArchiveBrowserDialog.h"

#include "interfaces/ArchivePluginInterface.h"
#include "interfaces/DestinationPluginInterface.h"
#include "interfaces/SendPluginInterface.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRandomGenerator>

namespace {

constexpr int PasswordLength = 16;

QString formatSize(qint64 bytes)
{
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    }
    if (bytes < 1024LL * 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    }
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
}

QString fieldTypeName(ConfigField::Type type)
{
    switch (type) {
    case ConfigField::Type::Password:
        return QStringLiteral("password");
    case ConfigField::Type::Integer:
        return QStringLiteral("integer");
    case ConfigField::Type::Bool:
        return QStringLiteral("bool");
    case ConfigField::Type::Choice:
        return QStringLiteral("choice");
    case ConfigField::Type::Multiline:
        return QStringLiteral("multiline");
    case ConfigField::Type::Text:
        break;
    }
    return QStringLiteral("text");
}

QString browseName(ConfigField::Browse browse)
{
    switch (browse) {
    case ConfigField::Browse::Directory:
        return QStringLiteral("directory");
    case ConfigField::Browse::OpenFile:
        return QStringLiteral("openFile");
    case ConfigField::Browse::SaveFile:
        return QStringLiteral("saveFile");
    case ConfigField::Browse::None:
        break;
    }
    return QString();
}

QVariantMap fieldToMap(const ConfigField& field)
{
    QVariantMap map;
    map.insert(QStringLiteral("key"), field.key);
    map.insert(QStringLiteral("label"), field.label);
    map.insert(QStringLiteral("help"), field.help);
    map.insert(QStringLiteral("placeholder"), field.placeholder);
    map.insert(QStringLiteral("type"), fieldTypeName(field.type));
    map.insert(QStringLiteral("browse"), browseName(field.browse));
    map.insert(QStringLiteral("choices"), field.choices);
    map.insert(QStringLiteral("choiceLabels"),
               field.choiceLabels.size() == field.choices.size() ? field.choiceLabels : field.choices);
    map.insert(QStringLiteral("defaultValue"), field.defaultValue);
    map.insert(QStringLiteral("required"), field.isRequired());
    map.insert(QStringLiteral("recipient"), field.isRecipient());
    map.insert(QStringLiteral("advanced"), field.isAdvanced());
    return map;
}

QVariantList schemaToMaps(const ConfigSchema& schema)
{
    QVariantList list;
    for (const ConfigField& field : schema) {
        list.append(fieldToMap(field));
    }
    return list;
}

QString sanitizeBaseName(QString name)
{
    name = name.trimmed();
    const QString forbidden = QStringLiteral("\\/:*?\"<>| ");
    for (const QChar character : forbidden) {
        name.replace(character, QLatin1Char('_'));
    }
    while (name.contains(QStringLiteral("__"))) {
        name.replace(QStringLiteral("__"), QStringLiteral("_"));
    }
    return name.isEmpty() ? QStringLiteral("absand") : name;
}

qint64 pathSize(const QString& path)
{
    const QFileInfo info(path);
    if (info.isFile()) {
        return info.size();
    }
    if (!info.isDir()) {
        return 0;
    }

    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

QStringList defaultLanguages()
{
    return { QStringLiteral("English"), QStringLiteral("Slovenian") };
}

QStringList languageLocales()
{
    return { QStringLiteral("en_US"), QStringLiteral("sl_SI") };
}

} // namespace

AppBackend::AppBackend(QObject* parent)
    : QObject(parent)
    , m_inputModel(new FilePathListModel(this))
{
    m_pluginManager.loadAllPlugins();
    m_archivePlugins = m_pluginManager.archivePlugins();
    m_destinationPlugins = m_pluginManager.destinationPlugins();
    m_sendPlugins = m_pluginManager.sendPlugins();

    // Prefer ZIP, like the classic dialog does.
    for (int index = 0; index < m_archivePlugins.size(); ++index) {
        if (m_archivePlugins.at(index)->defaultExtension().compare(QStringLiteral("zip"),
                                                                  Qt::CaseInsensitive) == 0) {
            m_formatIndex = index;
            break;
        }
    }

    m_destinationFolder = QDir::homePath();
    reloadDestinationPlugins();
    reloadChannels();
    generatePassword();

    qInfo() << "Kirigami frontend ready:" << m_archivePlugins.size() << "archive,"
            << m_destinationPlugins.size() << "destination," << m_sendPlugins.size() << "channel plugins";
}

AppBackend::~AppBackend() = default;

// ---------------------------------------------------------------- input files

QAbstractItemModel* AppBackend::inputModel()
{
    return m_inputModel;
}

QStringList AppBackend::files() const
{
    QStringList paths;
    for (int row = 0; row < m_inputModel->rowCount(); ++row) {
        paths.append(m_inputModel->pathAt(row));
    }
    return paths;
}

int AppBackend::inputCount() const
{
    return m_inputModel->rowCount();
}

QString AppBackend::inputSummary() const
{
    if (inputCount() == 0) {
        return tr("No files selected. Add files or folders to archive.");
    }
    if (inputCount() == 1) {
        return tr("Selected: %1").arg(QFileInfo(files().first()).fileName());
    }
    return tr("Selected: %1 items").arg(inputCount());
}

QString AppBackend::totalSizeText() const
{
    qint64 total = 0;
    for (const QString& path : files()) {
        total += pathSize(path);
    }
    return tr("Total size: %1").arg(formatSize(total));
}

void AppBackend::addFiles(const QStringList& paths)
{
    QStringList accepted;
    for (const QString& path : paths) {
        const QString trimmed = path.trimmed();
        if (trimmed.isEmpty() || !QFileInfo::exists(trimmed)) {
            continue;
        }
        accepted.append(QFileInfo(trimmed).absoluteFilePath());
    }

    if (accepted.isEmpty()) {
        return;
    }

    m_inputModel->addPaths(accepted);
    m_trackedOutputName = true;
    emit inputChanged();
}

void AppBackend::removeFile(int row)
{
    m_inputModel->removeAt(row);
    emit inputChanged();
}

void AppBackend::clearFiles()
{
    m_inputModel->clear();
    emit inputChanged();
}

// ------------------------------------------------------------ archive options

QVariantList AppBackend::formats() const
{
    QVariantList list;
    for (ArchivePluginInterface* plugin : m_archivePlugins) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), plugin->name());
        entry.insert(QStringLiteral("extension"), plugin->defaultExtension());
        entry.insert(QStringLiteral("supportsPassword"), plugin->supportsPassword());
        list.append(entry);
    }
    return list;
}

ArchivePluginInterface* AppBackend::currentArchivePlugin() const
{
    if (m_formatIndex < 0 || m_formatIndex >= m_archivePlugins.size()) {
        return nullptr;
    }
    return m_archivePlugins.at(m_formatIndex);
}

int AppBackend::formatIndex() const
{
    return m_formatIndex;
}

void AppBackend::setFormatIndex(int index)
{
    if (index == m_formatIndex || index < 0 || index >= m_archivePlugins.size()) {
        return;
    }

    m_formatIndex = index;
    m_encryptionMethodIndex = 0;
    if (!formatSupportsEncryption() && m_encrypt) {
        m_encrypt = false;
        emit encryptChanged();
    }
    m_trackedOutputName = true;
    emit formatChanged();
    emit outputChanged();
}

void AppBackend::chooseArchiveFormat(int index)
{
    setFormatIndex(index);
}

bool AppBackend::formatSupportsEncryption() const
{
    ArchivePluginInterface* plugin = currentArchivePlugin();
    return plugin && plugin->supportsPassword();
}

QStringList AppBackend::encryptionMethods() const
{
    ArchivePluginInterface* plugin = currentArchivePlugin();
    return plugin ? plugin->availableEncryptionMethods() : QStringList();
}

int AppBackend::encryptionMethodIndex() const
{
    return m_encryptionMethodIndex;
}

void AppBackend::setEncryptionMethodIndex(int index)
{
    if (index == m_encryptionMethodIndex) {
        return;
    }
    m_encryptionMethodIndex = index;
    emit formatChanged();
}

bool AppBackend::encrypt() const
{
    return m_encrypt;
}

void AppBackend::setEncrypt(bool enabled)
{
    const bool effective = enabled && formatSupportsEncryption();
    if (effective == m_encrypt) {
        return;
    }
    m_encrypt = effective;
    emit encryptChanged();
}

QString AppBackend::password() const
{
    return m_password;
}

void AppBackend::setPassword(const QString& password)
{
    if (password == m_password) {
        return;
    }
    m_password = password;
    emit passwordChanged();
}

QString AppBackend::passwordVisibleText() const
{
    return m_password;
}

void AppBackend::generatePassword()
{
    const QString charset = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%");
    QString generated;
    generated.reserve(PasswordLength);
    for (int index = 0; index < PasswordLength; ++index) {
        generated.append(charset.at(QRandomGenerator::securelySeeded().bounded(charset.size())));
    }
    m_password = generated;
    emit passwordChanged();
}

void AppBackend::copyPassword()
{
    if (m_password.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(m_password);
    setStatus(tr("Password copied to clipboard."));
}

void AppBackend::copyShareLink()
{
    if (m_resultShareLink.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(m_resultShareLink);
    setStatus(tr("Download link copied to clipboard."));
}

// --------------------------------------------------------------- output paths

QString AppBackend::defaultArchiveName() const
{
    ArchivePluginInterface* plugin = currentArchivePlugin();
    const QString extension = plugin ? plugin->defaultExtension() : QStringLiteral("zip");

    const QStringList paths = files();
    QString base = QStringLiteral("absand");
    if (paths.size() == 1) {
        base = sanitizeBaseName(QFileInfo(paths.first()).completeBaseName());
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
    return QStringLiteral("%1_%2.%3").arg(base, timestamp, extension);
}

QString AppBackend::outputFileName() const
{
    if (m_outputFileName.isEmpty() || m_trackedOutputName) {
        return defaultArchiveName();
    }
    return m_outputFileName;
}

void AppBackend::setOutputFileName(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed == m_outputFileName) {
        return;
    }
    m_outputFileName = trimmed;
    m_trackedOutputName = trimmed.isEmpty() || trimmed == defaultArchiveName();
    emit outputChanged();
}

QString AppBackend::destinationFolder() const
{
    return m_destinationFolder;
}

void AppBackend::setDestinationFolder(const QString& folder)
{
    const QString cleaned = folder.trimmed();
    if (cleaned == m_destinationFolder) {
        return;
    }
    m_destinationFolder = cleaned;
    emit outputChanged();
}

// ------------------------------------------------- destination plugin/profile

DestinationPluginInterface* AppBackend::currentDestination() const
{
    for (DestinationPluginInterface* plugin : m_destinationPlugins) {
        if (plugin->configKey() == m_destinationKey) {
            return plugin;
        }
    }
    return m_destinationPlugins.isEmpty() ? nullptr : m_destinationPlugins.first();
}

void AppBackend::reloadDestinationPlugins()
{
    m_destinationPlugins = m_pluginManager.destinationPlugins();

    // A plain local folder needs no credentials, so it is the default target.
    if (m_destinationKey.isEmpty()) {
        for (DestinationPluginInterface* plugin : m_destinationPlugins) {
            if (plugin->channelName() == QStringLiteral("local_folder")) {
                m_destinationKey = plugin->configKey();
                break;
            }
        }
        if (m_destinationKey.isEmpty() && !m_destinationPlugins.isEmpty()) {
            m_destinationKey = m_destinationPlugins.first()->configKey();
        }
    }

    refreshDestinationProfiles();

    if (DestinationPluginInterface* plugin = currentDestination()) {
        const QVariantMap config = mergedConfig(ConfigKind::Destination, plugin->configKey(),
                                                m_destinationProfile);
        const QString folder = plugin->suggestedLocalSaveFolder(config);
        if (!folder.isEmpty()) {
            m_destinationFolder = folder;
        }
    }

    emit destinationChanged();
    emit outputChanged();
}

QVariantList AppBackend::destinations() const
{
    QVariantList list;
    for (DestinationPluginInterface* plugin : m_destinationPlugins) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), plugin->name());
        entry.insert(QStringLiteral("key"), plugin->configKey());
        list.append(entry);
    }
    return list;
}

int AppBackend::destinationIndex() const
{
    return m_destinationPlugins.indexOf(currentDestination());
}

void AppBackend::setDestinationIndex(int index)
{
    if (index < 0 || index >= m_destinationPlugins.size()) {
        return;
    }
    const QString key = m_destinationPlugins.at(index)->configKey();
    if (key == m_destinationKey) {
        return;
    }

    m_destinationKey = key;
    m_destinationProfile.clear();
    m_destinationTargetOverride.clear();
    refreshDestinationProfiles();

    if (DestinationPluginInterface* plugin = currentDestination()) {
        const QVariantMap config = mergedConfig(ConfigKind::Destination, plugin->configKey(),
                                                m_destinationProfile);
        const QString folder = plugin->suggestedLocalSaveFolder(config);
        if (!folder.isEmpty()) {
            m_destinationFolder = folder;
        }
    }

    emit destinationChanged();
    emit outputChanged();
}

QString AppBackend::destinationName() const
{
    DestinationPluginInterface* plugin = currentDestination();
    return plugin ? plugin->name() : QString();
}

QString AppBackend::storedProfileName(ConfigKind kind, const QString& key) const
{
    const QString stored = UserConfig::activeProfile(kind, key);
    return stored.isEmpty() ? UserConfig::defaultProfileName() : stored;
}

void AppBackend::refreshDestinationProfiles()
{
    DestinationPluginInterface* plugin = currentDestination();
    if (!plugin) {
        m_destinationProfile.clear();
        return;
    }

    const QStringList names = UserConfig::profileNames(ConfigKind::Destination, plugin->configKey());
    if (!names.contains(m_destinationProfile)) {
        m_destinationProfile = storedProfileName(ConfigKind::Destination, plugin->configKey());
    }
}

QStringList AppBackend::destinationProfiles() const
{
    DestinationPluginInterface* plugin = currentDestination();
    return plugin ? UserConfig::profileNames(ConfigKind::Destination, plugin->configKey())
                  : QStringList();
}

QString AppBackend::destinationProfile() const
{
    return m_destinationProfile;
}

void AppBackend::setDestinationProfile(const QString& name)
{
    DestinationPluginInterface* plugin = currentDestination();
    if (!plugin || name.trimmed().isEmpty() || name == m_destinationProfile) {
        return;
    }

    const ConfigSchema schema = plugin->configSchema();
    if (const ConfigField* target = schemaRecipientField(schema)) {
        persistRecipient(ConfigKind::Destination, plugin->configKey(), target->key,
                         m_destinationTargetOverride);
    }

    m_destinationProfile = name;
    m_destinationTargetOverride.clear();
    UserConfig::setActiveProfile(ConfigKind::Destination, plugin->configKey(), name);

    const QVariantMap config = mergedConfig(ConfigKind::Destination, plugin->configKey(), name);
    const QString folder = plugin->suggestedLocalSaveFolder(config);
    if (!folder.isEmpty()) {
        m_destinationFolder = folder;
    }

    emit destinationChanged();
    emit outputChanged();
}

bool AppBackend::destinationHasTarget() const
{
    DestinationPluginInterface* plugin = currentDestination();
    if (!plugin) {
        return false;
    }
    const ConfigSchema schema = plugin->configSchema();
    return schemaRecipientField(schema) != nullptr;
}

QString AppBackend::destinationTargetLabel() const
{
    DestinationPluginInterface* plugin = currentDestination();
    if (!plugin) {
        return {};
    }
    const ConfigSchema schema = plugin->configSchema();
    const ConfigField* target = schemaRecipientField(schema);
    return target ? target->label : QString();
}

QString AppBackend::destinationTargetPlaceholder() const
{
    DestinationPluginInterface* plugin = currentDestination();
    if (!plugin) {
        return {};
    }
    const ConfigSchema schema = plugin->configSchema();
    const ConfigField* target = schemaRecipientField(schema);
    return target ? target->placeholder : QString();
}

QString AppBackend::destinationTarget() const
{
    if (!m_destinationTargetOverride.isEmpty()) {
        return m_destinationTargetOverride;
    }

    DestinationPluginInterface* plugin = currentDestination();
    if (!plugin) {
        return {};
    }
    const ConfigSchema schema = plugin->configSchema();
    const ConfigField* target = schemaRecipientField(schema);
    if (!target) {
        return {};
    }
    return mergedConfig(ConfigKind::Destination, plugin->configKey(), m_destinationProfile)
        .value(target->key)
        .toString();
}

void AppBackend::setDestinationTarget(const QString& value)
{
    if (value == m_destinationTargetOverride) {
        return;
    }
    m_destinationTargetOverride = value;
    emit destinationChanged();
}

// ------------------------------------------------------ channel and recipient

void AppBackend::reloadChannels()
{
    m_sendPlugins = m_pluginManager.sendPlugins();
    refreshChannelProfiles();
    emit channelChanged();
}

SendPluginInterface* AppBackend::currentChannel() const
{
    if (m_channelIndex < 0 || m_channelIndex >= m_sendPlugins.size()) {
        return nullptr;
    }
    return m_sendPlugins.at(m_channelIndex);
}

QVariantList AppBackend::channels() const
{
    // Entry 0 is "Do not send", so the combo box can stay a plain list.
    QVariantList list;
    QVariantMap none;
    none.insert(QStringLiteral("name"), tr("Do not send"));
    none.insert(QStringLiteral("key"), QString());
    list.append(none);

    for (SendPluginInterface* plugin : m_sendPlugins) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), plugin->name());
        entry.insert(QStringLiteral("key"), plugin->configKey());
        list.append(entry);
    }
    return list;
}

int AppBackend::channelIndex() const
{
    return m_channelIndex < 0 ? 0 : m_channelIndex + 1;
}

void AppBackend::setChannelIndex(int index)
{
    const int target = index <= 0 ? -1 : index - 1;
    if (target == m_channelIndex || target >= m_sendPlugins.size()) {
        return;
    }

    if (SendPluginInterface* previous = currentChannel()) {
        const ConfigSchema schema = previous->configSchema();
        if (const ConfigField* recipient = schemaRecipientField(schema)) {
            persistRecipient(ConfigKind::Send, previous->configKey(), recipient->key,
                             m_channelRecipientOverride);
        }
    }

    m_channelIndex = target;
    m_channelProfile.clear();
    m_channelRecipientOverride.clear();
    refreshChannelProfiles();
    emit channelChanged();
}

QString AppBackend::channelName() const
{
    SendPluginInterface* plugin = currentChannel();
    return plugin ? plugin->name() : tr("Do not send");
}

QStringList AppBackend::channelProfiles() const
{
    SendPluginInterface* plugin = currentChannel();
    return plugin ? UserConfig::profileNames(ConfigKind::Send, plugin->configKey()) : QStringList();
}

void AppBackend::refreshChannelProfiles()
{
    SendPluginInterface* plugin = currentChannel();
    if (!plugin) {
        m_channelProfile.clear();
        return;
    }

    const QStringList names = UserConfig::profileNames(ConfigKind::Send, plugin->configKey());
    if (!names.contains(m_channelProfile)) {
        m_channelProfile = storedProfileName(ConfigKind::Send, plugin->configKey());
    }
}

QString AppBackend::channelProfile() const
{
    return m_channelProfile;
}

void AppBackend::setChannelProfile(const QString& name)
{
    SendPluginInterface* plugin = currentChannel();
    if (!plugin || name.trimmed().isEmpty() || name == m_channelProfile) {
        return;
    }

    const ConfigSchema schema = plugin->configSchema();
    if (const ConfigField* recipient = schemaRecipientField(schema)) {
        persistRecipient(ConfigKind::Send, plugin->configKey(), recipient->key,
                         m_channelRecipientOverride);
    }

    m_channelProfile = name;
    m_channelRecipientOverride.clear();
    UserConfig::setActiveProfile(ConfigKind::Send, plugin->configKey(), name);
    emit channelChanged();
}

bool AppBackend::channelHasRecipient() const
{
    SendPluginInterface* plugin = currentChannel();
    if (!plugin) {
        return false;
    }
    const ConfigSchema schema = plugin->configSchema();
    return schemaRecipientField(schema) != nullptr;
}

QString AppBackend::channelRecipientLabel() const
{
    SendPluginInterface* plugin = currentChannel();
    if (!plugin) {
        return {};
    }
    const ConfigSchema schema = plugin->configSchema();
    const ConfigField* recipient = schemaRecipientField(schema);
    return recipient ? recipient->label : QString();
}

QString AppBackend::channelRecipientPlaceholder() const
{
    SendPluginInterface* plugin = currentChannel();
    if (!plugin) {
        return {};
    }
    const ConfigSchema schema = plugin->configSchema();
    const ConfigField* recipient = schemaRecipientField(schema);
    return recipient ? recipient->placeholder : QString();
}

QString AppBackend::channelRecipient() const
{
    if (!m_channelRecipientOverride.isEmpty()) {
        return m_channelRecipientOverride;
    }

    SendPluginInterface* plugin = currentChannel();
    if (!plugin) {
        return {};
    }
    const ConfigSchema schema = plugin->configSchema();
    const ConfigField* recipient = schemaRecipientField(schema);
    if (!recipient) {
        return {};
    }
    return mergedConfig(ConfigKind::Send, plugin->configKey(), m_channelProfile)
        .value(recipient->key)
        .toString();
}

void AppBackend::setChannelRecipient(const QString& value)
{
    if (value == m_channelRecipientOverride) {
        return;
    }
    m_channelRecipientOverride = value;
    emit channelChanged();
}

// ------------------------------------------------------------ config plumbing

QVariantMap AppBackend::mergedConfig(ConfigKind kind, const QString& key,
                                     const QString& profile) const
{
    QVariantMap config;
    const QString name = profile.isEmpty() ? storedProfileName(kind, key) : profile;

    if (kind == ConfigKind::Destination) {
        for (DestinationPluginInterface* plugin : m_destinationPlugins) {
            if (plugin->configKey() == key) {
                config = plugin->defaultConfiguration();
                break;
            }
        }
    } else {
        for (SendPluginInterface* plugin : m_sendPlugins) {
            if (plugin->configKey() == key) {
                config = plugin->defaultConfiguration();
                break;
            }
        }
    }

    const QVariantMap stored = UserConfig::loadProfile(kind, key, name);
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
        config.insert(it.key(), it.value());
    }
    return config;
}

void AppBackend::persistRecipient(ConfigKind kind, const QString& key, const QString& fieldKey,
                                  const QString& value)
{
    if (value.isEmpty()) {
        return;
    }

    const QString profile = storedProfileName(kind, key);
    QVariantMap config = UserConfig::loadProfile(kind, key, profile);
    if (config.value(fieldKey).toString() == value) {
        return;
    }
    config.insert(fieldKey, value);
    UserConfig::saveProfile(kind, key, profile, config);
}

// ------------------------------------------------------------------ status

bool AppBackend::busy() const
{
    return m_busy;
}

int AppBackend::progress() const
{
    return m_progress;
}

QString AppBackend::statusText() const
{
    return m_statusText;
}

void AppBackend::setStatus(const QString& text)
{
    if (text == m_statusText) {
        return;
    }
    m_statusText = text;
    emit statusChanged();
}

void AppBackend::setProgress(int value)
{
    const int clamped = qBound(0, value, 100);
    if (clamped == m_progress) {
        return;
    }
    m_progress = clamped;
    emit progressChanged();
}

void AppBackend::setBusy(bool busy)
{
    if (busy == m_busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

// ------------------------------------------------------------------ result

bool AppBackend::resultVisible() const
{
    return m_resultVisible;
}

QString AppBackend::resultTitle() const
{
    return m_resultTitle;
}

QString AppBackend::resultDetails() const
{
    return m_resultDetails;
}

QString AppBackend::resultProblems() const
{
    return m_resultProblems;
}

QString AppBackend::resultShareLink() const
{
    return m_resultShareLink;
}

bool AppBackend::resultHasPassword() const
{
    return m_resultHasPassword;
}

void AppBackend::dismissResult()
{
    if (!m_resultVisible) {
        return;
    }
    m_resultVisible = false;
    emit resultChanged();
}

void AppBackend::openArchiveBrowser()
{
    ArchiveBrowserDialog dialog;
    dialog.exec();
}

void AppBackend::buildResult(bool archiveCreated, bool transferSucceeded,
                             const QString& transferError, bool deliverySucceeded,
                             const QString& deliveryError, const QString& shareLink,
                             const QString& archivePath)
{
    const QFileInfo archiveInfo(archivePath);

    QStringList details;
    details << archiveInfo.fileName();
    details << tr("Size: %1").arg(formatSize(archiveInfo.size()));

    QStringList problems;
    if (!archiveCreated) {
        problems << tr("The archive could not be created.");
    }

    if (DestinationPluginInterface* destination = currentDestination()) {
        if (transferSucceeded) {
            details << tr("Transferred with the %1 destination.").arg(destination->name());
        } else {
            problems << tr("Transfer failed — %1").arg(transferError);
        }
    }
    if (!shareLink.isEmpty()) {
        details << tr("Download link: %1").arg(shareLink);
    }

    const bool passwordAttempted = m_encrypt && !m_password.isEmpty();
    if (passwordAttempted) {
        if (deliverySucceeded) {
            if (SendPluginInterface* channel = currentChannel()) {
                details << tr("Password delivered via %1.").arg(channel->name());
            }
        } else if (!transferSucceeded) {
            problems << tr("The password was not sent because the transfer failed.");
        } else {
            problems << tr("Password delivery failed — %1").arg(deliveryError);
        }
    }

    m_resultDetails = details.join(QLatin1Char('\n'));
    m_resultProblems = problems.join(QLatin1Char('\n'));
    m_resultTitle = problems.isEmpty() ? tr("Archive created") : tr("Archive created with problems");
    m_resultShareLink = shareLink;
    m_resultHasPassword = passwordAttempted;
    m_resultVisible = true;
    emit resultChanged();
}

// -------------------------------------------------------------- archive flow

bool AppBackend::startArchive()
{
    if (m_busy) {
        return false;
    }

    const QStringList paths = files();
    if (paths.isEmpty()) {
        setStatus(tr("Select at least one file or folder first."));
        return false;
    }

    ArchivePluginInterface* archivePlugin = currentArchivePlugin();
    if (!archivePlugin) {
        setStatus(tr("No archive plugin is available."));
        return false;
    }

    const bool useEncryption = m_encrypt && archivePlugin->supportsPassword();
    if (useEncryption && m_password.isEmpty()) {
        setStatus(tr("Please set a password for encryption."));
        return false;
    }

    QString folder = m_destinationFolder.trimmed();
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    if (!QDir().mkpath(folder)) {
        setStatus(tr("The folder %1 could not be created.").arg(folder));
        return false;
    }

    QString fileName = outputFileName().trimmed();
    if (fileName.isEmpty()) {
        fileName = defaultArchiveName();
    }
    const QString archivePath = QDir(folder).filePath(fileName);

    setBusy(true);
    setProgress(0);
    setStatus(tr("Creating the archive..."));

    const bool created = archivePlugin->createArchive(
        paths, archivePath, useEncryption ? m_password : QString(),
        [this](int value) {
            setProgress(value);
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        });

    if (!created) {
        setBusy(false);
        setProgress(0);
        setStatus(tr("The archive could not be created. Check that the folder is writable and "
                     "that the input files still exist."));
        return false;
    }

    // ---- transfer
    QString transferError;
    QString shareLink;
    bool transferSucceeded = true;

    if (DestinationPluginInterface* destination = currentDestination()) {
        setStatus(tr("Transferring with the %1 destination...").arg(destination->name()));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        QVariantMap config = mergedConfig(ConfigKind::Destination, destination->configKey(),
                                          m_destinationProfile);
        const ConfigSchema schema = destination->configSchema();
        if (const ConfigField* target = schemaRecipientField(schema)) {
            const QString value = destinationTarget().trimmed();
            if (!value.isEmpty()) {
                config.insert(target->key, value);
                persistRecipient(ConfigKind::Destination, destination->configKey(), target->key, value);
            }
        }

        QString validationError;
        if (!destination->validateConfiguration(config, &validationError)) {
            transferSucceeded = false;
            transferError = tr("%1 is not configured: %2").arg(destination->name(), validationError);
        } else {
            QString message;
            transferSucceeded = destination->transferArchive(
                archivePath, config,
                [&message, &shareLink](bool ok, QString text, QString link) {
                    Q_UNUSED(ok);
                    message = text;
                    shareLink = link;
                });
            if (!transferSucceeded) {
                transferError = message.isEmpty()
                    ? tr("%1: the transfer step failed.").arg(destination->name())
                    : message;
            }
        }
    }

    m_lastShareLink = shareLink;

    // ---- password delivery (only for an archive that really arrived)
    QString deliveryError;
    bool deliverySucceeded = true;
    const bool deliverPassword = useEncryption && !m_password.isEmpty();

    if (deliverPassword) {
        if (!transferSucceeded) {
            deliverySucceeded = false;
            deliveryError = tr("The transfer failed.");
        } else if (SendPluginInterface* channel = currentChannel()) {
            setStatus(tr("Delivering the password with %1...").arg(channel->name()));
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

            QVariantMap config = mergedConfig(ConfigKind::Send, channel->configKey(), m_channelProfile);
            const ConfigSchema schema = channel->configSchema();
            if (const ConfigField* recipient = schemaRecipientField(schema)) {
                const QString value = channelRecipient().trimmed();
                if (!value.isEmpty()) {
                    config.insert(recipient->key, value);
                    persistRecipient(ConfigKind::Send, channel->configKey(), recipient->key, value);
                }
            }
            config.insert(QStringLiteral("shareLink"), shareLink);

            QString validationError;
            if (!channel->validateConfiguration(config, &validationError)) {
                deliverySucceeded = false;
                deliveryError = tr("%1 is not configured: %2").arg(channel->name(), validationError);
            } else {
                bool sent = false;
                QString message;
                channel->send(m_password, config, [&sent, &message](bool ok, QString text) {
                    sent = ok;
                    message = text;
                });
                deliverySucceeded = sent;
                if (!sent) {
                    deliveryError = message.isEmpty()
                        ? tr("%1: the password could not be delivered.").arg(channel->name())
                        : message;
                }
            }
        }
    }

    setBusy(false);
    setProgress(0);
    setStatus(QString());
    buildResult(created, transferSucceeded, transferError, deliverySucceeded, deliveryError,
                shareLink, archivePath);
    return true;
}

// -------------------------------------------------------- plugin settings

QVariantList AppBackend::pluginSettings(bool sendMode)
{
    const ConfigKind kind = sendMode ? ConfigKind::Send : ConfigKind::Destination;
    QVariantList result;

    auto addPlugin = [&](const QString& name, const QString& key, const ConfigSchema& schema,
                         const QVariantMap& defaults, bool requiresConfiguration) {
        const QStringList profiles = UserConfig::profileNames(kind, key);
        const QString active = storedProfileName(kind, key);

        QVariantList profileList;
        for (const QString& profile : profiles) {
            profileList.append(profile);
        }

        QVariantMap entry;
        entry.insert(QStringLiteral("name"), name);
        entry.insert(QStringLiteral("key"), key);
        entry.insert(QStringLiteral("requiresConfiguration"), requiresConfiguration);
        entry.insert(QStringLiteral("schema"), schemaToMaps(schema));
        entry.insert(QStringLiteral("profiles"), profileList);
        entry.insert(QStringLiteral("activeProfile"), active);
        entry.insert(QStringLiteral("values"), mergedConfig(kind, key, active));
        entry.insert(QStringLiteral("defaults"), defaults);
        result.append(entry);
    };

    if (sendMode) {
        for (SendPluginInterface* plugin : m_sendPlugins) {
            if (!plugin->requiresConfiguration()) {
                continue;
            }
            addPlugin(plugin->name(), plugin->configKey(), plugin->configSchema(),
                      plugin->defaultConfiguration(), true);
        }
    } else {
        for (DestinationPluginInterface* plugin : m_destinationPlugins) {
            if (!plugin->requiresConfiguration()) {
                continue;
            }
            addPlugin(plugin->name(), plugin->configKey(), plugin->configSchema(),
                      plugin->defaultConfiguration(), true);
        }
    }

    return result;
}

QVariantMap AppBackend::profileValues(bool sendMode, const QString& key, const QString& profile)
{
    const ConfigKind kind = sendMode ? ConfigKind::Send : ConfigKind::Destination;
    return mergedConfig(kind, key, profile);
}

QString AppBackend::saveProfileValues(bool sendMode, const QString& key, const QString& profile,
                                      const QVariantMap& values)
{
    const ConfigKind kind = sendMode ? ConfigKind::Send : ConfigKind::Destination;
    const QString name = profile.isEmpty() ? storedProfileName(kind, key) : profile;

    QVariantMap defaults;
    QVariantMap merged = values;
    std::function<bool(const QVariantMap&, QString*)> validate;

    if (sendMode) {
        for (SendPluginInterface* plugin : m_sendPlugins) {
            if (plugin->configKey() != key) {
                continue;
            }
            defaults = plugin->defaultConfiguration();
            validate = [plugin](const QVariantMap& config, QString* error) {
                return plugin->validateConfiguration(config, error);
            };
            break;
        }
    } else {
        for (DestinationPluginInterface* plugin : m_destinationPlugins) {
            if (plugin->configKey() != key) {
                continue;
            }
            defaults = plugin->defaultConfiguration();
            validate = [plugin](const QVariantMap& config, QString* error) {
                return plugin->validateConfiguration(config, error);
            };
            break;
        }
    }

    for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
        if (!merged.contains(it.key())) {
            merged.insert(it.key(), it.value());
        }
    }

    // A profile that only carries a recipient is incomplete by design.
    bool incomplete = true;
    QVariantMap candidate = merged;
    QVariantMap reference = defaults;
    const ConfigSchema schema = [&]() -> ConfigSchema {
        if (sendMode) {
            for (SendPluginInterface* plugin : m_sendPlugins) {
                if (plugin->configKey() == key) {
                    return plugin->configSchema();
                }
            }
        } else {
            for (DestinationPluginInterface* plugin : m_destinationPlugins) {
                if (plugin->configKey() == key) {
                    return plugin->configSchema();
                }
            }
        }
        return {};
    }();
    for (const ConfigField& field : schema) {
        if (field.isRecipient()) {
            candidate.remove(field.key);
            reference.remove(field.key);
        }
    }
    incomplete = candidate == reference;

    if (!incomplete && validate) {
        QString error;
        if (!validate(merged, &error)) {
            return error;
        }
    }

    UserConfig::saveProfile(kind, key, name, merged);
    emit destinationChanged();
    emit channelChanged();
    return QString();
}

QString AppBackend::createProfile(bool sendMode, const QString& key, const QString& name,
                                  const QString& copyFrom)
{
    const ConfigKind kind = sendMode ? ConfigKind::Send : ConfigKind::Destination;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return tr("Please enter a profile name.");
    }
    if (UserConfig::profileNames(kind, key).contains(trimmed)) {
        return tr("A profile named \"%1\" already exists.").arg(trimmed);
    }

    // A recipient or target typed in the main window is not saved yet; store it
    // first so the copied profile really contains what the user sees.
    if (sendMode) {
        for (SendPluginInterface* plugin : m_sendPlugins) {
            if (plugin->configKey() != key) {
                continue;
            }
            const ConfigSchema schema = plugin->configSchema();
            if (const ConfigField* recipient = schemaRecipientField(schema)) {
                persistRecipient(ConfigKind::Send, key, recipient->key, m_channelRecipientOverride);
            }
            break;
        }
    } else {
        for (DestinationPluginInterface* plugin : m_destinationPlugins) {
            if (plugin->configKey() != key) {
                continue;
            }
            const ConfigSchema schema = plugin->configSchema();
            if (const ConfigField* target = schemaRecipientField(schema)) {
                persistRecipient(ConfigKind::Destination, key, target->key, m_destinationTargetOverride);
            }
            break;
        }
    }

    QVariantMap values = mergedConfig(kind, key, copyFrom);
    UserConfig::saveProfile(kind, key, trimmed, values);

    if (sendMode) {
        refreshChannelProfiles();
        m_channelProfile = trimmed;
        emit channelChanged();
    } else {
        refreshDestinationProfiles();
        m_destinationProfile = trimmed;
        emit destinationChanged();
    }
    return QString();
}

QString AppBackend::renameProfile(bool sendMode, const QString& key, const QString& from,
                                  const QString& to)
{
    const ConfigKind kind = sendMode ? ConfigKind::Send : ConfigKind::Destination;
    const QString trimmed = to.trimmed();
    if (trimmed.isEmpty()) {
        return tr("Please enter a profile name.");
    }
    if (trimmed == from) {
        return QString();
    }
    if (UserConfig::profileNames(kind, key).contains(trimmed)) {
        return tr("A profile named \"%1\" already exists.").arg(trimmed);
    }

    UserConfig::renameProfile(kind, key, from, trimmed);

    if (sendMode) {
        refreshChannelProfiles();
        m_channelProfile = trimmed;
        emit channelChanged();
    } else {
        refreshDestinationProfiles();
        m_destinationProfile = trimmed;
        emit destinationChanged();
    }
    return QString();
}

QString AppBackend::deleteProfile(bool sendMode, const QString& key, const QString& name)
{
    const ConfigKind kind = sendMode ? ConfigKind::Send : ConfigKind::Destination;
    if (UserConfig::profileNames(kind, key).size() <= 1) {
        return tr("At least one profile has to stay.");
    }

    UserConfig::deleteProfile(kind, key, name);

    if (sendMode) {
        m_channelProfile.clear();
        refreshChannelProfiles();
        emit channelChanged();
    } else {
        m_destinationProfile.clear();
        refreshDestinationProfiles();
        emit destinationChanged();
    }
    return QString();
}

void AppBackend::selectProfile(bool sendMode, const QString& key, const QString& name)
{
    const ConfigKind kind = sendMode ? ConfigKind::Send : ConfigKind::Destination;
    UserConfig::setActiveProfile(kind, key, name);

    if (sendMode) {
        m_channelProfile = name;
        refreshChannelProfiles();
        emit channelChanged();
    } else {
        m_destinationProfile = name;
        refreshDestinationProfiles();
        emit destinationChanged();
    }
}

// ------------------------------------------------------------------ language

QStringList AppBackend::languages() const
{
    return defaultLanguages();
}

int AppBackend::languageIndex() const
{
    const QString locale = UserConfig::language();
    const int index = languageLocales().indexOf(locale);
    return index < 0 ? 0 : index;
}

void AppBackend::setLanguageIndex(int index)
{
    const QStringList locales = languageLocales();
    if (index < 0 || index >= locales.size()) {
        return;
    }
    UserConfig::saveLanguage(locales.at(index));
    emit languageChanged();
}