// SPDX-License-Identifier: MPL-2.0

#include "NtfyPlugin.h"
#include "core/SendHttpHelper.h"

#include <QUrl>

QVariantMap NtfyPlugin::metadata() const
{
    return { { "name", name() }, { "version", version() }, { "description", description() },
             { "author", "Absand Team" }, { "channel", channelName() } };
}

QVariantMap NtfyPlugin::defaultConfiguration() const
{
    return { { "serverUrl", QStringLiteral("https://ntfy.sh") },
             { "topic", QString() },
             { "accessToken", QString() },
             { "title", tr("Absand archive password") },
             { "messageTemplate", tr("Archive password: {password}") } };
}

ConfigSchema NtfyPlugin::configSchema() const
{
    return {
        { QStringLiteral("serverUrl"), tr("Server"), tr("ntfy.sh, or your own self-hosted server"),
          QStringLiteral("https://ntfy.sh"), ConfigField::Type::Text, ConfigField::Browse::None, {},
          QStringLiteral("https://ntfy.sh"), ConfigField::Required },
        { QStringLiteral("topic"), tr("Topic"),
          tr("Choose a long random topic; anyone who knows it can read the message"),
          QStringLiteral("absand-9f2c1d7b4e"), ConfigField::Type::Text, ConfigField::Browse::None, {},
          QVariant(), ConfigField::Required | ConfigField::Recipient },
        { QStringLiteral("accessToken"), tr("Access token"),
          tr("Only needed on servers that protect the topic"), QString(),
          ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant() },
        { QStringLiteral("title"), tr("Notification title"), QString(), QString(),
          ConfigField::Type::Text, ConfigField::Browse::None, {},
          tr("Absand archive password"), ConfigField::Required },
        { QStringLiteral("messageTemplate"), tr("Message template"), tr("Must contain {password}"), QString(),
          ConfigField::Type::Multiline, ConfigField::Browse::None, {},
          tr("Archive password: {password}"), ConfigField::Required }
    };
}

bool NtfyPlugin::validateConfiguration(const QVariantMap& config, QString* error) const
{
    if (!config.value(QStringLiteral("serverUrl")).toString().trimmed().startsWith(QStringLiteral("http"))) {
        if (error) *error = tr("Please enter the ntfy server URL, for example https://ntfy.sh.");
        return false;
    }
    if (config.value(QStringLiteral("topic")).toString().trimmed().isEmpty()) {
        if (error) *error = tr("Please enter the ntfy topic.");
        return false;
    }
    if (!config.value(QStringLiteral("messageTemplate")).toString().contains(QStringLiteral("{password}"))) {
        if (error) *error = tr("The message template must contain {password}.");
        return false;
    }
    return true;
}

void NtfyPlugin::send(const QString& password, const QVariantMap& config,
                      std::function<void(bool, QString)> callback)
{
    QString error;
    if (password.isEmpty()) {
        error = tr("Cannot send an empty password.");
    } else if (validateConfiguration(config, &error)) {
        QString server = config.value(QStringLiteral("serverUrl")).toString().trimmed();
        while (server.endsWith(QLatin1Char('/'))) {
            server.chop(1);
        }

        const QString topic = config.value(QStringLiteral("topic")).toString().trimmed();
        const QString url = QStringLiteral("%1/%2").arg(server, QString::fromLatin1(QUrl::toPercentEncoding(topic)));

        QByteArray curlConfig = "silent\nshow-error\nfail-with-body\nrequest = \"POST\"\n";
        curlConfig += "connect-timeout = 20\n";
        curlConfig += "url = \"" + SendHttpHelper::escapeConfigValue(url.toUtf8()) + "\"\n";
        curlConfig += "header = \"Content-Type: text/plain; charset=UTF-8\"\n";

        const QString title = config.value(QStringLiteral("title")).toString().trimmed();
        if (!title.isEmpty()) {
            curlConfig += "header = \"Title: "
                + SendHttpHelper::escapeConfigValue(SendHttpHelper::sanitizedHeaderValue(title).toUtf8()) + "\"\n";
        }

        const QString token = config.value(QStringLiteral("accessToken")).toString().trimmed();
        if (!token.isEmpty()) {
            curlConfig += "header = \"Authorization: Bearer "
                + SendHttpHelper::escapeConfigValue(token.toUtf8()) + "\"\n";
        }

        curlConfig += "data-binary = \""
            + SendHttpHelper::escapeConfigValue(SendHttpHelper::messageFromTemplate(config, password).toUtf8())
            + "\"\n";

        if (SendHttpHelper::run(curlConfig, &error)) {
            if (callback) callback(true, tr("Password sent as an ntfy notification."));
            return;
        }
    }
    if (callback) callback(false, error);
}
