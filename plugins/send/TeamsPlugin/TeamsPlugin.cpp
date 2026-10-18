// SPDX-License-Identifier: MPL-2.0

#include "TeamsPlugin.h"
#include "core/SendHttpHelper.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QVariantMap TeamsPlugin::metadata() const
{
    return { { "name", name() }, { "version", version() }, { "description", description() },
             { "author", "Absand Team" }, { "channel", channelName() } };
}

QVariantMap TeamsPlugin::defaultConfiguration() const
{
    return { { "webhookUrl", QString() },
             { "messageTemplate", tr("Archive password: {password}") } };
}

ConfigSchema TeamsPlugin::configSchema() const
{
    return {
        { QStringLiteral("webhookUrl"), tr("Workflow webhook URL"),
          tr("HTTPS webhook of the Teams workflow that posts to your channel"), QStringLiteral("https://"),
          ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant(), ConfigField::Required },
        { QStringLiteral("messageTemplate"), tr("Message template"), tr("Must contain {password}"), QString(),
          ConfigField::Type::Multiline, ConfigField::Browse::None, {},
          tr("Archive password: {password}"), ConfigField::Required }
    };
}

bool TeamsPlugin::validateConfiguration(const QVariantMap& config, QString* error) const
{
    if (!config.value(QStringLiteral("webhookUrl")).toString().trimmed().startsWith(QStringLiteral("https://"))) {
        if (error) *error = tr("A valid HTTPS Teams workflow webhook URL is required.");
        return false;
    }
    if (!config.value(QStringLiteral("messageTemplate")).toString().contains(QStringLiteral("{password}"))) {
        if (error) *error = tr("The message template must contain {password}.");
        return false;
    }
    return true;
}

void TeamsPlugin::send(const QString& password, const QVariantMap& config,
                       std::function<void(bool, QString)> callback)
{
    QString error;
    if (password.isEmpty()) {
        error = tr("Cannot send an empty password.");
    } else if (validateConfiguration(config, &error)) {
        const QString message = SendHttpHelper::messageFromTemplate(config, password);

        const QJsonObject body{
            { QStringLiteral("type"), QStringLiteral("message") },
            { QStringLiteral("attachments"), QJsonArray{
                QJsonObject{
                    { QStringLiteral("contentType"), QStringLiteral("application/vnd.microsoft.card.adaptive") },
                    { QStringLiteral("contentUrl"), QJsonValue::Null },
                    { QStringLiteral("content"), QJsonObject{
                        { QStringLiteral("$schema"), QStringLiteral("http://adaptivecards.io/schemas/adaptive-card.json") },
                        { QStringLiteral("type"), QStringLiteral("AdaptiveCard") },
                        { QStringLiteral("version"), QStringLiteral("1.4") },
                        { QStringLiteral("body"), QJsonArray{
                            QJsonObject{ { QStringLiteral("type"), QStringLiteral("TextBlock") },
                                         { QStringLiteral("text"), message },
                                         { QStringLiteral("wrap"), true } }
                        } }
                    } }
                }
            } }
        };

        if (SendHttpHelper::postJson(config.value(QStringLiteral("webhookUrl")).toString().trimmed(),
                                     QJsonDocument(body).toJson(QJsonDocument::Compact), {}, &error)) {
            if (callback) callback(true, tr("Password sent over Microsoft Teams."));
            return;
        }
    }
    if (callback) callback(false, error);
}
