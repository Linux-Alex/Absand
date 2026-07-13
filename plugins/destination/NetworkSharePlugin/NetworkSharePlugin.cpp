// SPDX-License-Identifier: MPL-2.0

#include "NetworkSharePlugin.h"

#include "core/TransferHelpers.h"

#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>

namespace {

QLineEdit* shareEdit(QWidget* widget)
{
    return widget->findChild<QLineEdit*>(QStringLiteral("sharePath"));
}

QLineEdit* saveEdit(QWidget* widget)
{
    return widget->findChild<QLineEdit*>(QStringLiteral("saveFolder"));
}

QString defaultSaveFolder()
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    return folder;
}

} // namespace

NetworkSharePlugin::NetworkSharePlugin(QObject* parent)
    : QObject(parent)
{
}

QVariantMap NetworkSharePlugin::metadata() const
{
    return {
        { "name", name() },
        { "version", version() },
        { "description", description() },
        { "author", "Absand Team" },
        { "channel", channelName() }
    };
}

QVariantMap NetworkSharePlugin::defaultConfiguration() const
{
    return {
        { "sharePath", QDir::homePath() },
        { "saveFolder", defaultSaveFolder() }
    };
}

QWidget* NetworkSharePlugin::createConfigWidget(QWidget* parent)
{
    auto* widget = new QWidget(parent);
    auto* layout = new QFormLayout(widget);

    auto* shareRow = new QWidget(widget);
    auto* shareLayout = new QHBoxLayout(shareRow);
    shareLayout->setContentsMargins(0, 0, 0, 0);
    auto* share = new QLineEdit(shareRow);
    share->setObjectName("sharePath");
    auto* shareBrowse = new QPushButton(tr("Browse..."), shareRow);
    shareLayout->addWidget(share, 1);
    shareLayout->addWidget(shareBrowse);
    layout->addRow(tr("Share path"), shareRow);

    auto* saveRow = new QWidget(widget);
    auto* saveLayout = new QHBoxLayout(saveRow);
    saveLayout->setContentsMargins(0, 0, 0, 0);
    auto* save = new QLineEdit(saveRow);
    save->setObjectName("saveFolder");
    auto* saveBrowse = new QPushButton(tr("Browse..."), saveRow);
    saveLayout->addWidget(save, 1);
    saveLayout->addWidget(saveBrowse);
    layout->addRow(tr("Default save folder"), saveRow);

    QObject::connect(shareBrowse, &QPushButton::clicked, shareRow, [share, shareRow]() {
        const QString dir = QFileDialog::getExistingDirectory(shareRow, QObject::tr("Choose Share Folder"), share->text());
        if (!dir.isEmpty()) {
            share->setText(dir);
        }
    });
    QObject::connect(saveBrowse, &QPushButton::clicked, saveRow, [save, saveRow]() {
        const QString dir = QFileDialog::getExistingDirectory(saveRow, QObject::tr("Choose Save Folder"), save->text());
        if (!dir.isEmpty()) {
            save->setText(dir);
        }
    });

    return widget;
}

void NetworkSharePlugin::loadConfigurationWidget(QWidget* widget, const QVariantMap& config) const
{
    if (auto* share = shareEdit(widget)) {
        share->setText(config.value("sharePath", QDir::homePath()).toString());
    }
    if (auto* save = saveEdit(widget)) {
        save->setText(config.value("saveFolder", defaultSaveFolder()).toString());
    }
}

QVariantMap NetworkSharePlugin::saveConfigurationWidget(QWidget* widget) const
{
    QVariantMap config;
    if (auto* share = shareEdit(widget)) {
        config["sharePath"] = share->text().trimmed();
    }
    if (auto* save = saveEdit(widget)) {
        config["saveFolder"] = save->text().trimmed();
    }
    return config;
}

bool NetworkSharePlugin::validateConfiguration(const QVariantMap& config, QString* errorMessage) const
{
    const QString sharePath = config.value("sharePath").toString().trimmed();
    if (sharePath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("Please choose a network share path.");
        }
        return false;
    }
    return true;
}

QString NetworkSharePlugin::suggestedLocalSaveFolder(const QVariantMap& config) const
{
    return config.value("saveFolder", defaultSaveFolder()).toString();
}

bool NetworkSharePlugin::transferArchive(const QString& archivePath,
                                         const QVariantMap& config,
                                         std::function<void(bool, QString)> resultCallback)
{
    QString error;
    const QString sharePath = config.value("sharePath").toString();
    const bool ok = TransferHelpers::copyFileToFolder(archivePath, sharePath, &error);
    if (resultCallback) {
        resultCallback(ok, ok ? tr("Copied to network share") : error);
    }
    return ok;
}
