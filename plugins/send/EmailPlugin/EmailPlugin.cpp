// SPDX-License-Identifier: MPL-2.0

#include "EmailPlugin.h"
#include "core/SendHttpHelper.h"
#include "core/SmtpMessage.h"

QVariantMap EmailPlugin::metadata() const
{
    return { { "name", name() }, { "version", version() }, { "description", description() },
             { "author", "Absand Team" }, { "channel", channelName() } };
}

QVariantMap EmailPlugin::defaultConfiguration() const
{
    return { { "host", QString() },
             { "port", 587 },
             { "security", QStringLiteral("starttls") },
             { "username", QString() },
             { "password", QString() },
             { "sender", QString() },
             { "recipient", QString() },
             { "subject", tr("Absand archive password") },
             { "messageTemplate", tr("Archive password: {password}") } };
}

ConfigSchema EmailPlugin::configSchema() const
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
          tr("Absand archive password"), ConfigField::Required },
        { QStringLiteral("messageTemplate"), tr("Message template"), tr("Must contain {password}"), QString(),
          ConfigField::Type::Multiline, ConfigField::Browse::None, {},
          tr("Archive password: {password}"), ConfigField::Required }
    };
}

bool EmailPlugin::validateConfiguration(const QVariantMap& config, QString* error) const
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

    if (!config.value(QStringLiteral("messageTemplate")).toString().contains(QStringLiteral("{password}"))) {
        if (error) *error = tr("The message template must contain {password}.");
        return false;
    }
    return true;
}

void EmailPlugin::send(const QString& password, const QVariantMap& config,
                       std::function<void(bool, QString)> callback)
{
    QString error;
    if (password.isEmpty()) {
        error = tr("Cannot send an empty password.");
    } else if (validateConfiguration(config, &error)) {
        SmtpMessage::Settings settings = SmtpMessage::settingsFromConfig(
            config, tr("Absand archive password"));
        if (settings.sender.isEmpty()) {
            settings.sender = settings.username;
        }

        const QByteArray message = SmtpMessage::simpleMessage(
            settings, SendHttpHelper::messageFromTemplate(config, password));

        if (SmtpMessage::send(settings, message, &error)) {
            if (callback) callback(true, tr("Password sent by e-mail."));
            return;
        }
    }
    if (callback) callback(false, error);
}
