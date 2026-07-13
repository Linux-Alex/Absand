// SPDX-License-Identifier: MPL-2.0

#include "SFTPDestinationPlugin.h"

#include "core/TransferHelpers.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>

namespace {

QLineEdit* line(QWidget* widget, const char* name)
{
    return widget->findChild<QLineEdit*>(QString::fromUtf8(name));
}

QString defaultSaveFolder()
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    return folder;
}

QWidget* folderRow(QWidget* parent, QLineEdit*& editOut, const QString& objectName, const QString& title)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* edit = new QLineEdit(row);
    edit->setObjectName(objectName);
    auto* browse = new QPushButton(QObject::tr("Browse..."), row);
    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    QObject::connect(browse, &QPushButton::clicked, row, [edit, row, title]() {
        const QString dir = QFileDialog::getExistingDirectory(row, title, edit->text());
        if (!dir.isEmpty()) {
            edit->setText(dir);
        }
    });
    editOut = edit;
    return row;
}

} // namespace

SFTPDestinationPlugin::SFTPDestinationPlugin(QObject* parent)
    : QObject(parent)
{
}

QVariantMap SFTPDestinationPlugin::metadata() const
{
    return {
        { "name", name() },
        { "version", version() },
        { "description", description() },
        { "author", "Absand Team" },
        { "channel", channelName() }
    };
}

QVariantMap SFTPDestinationPlugin::defaultConfiguration() const
{
    return {
        { "host", QString() },
        { "port", 22 },
        { "username", QString() },
        { "password", QString() },
        { "remotePath", QStringLiteral("/") },
        { "saveFolder", defaultSaveFolder() }
    };
}

QWidget* SFTPDestinationPlugin::createConfigWidget(QWidget* parent)
{
    auto* widget = new QWidget(parent);
    auto* layout = new QFormLayout(widget);

    auto* host = new QLineEdit(widget);
    host->setObjectName("host");
    layout->addRow(tr("Host"), host);

    auto* port = new QLineEdit(widget);
    port->setObjectName("port");
    layout->addRow(tr("Port"), port);

    auto* username = new QLineEdit(widget);
    username->setObjectName("username");
    layout->addRow(tr("Username"), username);

    auto* password = new QLineEdit(widget);
    password->setObjectName("password");
    password->setEchoMode(QLineEdit::Password);
    layout->addRow(tr("Password"), password);

    auto* remotePath = new QLineEdit(widget);
    remotePath->setObjectName("remotePath");
    layout->addRow(tr("Remote folder"), remotePath);

    QLineEdit* saveFolder = nullptr;
    layout->addRow(tr("Default save folder"), folderRow(widget, saveFolder, "saveFolder", tr("Choose Save Folder")));

    return widget;
}

void SFTPDestinationPlugin::loadConfigurationWidget(QWidget* widget, const QVariantMap& config) const
{
    const QVariantMap defaults = defaultConfiguration();
    if (auto* edit = line(widget, "host")) edit->setText(config.value("host", defaults.value("host")).toString());
    if (auto* edit = line(widget, "port")) edit->setText(QString::number(config.value("port", defaults.value("port")).toInt()));
    if (auto* edit = line(widget, "username")) edit->setText(config.value("username", defaults.value("username")).toString());
    if (auto* edit = line(widget, "password")) edit->setText(config.value("password", defaults.value("password")).toString());
    if (auto* edit = line(widget, "remotePath")) edit->setText(config.value("remotePath", defaults.value("remotePath")).toString());
    if (auto* edit = line(widget, "saveFolder")) edit->setText(config.value("saveFolder", defaults.value("saveFolder")).toString());
}

QVariantMap SFTPDestinationPlugin::saveConfigurationWidget(QWidget* widget) const
{
    QVariantMap config;
    if (auto* edit = line(widget, "host")) config["host"] = edit->text().trimmed();
    if (auto* edit = line(widget, "port")) config["port"] = edit->text().trimmed().toInt();
    if (auto* edit = line(widget, "username")) config["username"] = edit->text().trimmed();
    if (auto* edit = line(widget, "password")) config["password"] = edit->text();
    if (auto* edit = line(widget, "remotePath")) config["remotePath"] = edit->text().trimmed();
    if (auto* edit = line(widget, "saveFolder")) config["saveFolder"] = edit->text().trimmed();
    return config;
}

bool SFTPDestinationPlugin::validateConfiguration(const QVariantMap& config, QString* errorMessage) const
{
    const QString host = config.value("host").toString().trimmed();
    if (host.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("Please enter the SFTP host.");
        }
        return false;
    }
    return true;
}

QString SFTPDestinationPlugin::suggestedLocalSaveFolder(const QVariantMap& config) const
{
    return config.value("saveFolder", defaultSaveFolder()).toString();
}

bool SFTPDestinationPlugin::transferArchive(const QString& archivePath,
                                            const QVariantMap& config,
                                            std::function<void(bool, QString)> resultCallback)
{
    const QString host = config.value("host").toString().trimmed();
    const int port = config.value("port", 22).toInt();
    const QString username = config.value("username").toString().trimmed();
    const QString password = config.value("password").toString();
    const QString remotePath = config.value("remotePath", QStringLiteral("/")).toString();
    const QString fileName = QFileInfo(archivePath).fileName();

    QStringList args;
    args << "-T" << archivePath;
    if (!username.isEmpty() || !password.isEmpty()) {
        args << "--user" << (username + ":" + password);
    }
    args << TransferHelpers::buildTargetUrl("sftp", host, port, remotePath, fileName);

    QString error;
    const bool ok = TransferHelpers::uploadWithCurl(args, &error);
    if (resultCallback) {
        resultCallback(ok, ok ? tr("Uploaded to SFTP server") : error);
    }
    return ok;
}
