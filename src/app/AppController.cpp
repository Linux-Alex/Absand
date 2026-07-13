// SPDX-License-Identifier: MPL-2.0

#include "AppController.h"

#include "core/SevenZipRuntime.h"
#include "core/TransferHelpers.h"
#include "core/UserConfig.h"
#include "interfaces/ArchivePluginInterface.h"
#include "interfaces/DestinationPluginInterface.h"
#include "models/ArchiveEntryModel.h"
#include "models/FilePathListModel.h"
#include <bit7z/bit7z.hpp>
#include <bit7z/bitabstractarchivecreator.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitarchivewriter.hpp>

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace {

QString normalizeSeparators(QString path)
{
    path.replace('\\', '/');
    while (path.startsWith('/')) {
        path.remove(0, 1);
    }
    while (path.contains("//")) {
        path.replace("//", "/");
    }
    return path;
}

QString formatDateTime(const std::chrono::time_point<std::chrono::system_clock>& timePoint)
{
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
    if (ms <= 0) {
        return QString();
    }
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString fileTypeLabel(bool isDir, const QString& path)
{
    if (isDir) {
        return QStringLiteral("Folder");
    }

    const QFileInfo info(path);
    const QString suffix = info.suffix();
    return suffix.isEmpty() ? QStringLiteral("File") : suffix.toUpper() + QStringLiteral(" file");
}

QString humanReadableSize(std::uint64_t size)
{
    const double value = static_cast<double>(size);
    if (size < 1024) {
        return QString::number(size) + QStringLiteral(" B");
    }
    if (size < 1024ull * 1024ull) {
        return QString::number(value / 1024.0, 'f', 1) + QStringLiteral(" KB");
    }
    if (size < 1024ull * 1024ull * 1024ull) {
        return QString::number(value / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    }
    return QString::number(value / (1024.0 * 1024.0 * 1024.0), 'f', 1) + QStringLiteral(" GB");
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent)
    , m_inputFiles(std::make_unique<FilePathListModel>(this))
    , m_archiveEntries(std::make_unique<ArchiveEntryModel>(this))
{
    loadPlugins();
    syncDefaultsFromPlugins();
}

FilePathListModel* AppController::inputFilesModel() const
{
    return m_inputFiles.get();
}

ArchiveEntryModel* AppController::archiveEntriesModel() const
{
    return m_archiveEntries.get();
}

QStringList AppController::archiveFormats() const
{
    return m_archiveFormatNames;
}

QStringList AppController::destinationTargets() const
{
    return m_destinationTargets;
}

QStringList AppController::destinationTargetKeys() const
{
    QStringList keys;
    keys.reserve(m_destinationPlugins.size());
    for (DestinationPluginInterface* plugin : m_destinationPlugins) {
        keys.append(plugin ? plugin->configKey() : QString());
    }
    return keys;
}

int AppController::archiveFormatIndex() const
{
    return m_archiveFormatIndex;
}

void AppController::setArchiveFormatIndex(int index)
{
    if (index < 0 || index >= m_archivePlugins.size() || index == m_archiveFormatIndex) {
        return;
    }
    m_archiveFormatIndex = index;
    emit pluginDataChanged();
    emit archiveOptionsChanged();
    updateDestinationPath();
}

int AppController::destinationTargetIndex() const
{
    return m_destinationTargetIndex;
}

void AppController::setDestinationTargetIndex(int index)
{
    if (index < 0 || index >= m_destinationPlugins.size() || index == m_destinationTargetIndex) {
        return;
    }
    m_destinationTargetIndex = index;
    emit pluginDataChanged();
    updateDestinationPath();
}

bool AppController::encryptArchive() const
{
    return m_encryptArchive;
}

void AppController::setEncryptArchive(bool enabled)
{
    if (m_encryptArchive == enabled) {
        return;
    }
    m_encryptArchive = enabled;
    emit archiveOptionsChanged();
}

QString AppController::password() const
{
    return m_password;
}

void AppController::setPassword(const QString& password)
{
    if (m_password == password) {
        return;
    }
    m_password = password;
    emit archiveOptionsChanged();
}

QString AppController::destinationPath() const
{
    return m_destinationPath;
}

void AppController::setDestinationPath(const QString& path)
{
    if (m_destinationPath == path) {
        return;
    }
    m_destinationPath = path;
    emit destinationChanged();
}

QString AppController::statusMessage() const
{
    return m_statusMessage;
}

QString AppController::browserStatusMessage() const
{
    return m_browserStatusMessage;
}

bool AppController::archiveLoaded() const
{
    return m_archiveLoaded;
}

QString AppController::currentArchivePath() const
{
    return m_currentArchivePath;
}

int AppController::browserViewMode() const
{
    return m_browserViewMode;
}

void AppController::setBrowserViewMode(int mode)
{
    if (m_browserViewMode == mode) {
        return;
    }
    m_browserViewMode = mode;
    emit browserStateChanged();
}

void AppController::addFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(nullptr, tr("Select Files to Add"));
    m_inputFiles->addPaths(files);
    updateDestinationPath();
    emit statusMessageChanged();
}

void AppController::removeInputAt(int index)
{
    m_inputFiles->removeAt(index);
    updateDestinationPath();
}

void AppController::clearInputs()
{
    m_inputFiles->clear();
    updateDestinationPath();
}

QString AppController::chooseDirectory(const QString& title, const QString& startDir) const
{
    const QString resolvedStartDir = startDir.isEmpty() ? QDir::homePath() : startDir;
    return QFileDialog::getExistingDirectory(nullptr, title, resolvedStartDir);
}

void AppController::openDestinationFolderPicker()
{
    const QString startDir = QFileInfo(m_destinationPath).absolutePath().isEmpty()
        ? QDir::homePath()
        : QFileInfo(m_destinationPath).absolutePath();
    const QString folder = QFileDialog::getExistingDirectory(nullptr, tr("Select Destination Folder"), startDir);
    if (!folder.isEmpty()) {
        setDestinationPath(QDir(folder).filePath(getDefaultFileName()));
    }
}

void AppController::configureDestinations()
{
    emit showDestinationSettingsRequested();
}

QVariantMap AppController::destinationConfigForIndex(int index) const
{
    if (index < 0 || index >= m_destinationPlugins.size()) {
        return {};
    }

    DestinationPluginInterface* plugin = m_destinationPlugins.at(index);
    if (!plugin) {
        return {};
    }

    QVariantMap config = plugin->defaultConfiguration();
    const QVariantMap stored = UserConfig::loadDestinationConfig(plugin->configKey());
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
        config[it.key()] = it.value();
    }
    return config;
}

void AppController::saveDestinationConfigForIndex(int index, const QVariantMap& config)
{
    if (index < 0 || index >= m_destinationPlugins.size()) {
        return;
    }

    DestinationPluginInterface* plugin = m_destinationPlugins.at(index);
    if (!plugin) {
        return;
    }

    UserConfig::saveDestinationConfig(plugin->configKey(), config);
    if (index == m_destinationTargetIndex) {
        updateDestinationPath();
    }
}

void AppController::createArchive()
{
    if (m_inputFiles->rowCount() == 0) {
        showError(tr("No files selected"), tr("Please add some files first."));
        return;
    }

    auto* plugin = currentArchivePlugin();
    if (!plugin) {
        showError(tr("No archive format selected"), tr("Please select an archive format."));
        return;
    }

    const QString outputPath = m_destinationPath.isEmpty()
        ? QFileInfo(getDefaultFileName()).absoluteFilePath()
        : m_destinationPath;

    if (outputPath.isEmpty()) {
        showError(tr("No destination selected"), tr("Please choose where to save the archive."));
        return;
    }

    const bool encrypt = m_encryptArchive && plugin->supportsPassword();
    if (m_encryptArchive && !plugin->supportsPassword()) {
        showWarning(tr("Encryption not supported"),
                    tr("The selected archive format does not support encryption. The archive will be created without a password."));
    }

    if (encrypt && m_password.isEmpty()) {
        showError(tr("Missing password"), tr("Please enter a password or disable encryption."));
        return;
    }

    try {
        const QString runtimePath = resolveSevenZipLibraryPath();
        if (runtimePath.isEmpty()) {
            showError(tr("Missing 7-Zip runtime"), tr("Could not find the bundled 7-Zip runtime."));
            return;
        }

        m_library = std::make_unique<bit7z::Bit7zLibrary>(runtimePath.toStdString());
        const QStringList inputs = m_inputFiles->paths();
        const std::vector<bit7z::tstring> sources = [&]() {
            std::vector<bit7z::tstring> result;
            result.reserve(static_cast<std::size_t>(inputs.size()));
            for (const QString& input : inputs) {
                result.push_back(input.toStdString());
            }
            return result;
        }();

        if (sources.empty()) {
            showError(tr("No files selected"), tr("Please add some files first."));
            return;
        }

        m_format = &bit7z::BitFormat::Zip;
        if (plugin->defaultExtension().compare(QStringLiteral("7z"), Qt::CaseInsensitive) == 0) {
            m_format = &bit7z::BitFormat::SevenZip;
        }

        bit7z::BitArchiveWriter writer(
            *m_library,
            outputPath.toStdString(),
            *m_format,
            encrypt ? m_password.toStdString() : std::string()
        );
        writer.setUpdateMode(bit7z::UpdateMode::Update);
        writer.addFiles(sources);
        writer.compressTo(outputPath.toStdString());

        m_statusMessage = tr("Archive created: %1").arg(QFileInfo(outputPath).fileName());
        emit statusMessageChanged();

        transferArchiveIfNeeded(outputPath);
    } catch (const std::exception& ex) {
        showError(tr("Archive failed"), QString::fromUtf8(ex.what()));
    } catch (...) {
        showError(tr("Archive failed"), tr("An unknown error occurred."));
    }
}

void AppController::openArchive()
{
    const QString archivePath = QFileDialog::getOpenFileName(nullptr, tr("Open Archive"), QString(), tr("Archives (*.7z *.zip);;All Files (*)"));
    if (archivePath.isEmpty()) {
        return;
    }

    const QString password = promptPassword();
    if (!loadArchiveFile(archivePath, password)) {
        return;
    }
    emit showBrowserRequested();
}

void AppController::addFilesToArchive()
{
    if (!m_archiveLoaded || !m_reader || !m_format || m_currentArchivePath.isEmpty()) {
        showWarning(tr("Archive not ready"), tr("Open a ZIP or 7z archive first."));
        return;
    }

    const QStringList files = QFileDialog::getOpenFileNames(nullptr, tr("Select Files to Add"));
    if (files.isEmpty()) {
        return;
    }

    try {
        bit7z::BitArchiveWriter writer(
            *m_library,
            m_currentArchivePath.toStdString(),
            *m_format,
            m_currentArchivePassword.toStdString()
        );
        writer.setUpdateMode(bit7z::UpdateMode::Update);
        std::vector<bit7z::tstring> sources;
        for (const QString& file : files) {
            sources.push_back(file.toStdString());
        }
        writer.addFiles(sources);
        writer.compressTo(m_currentArchivePath.toStdString());
        loadArchiveFile(m_currentArchivePath, m_currentArchivePassword);
    } catch (const std::exception& ex) {
        showError(tr("Add files failed"), QString::fromUtf8(ex.what()));
    }
}

void AppController::refreshArchive()
{
    if (m_currentArchivePath.isEmpty()) {
        return;
    }
    loadArchiveFile(m_currentArchivePath, m_currentArchivePassword);
}

void AppController::browserSetCurrentIndex(int index)
{
    m_browserCurrentIndex = index;
    emit browserStateChanged();
}

void AppController::browserOpenCurrent()
{
    if (m_browserCurrentIndex < 0 || !m_reader) {
        return;
    }

    const QString path = m_archiveEntries->pathAt(m_browserCurrentIndex);
    if (path.isEmpty()) {
        return;
    }

    const QString scratchDir = QFileInfo(QDir::temp().filePath(QStringLiteral("absand-open"))).absoluteFilePath();
    QDir().mkpath(scratchDir);
    try {
        m_reader->extractTo(scratchDir.toStdString(), bit7z::IndicesVector{ static_cast<std::uint32_t>(m_browserCurrentIndex) });
        const QString extractedPath = QDir(scratchDir).filePath(path);
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(extractedPath))) {
            showError(tr("Open failed"), tr("Could not open the extracted item."));
        }
    } catch (...) {
        showError(tr("Open failed"), tr("Could not extract the selected item."));
    }
}

void AppController::browserOpenCurrentWith()
{
    const QString program = promptProgramPath();
    if (program.isEmpty()) {
        return;
    }
    if (m_browserCurrentIndex < 0 || !m_reader) {
        return;
    }

    const QString path = m_archiveEntries->pathAt(m_browserCurrentIndex);
    if (path.isEmpty()) {
        return;
    }

    const QString scratchDir = QFileInfo(QDir::temp().filePath(QStringLiteral("absand-openwith"))).absoluteFilePath();
    QDir().mkpath(scratchDir);
    try {
        m_reader->extractTo(scratchDir.toStdString(), bit7z::IndicesVector{ static_cast<std::uint32_t>(m_browserCurrentIndex) });
        const QString extractedPath = QDir(scratchDir).filePath(path);
        if (!QProcess::startDetached(program, { extractedPath })) {
            showError(tr("Launch failed"), tr("Could not launch the selected program."));
        }
    } catch (...) {
        showError(tr("Launch failed"), tr("Could not extract the selected item."));
    }
}

void AppController::browserExtractCurrent()
{
    if (m_browserCurrentIndex < 0 || !m_reader) {
        return;
    }

    const QString destinationDir = promptDestinationDirectory(tr("Choose Copy Destination"), QDir::homePath());
    if (destinationDir.isEmpty()) {
        return;
    }

    try {
        m_reader->extractTo(destinationDir.toStdString(), bit7z::IndicesVector{ static_cast<std::uint32_t>(m_browserCurrentIndex) });
    } catch (const std::exception& ex) {
        showError(tr("Extract failed"), QString::fromUtf8(ex.what()));
    }
}

void AppController::browserCopyCurrentPath()
{
    if (m_browserCurrentIndex < 0) {
        return;
    }
    const QString path = m_archiveEntries->pathAt(m_browserCurrentIndex);
    if (path.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(path);
}

QString AppController::getDefaultFileName() const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString baseName = m_inputFiles->paths().size() == 1
        ? QFileInfo(m_inputFiles->paths().first()).baseName().replace(' ', '_')
        : QStringLiteral("absand");
    return QStringLiteral("%1_%2.%3").arg(baseName, timestamp, currentArchiveExtension());
}

QString AppController::currentArchiveExtension() const
{
    auto* plugin = currentArchivePlugin();
    return plugin ? plugin->defaultExtension() : QStringLiteral("zip");
}

qint64 AppController::calculateTotalSize() const
{
    qint64 total = 0;
    for (const QString& path : m_inputFiles->paths()) {
        QFileInfo info(path);
        if (info.isFile()) {
            total += info.size();
        } else if (info.isDir()) {
            QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                total += it.fileInfo().size();
            }
        }
    }
    return total;
}

void AppController::updateDestinationPath()
{
    if (m_inputFiles->rowCount() == 0) {
        m_destinationPath.clear();
        emit destinationChanged();
        return;
    }

    QString folder = QFileInfo(m_destinationPath).absolutePath();
    if (folder.isEmpty() || folder == QStringLiteral(".")) {
        folder = currentDestinationPlugin()
            ? currentDestinationPlugin()->suggestedLocalSaveFolder(currentDestinationConfig())
            : QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    }
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }

    QString fileName = QFileInfo(m_destinationPath).completeBaseName();
    if (fileName.isEmpty() || m_destinationPath.isEmpty()) {
        fileName = QFileInfo(getDefaultFileName()).completeBaseName();
    }
    setDestinationPath(QDir(folder).filePath(QStringLiteral("%1.%2").arg(fileName, currentArchiveExtension())));
    m_statusMessage = tr("Selected %1 item(s), %2").arg(m_inputFiles->rowCount()).arg(humanReadableSize(static_cast<std::uint64_t>(calculateTotalSize())));
    emit statusMessageChanged();
}

void AppController::loadPlugins()
{
    m_pluginManager.loadAllPlugins();
    m_archivePlugins = m_pluginManager.archivePlugins();
    m_destinationPlugins = m_pluginManager.destinationPlugins();

    m_archiveFormatNames.clear();
    for (ArchivePluginInterface* plugin : m_archivePlugins) {
        m_archiveFormatNames.append(QStringLiteral("%1 (%2)").arg(plugin->name(), plugin->defaultExtension()));
    }

    m_destinationTargets.clear();
    for (DestinationPluginInterface* plugin : m_destinationPlugins) {
        m_destinationTargets.append(plugin->name());
    }

    emit pluginDataChanged();
}

void AppController::syncDefaultsFromPlugins()
{
    if (!m_archivePlugins.isEmpty()) {
        m_archiveFormatIndex = 0;
        for (int i = 0; i < m_archivePlugins.size(); ++i) {
            if (m_archivePlugins.at(i)->defaultExtension().compare(QStringLiteral("zip"), Qt::CaseInsensitive) == 0) {
                m_archiveFormatIndex = i;
                break;
            }
        }
    }

    if (!m_destinationPlugins.isEmpty()) {
        m_destinationTargetIndex = 0;
        for (int i = 0; i < m_destinationPlugins.size(); ++i) {
            if (m_destinationPlugins.at(i)->configKey().compare(QStringLiteral("local_folder"), Qt::CaseInsensitive) == 0) {
                m_destinationTargetIndex = i;
                break;
            }
        }
    }

    m_encryptArchive = false;
    m_password.clear();
    updateDestinationPath();
    emit archiveOptionsChanged();
    emit pluginDataChanged();
}

ArchivePluginInterface* AppController::currentArchivePlugin() const
{
    if (m_archiveFormatIndex < 0 || m_archiveFormatIndex >= m_archivePlugins.size()) {
        return nullptr;
    }
    return m_archivePlugins.at(m_archiveFormatIndex);
}

DestinationPluginInterface* AppController::currentDestinationPlugin() const
{
    if (m_destinationTargetIndex < 0 || m_destinationTargetIndex >= m_destinationPlugins.size()) {
        return nullptr;
    }
    return m_destinationPlugins.at(m_destinationTargetIndex);
}

QVariantMap AppController::currentDestinationConfig() const
{
    if (auto* plugin = currentDestinationPlugin()) {
        QVariantMap config = plugin->defaultConfiguration();
        const QVariantMap stored = UserConfig::loadDestinationConfig(plugin->configKey());
        for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
            config[it.key()] = it.value();
        }
        return config;
    }
    return {};
}

void AppController::loadArchiveEntries()
{
    if (!m_reader) {
        m_archiveEntries->setEntries({});
        return;
    }

    QVector<ArchiveEntryModel::Entry> entries;
    for (std::uint32_t i = 0; i < m_reader->itemsCount(); ++i) {
        const auto item = m_reader->itemAt(i);
        const QString archivePath = normalizeSeparators(QString::fromStdString(item.path()));
        if (archivePath.isEmpty()) {
            continue;
        }

        ArchiveEntryModel::Entry entry;
        entry.path = archivePath;
        entry.name = QFileInfo(archivePath).fileName();
        entry.isDir = item.isDir();
        entry.sizeText = item.isDir() ? QString() : humanReadableSize(item.size());
        entry.typeText = fileTypeLabel(item.isDir(), archivePath);
        entry.modifiedText = formatDateTime(item.lastWriteTime());
        entry.iconName = item.isDir() ? QStringLiteral("folder") : QStringLiteral("text-x-generic");
        entry.depth = archivePath.count('/');
        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.path.toLower() < b.path.toLower();
    });

    m_archiveEntries->setEntries(entries);
    m_browserStatusMessage = tr("%1 item(s) loaded from %2").arg(entries.size()).arg(QFileInfo(m_currentArchivePath).fileName());
    emit browserStatusMessageChanged();
}

bool AppController::loadArchiveFile(const QString& archivePath, const QString& password)
{
    const QString lower = QFileInfo(archivePath).suffix().toLower();
    const bit7z::BitInOutFormat* format = nullptr;
    if (lower == QStringLiteral("zip")) {
        format = &bit7z::BitFormat::Zip;
    } else if (lower == QStringLiteral("7z")) {
        format = &bit7z::BitFormat::SevenZip;
    }

    if (!format) {
        showError(tr("Unsupported archive type"), tr("Please open a .zip or .7z archive."));
        return false;
    }

    const QString runtimePath = resolveSevenZipLibraryPath();
    if (runtimePath.isEmpty()) {
        showError(tr("Missing 7-Zip runtime"), tr("Could not find the bundled 7-Zip runtime."));
        return false;
    }

    try {
        m_library = std::make_unique<bit7z::Bit7zLibrary>(runtimePath.toStdString());
        m_reader = std::make_unique<bit7z::BitArchiveReader>(
            *m_library,
            archivePath.toStdString(),
            *format,
            password.toStdString()
        );
        m_format = format;
        m_currentArchivePath = archivePath;
        m_currentArchivePassword = password;
        m_archiveLoaded = true;
        m_browserCurrentIndex = -1;
        loadArchiveEntries();
        emit browserStateChanged();
        m_browserStatusMessage = tr("Opened %1").arg(QFileInfo(archivePath).fileName());
        emit browserStatusMessageChanged();
        return true;
    } catch (const std::exception& ex) {
        showError(tr("Open failed"), QString::fromUtf8(ex.what()));
    } catch (...) {
        showError(tr("Open failed"), tr("Could not open the archive."));
    }

    return false;
}

QString AppController::promptPassword() const
{
    bool ok = false;
    const QString password = QInputDialog::getText(
        nullptr,
        tr("Archive Password"),
        tr("Enter password if this archive is encrypted:"),
        QLineEdit::Password,
        QString(),
        &ok
    );
    return ok ? password : QString();
}

QString AppController::promptDestinationDirectory(const QString& title, const QString& startDir) const
{
    return QFileDialog::getExistingDirectory(nullptr, title, startDir);
}

QString AppController::promptProgramPath() const
{
    return QFileDialog::getOpenFileName(nullptr, tr("Choose Program"), QDir::homePath(), tr("Programs (*);;All Files (*)"));
}

void AppController::showInfo(const QString& title, const QString& text) const
{
    QMessageBox::information(nullptr, title, text);
}

void AppController::showWarning(const QString& title, const QString& text) const
{
    QMessageBox::warning(nullptr, title, text);
}

void AppController::showError(const QString& title, const QString& text) const
{
    QMessageBox::critical(nullptr, title, text);
}

void AppController::transferArchiveIfNeeded(const QString& archivePath)
{
    auto* plugin = currentDestinationPlugin();
    if (!plugin) {
        return;
    }

    const QVariantMap config = currentDestinationConfig();
    QString error;
    if (!plugin->validateConfiguration(config, &error)) {
        showWarning(tr("Destination not ready"), error);
        return;
    }

    plugin->transferArchive(archivePath, config, nullptr);
}
