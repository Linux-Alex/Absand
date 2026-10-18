// SPDX-License-Identifier: MPL-2.0

#include "WebhookPlugin.h"
#include "core/SendHttpHelper.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString presetOf(const QVariantMap& config)
{
    return config.value(QStringLiteral("preset"), QStringLiteral("Custom")).toString();
}

QByteArray jsonBody(const QString& key, const QString& value)
{
    return QJsonDocument(QJsonObject{ { key, value } }).toJson(QJsonDocument::Compact);
}

} // namespace

QVariantMap WebhookPlugin::metadata() const
{
    return { { "name", name() }, { "version", version() }, { "description", description() },
             { "author", "Absand Team" }, { "channel", channelName() } };
}

QVariantMap WebhookPlugin::defaultConfiguration() const
{
    return { { "preset", QStringLiteral("Custom") },
             { "url", QString() },
             { "authHeader", QString() },
             { "contentType", QStringLiteral("json") },
             { "bodyTemplate", QStringLiteral("{\"text\": \"{message}\"}") },
             { "messageTemplate", tr("Archive password: {password}") } };
}

ConfigSchema WebhookPlugin::configSchema() const
{
    return {
        { QStringLiteral("preset"), tr("Service"),
          tr("Chooses the message format; the URL is always yours"), QString(),
          ConfigField::Type::Choice, ConfigField::Browse::None,
          { QStringLiteral("Custom"), QStringLiteral("Slack / Mattermost"), QStringLiteral("Google Chat"),
            QStringLiteral("Discord"), QStringLiteral("Gotify") },
          QStringLiteral("Custom"), ConfigField::Required },
        { QStringLiteral("url"), tr("Webhook URL"),
          tr("Gotify: https://host/message?token=APP_TOKEN"), QStringLiteral("https://"),
          ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant(), ConfigField::Required },
        { QStringLiteral("authHeader"), tr("Additional header"),
          tr("Optional, for example Authorization: Bearer abc"), QString(),
          ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant(), ConfigField::Advanced },
        { QStringLiteral("contentType"), tr("Content type"), QString(),
          QString(),
          ConfigField::Type::Choice, ConfigField::Browse::None,
          { QStringLiteral("json"), QStringLiteral("form"), QStringLiteral("text") },
          QStringLiteral("json"), ConfigField::Advanced },
        { QStringLiteral("bodyTemplate"), tr("Request body"),
          tr("Used by the custom service. {message} and {password} are replaced"), QString(),
          ConfigField::Type::Multiline, ConfigField::Browse::None, {},
          QStringLiteral("{\"text\": \"{message}\"}"), ConfigField::Advanced },
        { QStringLiteral("messageTemplate"), tr("Message template"), tr("Must contain {password}"), QString(),
          ConfigField::Type::Multiline, ConfigField::Browse::None, {},
          tr("Archive password: {password}"), ConfigField::Required }
    };
}

bool WebhookPlugin::validateConfiguration(const QVariantMap& config, QString* error) const
{
    const QString url = config.value(QStringLiteral("url")).toString().trimmed();
    if (!url.startsWith(QStringLiteral("http://")) && !url.startsWith(QStringLiteral("https://"))) {
        if (error) *error = tr("Please enter the webhook URL.");
        return false;
    }
    if (!config.value(QStringLiteral("messageTemplate")).toString().contains(QStringLiteral("{password}"))) {
        if (error) *error = tr("The message template must contain {password}.");
        return false;
    }
    return true;
}

void WebhookPlugin::send(const QString& password, const QVariantMap& config,
                         std::function<void(bool, QString)> callback)
{
    QString error;
    if (password.isEmpty()) {
        error = tr("Cannot send an empty password.");
    } else if (validateConfiguration(config, &error)) {
        const QString preset = presetOf(config);
        const QString message = SendHttpHelper::messageFromTemplate(config, password);
        const QString contentType = config.value(QStringLiteral("contentType"), QStringLiteral("json")).toString();

        QByteArray body;
        QByteArray contentTypeHeader;
        if (preset == QStringLiteral("Slack / Mattermost") || preset == QStringLiteral("Google Chat")) {
            body = jsonBody(QStringLiteral("text"), message);
            contentTypeHeader = "application/json";
        } else if (preset == QStringLiteral("Discord")) {
            body = jsonBody(QStringLiteral("content"), message);
            contentTypeHeader = "application/json";
        } else if (preset == QStringLiteral("Gotify")) {
            body = jsonBody(QStringLiteral("message"), message);
            body = QJsonDocument(QJsonObject{ { QStringLiteral("message"), message },
                                              { QStringLiteral("title"), tr("Absand") } })
                       .toJson(QJsonDocument::Compact);
            contentTypeHeader = "application/json";
        } else if (contentType == QStringLiteral("form")) {
            body = "message=" + QUrl::toPercentEncoding(message);
            contentTypeHeader = "application/x-www-form-urlencoded";
        } else if (contentType == QStringLiteral("text")) {
            body = message.toUtf8();
            contentTypeHeader = "text/plain; charset=UTF-8";
        } else {
            QString templated = config.value(QStringLiteral("bodyTemplate")).toString();
            if (templated.trimmed().isEmpty()) {
                templated = QStringLiteral("{\"text\": \"{message}\"}");
            }
            // Escape for JSON only when the template looks like JSON.
            templated.replace(QStringLiteral("{message}"), message);
            templated.replace(QStringLiteral("{password}"), password);
            body = templated.toUtf8();
            contentTypeHeader = "application/json";
        }

        QByteArray curlConfig = "silent\nshow-error\nfail-with-body\nrequest = \"POST\"\n";
        curlConfig += "connect-timeout = 20\n";
        curlConfig += "url = \"" + SendHttpHelper::escapeConfigValue(
                          config.value(QStringLiteral("url")).toString().trimmed().toUtf8()) + "\"\n";
        curlConfig += "header = \"Content-Type: " + contentTypeHeader + "\"\n";

        const QString extraHeader = config.value(QStringLiteral("authHeader")).toString().trimmed();
        if (!extraHeader.isEmpty()) {
            curlConfig += "header = \"" + SendHttpHelper::escapeConfigValue(
                              SendHttpHelper::sanitizedHeaderValue(extraHeader).toUtf8()) + "\"\n";
        }
        curlConfig += "data-binary = \"" + SendHttpHelper::escapeConfigValue(body) + "\"\n";

        if (SendHttpHelper::run(curlConfig, &error)) {
            if (callback) callback(true, tr("Password sent to the webhook."));
            return;
        }
    }
    if (callback) callback(false, error);
}
