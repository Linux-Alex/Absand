// SPDX-License-Identifier: MPL-2.0

#ifndef SENDHTTPHELPER_H
#define SENDHTTPHELPER_H

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace SendHttpHelper {

inline bool postJson(const QString& url, const QByteArray& body,
                     const QStringList& extraHeaders, QString* errorMessage)
{
    auto escaped = [](QByteArray value) {
        value.replace("\\", "\\\\");
        value.replace("\"", "\\\"");
        value.replace("\r", "\\r");
        value.replace("\n", "\\n");
        return value;
    };
    QByteArray curlConfig = "fail-with-body\nsilent\nshow-error\nrequest = \"POST\"\n";
    curlConfig += "url = \"" + escaped(url.toUtf8()) + "\"\n";
    curlConfig += "header = \"Content-Type: application/json\"\n";
    for (const QString& header : extraHeaders)
        curlConfig += "header = \"" + escaped(header.toUtf8()) + "\"\n";
    curlConfig += "data-binary = \"" + escaped(body) + "\"\n";

    QProcess process;
    // Supplying the URL, bearer token, and JSON through stdin keeps secrets out
    // of the process command line and process listings.
    process.start(QStringLiteral("curl"), {QStringLiteral("--config"), QStringLiteral("-")});
    if (!process.waitForStarted(3000)) {
        if (errorMessage) *errorMessage = QObject::tr("Could not start curl.");
        return false;
    }
    process.write(curlConfig);
    process.closeWriteChannel();
    if (!process.waitForFinished(30000)) {
        process.kill();
        if (errorMessage) *errorMessage = QObject::tr("The request timed out.");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            QString detail = QString::fromUtf8(process.readAllStandardError()).trimmed();
            if (detail.isEmpty()) detail = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
            *errorMessage = detail.isEmpty() ? QObject::tr("The server rejected the request.") : detail;
        }
        return false;
    }
    return true;
}

inline QString messageFromTemplate(const QVariantMap& config, const QString& password)
{
    QString message = config.value(QStringLiteral("messageTemplate"),
                                   QObject::tr("Archive password: {password}")).toString();
    return message.replace(QStringLiteral("{password}"), password);
}

} // namespace SendHttpHelper

#endif
