// SPDX-License-Identifier: MPL-2.0

#include "MatrixPlugin.h"
#include "core/SendHttpHelper.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

QVariantMap MatrixPlugin::metadata() const
{
    return { { "name", name() }, { "version", version() }, { "description", description() },
             { "author", "Absand Team" }, { "channel", channelName() } };
}

QVariantMap MatrixPlugin::defaultConfiguration() const
{
    return { { "homeserver", QStringLiteral("https://matrix.org") },
             { "accessToken", QString() },
             { "roomId", QString() },
             { "messageTemplate", tr("Archive password: {password}") } };
}

ConfigSchema MatrixPlugin::configSchema() const
{
    return {
        { QStringLiteral("homeserver"), tr("Homeserver URL"), tr("For example https://matrix.org"),
          QStringLiteral("https://matrix.org"), ConfigField::Type::Text, ConfigField::Browse::None, {},
          QStringLiteral("https://matrix.org"), ConfigField::Required },
        { QStringLiteral("accessToken"), tr("Access token"), tr("Token of a user that may post in the room"),
          QString(), ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant(),
          ConfigField::Required },
        { QStringLiteral("roomId"), tr("Room ID"), tr("Normally starts with !"), QStringLiteral("!room:matrix.org"),
          ConfigField::Type::Text, ConfigField::Browse::None, {}, QVariant(),
          ConfigField::Required | ConfigField::Recipient },
        { QStringLiteral("messageTemplate"), tr("Message template"), tr("Must contain {password}"), QString(),
          ConfigField::Type::Multiline, ConfigField::Browse::None, {},
          tr("Archive password: {password}"), ConfigField::Required }
    };
}

bool MatrixPlugin::validateConfiguration(const QVariantMap& config, QString* error) const
{
    for (const char* key : { "homeserver", "accessToken", "roomId" }) {
        if (config.value(QString::fromLatin1(key)).toString().trimmed().isEmpty()) {
            if (error) *error = tr("Homeserver, access token, and room ID are required.");
            return false;
        }
    }
    if (!config.value(QStringLiteral("messageTemplate")).toString().contains(QStringLiteral("{password}"))) {
        if (error) *error = tr("The message template must contain {password}.");
        return false;
    }
    return true;
}

void MatrixPlugin::send(const QString& password, const QVariantMap& config,
                        std::function<void(bool, QString)> callback)
{
    QString error;
    if (password.isEmpty()) {
        error = tr("Cannot send an empty password.");
    } else if (validateConfiguration(config, &error)) {
        QString base = config.value(QStringLiteral("homeserver")).toString().trimmed();
        while (base.endsWith(QLatin1Char('/'))) {
            base.chop(1);
        }

        const QString room = QString::fromLatin1(
            QUrl::toPercentEncoding(config.value(QStringLiteral("roomId")).toString().trimmed()));
        const QString transaction = QString::number(QDateTime::currentMSecsSinceEpoch());
        const QString url = QStringLiteral("%1/_matrix/client/v3/rooms/%2/send/m.room.message/%3")
                                .arg(base, room, transaction);

        const QByteArray body = QJsonDocument(QJsonObject{
            { QStringLiteral("msgtype"), QStringLiteral("m.text") },
            { QStringLiteral("body"), SendHttpHelper::messageFromTemplate(config, password) }
        }).toJson(QJsonDocument::Compact);

        const QStringList headers{ QStringLiteral("Authorization: Bearer %1")
                                      .arg(config.value(QStringLiteral("accessToken")).toString().trimmed()) };

        if (SendHttpHelper::postJson(url, body, headers, &error)) {
            if (callback) callback(true, tr("Password sent over Matrix."));
            return;
        }
    }
    if (callback) callback(false, error);
}
