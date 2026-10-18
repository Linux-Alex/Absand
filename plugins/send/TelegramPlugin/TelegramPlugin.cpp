// SPDX-License-Identifier: MPL-2.0

#include "TelegramPlugin.h"
#include "core/SendHttpHelper.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace {
bool containsPlaceholder(const QVariantMap& config, const char* key)
{
    return config.value(QString::fromLatin1(key)).toString().contains(QStringLiteral("{password}"));
}
} // namespace

QVariantMap TelegramPlugin::metadata() const
{
    return { { "name", name() }, { "version", version() }, { "description", description() },
             { "author", "Absand Team" }, { "channel", channelName() } };
}

QVariantMap TelegramPlugin::defaultConfiguration() const
{
    return { { "botToken", QString() },
             { "chatId", QString() },
             { "messageTemplate", tr("Archive password: {password}") } };
}

ConfigSchema TelegramPlugin::configSchema() const
{
    return {
        { QStringLiteral("botToken"), tr("Bot token"), tr("Token created with @BotFather"), QString(),
          ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant(), ConfigField::Required },
        { QStringLiteral("chatId"), tr("Chat ID"), tr("Numeric chat or group ID"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::None, {}, QVariant(),
          ConfigField::Required | ConfigField::Recipient },
        { QStringLiteral("messageTemplate"), tr("Message template"), tr("Must contain {password}"), QString(),
          ConfigField::Type::Multiline, ConfigField::Browse::None, {},
          tr("Archive password: {password}"), ConfigField::Required }
    };
}

bool TelegramPlugin::validateConfiguration(const QVariantMap& config, QString* error) const
{
    if (config.value(QStringLiteral("botToken")).toString().trimmed().isEmpty()
        || config.value(QStringLiteral("chatId")).toString().trimmed().isEmpty()) {
        if (error) *error = tr("Bot token and chat ID are required.");
        return false;
    }
    if (!containsPlaceholder(config, "messageTemplate")) {
        if (error) *error = tr("The message template must contain {password}.");
        return false;
    }
    return true;
}

void TelegramPlugin::send(const QString& password, const QVariantMap& config,
                          std::function<void(bool, QString)> callback)
{
    QString error;
    if (password.isEmpty()) {
        error = tr("Cannot send an empty password.");
    } else if (validateConfiguration(config, &error)) {
        const QString url = QStringLiteral("https://api.telegram.org/bot%1/sendMessage")
                                .arg(config.value(QStringLiteral("botToken")).toString().trimmed());
        const QByteArray body = QJsonDocument(QJsonObject{
            { QStringLiteral("chat_id"), config.value(QStringLiteral("chatId")).toString().trimmed() },
            { QStringLiteral("text"), SendHttpHelper::messageFromTemplate(config, password) }
        }).toJson(QJsonDocument::Compact);

        if (SendHttpHelper::postJson(url, body, {}, &error)) {
            if (callback) callback(true, tr("Password sent over Telegram."));
            return;
        }
    }
    if (callback) callback(false, error);
}
