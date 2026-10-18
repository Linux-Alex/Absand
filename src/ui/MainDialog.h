// SPDX-License-Identifier: MPL-2.0

#ifndef MAINDIALOG_H
#define MAINDIALOG_H

#include <QDialog>
#include <QHash>
#include <QStringList>
#include <QVariantMap>
#include <memory>
#include "core/PluginManager.h"

// Forward declare libzip structs (C types)
struct zip;
struct zip_source;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class DestinationPluginInterface;
class SendPluginInterface;

QT_BEGIN_NAMESPACE
namespace Ui { class MainDialog; }
QT_END_NAMESPACE

class MainDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MainDialog(const QStringList& inputPaths, QWidget* parent = nullptr);
    ~MainDialog() override;

protected:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

public slots:
    void reject() override;

private slots:
    void onAddFilesClicked();
    void onRemoveSelectedClicked();
    void onClearFilesClicked();
    void onBrowseDestinationClicked();
    void onArchiveButtonClicked();
    void onPasswordGenerateClicked();
    void onPasswordCopyClicked();
    void onPasswordShowToggle(bool checked);
    void onEncryptionCheckboxChanged(bool checked);
    void onFormatChanged(int index);
    void onDestinationTypeChanged(int index);
    void onOpenArchiveBrowserClicked();
    void onOpenDestinationSettingsClicked();
    void onThemeChanged(int index);
    void onOpenSendSettingsClicked();
    void onLanguageChanged(int index);
    void onSendChannelChanged(int index);
    void onSendProfileChanged(int index);
    void onDestinationProfileChanged(int index);
    void storeSendRecipient();
    void storeDestinationRecipient();

signals:
    void languageChangeRequested(const QString& locale);

private:
    void setupUI();
    void setupAppearanceBar();
    void setupDestinationControls();
    void setupSendControls();
    void updateArchiveInfo();
    void generateRandomPassword();
    void loadArchivePlugins();
    void loadDestinationPlugins();
    void updateEncryptionControls();
    void updateDestinationPathForCurrentFormat();
    void refreshDestinationPath();
    void updateDestinationSelection();
    void updateSendChannelControls();
    void updateDestinationProfileControls();
    void refreshDestinationPlugins();
    DestinationPluginInterface* currentDestinationPlugin() const;
    QVariantMap currentDestinationConfig() const;
    bool transferArchiveIfNeeded(const QString& archivePath, QString* errorMessage = nullptr,
                                 QString* shareLink = nullptr);
    bool sendPasswordIfNeeded(const QString& password, QString* errorMessage = nullptr);
    SendPluginInterface* currentSendPlugin() const;
    QVariantMap currentSendConfig() const;
    void refreshInputList();
    void updateInputSummary();
    void addInputPaths(const QStringList& paths);
    bool createArchive(const QString& outputPath, bool encrypt, const QString& password);
    QString getDefaultFileName() const;
    qint64 calculateTotalSize() const;
    void showError(const QString& message);
    void showSuccess(const QString& message);
    
    std::unique_ptr<Ui::MainDialog> ui;
    PluginManager m_pluginManager;
    QStringList m_inputPaths;
    QString m_selectedDestination;
    QString m_currentPassword;
    QString m_lastArchivePath;
    QString m_generatedDestinationPath;
    QString m_pendingShareLink;
    bool m_busy = false;
    QComboBox* m_themeCombo = nullptr;
    QComboBox* m_languageCombo = nullptr;
    QComboBox* m_sendCombo = nullptr;
    QComboBox* m_sendProfileCombo = nullptr;
    QLabel* m_sendProfileLabel = nullptr;
    QLabel* m_sendRecipientLabel = nullptr;
    QLineEdit* m_sendRecipientEdit = nullptr;
    QComboBox* m_destinationProfileCombo = nullptr;
    QLabel* m_destinationProfileLabel = nullptr;
    QLabel* m_destinationRecipientLabel = nullptr;
    QLineEdit* m_destinationRecipientEdit = nullptr;
    QLabel* m_themeLabel = nullptr;
    QLabel* m_languageLabel = nullptr;
    QLabel* m_sendLabel = nullptr;
    QPushButton* m_openArchiveBrowserButton = nullptr;
    QPushButton* m_destinationSettingsButton = nullptr;
    QPushButton* m_sendSettingsButton = nullptr;
    QHash<QString, QVariantMap> m_destinationConfigCache;
    
    static constexpr int PASSWORD_LENGTH = 16;
};

#endif // MAINDIALOG_H
