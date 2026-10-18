// SPDX-License-Identifier: MPL-2.0

#include "DiscordPlugin.h"
#include "core/SendHttpHelper.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QVariantMap DiscordPlugin::metadata() const
{
    return { { "name", name() }, { "version", version() }, { "description", description() },
             { "author", "Absand Team" }, { "channel", channelName() } };
}

QVariantMap DiscordPlugin::defaultConfiguration() const
{
    return { { "webhookUrl", QString() },
             { "username", QStringLiteral("Absand") },
             { "messageTemplate", tr("Archive password: {password}") } };
}

ConfigSchema DiscordPlugin::configSchema() const
{
    return {
        { QStringLiteral("webhookUrl"), tr("Webhook URL"),
          tr("Channel settings -> Integrations -> Webhooks -> Copy webhook URL"), QStringLiteral("https://discord.com/api/webhooks/..."),
          ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant(), ConfigField::Required },
        { QStringLiteral("username"), tr("Sender name"), tr("Name shown next to the message"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::None, {}, QStringLiteral("Absand") },
        { QStringLiteral("messageTemplate"), tr("Message template"), tr("Must contain {password}"), QString(),
          ConfigField::Type::Multiline, ConfigField::Browse::None, {},
          tr("Archive password: {password}"), ConfigField::Required }
    };
}

bool DiscordPlugin::validateConfiguration(const QVariantMap& config, QString* error) const
{
    if (!config.value(QStringLiteral("webhookUrl")).toString().trimmed().startsWith(QStringLiteral("https://"))) {
        if (error) *error = tr("A valid HTTPS Discord webhook URL is required.");
        return false;
    }
    if (!config.value(QStringLiteral("messageTemplate")).toString().contains(QStringLiteral("{password}"))) {
        if (error) *error = tr("The message template must contain {password}.");
        return false;
    }
    return true;
}

void DiscordPlugin::send(const QString& password, const QVariantMap& config,
                         std::function<void(bool, QString)> callback)
{
    QString error;
    if (password.isEmpty()) {
        error = tr("Cannot send an empty password.");
    } else if (validateConfiguration(config, &error)) {
        QJsonObject body{ { QStringLiteral("content"), SendHttpHelper::messageFromTemplate(config, password) } };
        const QString username = config.value(QStringLiteral("username")).toString().trimmed();
        if (!username.isEmpty()) {
            body.insert(QStringLiteral("username"), username);
        }
        body.insert(QStringLiteral("allowed_mentions"), QJsonObject{ { QStringLiteral("parse"), QJsonArray() } });

        if (SendHttpHelper::postJson(config.value(QStringLiteral("webhookUrl")).toString().trimmed(),
                                     QJsonDocument(body).toJson(QJsonDocument::Compact), {}, &error)) {
            if (callback) callback(true, tr("Password sent over Discord."));
            return;
        }
    }
    if (callback) callback(false, error);
}
