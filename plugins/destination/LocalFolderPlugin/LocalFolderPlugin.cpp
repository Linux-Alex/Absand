// SPDX-License-Identifier: MPL-2.0

#include "LocalFolderPlugin.h"

#include "core/TransferHelpers.h"

#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>

namespace {

QLineEdit* folderEdit(QWidget* widget)
{
    return widget->findChild<QLineEdit*>(QStringLiteral("targetFolder"));
}

QString defaultFolder()
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    return folder;
}

} // namespace

LocalFolderPlugin::LocalFolderPlugin(QObject* parent)
    : QObject(parent)
{
}

QVariantMap LocalFolderPlugin::metadata() const
{
    return {
        { "name", name() },
        { "version", version() },
        { "description", description() },
        { "author", "Absand Team" },
        { "channel", channelName() }
    };
}

QVariantMap LocalFolderPlugin::defaultConfiguration() const
{
    return { { "targetFolder", defaultFolder() } };
}

QWidget* LocalFolderPlugin::createConfigWidget(QWidget* parent)
{
    auto* widget = new QWidget(parent);
    auto* layout = new QFormLayout(widget);

    auto* row = new QWidget(widget);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    auto* edit = new QLineEdit(row);
    edit->setObjectName("targetFolder");
    auto* browse = new QPushButton(tr("Browse..."), row);
    rowLayout->addWidget(edit, 1);
    rowLayout->addWidget(browse);

    layout->addRow(tr("Target folder"), row);

    QObject::connect(browse, &QPushButton::clicked, row, [edit, row]() {
        const QString dir = QFileDialog::getExistingDirectory(row, QObject::tr("Choose Folder"), edit->text());
        if (!dir.isEmpty()) {
            edit->setText(dir);
        }
    });

    return widget;
}

void LocalFolderPlugin::loadConfigurationWidget(QWidget* widget, const QVariantMap& config) const
{
    if (auto* edit = folderEdit(widget)) {
        edit->setText(config.value("targetFolder", defaultFolder()).toString());
    }
}

QVariantMap LocalFolderPlugin::saveConfigurationWidget(QWidget* widget) const
{
    QVariantMap config;
    if (auto* edit = folderEdit(widget)) {
        config["targetFolder"] = edit->text().trimmed();
    }
    return config;
}

bool LocalFolderPlugin::validateConfiguration(const QVariantMap& config, QString* errorMessage) const
{
    const QString folder = config.value("targetFolder").toString().trimmed();
    if (folder.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("Please choose a target folder.");
        }
        return false;
    }
    return true;
}

QString LocalFolderPlugin::suggestedLocalSaveFolder(const QVariantMap& config) const
{
    return config.value("targetFolder", defaultFolder()).toString();
}

bool LocalFolderPlugin::transferArchive(const QString& archivePath,
                                        const QVariantMap& config,
                                        std::function<void(bool, QString)> resultCallback)
{
    QString error;
    const QString folder = suggestedLocalSaveFolder(config);
    const bool ok = TransferHelpers::copyFileToFolder(archivePath, folder, &error);
    if (resultCallback) {
        resultCallback(ok, ok ? tr("Saved to %1").arg(folder) : error);
    }
    return ok;
}
