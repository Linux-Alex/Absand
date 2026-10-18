// SPDX-License-Identifier: MPL-2.0

#include "EmailDestinationPlugin.h"

#include "core/SendHttpHelper.h"
#include "core/SmtpMessage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

QString defaultSaveFolder()
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    return folder;
}

} // namespace

EmailDestinationPlugin::EmailDestinationPlugin(QObject* parent)
    : QObject(parent)
{
}

QVariantMap EmailDestinationPlugin::metadata() const
{
    return {
        { "name", name() },
        { "version", version() },
        { "description", description() },
        { "author", "Absand Team" },
        { "channel", channelName() }
    };
}

QVariantMap EmailDestinationPlugin::defaultConfiguration() const
{
    return { { "host", QString() },
             { "port", 587 },
             { "security", QStringLiteral("starttls") },
             { "username", QString() },
             { "password", QString() },
             { "sender", QString() },
             { "recipient", QString() },
             { "subject", tr("Encrypted archive from Absand") },
             { "messageTemplate", tr("The encrypted archive is attached. The password is delivered separately.") },
             { "maxAttachmentMb", 20 },
             { "saveFolder", defaultSaveFolder() } };
}

ConfigSchema EmailDestinationPlugin::configSchema() const
{
    return {
        { QStringLiteral("host"), tr("SMTP server"), tr("For example smtp.gmail.com"), QStringLiteral("smtp.example.com"),
          ConfigField::Type::Text, ConfigField::Browse::None, {}, QVariant(), ConfigField::Required },
        { QStringLiteral("port"), tr("Port"), tr("587 for STARTTLS, 465 for implicit TLS"), QStringLiteral("587"),
          ConfigField::Type::Integer, ConfigField::Browse::None, {}, 587, ConfigField::Required },
        { QStringLiteral("security"), tr("Security"), QString(), QString(),
          ConfigField::Type::Choice, ConfigField::Browse::None,
          { QStringLiteral("starttls"), QStringLiteral("ssl"), QStringLiteral("none") },
          QStringLiteral("starttls"), ConfigField::Required },
        { QStringLiteral("username"), tr("Username"), tr("Often the full e-mail address"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::None, {}, QVariant() },
        { QStringLiteral("password"), tr("Password"),
          tr("For Gmail and most providers create an app-specific password"), QString(),
          ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant(), ConfigField::Required },
        { QStringLiteral("sender"), tr("Sender address"), tr("Defaults to the username"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::None, {}, QVariant() },
        { QStringLiteral("recipient"), tr("Recipient"), tr("Separate several addresses with commas"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::None, {}, QVariant(),
          ConfigField::Required | ConfigField::Recipient },
        { QStringLiteral("subject"), tr("Subject"), QString(), QString(),
          ConfigField::Type::Text, ConfigField::Browse::None, {},
          tr("Encrypted archive from Absand"), ConfigField::Required },
        { QStringLiteral("messageTemplate"), tr("Message text"), QString(), QString(),
          ConfigField::Type::Multiline, ConfigField::Browse::None, {},
          tr("The encrypted archive is attached. The password is delivered separately."), ConfigField::Required },
        { QStringLiteral("maxAttachmentMb"), tr("Maximum attachment size (MB)"),
          tr("Most mail servers reject messages over roughly 25 MB"), QStringLiteral("20"),
          ConfigField::Type::Integer, ConfigField::Browse::None, {}, 20, ConfigField::Required },
        { QStringLiteral("saveFolder"), tr("Default save folder"),
          tr("Where the archive is written before it is mailed"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::Directory, {},
          defaultSaveFolder(), ConfigField::Required }
    };
}

bool EmailDestinationPlugin::validateConfiguration(const QVariantMap& config, QString* error) const
{
    if (config.value(QStringLiteral("host")).toString().trimmed().isEmpty()) {
        if (error) *error = tr("Please enter the SMTP server.");
        return false;
    }

    const int port = config.value(QStringLiteral("port"), 587).toInt();
    if (port <= 0 || port > 65535) {
        if (error) *error = tr("Please enter a port between 1 and 65535.");
        return false;
    }

    const QStringList recipients = SendHttpHelper::splitAddresses(
        config.value(QStringLiteral("recipient")).toString());
    if (recipients.isEmpty()) {
        if (error) *error = tr("Please enter at least one recipient address.");
        return false;
    }
    for (const QString& recipient : recipients) {
        if (!recipient.contains(QLatin1Char('@')) || recipient.startsWith(QLatin1Char('@'))) {
            if (error) *error = tr("%1 does not look like an e-mail address.").arg(recipient);
            return false;
        }
    }

    if (config.value(QStringLiteral("maxAttachmentMb"), 20).toInt() <= 0) {
        if (error) *error = tr("The maximum attachment size must be greater than zero.");
        return false;
    }

    return true;
}

QString EmailDestinationPlugin::suggestedLocalSaveFolder(const QVariantMap& config) const
{
    return config.value("saveFolder", defaultSaveFolder()).toString();
}

bool EmailDestinationPlugin::transferArchive(const QString& archivePath,
                                             const QVariantMap& config,
                                             TransferCallback resultCallback)
{
    auto report = [resultCallback](bool ok, const QString& message) {
        if (resultCallback) {
            resultCallback(ok, message, QString());
        }
        return ok;
    };

    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists() || !archiveInfo.isFile()) {
        return report(false, tr("The archive file no longer exists."));
    }

    const qint64 limitBytes = static_cast<qint64>(
        config.value(QStringLiteral("maxAttachmentMb"), 20).toInt()) * 1024 * 1024;
    if (limitBytes > 0 && archiveInfo.size() > limitBytes) {
        return report(false, tr("The archive is larger than the %1 MB attachment limit. "
                                "Raise the limit in the destination settings or use another destination.")
                                .arg(config.value(QStringLiteral("maxAttachmentMb"), 20).toInt()));
    }

    QFile archive(archivePath);
    if (!archive.open(QIODevice::ReadOnly)) {
        return report(false, tr("Cannot read the archive: %1").arg(archive.errorString()));
    }
    const QByteArray content = archive.readAll();
    archive.close();

    SmtpMessage::Settings settings = SmtpMessage::settingsFromConfig(
        config, tr("Encrypted archive from Absand"));
    if (settings.sender.isEmpty()) {
        settings.sender = settings.username;
    }

    const QString body = config.value(QStringLiteral("messageTemplate")).toString().trimmed();
    const QByteArray message = SmtpMessage::attachmentMessage(
        settings,
        body.isEmpty() ? tr("The encrypted archive is attached.") : body,
        archiveInfo.fileName(),
        content);

    QString error;
    if (!SmtpMessage::send(settings, message, &error, 10 * 60 * 1000)) {
        return report(false, tr("The archive could not be mailed: %1").arg(error));
    }

    return report(true, tr("Archive mailed to %1.").arg(settings.recipients.join(QStringLiteral(", "))));
}
