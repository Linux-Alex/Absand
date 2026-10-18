// SPDX-License-Identifier: MPL-2.0

#include "SmsPlugin.h"
#include "core/SendHttpHelper.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace {

QString providerOf(const QVariantMap& config)
{
    return config.value(QStringLiteral("provider"), QStringLiteral("Textbelt")).toString();
}

} // namespace

QVariantMap SmsPlugin::metadata() const
{
    return { { "name", name() }, { "version", version() }, { "description", description() },
             { "author", "Absand Team" }, { "channel", channelName() } };
}

QVariantMap SmsPlugin::defaultConfiguration() const
{
    return { { "provider", QStringLiteral("Textbelt") },
             { "recipient", QString() },
             { "accountId", QString() },
             { "apiSecret", QString() },
             { "sender", QString() },
             { "urlTemplate", QStringLiteral("https://example.com/sms?to={recipient}&text={message}") },
             { "messageTemplate", tr("Archive password: {password}") } };
}

ConfigSchema SmsPlugin::configSchema() const
{
    return {
        { QStringLiteral("provider"), tr("Provider"),
          tr("Textbelt offers a free text per day; Twilio and Vonage are paid but reliable"),
          QString(),
          ConfigField::Type::Choice, ConfigField::Browse::None,
          { QStringLiteral("Textbelt"), QStringLiteral("Twilio"), QStringLiteral("Vonage"), QStringLiteral("Custom URL") },
          QStringLiteral("Textbelt"), ConfigField::Required },
        { QStringLiteral("recipient"), tr("Phone number"), tr("International format, for example +38640123456"),
          QStringLiteral("+38640123456"), ConfigField::Type::Text, ConfigField::Browse::None, {},
          QVariant(), ConfigField::Required | ConfigField::Recipient },
        { QStringLiteral("accountId"), tr("Account / API key"),
          tr("Twilio: Account SID. Vonage: API key. Textbelt: API key (\"textbelt\" for the free tier)"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::None, {}, QVariant() },
        { QStringLiteral("apiSecret"), tr("API secret"),
          tr("Twilio auth token or Vonage API secret"), QString(),
          ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant() },
        { QStringLiteral("sender"), tr("Sender"), tr("Twilio/Vonage sender number or name"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::None, {}, QVariant() },
        { QStringLiteral("urlTemplate"), tr("Gateway URL template"),
          tr("Used with the custom provider. {recipient} and {message} are replaced"), QStringLiteral("https://example.com/sms?to={recipient}&text={message}"),
          ConfigField::Type::Text, ConfigField::Browse::None, {},
          QStringLiteral("https://example.com/sms?to={recipient}&text={message}"), ConfigField::Advanced },
        { QStringLiteral("messageTemplate"), tr("Message template"), tr("Must contain {password}"), QString(),
          ConfigField::Type::Multiline, ConfigField::Browse::None, {},
          tr("Archive password: {password}"), ConfigField::Required }
    };
}

bool SmsPlugin::validateConfiguration(const QVariantMap& config, QString* error) const
{
    const QString recipient = config.value(QStringLiteral("recipient")).toString().trimmed();
    if (recipient.isEmpty()) {
        if (error) *error = tr("Please enter the phone number of the recipient.");
        return false;
    }
    if (recipient.count(QLatin1Char('+')) > 1 || recipient.size() < 6) {
        if (error) *error = tr("%1 does not look like a phone number.").arg(recipient);
        return false;
    }

    const QString provider = providerOf(config);
    if (provider == QStringLiteral("Twilio") || provider == QStringLiteral("Vonage")) {
        if (config.value(QStringLiteral("accountId")).toString().trimmed().isEmpty()
            || config.value(QStringLiteral("apiSecret")).toString().trimmed().isEmpty()) {
            if (error) *error = tr("The account key and API secret are required for %1.").arg(provider);
            return false;
        }
    }
    if (provider == QStringLiteral("Custom URL")
        && config.value(QStringLiteral("urlTemplate")).toString().trimmed().isEmpty()) {
        if (error) *error = tr("Please enter the gateway URL template.");
        return false;
    }

    if (!config.value(QStringLiteral("messageTemplate")).toString().contains(QStringLiteral("{password}"))) {
        if (error) *error = tr("The message template must contain {password}.");
        return false;
    }
    return true;
}

void SmsPlugin::send(const QString& password, const QVariantMap& config,
                     std::function<void(bool, QString)> callback)
{
    QString error;
    if (password.isEmpty()) {
        error = tr("Cannot send an empty password.");
    } else if (validateConfiguration(config, &error)) {
        const QString provider = providerOf(config);
        const QString recipient = config.value(QStringLiteral("recipient")).toString().trimmed();
        const QString sender = config.value(QStringLiteral("sender")).toString().trimmed();
        const QString accountId = config.value(QStringLiteral("accountId")).toString().trimmed();
        const QString apiSecret = config.value(QStringLiteral("apiSecret")).toString();
        const QString message = SendHttpHelper::messageFromTemplate(config, password);

        QByteArray curlConfig = "silent\nshow-error\nfail-with-body\nconnect-timeout = 20\n";
        QByteArray response;

        if (provider == QStringLiteral("Twilio")) {
            const QString url = QStringLiteral("https://api.twilio.com/2010-04-01/Accounts/%1/Messages.json")
                                    .arg(QString::fromLatin1(QUrl::toPercentEncoding(accountId)));
            curlConfig += "user = \"" + SendHttpHelper::escapeConfigValue(
                              (accountId + QLatin1Char(':') + apiSecret).toUtf8()) + "\"\n";
            curlConfig += "request = \"POST\"\n";
            curlConfig += "url = \"" + SendHttpHelper::escapeConfigValue(url.toUtf8()) + "\"\n";
            curlConfig += "data-urlencode = \"To=" + SendHttpHelper::escapeConfigValue(recipient.toUtf8()) + "\"\n";
            if (!sender.isEmpty()) {
                curlConfig += "data-urlencode = \"From=" + SendHttpHelper::escapeConfigValue(sender.toUtf8()) + "\"\n";
            }
            curlConfig += "data-urlencode = \"Body=" + SendHttpHelper::escapeConfigValue(message.toUtf8()) + "\"\n";
        } else if (provider == QStringLiteral("Vonage")) {
            curlConfig += "request = \"POST\"\n";
            curlConfig += "url = \"https://rest.nexmo.com/sms/json\"\n";
            curlConfig += "data-urlencode = \"api_key=" + SendHttpHelper::escapeConfigValue(accountId.toUtf8()) + "\"\n";
            curlConfig += "data-urlencode = \"api_secret=" + SendHttpHelper::escapeConfigValue(apiSecret.toUtf8()) + "\"\n";
            curlConfig += "data-urlencode = \"to=" + SendHttpHelper::escapeConfigValue(recipient.toUtf8()) + "\"\n";
            curlConfig += "data-urlencode = \"from="
                + SendHttpHelper::escapeConfigValue((sender.isEmpty() ? QStringLiteral("Absand") : sender).toUtf8()) + "\"\n";
            curlConfig += "data-urlencode = \"text=" + SendHttpHelper::escapeConfigValue(message.toUtf8()) + "\"\n";
        } else if (provider == QStringLiteral("Custom URL")) {
            QString url = config.value(QStringLiteral("urlTemplate")).toString().trimmed();
            url.replace(QStringLiteral("{recipient}"), QString::fromLatin1(QUrl::toPercentEncoding(recipient)));
            url.replace(QStringLiteral("{message}"), QString::fromLatin1(QUrl::toPercentEncoding(message)));
            curlConfig += "url = \"" + SendHttpHelper::escapeConfigValue(url.toUtf8()) + "\"\n";
        } else { // Textbelt
            const QString key = accountId.isEmpty() ? QStringLiteral("textbelt") : accountId;
            curlConfig += "request = \"POST\"\n";
            curlConfig += "url = \"https://textbelt.com/text\"\n";
            curlConfig += "data-urlencode = \"phone=" + SendHttpHelper::escapeConfigValue(recipient.toUtf8()) + "\"\n";
            curlConfig += "data-urlencode = \"message=" + SendHttpHelper::escapeConfigValue(message.toUtf8()) + "\"\n";
            curlConfig += "data-urlencode = \"key=" + SendHttpHelper::escapeConfigValue(key.toUtf8()) + "\"\n";
        }

        if (SendHttpHelper::run(curlConfig, &error, &response)) {
            // Textbelt replies with a JSON document that can still report a failure.
            const QJsonObject object = QJsonDocument::fromJson(response).object();
            if (object.contains(QStringLiteral("success")) && !object.value(QStringLiteral("success")).toBool()) {
                const QString detail = object.value(QStringLiteral("error")).toString();
                if (callback) {
                    callback(false, detail.isEmpty()
                                       ? tr("The SMS provider rejected the message.")
                                       : tr("The SMS provider rejected the message: %1").arg(detail));
                }
                return;
            }
            if (callback) callback(true, tr("Password sent by SMS."));
            return;
        }
    }
    if (callback) callback(false, error);
}
