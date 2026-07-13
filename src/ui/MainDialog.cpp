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

#ifndef ZIP_EM_AES_256
#define ZIP_EM_AES_256 0x6601
#endif

#ifndef ZIP_EM_TRAD_PKWARE
#define ZIP_EM_TRAD_PKWARE 0x0001
#endif

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
    setWindowTitle("Absand - Secure Archive Sender");

    ui->inputListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->inputListWidget->setAlternatingRowColors(true);
    ui->inputListWidget->setSortingEnabled(false);
    ui->inputListWidget->setDragEnabled(false);
    ui->inputListWidget->setDropIndicatorShown(true);
    ui->inputListWidget->setDefaultDropAction(Qt::CopyAction);
    ui->inputListWidget->setToolTip("Drop files or folders here");

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
    ui->destinationPathEdit->setText(currentDir + "/" + getDefaultFileName());
    
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
    connect(ui->encryptCheckbox, &QCheckBox::checkStateChanged, this, &MainDialog::onEncryptionCheckboxChanged);
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
    row->addWidget(m_sendLabel); row->addWidget(m_sendCombo, 1); row->addWidget(m_sendSettingsButton);
    ui->verticalLayout_2->addLayout(row);
    connect(m_sendSettingsButton, &QPushButton::clicked, this, &MainDialog::onOpenSendSettingsClicked);
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
        if (m_languageCombo) {
            const QSignalBlocker blocker(m_languageCombo);
            m_languageCombo->setItemText(0, tr("English"));
            m_languageCombo->setItemText(1, tr("Slovenian"));
        }
        if (m_sendCombo) m_sendCombo->setItemText(0, tr("Do not send"));
        updateInputSummary();
        updateArchiveInfo();
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
        layout->addWidget(m_destinationSettingsButton);
        connect(m_destinationSettingsButton, &QPushButton::clicked,
                this, &MainDialog::onOpenDestinationSettingsClicked);
    }
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
    }

    updateDestinationSelection();
}

void MainDialog::onEncryptionMethodChanged(int index)
{
    Q_UNUSED(index);
    // Encryption method selection logic
}

void MainDialog::onOpenArchiveBrowserClicked()
{
    ArchiveBrowserDialog dialog(this);
    dialog.exec();
}

void MainDialog::onOpenDestinationSettingsClicked()
{
    DestinationSettingsDialog dialog(&m_pluginManager, this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshDestinationPlugins();
        updateDestinationSelection();
    }
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
    SendSettingsDialog dialog(&m_pluginManager, this);
    dialog.exec();
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
    ui->destinationPathEdit->setText(m_selectedDestination + "/" + getDefaultFileName());
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
    QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Add");
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
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Destination Folder"), m_selectedDestination);
    if (!dir.isEmpty()) {
        m_selectedDestination = dir;
        ui->destinationPathEdit->setText(dir + "/" + getDefaultFileName());
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

void MainDialog::updateDestinationPathForCurrentFormat()
{
    const ArchivePluginInterface* plugin = ui->formatCombo->currentData().value<ArchivePluginInterface*>();
    const QString newExt = plugin ? plugin->defaultExtension() : QStringLiteral("zip");
    QString currentPath = ui->destinationPathEdit->text().trimmed();

    if (currentPath.isEmpty()) {
        ui->destinationPathEdit->setText(m_selectedDestination + "/" + getDefaultFileName());
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

void MainDialog::updateDestinationSelection()
{
    auto* plugin = currentDestinationPlugin();
    if (plugin) {
        const QVariantMap config = currentDestinationConfig();
        m_selectedDestination = plugin->suggestedLocalSaveFolder(config);
        if (m_selectedDestination.isEmpty()) {
            m_selectedDestination = QDir::homePath();
        }
        ui->destinationPathEdit->setText(m_selectedDestination + "/" + getDefaultFileName());
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
            ui->encryptCheckbox->setToolTip("This format does not support encryption");
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
    showSuccess("Password copied to clipboard!");
}

void MainDialog::onPasswordShowToggle(bool checked)
{
    ui->passwordEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

void MainDialog::onEncryptionCheckboxChanged(int state)
{
    Q_UNUSED(state);
    updateEncryptionControls();
}

bool MainDialog::createArchive(const QString& outputPath, bool encrypt, const QString& password)
{
    // Get the selected archive plugin
    ArchivePluginInterface* plugin = ui->formatCombo->currentData().value<ArchivePluginInterface*>();
    if (!plugin) {
        showError("No archive plugin selected!");
        return false;
    }
    
    qDebug() << "Using plugin:" << plugin->name() << "to create archive:" << outputPath;
    
    // Create archive using the plugin
    return plugin->createArchive(m_inputPaths, outputPath, encrypt ? password : QString(), nullptr);
}

void MainDialog::transferArchiveIfNeeded(const QString& archivePath)
{
    auto* plugin = currentDestinationPlugin();
    if (!plugin) {
        return;
    }

    const QVariantMap config = currentDestinationConfig();
    QString error;
    if (!plugin->validateConfiguration(config, &error)) {
        QMessageBox::warning(this, tr("Destination Not Ready"),
                             tr("%1: %2").arg(plugin->name(), error));
        return;
    }

    if (!plugin->transferArchive(archivePath, config, nullptr)) {
        QMessageBox::warning(this, tr("Transfer Failed"),
                             tr("The archive was created, but the transfer step failed."));
    }
}

SendPluginInterface* MainDialog::currentSendPlugin() const
{
    if (!m_sendCombo) return nullptr;
    return qobject_cast<SendPluginInterface*>(m_sendCombo->currentData().value<QObject*>());
}

QVariantMap MainDialog::currentSendConfig() const
{
    auto* plugin = currentSendPlugin();
    if (!plugin) return {};
    QVariantMap config = plugin->defaultConfiguration();
    const QVariantMap stored = UserConfig::loadSendConfig(plugin->configKey());
    for (auto it = stored.cbegin(); it != stored.cend(); ++it) config[it.key()] = it.value();
    return config;
}

bool MainDialog::sendPasswordIfNeeded(const QString& password)
{
    auto* plugin = currentSendPlugin();
    if (!plugin) return true;
    const QVariantMap config = currentSendConfig();
    QString validationError;
    if (!plugin->validateConfiguration(config, &validationError)) {
        QMessageBox::warning(this, tr("Delivery Not Ready"), tr("%1: %2").arg(plugin->name(), validationError));
        return false;
    }
    bool success = false;
    QString message;
    plugin->send(password, config, [&](bool ok, QString text) { success = ok; message = text; });
    if (!success) QMessageBox::warning(this, tr("Password Delivery Failed"), message);
    return success;
}

void MainDialog::onArchiveButtonClicked()
{
    if (m_inputPaths.isEmpty()) {
        showError("No files selected");
        return;
    }
    
    QString destPath = ui->destinationPathEdit->text();
    if (destPath.isEmpty()) {
        showError("Please select a destination");
        return;
    }
    
    bool useEncryption = ui->encryptCheckbox->isChecked();
    QString password = ui->passwordEdit->text();
    
    // Check if the selected plugin supports encryption
    ArchivePluginInterface* plugin = ui->formatCombo->currentData().value<ArchivePluginInterface*>();
    if (useEncryption && plugin && !plugin->supportsPassword()) {
        QMessageBox::warning(this, "Encryption Not Supported",
            QString("%1 format does not support encryption.\n\n"
                    "The archive will be created WITHOUT encryption.\n\n"
                    "For encrypted archives, please use ZIP format.")
                    .arg(plugin->name()));
        useEncryption = false;
    }
    
    if (useEncryption && password.isEmpty()) {
        showError("Please set a password for encryption");
        return;
    }
    
    // Only confirm encryption if the format actually supports it
    if (useEncryption && plugin && plugin->supportsPassword()) {
        QMessageBox confirmBox(this);
        confirmBox.setWindowTitle("Confirm Encryption");
        confirmBox.setText(QString("Encrypt archive with %1?\n\nPassword: %2")
            .arg(ui->encryptionCombo->currentText())
            .arg(password));
        confirmBox.setInformativeText("AES-256 provides military-grade security.");
        confirmBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        confirmBox.setDefaultButton(QMessageBox::Yes);
        
        if (confirmBox.exec() != QMessageBox::Yes) {
            return;
        }
    }
    
    qDebug() << "Creating archive...";
    qDebug() << "  Destination:" << destPath;
    qDebug() << "  Encryption:" << (useEncryption ? ui->encryptionCombo->currentText() : "None");
    
    ui->archiveButton->setEnabled(false);
    ui->archiveButton->setText("Creating archive...");
    qApp->processEvents();
    
    bool success = createArchive(destPath, useEncryption, password);
    
    ui->archiveButton->setEnabled(true);
    ui->archiveButton->setText("Create Archive");
    
    if (success) {
        qDebug() << "Archive created successfully:" << destPath;
        m_lastArchivePath = destPath;

        transferArchiveIfNeeded(destPath);
        if (useEncryption && !password.isEmpty()) {
            sendPasswordIfNeeded(password);
        }
        
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Success");
        
        QString sizeStr;
        qint64 size = QFileInfo(destPath).size();
        if (size < 1024) {
            sizeStr = QString("%1 B").arg(size);
        } else if (size < 1024 * 1024) {
            sizeStr = QString("%1 KB").arg(size / 1024.0, 0, 'f', 2);
        } else {
            sizeStr = QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
        }
        
        msgBox.setText(QString("Archive created successfully!\n\n%1\n\nSize: %2")
            .arg(QFileInfo(destPath).fileName())
            .arg(sizeStr));
        
        QPushButton* copyPasswordBtn = nullptr;
        QPushButton* openFolderBtn = msgBox.addButton("Open Folder", QMessageBox::ActionRole);
        QPushButton* closeBtn = msgBox.addButton("Close", QMessageBox::AcceptRole);
        
        if (useEncryption && !password.isEmpty()) {
            copyPasswordBtn = msgBox.addButton("Copy Password", QMessageBox::ActionRole);
        }
        
        msgBox.exec();
        
        if (copyPasswordBtn && msgBox.clickedButton() == copyPasswordBtn) {
            QGuiApplication::clipboard()->setText(password);
            QMessageBox::information(this, "Copied", "Password copied to clipboard!");
        } else if (msgBox.clickedButton() == openFolderBtn) {
            QProcess::startDetached("dolphin", QStringList() << QFileInfo(destPath).absolutePath());
        }
        
        accept();
    } else {
        showError("Archive creation failed!");
    }
}

void MainDialog::showError(const QString& message)
{
    qWarning() << "ERROR:" << message;
    QMessageBox::critical(this, "Error", message);
}

void MainDialog::showSuccess(const QString& message)
{
    qDebug() << "SUCCESS:" << message;
    QMessageBox::information(this, "Success", message);
}
