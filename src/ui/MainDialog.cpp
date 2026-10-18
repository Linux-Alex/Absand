// SPDX-License-Identifier: MPL-2.0

#include "MainDialog.h"
#include "ui_MainDialog.h"
#include "ui/FileDropListWidget.h"
#include "ArchiveBrowserDialog.h"
#include "DestinationSettingsDialog.h"
#include "SendSettingsDialog.h"
#include "core/ThemeManager.h"
#include "core/UserConfig.h"
#include "interfaces/ArchivePluginInterface.h"
#include "interfaces/DestinationPluginInterface.h"
#include "interfaces/SendPluginInterface.h"
#include <zip.h>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QClipboard>
#include <QGuiApplication>
#include <QDebug>
#include <QRandomGenerator>
#include <QFileInfo>
#include <QDirIterator>
#include <QStandardPaths>
#include <QProcess>
#include <QListWidgetItem>
#include <QSet>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QSignalBlocker>
#include <QFrame>
#include <QPushButton>
#include <QTabWidget>
#include <QEvent>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QUrl>
#include <QCloseEvent>

namespace {

QString storedRecipient(UserConfig::ConfigKind kind, const QString& pluginKey, const QString& fieldKey)
{
    const QString profile = UserConfig::activeProfile(kind, pluginKey);
    return UserConfig::loadProfile(kind, pluginKey, profile).value(fieldKey).toString();
}

void storeRecipient(UserConfig::ConfigKind kind, const QString& pluginKey,
                    const QString& fieldKey, const QString& value)
{
    const QString profile = UserConfig::activeProfile(kind, pluginKey);
    QVariantMap config = UserConfig::loadProfile(kind, pluginKey, profile);
    if (config.value(fieldKey).toString() == value) {
        return;
    }
    config.insert(fieldKey, value);
    UserConfig::saveProfile(kind, pluginKey, profile, config);
}

// Opens the platform's file manager on the folder that holds the archive. The
// previous implementation always launched "dolphin", which silently did nothing
// on Windows (and on Linux systems without Dolphin).
bool revealInFileManager(const QString& filePath)
{
    const QFileInfo info(filePath);
    const QString folder = info.absolutePath();

#ifdef Q_OS_WIN
    if (QProcess::startDetached(QStringLiteral("explorer.exe"),
                                { QStringLiteral("/select,"), QDir::toNativeSeparators(info.absoluteFilePath()) })) {
        return true;
    }
#elif defined(Q_OS_MACOS)
    if (QProcess::startDetached(QStringLiteral("open"),
                                { QStringLiteral("-R"), info.absoluteFilePath() })) {
        return true;
    }
#else
    const QStringList fileManagers{
        QStringLiteral("dolphin"), QStringLiteral("nautilus"), QStringLiteral("nemo"),
        QStringLiteral("thunar"), QStringLiteral("pcmanfm"), QStringLiteral("caja")
    };
    for (const QString& candidate : fileManagers) {
        if (!QStandardPaths::findExecutable(candidate).isEmpty()
            && QProcess::startDetached(candidate, { folder })) {
            return true;
        }
    }
#endif

    return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

} // namespace

MainDialog::MainDialog(const QStringList& inputPaths, QWidget* parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::MainDialog>())
    , m_inputPaths(inputPaths)
{
    qDebug() << "MainDialog created with" << inputPaths.size() << "input paths";
    for (const QString& path : inputPaths) {
        qDebug() << "  Input path:" << path;
    }
    
    ui->setupUi(this);
    setupUI();
    setupAppearanceBar();
    setupDestinationControls();
    setupSendControls();
    refreshInputList();
    updateArchiveInfo();
    generateRandomPassword();
}

MainDialog::~MainDialog() = default;

void MainDialog::setupUI()
{
    setWindowTitle(tr("Absand - Secure Archive Sender"));

    ui->inputListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->inputListWidget->setAlternatingRowColors(true);
    ui->inputListWidget->setSortingEnabled(false);
    ui->inputListWidget->setDragEnabled(false);
    ui->inputListWidget->setDropIndicatorShown(true);
    ui->inputListWidget->setDefaultDropAction(Qt::CopyAction);
    ui->inputListWidget->setToolTip(tr("Drop files or folders here"));

    connect(ui->inputListWidget, &FileDropListWidget::filesDropped,
            this, &MainDialog::addInputPaths);
    
    // Set default values - encryption disabled until a supported format is selected
    ui->encryptCheckbox->setChecked(false);
    ui->encryptCheckbox->setEnabled(false);
    ui->encryptionCombo->setEnabled(false);
    ui->passwordEdit->setEnabled(false);
    ui->generatePasswordButton->setEnabled(false);
    ui->copyPasswordButton->setEnabled(false);
    ui->showPasswordCheckbox->setChecked(false);
    
    // Set default destination to current directory
    QString currentDir;
    if (!m_inputPaths.isEmpty()) {
        QFileInfo firstFile(m_inputPaths.first());
        currentDir = firstFile.absolutePath();
    } else {
        currentDir = QDir::homePath();
    }
    
    m_selectedDestination = currentDir;
    refreshDestinationPath();
    
    updateArchiveInfo();

    // Load plugins dynamically once the destination defaults are ready.
    loadArchivePlugins();
    loadDestinationPlugins();

    // Connect signals
    connect(ui->browseButton, &QPushButton::clicked, this, &MainDialog::onBrowseDestinationClicked);
    connect(ui->addFilesButton, &QPushButton::clicked, this, &MainDialog::onAddFilesClicked);
    connect(ui->removeSelectedButton, &QPushButton::clicked, this, &MainDialog::onRemoveSelectedClicked);
    connect(ui->clearFilesButton, &QPushButton::clicked, this, &MainDialog::onClearFilesClicked);
    connect(ui->archiveButton, &QPushButton::clicked, this, &MainDialog::onArchiveButtonClicked);
    connect(ui->generatePasswordButton, &QPushButton::clicked, this, &MainDialog::onPasswordGenerateClicked);
    connect(ui->copyPasswordButton, &QPushButton::clicked, this, &MainDialog::onPasswordCopyClicked);
    connect(ui->showPasswordCheckbox, &QCheckBox::toggled, this, &MainDialog::onPasswordShowToggle);
    connect(ui->encryptCheckbox, &QCheckBox::toggled, this, &MainDialog::onEncryptionCheckboxChanged);
    connect(ui->formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainDialog::onFormatChanged);
    connect(ui->destTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainDialog::onDestinationTypeChanged);

    updateInputSummary();
    ui->archiveButton->setEnabled(!m_inputPaths.isEmpty());
}

void MainDialog::setupAppearanceBar()
{
    auto* appearanceFrame = new QFrame(this);
    appearanceFrame->setObjectName("appearanceFrame");
    appearanceFrame->setFrameShape(QFrame::StyledPanel);
    appearanceFrame->setFrameShadow(QFrame::Plain);

    auto* layout = new QHBoxLayout(appearanceFrame);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    m_themeLabel = new QLabel(tr("Theme"), appearanceFrame);
    m_themeCombo = new QComboBox(appearanceFrame);
    m_themeCombo->addItems(ThemeManager::availableThemes());

    m_languageLabel = new QLabel(tr("Language"), appearanceFrame);
    m_languageCombo = new QComboBox(appearanceFrame);
    m_languageCombo->addItem(tr("English"), QStringLiteral("en_US"));
    m_languageCombo->addItem(tr("Slovenian"), QStringLiteral("sl_SI"));
    m_languageCombo->setCurrentIndex(qMax(0, m_languageCombo->findData(UserConfig::language())));

    m_openArchiveBrowserButton = new QPushButton(tr("Open Archive Browser..."), appearanceFrame);
    m_openArchiveBrowserButton->setAutoDefault(false);

    layout->addWidget(m_themeLabel);
    layout->addWidget(m_themeCombo, 0);
    layout->addWidget(m_languageLabel);
    layout->addWidget(m_languageCombo, 0);
    layout->addStretch(1);
    layout->addWidget(m_openArchiveBrowserButton);

    ui->verticalLayout->insertWidget(0, appearanceFrame);

    const QString savedTheme = ThemeManager::savedTheme();
    {
        const QSignalBlocker blocker(m_themeCombo);
        const int index = m_themeCombo->findText(savedTheme, Qt::MatchFixedString | Qt::MatchCaseSensitive);
        m_themeCombo->setCurrentIndex(index >= 0 ? index : 0);
    }

    ThemeManager::applyTheme(savedTheme);

    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainDialog::onThemeChanged);
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainDialog::onLanguageChanged);
    connect(m_openArchiveBrowserButton, &QPushButton::clicked,
            this, &MainDialog::onOpenArchiveBrowserClicked);
}

void MainDialog::setupSendControls()
{
    auto* row = new QHBoxLayout;
    m_sendLabel = new QLabel(tr("Send password via:"), ui->passwordGroupBox);
    m_sendCombo = new QComboBox(ui->passwordGroupBox);
    m_sendCombo->addItem(tr("Do not send"), QVariant::fromValue(static_cast<QObject*>(nullptr)));
    for (SendPluginInterface* plugin : m_pluginManager.sendPlugins()) {
        m_sendCombo->addItem(plugin->name(), QVariant::fromValue(dynamic_cast<QObject*>(plugin)));
    }
    m_sendSettingsButton = new QPushButton(tr("Configure channels..."), ui->passwordGroupBox);
    row->addWidget(m_sendLabel);
    row->addWidget(m_sendCombo, 1);
    row->addWidget(m_sendSettingsButton);
    ui->verticalLayout_2->addLayout(row);

    // Second row: which profile of the selected channel is used, and where the
    // password goes. Recipients live in the main window so a different address,
    // number, chat, or topic can be used without opening the settings.
    auto* detailRow = new QHBoxLayout;
    m_sendProfileLabel = new QLabel(tr("Profile:"), ui->passwordGroupBox);
    m_sendProfileCombo = new QComboBox(ui->passwordGroupBox);
    m_sendProfileCombo->setMinimumWidth(120);
    m_sendRecipientLabel = new QLabel(tr("Recipient:"), ui->passwordGroupBox);
    m_sendRecipientEdit = new QLineEdit(ui->passwordGroupBox);
    m_sendRecipientEdit->setClearButtonEnabled(true);

    detailRow->addWidget(m_sendProfileLabel);
    detailRow->addWidget(m_sendProfileCombo);
    detailRow->addSpacing(8);
    detailRow->addWidget(m_sendRecipientLabel);
    detailRow->addWidget(m_sendRecipientEdit, 1);
    ui->verticalLayout_2->addLayout(detailRow);

    connect(m_sendSettingsButton, &QPushButton::clicked, this, &MainDialog::onOpenSendSettingsClicked);
    connect(m_sendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainDialog::onSendChannelChanged);
    connect(m_sendProfileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainDialog::onSendProfileChanged);
    connect(m_sendRecipientEdit, &QLineEdit::editingFinished, this, &MainDialog::storeSendRecipient);

    updateSendChannelControls();
}

void MainDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        setWindowTitle(tr("Absand - Secure Archive Sender"));
        if (m_themeLabel) m_themeLabel->setText(tr("Theme"));
        if (m_languageLabel) m_languageLabel->setText(tr("Language"));
        if (m_openArchiveBrowserButton) m_openArchiveBrowserButton->setText(tr("Open Archive Browser..."));
        if (m_destinationSettingsButton) m_destinationSettingsButton->setText(tr("Configure..."));
        if (m_sendLabel) m_sendLabel->setText(tr("Send password via:"));
        if (m_sendSettingsButton) m_sendSettingsButton->setText(tr("Configure channels..."));
        if (m_sendProfileLabel) m_sendProfileLabel->setText(tr("Profile:"));
        if (m_destinationProfileLabel) m_destinationProfileLabel->setText(tr("Profile:"));
        if (m_languageCombo) {
            const QSignalBlocker blocker(m_languageCombo);
            m_languageCombo->setItemText(0, tr("English"));
            m_languageCombo->setItemText(1, tr("Slovenian"));
        }
        if (m_sendCombo) m_sendCombo->setItemText(0, tr("Do not send"));
        updateInputSummary();
        updateArchiveInfo();
        updateSendChannelControls();
        updateDestinationProfileControls();
        updateDestinationSelection();
    }
    QDialog::changeEvent(event);
}

void MainDialog::setupDestinationControls()
{
    ui->browseButton->setText(tr("Browse..."));
    ui->browseButton->setToolTip(tr("Choose the local folder where the archive is created before transfer."));

    if (auto* layout = ui->destinationGroupBox->findChild<QHBoxLayout*>(QStringLiteral("horizontalLayout_3"))) {
        m_destinationSettingsButton = new QPushButton(tr("Configure..."), ui->destinationGroupBox);
        m_destinationProfileLabel = new QLabel(tr("Profile:"), ui->destinationGroupBox);
        m_destinationProfileCombo = new QComboBox(ui->destinationGroupBox);
        m_destinationProfileCombo->setMinimumWidth(120);
        const int insertAt = layout->indexOf(ui->browseButton);
        layout->insertWidget(insertAt, m_destinationProfileLabel);
        layout->insertWidget(insertAt + 1, m_destinationProfileCombo);
        layout->addWidget(m_destinationSettingsButton);
        connect(m_destinationSettingsButton, &QPushButton::clicked,
                this, &MainDialog::onOpenDestinationSettingsClicked);

        // Where the archive is delivered: folder, share, remote path or address.
        auto* targetRow = new QHBoxLayout;
        m_destinationRecipientLabel = new QLabel(tr("Target:"), ui->destinationGroupBox);
        m_destinationRecipientEdit = new QLineEdit(ui->destinationGroupBox);
        m_destinationRecipientEdit->setClearButtonEnabled(true);
        targetRow->addWidget(m_destinationRecipientLabel);
        targetRow->addWidget(m_destinationRecipientEdit, 1);
        ui->verticalLayout_3->insertLayout(ui->verticalLayout_3->count() - 1, targetRow);
    }

    connect(m_destinationProfileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainDialog::onDestinationProfileChanged);
    connect(m_destinationRecipientEdit, &QLineEdit::editingFinished,
            this, &MainDialog::storeDestinationRecipient);
}

void MainDialog::loadArchivePlugins()
{
    // Load all plugins
    m_pluginManager.loadAllPlugins();
    
    // Populate format combo box with all archive plugins
    ui->formatCombo->clear();
    
    for (ArchivePluginInterface* plugin : m_pluginManager.archivePlugins()) {
        qDebug() << "Adding archive plugin:" << plugin->name();
        QStringList extensions = plugin->supportedExtensions();
        QString extensionStr = extensions.join(", ");
        ui->formatCombo->addItem(QString("%1 (%2)").arg(plugin->name(), extensionStr),
                                 QVariant::fromValue(plugin));
    }

    int defaultIndex = -1;
    for (int i = 0; i < ui->formatCombo->count(); ++i) {
        auto* plugin = ui->formatCombo->itemData(i).value<ArchivePluginInterface*>();
        if (plugin && plugin->defaultExtension().compare("zip", Qt::CaseInsensitive) == 0) {
            defaultIndex = i;
            break;
        }
    }
    if (defaultIndex < 0 && ui->formatCombo->count() > 0) {
        defaultIndex = 0;
    }

    if (defaultIndex >= 0) {
        ui->formatCombo->setCurrentIndex(defaultIndex);
    }

    if (ui->formatCombo->count() == 0) {
        qWarning() << "No archive plugins found!";
        ui->formatCombo->addItem("No plugins available", QVariant::fromValue(nullptr));
    }

    updateEncryptionControls();
    updateDestinationPathForCurrentFormat();
}

void MainDialog::loadDestinationPlugins()
{
    ui->destTypeCombo->clear();

    const auto plugins = m_pluginManager.destinationPlugins();
    for (DestinationPluginInterface* plugin : plugins) {
        if (!plugin) {
            continue;
        }

        const QVariantMap stored = UserConfig::loadDestinationConfig(plugin->configKey());
        QVariantMap config = plugin->defaultConfiguration();
        for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
            config[it.key()] = it.value();
        }
        m_destinationConfigCache.insert(plugin->configKey(), config);

        ui->destTypeCombo->addItem(plugin->name(), QVariant::fromValue(dynamic_cast<QObject*>(plugin)));
    }

    if (ui->destTypeCombo->count() == 0) {
        ui->destTypeCombo->addItem(tr("Local Folder"), QVariant::fromValue(static_cast<QObject*>(nullptr)));
    } else {
        // A plain local folder needs no credentials, so it is the sane default
        // even though the combo lists plugins alphabetically.
        for (int index = 0; index < ui->destTypeCombo->count(); ++index) {
            auto* candidate = qobject_cast<DestinationPluginInterface*>(
                ui->destTypeCombo->itemData(index).value<QObject*>());
            if (candidate && candidate->channelName() == QStringLiteral("local_folder")) {
                const QSignalBlocker blocker(ui->destTypeCombo);
                ui->destTypeCombo->setCurrentIndex(index);
                break;
            }
        }
    }

    updateDestinationProfileControls();
    updateDestinationSelection();
}

void MainDialog::onOpenArchiveBrowserClicked()
{
    ArchiveBrowserDialog dialog(this);
    dialog.exec();
}

void MainDialog::onOpenDestinationSettingsClicked()
{
    storeDestinationRecipient();
    DestinationSettingsDialog dialog(&m_pluginManager, this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshDestinationPlugins();
    }
    updateDestinationProfileControls();
    updateDestinationSelection();
}

void MainDialog::onThemeChanged(int index)
{
    if (!m_themeCombo) {
        return;
    }

    const QString themeName = m_themeCombo->itemText(index);
    ThemeManager::saveTheme(themeName);
    ThemeManager::applyTheme(themeName);
}

void MainDialog::onLanguageChanged(int index)
{
    if (!m_languageCombo || index < 0) return;
    const QString locale = m_languageCombo->itemData(index).toString();
    UserConfig::saveLanguage(locale);
    emit languageChangeRequested(locale);
}

void MainDialog::onOpenSendSettingsClicked()
{
    storeSendRecipient();
    SendSettingsDialog dialog(&m_pluginManager, this);
    dialog.exec();
    // Profiles may have been added, renamed, or removed.
    updateSendChannelControls();
}

void MainDialog::updateArchiveInfo()
{
    qint64 totalSize = calculateTotalSize();
    
    QString sizeStr;
    if (totalSize < 1024) {
        sizeStr = tr("%1 B").arg(totalSize);
    } else if (totalSize < 1024 * 1024) {
        sizeStr = tr("%1 KB").arg(totalSize / 1024);
    } else if (totalSize < 1024 * 1024 * 1024) {
        sizeStr = tr("%1 MB").arg(totalSize / (1024 * 1024));
    } else {
        sizeStr = tr("%1 GB").arg(totalSize / (1024 * 1024 * 1024));
    }
    
    ui->sizeLabel->setText(tr("Total size: %1").arg(sizeStr));
    ui->itemsLabel->setText(tr("Items: %1").arg(m_inputPaths.size()));
}

void MainDialog::updateInputSummary()
{
    QString summary;
    if (m_inputPaths.isEmpty()) {
        summary = tr("No files selected. Click Add Files or drop items into the list below.");
    } else if (m_inputPaths.size() == 1) {
        QFileInfo info(m_inputPaths.first());
        summary = tr("Selected: %1").arg(info.fileName());
    } else {
        summary = tr("Selected: %1 items").arg(m_inputPaths.size());
    }

    ui->inputLabel->setText(summary);
}

void MainDialog::refreshInputList()
{
    ui->inputListWidget->clear();

    for (const QString& path : m_inputPaths) {
        QFileInfo info(path);
        QListWidgetItem* item = new QListWidgetItem(info.fileName().isEmpty() ? path : info.fileName());
        item->setToolTip(path);
        item->setData(Qt::UserRole, path);
        ui->inputListWidget->addItem(item);
    }

    updateInputSummary();
    updateArchiveInfo();
    refreshDestinationPath();
    ui->archiveButton->setEnabled(!m_inputPaths.isEmpty());
}

void MainDialog::addInputPaths(const QStringList& paths)
{
    bool changed = false;

    for (const QString& path : paths) {
        QFileInfo info(path);
        if (!info.exists()) {
            continue;
        }

        if (m_inputPaths.contains(path)) {
            continue;
        }

        m_inputPaths.append(path);
        changed = true;
    }

    if (changed) {
        refreshInputList();
    }
}

void MainDialog::onAddFilesClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Select Files to Add"));
    addInputPaths(files);
}

void MainDialog::onRemoveSelectedClicked()
{
    QList<QListWidgetItem*> selectedItems = ui->inputListWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    QSet<QString> removedPaths;
    for (QListWidgetItem* item : selectedItems) {
        removedPaths.insert(item->data(Qt::UserRole).toString());
    }

    QStringList updated;
    for (const QString& path : m_inputPaths) {
        if (!removedPaths.contains(path)) {
            updated.append(path);
        }
    }

    m_inputPaths = updated;
    refreshInputList();
}

void MainDialog::onClearFilesClicked()
{
    if (m_inputPaths.isEmpty()) {
        return;
    }

    m_inputPaths.clear();
    refreshInputList();
}

qint64 MainDialog::calculateTotalSize() const
{
    qint64 total = 0;
    
    for (const QString& path : m_inputPaths) {
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

QString MainDialog::getDefaultFileName() const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString baseName;
    
    if (m_inputPaths.size() == 1) {
        QFileInfo info(m_inputPaths.first());
        baseName = info.baseName();
        baseName = baseName.replace(" ", "_");
    } else {
        baseName = "absand";
    }
    
    // Get the default extension from the selected plugin
    ArchivePluginInterface* plugin = ui->formatCombo->currentData().value<ArchivePluginInterface*>();
    if (plugin) {
        return QString("%1_%2.%3").arg(baseName, timestamp, plugin->defaultExtension());
    }
    
    return QString("%1_%2.zip").arg(baseName, timestamp);
}

void MainDialog::onBrowseDestinationClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select Destination Folder"), m_selectedDestination);
    if (!dir.isEmpty()) {
        m_selectedDestination = dir;
        m_generatedDestinationPath.clear();
        refreshDestinationPath();
    }
}

void MainDialog::onDestinationTypeChanged(int index)
{
    Q_UNUSED(index);
    updateDestinationSelection();
}

void MainDialog::onFormatChanged(int index)
{
    Q_UNUSED(index);
    updateArchiveInfo();
    updateDestinationPathForCurrentFormat();
    updateEncryptionControls();
}

void MainDialog::refreshDestinationPath()
{
    // Only replace the path when it is still the generated default; otherwise a
    // file name typed by the user would be discarded whenever files are added or
    // removed from the list.
    const QString current = ui->destinationPathEdit->text().trimmed();
    if (!current.isEmpty() && current != m_generatedDestinationPath) {
        return;
    }

    m_generatedDestinationPath = m_selectedDestination + '/' + getDefaultFileName();
    ui->destinationPathEdit->setText(m_generatedDestinationPath);
}

void MainDialog::updateDestinationPathForCurrentFormat()
{
    const ArchivePluginInterface* plugin = ui->formatCombo->currentData().value<ArchivePluginInterface*>();
    const QString newExt = plugin ? plugin->defaultExtension() : QStringLiteral("zip");
    QString currentPath = ui->destinationPathEdit->text().trimmed();

    if (currentPath.isEmpty()) {
        refreshDestinationPath();
        return;
    }

    QFileInfo info(currentPath);
    QString dir = info.absolutePath();
    QString baseName = info.completeBaseName();

    if (baseName.isEmpty()) {
        baseName = info.fileName();
    }
    if (dir.isEmpty() || dir == ".") {
        dir = m_selectedDestination;
    }

    ui->destinationPathEdit->setText(dir + "/" + baseName + "." + newExt);
}

DestinationPluginInterface* MainDialog::currentDestinationPlugin() const
{
    QObject* object = ui->destTypeCombo->currentData().value<QObject*>();
    return qobject_cast<DestinationPluginInterface*>(object);
}

QVariantMap MainDialog::currentDestinationConfig() const
{
    auto* plugin = currentDestinationPlugin();
    if (!plugin) {
        return {};
    }

    QVariantMap config = plugin->defaultConfiguration();
    const QVariantMap stored = UserConfig::loadDestinationConfig(plugin->configKey());
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
        config[it.key()] = it.value();
    }

    // The target is edited in the main window and wins over the stored value.
    const ConfigSchema schema = plugin->configSchema();
    const ConfigField* target = schemaRecipientField(schema);
    if (target && m_destinationRecipientEdit) {
        const QString text = m_destinationRecipientEdit->text().trimmed();
        if (!text.isEmpty()) {
            config.insert(target->key, text);
        }
    }

    return config;
}

void MainDialog::onDestinationProfileChanged(int index)
{
    auto* plugin = currentDestinationPlugin();
    if (!plugin || !m_destinationProfileCombo || index < 0) {
        return;
    }

    const QString chosen = m_destinationProfileCombo->itemText(index);
    if (chosen.isEmpty()) {
        return;
    }

    storeDestinationRecipient();
    UserConfig::setActiveProfile(UserConfig::ConfigKind::Destination, plugin->configKey(), chosen);
    updateDestinationProfileControls();
    updateDestinationSelection();
}

void MainDialog::storeDestinationRecipient()
{
    auto* plugin = currentDestinationPlugin();
    if (!plugin || !m_destinationRecipientEdit) {
        return;
    }

    const ConfigSchema schema = plugin->configSchema();
    const ConfigField* target = schemaRecipientField(schema);
    if (!target) {
        return;
    }

    storeRecipient(UserConfig::ConfigKind::Destination, plugin->configKey(), target->key,
                   m_destinationRecipientEdit->text().trimmed());
}

void MainDialog::updateDestinationProfileControls()
{
    auto* plugin = currentDestinationPlugin();

    if (m_destinationProfileLabel) {
        m_destinationProfileLabel->setVisible(plugin != nullptr);
    }
    if (m_destinationProfileCombo) {
        const QSignalBlocker blocker(m_destinationProfileCombo);
        m_destinationProfileCombo->setVisible(plugin != nullptr);
        m_destinationProfileCombo->clear();
        if (plugin) {
            m_destinationProfileCombo->addItems(
                UserConfig::profileNames(UserConfig::ConfigKind::Destination, plugin->configKey()));
            const QString active =
                UserConfig::activeProfile(UserConfig::ConfigKind::Destination, plugin->configKey());
            const int index = m_destinationProfileCombo->findText(active);
            m_destinationProfileCombo->setCurrentIndex(index >= 0 ? index : 0);
        }
    }

    const ConfigSchema schema = plugin ? plugin->configSchema() : ConfigSchema();
    const ConfigField* target = schemaRecipientField(schema);

    if (m_destinationRecipientLabel) {
        m_destinationRecipientLabel->setVisible(target != nullptr);
        if (target) {
            m_destinationRecipientLabel->setText(target->label.isEmpty() ? tr("Target:")
                                                                       : target->label + QLatin1Char(':'));
        }
    }
    if (m_destinationRecipientEdit) {
        m_destinationRecipientEdit->setVisible(target != nullptr);
        if (target) {
            m_destinationRecipientEdit->setPlaceholderText(target->placeholder);
            m_destinationRecipientEdit->setToolTip(target->help.isEmpty() ? target->label : target->help);
            m_destinationRecipientEdit->setText(
                storedRecipient(UserConfig::ConfigKind::Destination, plugin->configKey(), target->key));
        } else {
            m_destinationRecipientEdit->clear();
        }
    }
}

void MainDialog::updateDestinationSelection()
{
    auto* plugin = currentDestinationPlugin();
    if (plugin) {
        const QVariantMap config = currentDestinationConfig();
        m_selectedDestination = plugin->suggestedLocalSaveFolder(config);
        if (m_selectedDestination.isEmpty()) {
            m_selectedDestination = QDir::homePath();
        }
        m_generatedDestinationPath.clear();
        refreshDestinationPath();
        ui->browseButton->setEnabled(true);
        if (m_destinationSettingsButton) {
            m_destinationSettingsButton->setEnabled(true);
        }
        ui->pluginDestLabel->setVisible(true);
        ui->pluginDestLabel->setText(tr("Destination plugin: %1").arg(plugin->name()));
    } else {
        if (m_selectedDestination.isEmpty()) {
            m_selectedDestination = QDir::homePath();
        }
        ui->pluginDestLabel->setVisible(false);
        ui->browseButton->setEnabled(true);
    }
    updateDestinationPathForCurrentFormat();
}

void MainDialog::refreshDestinationPlugins()
{
    loadDestinationPlugins();
}

void MainDialog::updateEncryptionControls()
{
    ui->encryptionCombo->clear();
    ui->encryptCheckbox->setToolTip(QString());

    ArchivePluginInterface* plugin = ui->formatCombo->currentData().value<ArchivePluginInterface*>();
    if (plugin) {
        bool supportsPassword = plugin->supportsPassword();
        
        // Enable/disable encryption controls based on plugin support
        ui->encryptCheckbox->setEnabled(supportsPassword);
        const bool encryptionEnabled = supportsPassword && ui->encryptCheckbox->isChecked();
        ui->encryptionCombo->setEnabled(encryptionEnabled);
        ui->passwordEdit->setEnabled(encryptionEnabled);
        ui->generatePasswordButton->setEnabled(encryptionEnabled);
        ui->copyPasswordButton->setEnabled(encryptionEnabled);
        
        // If plugin doesn't support encryption, uncheck and disable
        if (!supportsPassword) {
            ui->encryptCheckbox->setChecked(false);
            ui->encryptCheckbox->setToolTip(tr("This format does not support encryption"));
        } else {
            ui->encryptCheckbox->setToolTip("");
        }
        
        // Update encryption methods combo based on plugin
        QStringList methods = plugin->availableEncryptionMethods();
        for (const QString& method : methods) {
            ui->encryptionCombo->addItem(method);
        }
        
        qDebug() << "Selected plugin:" << plugin->name() 
                 << "supports password:" << supportsPassword
                 << "encryption methods:" << methods;
    }
}

void MainDialog::generateRandomPassword()
{
    const QString charset = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%";
    QString password;
    password.reserve(PASSWORD_LENGTH);
    
    for (int i = 0; i < PASSWORD_LENGTH; ++i) {
        int index = QRandomGenerator::securelySeeded().bounded(charset.length());
        password.append(charset[index]);
    }
    
    m_currentPassword = password;
    ui->passwordEdit->setText(password);
}

void MainDialog::onPasswordGenerateClicked()
{
    generateRandomPassword();
}

void MainDialog::onPasswordCopyClicked()
{
    QGuiApplication::clipboard()->setText(ui->passwordEdit->text());
    showSuccess(tr("Password copied to clipboard!"));
}

void MainDialog::onPasswordShowToggle(bool checked)
{
    ui->passwordEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

void MainDialog::onEncryptionCheckboxChanged(bool checked)
{
    Q_UNUSED(checked);
    updateEncryptionControls();
}

bool MainDialog::createArchive(const QString& outputPath, bool encrypt, const QString& password)
{
    // Get the selected archive plugin
    ArchivePluginInterface* plugin = ui->formatCombo->currentData().value<ArchivePluginInterface*>();
    if (!plugin) {
        showError(tr("No archive plugin is available."));
        return false;
    }

    qDebug() << "Using plugin:" << plugin->name() << "to create archive:" << outputPath;

    // Plugins report their progress, so the window keeps repainting (and Windows
    // does not mark Absand as "not responding") while large archives are built.
    QProgressDialog progress(tr("Creating the archive..."), QString(), 0, 100, this);
    progress.setWindowTitle(tr("Creating Archive"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setWindowFlag(Qt::WindowCloseButtonHint, false);
    progress.setValue(0);

    const bool created = plugin->createArchive(
        m_inputPaths, outputPath, encrypt ? password : QString(),
        [&progress](int value) {
            progress.setValue(qBound(0, value, 100));
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        });

    progress.close();
    return created;
}

bool MainDialog::transferArchiveIfNeeded(const QString& archivePath, QString* errorMessage,
                                          QString* shareLink)
{
    auto* plugin = currentDestinationPlugin();
    if (!plugin) {
        return true;
    }

    storeDestinationRecipient();
    const QVariantMap config = currentDestinationConfig();
    QString error;
    if (!plugin->validateConfiguration(config, &error)) {
        if (errorMessage) {
            *errorMessage = tr("%1 is not configured: %2").arg(plugin->name(), error);
        }
        return false;
    }

    bool success = false;
    QString message;
    QString link;
    plugin->transferArchive(archivePath, config,
                            [&success, &message, &link](bool ok, QString text, QString sharedLink) {
                                success = ok;
                                message = text;
                                link = sharedLink;
                            });

    if (shareLink) {
        *shareLink = link;
    }

    if (!success && errorMessage) {
        *errorMessage = message.isEmpty()
            ? tr("%1: the transfer step failed.").arg(plugin->name())
            : tr("%1: %2").arg(plugin->name(), message);
    }
    return success;
}

SendPluginInterface* MainDialog::currentSendPlugin() const
{
    if (!m_sendCombo) return nullptr;
    return qobject_cast<SendPluginInterface*>(m_sendCombo->currentData().value<QObject*>());
}

QVariantMap MainDialog::currentSendConfig() const
{
    auto* plugin = currentSendPlugin();
    if (!plugin) {
        return {};
    }

    QVariantMap config = plugin->defaultConfiguration();
    const QVariantMap stored = UserConfig::loadSendConfig(plugin->configKey());
    for (auto it = stored.cbegin(); it != stored.cend(); ++it) {
        config[it.key()] = it.value();
    }

    // The recipient is edited in the main window and wins over the stored value.
    const ConfigSchema schema = plugin->configSchema();
    const ConfigField* recipient = schemaRecipientField(schema);
    if (recipient && m_sendRecipientEdit) {
        const QString text = m_sendRecipientEdit->text().trimmed();
        if (!text.isEmpty()) {
            config.insert(recipient->key, text);
        }
    }

    // Fills the {link} placeholder of the message template.
    config.insert(QStringLiteral("shareLink"), m_pendingShareLink);

    return config;
}

void MainDialog::onSendChannelChanged(int index)
{
    Q_UNUSED(index);
    storeSendRecipient();
    updateSendChannelControls();
}

void MainDialog::onSendProfileChanged(int index)
{
    auto* plugin = currentSendPlugin();
    if (!plugin || !m_sendProfileCombo || index < 0) {
        return;
    }

    const QString chosen = m_sendProfileCombo->itemText(index);
    if (chosen.isEmpty()) {
        return;
    }

    storeSendRecipient();
    UserConfig::setActiveProfile(UserConfig::ConfigKind::Send, plugin->configKey(), chosen);
    updateSendChannelControls();
}

void MainDialog::storeSendRecipient()
{
    auto* plugin = currentSendPlugin();
    if (!plugin || !m_sendRecipientEdit) {
        return;
    }

    const ConfigSchema schema = plugin->configSchema();
    const ConfigField* recipient = schemaRecipientField(schema);
    if (!recipient) {
        return;
    }

    storeRecipient(UserConfig::ConfigKind::Send, plugin->configKey(), recipient->key,
                   m_sendRecipientEdit->text().trimmed());
}

void MainDialog::updateSendChannelControls()
{
    auto* plugin = currentSendPlugin();
    const bool hasPlugin = plugin != nullptr;

    if (m_sendProfileLabel) {
        m_sendProfileLabel->setVisible(hasPlugin);
    }
    if (m_sendProfileCombo) {
        const QSignalBlocker blocker(m_sendProfileCombo);
        m_sendProfileCombo->setVisible(hasPlugin);
        m_sendProfileCombo->clear();
        if (plugin) {
            m_sendProfileCombo->addItems(
                UserConfig::profileNames(UserConfig::ConfigKind::Send, plugin->configKey()));
            const QString active = UserConfig::activeProfile(UserConfig::ConfigKind::Send, plugin->configKey());
            const int index = m_sendProfileCombo->findText(active);
            m_sendProfileCombo->setCurrentIndex(index >= 0 ? index : 0);
        }
    }

    const ConfigSchema schema = plugin ? plugin->configSchema() : ConfigSchema();
    const ConfigField* recipient = schemaRecipientField(schema);

    if (m_sendRecipientLabel) {
        m_sendRecipientLabel->setVisible(recipient != nullptr);
        if (recipient) {
            m_sendRecipientLabel->setText(recipient->label.isEmpty() ? tr("Recipient:") : recipient->label + QLatin1Char(':'));
        }
    }
    if (m_sendRecipientEdit) {
        m_sendRecipientEdit->setVisible(recipient != nullptr);
        if (recipient) {
            m_sendRecipientEdit->setPlaceholderText(recipient->placeholder);
            m_sendRecipientEdit->setToolTip(recipient->help.isEmpty() ? recipient->label : recipient->help);
            m_sendRecipientEdit->setText(
                storedRecipient(UserConfig::ConfigKind::Send, plugin->configKey(), recipient->key));
        } else {
            m_sendRecipientEdit->clear();
        }
    }
}

bool MainDialog::sendPasswordIfNeeded(const QString& password, QString* errorMessage)
{
    auto* plugin = currentSendPlugin();
    if (!plugin) return true;

    storeSendRecipient();
    const QVariantMap config = currentSendConfig();
    QString validationError;
    if (!plugin->validateConfiguration(config, &validationError)) {
        if (errorMessage) {
            *errorMessage = tr("%1 is not configured: %2").arg(plugin->name(), validationError);
        }
        return false;
    }
    bool success = false;
    QString message;
    plugin->send(password, config, [&](bool ok, QString text) { success = ok; message = text; });
    if (!success && errorMessage) {
        *errorMessage = message.isEmpty()
            ? tr("%1: the password could not be delivered.").arg(plugin->name())
            : message;
    }
    return success;
}

void MainDialog::onArchiveButtonClicked()
{
    if (m_inputPaths.isEmpty()) {
        showError(tr("Select at least one file or folder first."));
        return;
    }

    const QString destPath = ui->destinationPathEdit->text().trimmed();
    if (destPath.isEmpty()) {
        showError(tr("Please choose where the archive should be created."));
        return;
    }

    ArchivePluginInterface* plugin = ui->formatCombo->currentData().value<ArchivePluginInterface*>();
    if (!plugin) {
        showError(tr("No archive plugin is available."));
        return;
    }

    bool useEncryption = ui->encryptCheckbox->isChecked();
    const QString password = ui->passwordEdit->text();

    if (useEncryption && !plugin->supportsPassword()) {
        QMessageBox::warning(this, tr("Encryption Not Supported"),
            tr("%1 does not support encryption.\n\n"
               "The archive will be created WITHOUT encryption.\n\n"
               "Use the ZIP or 7z format for encrypted archives.")
                .arg(plugin->name()));
        useEncryption = false;
    }

    if (useEncryption && password.isEmpty()) {
        showError(tr("Please set a password for encryption."));
        return;
    }

    if (useEncryption) {
        QMessageBox confirmBox(this);
        confirmBox.setWindowTitle(tr("Confirm Encryption"));
        confirmBox.setIcon(QMessageBox::Question);
        confirmBox.setText(tr("Encrypt the archive with %1?\n\nPassword: %2")
                               .arg(ui->encryptionCombo->currentText(), password));
        confirmBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        confirmBox.setDefaultButton(QMessageBox::Yes);

        if (confirmBox.exec() != QMessageBox::Yes) {
            return;
        }
    }

    // The archive is always written to a real folder first; a destination plugin
    // (network share, FTP, SFTP) transfers it afterwards.
    const QFileInfo destinationInfo(destPath);
    const QString destinationFolder = destinationInfo.absolutePath();
    if (destinationFolder.isEmpty() || !QDir().mkpath(destinationFolder)) {
        showError(tr("The folder %1 does not exist and could not be created.").arg(destinationFolder));
        return;
    }

    qDebug() << "Creating archive...";
    qDebug() << "  Destination:" << destPath;
    qDebug() << "  Encryption:" << (useEncryption ? ui->encryptionCombo->currentText() : QStringLiteral("None"));

    const QString idleButtonText = ui->archiveButton->text();
    ui->archiveButton->setEnabled(false);
    ui->archiveButton->setText(tr("Creating archive..."));

    // Creating, transferring, and delivering all run inside this call; the event
    // loop is pumped meanwhile, so a stray close request must be ignored until
    // the work is finished.
    m_busy = true;

    const bool success = createArchive(destPath, useEncryption, password);

    if (!success) {
        m_busy = false;
        ui->archiveButton->setEnabled(true);
        ui->archiveButton->setText(idleButtonText);
        showError(tr("The archive could not be created.\n\n"
                     "Check that the destination folder is writable and that the "
                     "input files are still available."));
        return;
    }

    qDebug() << "Archive created successfully:" << destPath;
    m_lastArchivePath = destPath;

    QString transferError;
    QString shareLink;
    const bool transferSucceeded = transferArchiveIfNeeded(destPath, &transferError, &shareLink);
    m_pendingShareLink = shareLink;

    const bool passwordCanBeSent = useEncryption && !password.isEmpty();
    QString deliveryError;
    // A password for an archive that never reached its destination is useless,
    // and for a web share the link is part of the message, so delivery only
    // happens after a successful transfer.
    const bool deliverySucceeded = passwordCanBeSent && transferSucceeded
        ? sendPasswordIfNeeded(password, &deliveryError)
        : false;

    QString sizeText;
    const qint64 size = QFileInfo(destPath).size();
    if (size < 1024) {
        sizeText = tr("%1 B").arg(size);
    } else if (size < 1024 * 1024) {
        sizeText = tr("%1 KB").arg(size / 1024.0, 0, 'f', 2);
    } else {
        sizeText = tr("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
    }

    QStringList details;
    details << QFileInfo(destPath).fileName();
    details << tr("Size: %1").arg(sizeText);

    QStringList problems;
    if (DestinationPluginInterface* destination = currentDestinationPlugin()) {
        if (transferSucceeded) {
            details << tr("Transferred with the %1 destination.").arg(destination->name());
        } else {
            problems << tr("Transfer failed — %1").arg(transferError);
        }
    }
    if (!shareLink.isEmpty()) {
        details << tr("Download link: %1").arg(shareLink);
    }
    if (passwordCanBeSent) {
        if (deliverySucceeded) {
            if (SendPluginInterface* channel = currentSendPlugin()) {
                details << tr("Password delivered via %1.").arg(channel->name());
            }
        } else if (!transferSucceeded) {
            problems << tr("The password was not sent because the transfer failed.");
        } else {
            problems << tr("Password delivery failed — %1").arg(deliveryError);
        }
    }

    // The archive exists and every follow-up step is finished.
    m_busy = false;
    ui->archiveButton->setEnabled(true);
    ui->archiveButton->setText(idleButtonText);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(problems.isEmpty() ? tr("Archive Created") : tr("Archive Created With Problems"));
    msgBox.setIcon(problems.isEmpty() ? QMessageBox::Information : QMessageBox::Warning);
    msgBox.setText(tr("Archive created successfully!\n\n%1").arg(details.join(QLatin1Char('\n'))));
    if (!problems.isEmpty()) {
        msgBox.setInformativeText(problems.join(QLatin1Char('\n')));
    }

    QPushButton* openFolderBtn = msgBox.addButton(tr("Show in Folder"), QMessageBox::ActionRole);
    QPushButton* copyPasswordBtn = nullptr;
    if (passwordCanBeSent) {
        copyPasswordBtn = msgBox.addButton(tr("Copy Password"), QMessageBox::ActionRole);
    }
    QPushButton* copyLinkBtn = nullptr;
    if (!shareLink.isEmpty()) {
        copyLinkBtn = msgBox.addButton(tr("Copy Link"), QMessageBox::ActionRole);
    }
    msgBox.addButton(tr("Close"), QMessageBox::AcceptRole);

    msgBox.exec();

    if (copyPasswordBtn && msgBox.clickedButton() == copyPasswordBtn) {
        QGuiApplication::clipboard()->setText(password);
        QMessageBox::information(this, tr("Copied"), tr("Password copied to clipboard!"));
    } else if (copyLinkBtn && msgBox.clickedButton() == copyLinkBtn) {
        QGuiApplication::clipboard()->setText(m_pendingShareLink);
        QMessageBox::information(this, tr("Copied"), tr("Download link copied to clipboard."));
    } else if (msgBox.clickedButton() == openFolderBtn) {
        if (!revealInFileManager(destPath)) {
            showError(tr("The archive folder could not be opened."));
        }
    }

    accept();
}

void MainDialog::closeEvent(QCloseEvent* event)
{
    if (m_busy) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void MainDialog::reject()
{
    if (m_busy) {
        return;
    }
    QDialog::reject();
}

void MainDialog::showError(const QString& message)
{
    qWarning() << "ERROR:" << message;
    QMessageBox::critical(this, tr("Error"), message);
}

void MainDialog::showSuccess(const QString& message)
{
    qDebug() << "SUCCESS:" << message;
    QMessageBox::information(this, tr("Success"), message);
}