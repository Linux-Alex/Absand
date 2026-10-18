// SPDX-License-Identifier: MPL-2.0

#ifndef SENDHTTPHELPER_H
#define SENDHTTPHELPER_H

#include "CurlExecutable.h"
#include "ProcessWait.h"

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace SendHttpHelper {

/// Escapes a value for a curl configuration document.
inline QByteArray escapeConfigValue(QByteArray value)
{
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\r", "\\r");
    value.replace("\n", "\\n");
    return value;
}

/**
 * Runs curl with a configuration document supplied on stdin
 * ("curl --config -"), so URLs, tokens, and payloads never appear in the
 * process listing or in shell history.
 */
inline bool run(const QByteArray& configText, QString* errorMessage,
                QByteArray* response = nullptr, int timeoutMs = 30000)
{
    QProcess process;
    process.start(CurlExecutable::resolve(), { QStringLiteral("--config"), QStringLiteral("-") });
    if (!process.waitForStarted(3000)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not start curl.");
        }
        return false;
    }

    process.write(configText);
    process.closeWriteChannel();

    if (!ProcessWait::waitFinished(process, timeoutMs)) {
        process.kill();
        process.waitForFinished(2000);
        if (errorMessage) {
            *errorMessage = QObject::tr("The request timed out.");
        }
        return false;
    }

    const QByteArray standardOutput = process.readAllStandardOutput();
    if (response) {
        *response = standardOutput;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            QString detail = QString::fromUtf8(process.readAllStandardError()).trimmed();
            if (detail.isEmpty()) {
                detail = QString::fromUtf8(standardOutput).trimmed();
            }
            if (process.exitCode() == 2 && detail.isEmpty()) {
                detail = QObject::tr("curl rejected the request options; please install a current curl.");
            }
            *errorMessage = detail.isEmpty() ? QObject::tr("The server rejected the request.") : detail;
        }
        return false;
    }

    return true;
}

inline bool postJson(const QString& url, const QByteArray& body,
                     const QStringList& extraHeaders, QString* errorMessage,
                     QByteArray* response = nullptr)
{
    QByteArray curlConfig = "fail-with-body\nsilent\nshow-error\nrequest = \"POST\"\n";
    curlConfig += "connect-timeout = 20\n";
    curlConfig += "url = \"" + escapeConfigValue(url.toUtf8()) + "\"\n";
    curlConfig += "header = \"Content-Type: application/json\"\n";
    for (const QString& header : extraHeaders) {
        curlConfig += "header = \"" + escapeConfigValue(header.toUtf8()) + "\"\n";
    }
    curlConfig += "data-binary = \"" + escapeConfigValue(body) + "\"\n";

    return run(curlConfig, errorMessage, response);
}

/**
 * Fills the message template.
 *
 * {password} becomes the archive password. {link} becomes the download link of a
 * web-share upload; when the archive was not uploaded anywhere the placeholder
 * simply disappears, so the same template works for every destination.
 */
inline QString messageFromTemplate(const QVariantMap& config, const QString& password)
{
    QString message = config.value(QStringLiteral("messageTemplate"),
                                   QObject::tr("Archive password: {password}")).toString();
    message.replace(QStringLiteral("{password}"), password);
    message.replace(QStringLiteral("{link}"), config.value(QStringLiteral("shareLink")).toString());
    return message;
}

/// Splits "a@example.com; b@example.com, c@example.com" into single addresses.
inline QStringList splitAddresses(const QString& text)
{
    QString unified = text;
    unified.replace(QLatin1Char(';'), QLatin1Char(','));
    unified.replace(QLatin1Char('\n'), QLatin1Char(','));

    QStringList addresses;
    const QStringList parts = unified.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty() && !addresses.contains(trimmed)) {
            addresses.append(trimmed);
        }
    }
    return addresses;
}

/// Removes line breaks so a value can never inject extra headers into a message.
inline QString sanitizedHeaderValue(const QString& value)
{
    QString result = value;
    result.remove(QLatin1Char('\r'));
    result.remove(QLatin1Char('\n'));
    return result;
}

} // namespace SendHttpHelper

#endif
