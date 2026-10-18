// SPDX-License-Identifier: MPL-2.0

#ifndef SMTPMESSAGE_H
#define SMTPMESSAGE_H

#include "SendHttpHelper.h"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantMap>

/**
 * Builds RFC 5322 messages and runs curl's SMTP support.
 *
 * curl is used instead of a mail library so that mail delivery behaves exactly
 * like the other channels: no extra dependency, a proper TLS stack, and
 * credentials stay out of the process listing because they travel through a
 * curl configuration document.
 */
namespace SmtpMessage {

struct Settings
{
    QString host;
    int port = 587;
    QString security = QStringLiteral("starttls");   // starttls | ssl | none
    QString username;
    QString password;
    QString sender;
    QStringList recipients;
    QString subject;
    QString messageTemplate;
};

inline Settings settingsFromConfig(const QVariantMap& config, const QString& defaultSubject)
{
    Settings settings;
    settings.host = config.value(QStringLiteral("host")).toString().trimmed();
    settings.port = config.value(QStringLiteral("port"), 587).toInt();
    settings.security = config.value(QStringLiteral("security"), QStringLiteral("starttls")).toString();
    settings.username = config.value(QStringLiteral("username")).toString().trimmed();
    settings.password = config.value(QStringLiteral("password")).toString();
    settings.sender = config.value(QStringLiteral("sender")).toString().trimmed();
    settings.recipients = SendHttpHelper::splitAddresses(
        config.value(QStringLiteral("recipient")).toString());
    settings.subject = config.value(QStringLiteral("subject"), defaultSubject).toString();
    settings.messageTemplate = config.value(QStringLiteral("messageTemplate")).toString();
    return settings;
}

inline QString encodedSubject(const QString& subject)
{
    const QString value = SendHttpHelper::sanitizedHeaderValue(subject).trimmed();
    if (value.isEmpty()) {
        return QStringLiteral("Absand archive password");
    }

    bool ascii = true;
    for (const QChar character : value) {
        if (character.unicode() > 127) {
            ascii = false;
            break;
        }
    }
    if (ascii) {
        return value;
    }

    // RFC 2047 encoded word, so non-ASCII subjects survive every client.
    return QStringLiteral("=?UTF-8?B?%1?=")
        .arg(QString::fromLatin1(value.toUtf8().toBase64()));
}

inline QByteArray baseHeaders(const Settings& settings, const QString& boundary = QString())
{
    QByteArray headers;
    headers += "Date: " + QDateTime::currentDateTimeUtc().toString(Qt::RFC2822Date).toLatin1() + "\r\n";
    headers += "From: " + SendHttpHelper::sanitizedHeaderValue(settings.sender).toUtf8() + "\r\n";
    headers += "To: " + SendHttpHelper::sanitizedHeaderValue(settings.recipients.join(QStringLiteral(", "))).toUtf8() + "\r\n";
    headers += "Subject: " + encodedSubject(settings.subject).toUtf8() + "\r\n";
    headers += "MIME-Version: 1.0\r\n";
    if (boundary.isEmpty()) {
        headers += "Content-Type: text/plain; charset=UTF-8\r\n";
        headers += "Content-Transfer-Encoding: 8bit\r\n";
    } else {
        headers += "Content-Type: multipart/mixed; boundary=\"" + boundary.toUtf8() + "\"\r\n";
    }
    return headers;
}

inline QByteArray simpleMessage(const Settings& settings, const QString& body)
{
    QByteArray message = baseHeaders(settings);
    message += "\r\n";
    message += body.toUtf8();
    message += "\r\n";
    return message;
}

/// Wraps base64 into 76 character lines as required by RFC 2045.
inline QByteArray wrappedBase64(const QByteArray& data)
{
    const QByteArray encoded = data.toBase64();
    QByteArray wrapped;
    wrapped.reserve(encoded.size() + encoded.size() / 76 + 4);
    for (int offset = 0; offset < encoded.size(); offset += 76) {
        wrapped += encoded.mid(offset, 76);
        wrapped += "\r\n";
    }
    return wrapped;
}

inline QByteArray attachmentMessage(const Settings& settings, const QString& body,
                                    const QString& attachmentName, const QByteArray& attachmentData)
{
    const QString boundary = QStringLiteral("----AbsandMail%1")
                                 .arg(QDateTime::currentMSecsSinceEpoch());
    QByteArray message = baseHeaders(settings, boundary);
    message += "\r\n";

    message += "--" + boundary.toUtf8() + "\r\n";
    message += "Content-Type: text/plain; charset=UTF-8\r\n";
    message += "Content-Transfer-Encoding: 8bit\r\n\r\n";
    message += body.toUtf8() + "\r\n";

    message += "--" + boundary.toUtf8() + "\r\n";
    message += "Content-Type: application/octet-stream; name=\"" + attachmentName.toUtf8() + "\"\r\n";
    message += "Content-Disposition: attachment; filename=\"" + attachmentName.toUtf8() + "\"\r\n";
    message += "Content-Transfer-Encoding: base64\r\n\r\n";
    message += wrappedBase64(attachmentData);

    message += "--" + boundary.toUtf8() + "--\r\n";
    return message;
}

/// Builds the curl configuration that delivers a prepared message file.
inline QByteArray curlConfig(const Settings& settings, const QString& messageFile)
{
    const bool implicitTls = settings.security.compare(QStringLiteral("ssl"), Qt::CaseInsensitive) == 0;
    const bool startTls = settings.security.compare(QStringLiteral("starttls"), Qt::CaseInsensitive) == 0;

    const QString scheme = implicitTls ? QStringLiteral("smtps") : QStringLiteral("smtp");
    const int port = settings.port > 0 ? settings.port : (implicitTls ? 465 : 587);

    QByteArray config = "silent\nshow-error\n";
    config += "connect-timeout = 20\n";
    config += "url = \"" + SendHttpHelper::escapeConfigValue(
                  QStringLiteral("%1://%2:%3").arg(scheme, settings.host).arg(port).toUtf8()) + "\"\n";

    if (!settings.username.isEmpty() || !settings.password.isEmpty()) {
        config += "user = \"" + SendHttpHelper::escapeConfigValue(
                      (settings.username + QLatin1Char(':') + settings.password).toUtf8()) + "\"\n";
    }
    if (startTls) {
        config += "ssl-reqd\n";
    }
    const QString sender = settings.sender.isEmpty() ? settings.username : settings.sender;
    config += "mail-from = \"" + SendHttpHelper::escapeConfigValue(sender.toUtf8()) + "\"\n";
    for (const QString& recipient : settings.recipients) {
        config += "mail-rcpt = \"" + SendHttpHelper::escapeConfigValue(recipient.toUtf8()) + "\"\n";
    }
    config += "upload-file = \"" + SendHttpHelper::escapeConfigValue(messageFile.toUtf8()) + "\"\n";
    return config;
}

/**
 * Writes the message to a private temporary file and hands it to curl.
 * The file can contain the password, so it is created inside a temporary
 * directory and removed as soon as the transfer finished.
 */
inline bool send(const Settings& settings, const QByteArray& message, QString* errorMessage,
                 int timeoutMs = 120000)
{
    if (settings.host.isEmpty()) {
        if (errorMessage) *errorMessage = QObject::tr("Please enter the SMTP host.");
        return false;
    }
    if (settings.recipients.isEmpty()) {
        if (errorMessage) *errorMessage = QObject::tr("Please enter at least one recipient address.");
        return false;
    }

    QTemporaryDir directory;
    if (!directory.isValid()) {
        if (errorMessage) *errorMessage = QObject::tr("Could not create a temporary file for the mail.");
        return false;
    }
    directory.setAutoRemove(true);

    const QString messageFile = directory.filePath(QStringLiteral("message.eml"));
    QFile file(messageFile);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = QObject::tr("Could not write the mail message.");
        return false;
    }
    file.write(message);
    file.close();

    return SendHttpHelper::run(curlConfig(settings, messageFile), errorMessage, nullptr, timeoutMs);
}

} // namespace SmtpMessage

#endif
