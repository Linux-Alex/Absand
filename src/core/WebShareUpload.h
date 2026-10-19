// SPDX-License-Identifier: MPL-2.0

#ifndef WEBSHAREUPLOAD_H
#define WEBSHAREUPLOAD_H

#include "SendHttpHelper.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

/**
 * Uploads an archive to an anonymous web file host and returns the share link.
 *
 * All providers are reachable with the same curl based machinery the other
 * channels use, so Absand needs no extra dependency and no upload binary:
 * the multipart form, the credentials, and the file are handed to curl through
 * a configuration document on stdin.
 */
namespace WebShareUpload {

struct Provider
{
    QString id;
    QString label;
    int limitMb = 0;        ///< 0 means "no published limit"
    QString note;           ///< short description of the storage lifetime
};

inline QVector<Provider> providers()
{
    return {
        { QStringLiteral("gofile"), QStringLiteral("Gofile"), 0,
          QObject::tr("no published limit, free JSON API, keyless") },
        { QStringLiteral("litterbox"), QStringLiteral("Litterbox"), 1024,
          QObject::tr("temporary: the file is deleted after 72 hours") },
        { QStringLiteral("tempsh"), QStringLiteral("temp.sh"), 4096,
          QObject::tr("temporary: three day expiry, largest anonymous limit") },
        { QStringLiteral("x0at"), QStringLiteral("x0.at"), 1024,
          QObject::tr("temporary, successor of 0x0.st") },
        { QStringLiteral("catbox"), QStringLiteral("catbox.moe"), 200,
          QObject::tr("permanent store") },
        { QStringLiteral("tmpfiles"), QStringLiteral("tmpfiles.org"), 100,
          QObject::tr("short lived: 1 to 48 hours") },
        { QStringLiteral("pixeldrain"), QStringLiteral("pixeldrain"), 20480,
          QObject::tr("needs an API key, 20 GB per file") },
        { QStringLiteral("custom"), QStringLiteral("Custom endpoint"), 0,
          QObject::tr("any host that accepts a multipart POST with a 'file' field") }
    };
}

inline QStringList providerIds()
{
    QStringList ids;
    for (const Provider& provider : providers()) {
        ids.append(provider.id);
    }
    return ids;
}

inline Provider providerById(const QString& id)
{
    for (const Provider& provider : providers()) {
        if (provider.id == id) {
            return provider;
        }
    }
    return providers().first();
}

inline bool requiresToken(const QString& providerId)
{
    return providerId == QStringLiteral("pixeldrain");
}

struct Result
{
    bool success = false;
    QString message;
    QString link;
};

/// Escapes a local path for a curl configuration value: backslashes (Windows
/// paths) and quotes must be escaped, because the value itself is quoted.
inline QByteArray escapeConfigPath(const QString& path)
{
    QByteArray escaped = path.toUtf8();
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return escaped;
}

/// Builds a multipart "form" line. The path is quoted so spaces survive and so
/// commas and semicolons are not read as curl form separators.
inline QByteArray multipartForm(const QByteArray& field, const QString& path)
{
    return "form = \"" + field + "=@\\\"" + escapeConfigPath(path) + "\\\"\"\n";
}

/**
 * Builds the curl configuration for one provider.
 * @param providerId one of providerIds()
 * @param config     plugin configuration (token, expiry, custom URL, cached server)
 * @param filePath   local archive
 */
inline QByteArray curlConfig(const QString& providerId, const QVariantMap& config,
                             const QString& filePath, QString* errorMessage)
{
    const QString token = config.value(QStringLiteral("accountToken")).toString().trimmed();
    const QString fileName = QFileInfo(filePath).fileName();
    const QString expiry = config.value(QStringLiteral("expiry"), QStringLiteral("72h")).toString();

    QByteArray curlConfig = "silent\nshow-error\nfail-with-body\nconnect-timeout = 20\n";
    // Some hosts throttle or refuse requests that use curl's default agent.
    curlConfig += "header = \"User-Agent: Absand/1.0\"\n";

    if (providerId == QStringLiteral("gofile")) {
        QString server = config.value(QStringLiteral("cachedServer")).toString().trimmed();
        if (server.isEmpty()) {
            server = QStringLiteral("store1");
        }
        curlConfig += "request = \"POST\"\n";
        curlConfig += "url = \"https://" + server.toUtf8() + ".gofile.io/contents/uploadfile\"\n";
        curlConfig += multipartForm("file", filePath);
        if (!token.isEmpty()) {
            curlConfig += "form = \"token=" + SendHttpHelper::escapeConfigValue(token.toUtf8()) + "\"\n";
        }
        return curlConfig;
    }

    if (providerId == QStringLiteral("litterbox")) {
        curlConfig += "request = \"POST\"\n";
        curlConfig += "url = \"https://litterbox.catbox.moe/resources/internals/api.php\"\n";
        curlConfig += "form = \"reqtype=fileupload\"\n";
        curlConfig += "form = \"time=" + SendHttpHelper::escapeConfigValue(expiry.toUtf8()) + "\"\n";
        curlConfig += multipartForm("fileToUpload", filePath);
        return curlConfig;
    }

    if (providerId == QStringLiteral("tempsh")) {
        curlConfig += "url = \"https://temp.sh/upload\"\n";
        curlConfig += multipartForm("file", filePath);
        return curlConfig;
    }

    if (providerId == QStringLiteral("x0at")) {
        curlConfig += "url = \"https://x0.at/\"\n";
        curlConfig += "form = \"keep_name=1\"\n";
        curlConfig += multipartForm("file", filePath);
        return curlConfig;
    }

    if (providerId == QStringLiteral("catbox")) {
        curlConfig += "request = \"POST\"\n";
        curlConfig += "url = \"https://catbox.moe/user/api.php\"\n";
        curlConfig += "form = \"reqtype=fileupload\"\n";
        if (!token.isEmpty()) {
            curlConfig += "form = \"userhash=" + SendHttpHelper::escapeConfigValue(token.toUtf8()) + "\"\n";
        }
        curlConfig += multipartForm("fileToUpload", filePath);
        return curlConfig;
    }

    if (providerId == QStringLiteral("tmpfiles")) {
        curlConfig += "url = \"https://tmpfiles.org/api/v1/upload\"\n";
        curlConfig += "header = \"Accept: application/json\"\n";
        curlConfig += multipartForm("file", filePath);
        return curlConfig;
    }

    if (providerId == QStringLiteral("pixeldrain")) {
        if (token.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QObject::tr("Pixeldrain needs an API key.");
            }
            return {};
        }
        curlConfig += "request = \"PUT\"\n";
        curlConfig += "url = \"https://pixeldrain.com/api/file/"
            + QUrl::toPercentEncoding(fileName) + "\"\n";
        // Pixeldrain expects HTTP basic authentication with an empty user name.
        curlConfig += "user = \":" + SendHttpHelper::escapeConfigValue(token.toUtf8()) + "\"\n";
        curlConfig += "upload-file = \"" + escapeConfigPath(filePath) + "\"\n";
        return curlConfig;
    }

    // Custom endpoint: a plain multipart POST to whatever the user configured.
    const QString customUrl = config.value(QStringLiteral("customUrl")).toString().trimmed();
    if (customUrl.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Please enter the upload endpoint URL.");
        }
        return {};
    }
    curlConfig += "url = \"" + SendHttpHelper::escapeConfigValue(customUrl.toUtf8()) + "\"\n";
    curlConfig += multipartForm("file", filePath);
    return curlConfig;
}

/// Extracts the share link from a provider response.
inline QString parseResponse(const QString& providerId, const QByteArray& response, QString* errorMessage)
{
    const QString text = QString::fromUtf8(response).trimmed();

    if (providerId == QStringLiteral("tmpfiles")) {
        const QJsonObject object = QJsonDocument::fromJson(response).object();
        if (object.value(QStringLiteral("status")).toString() != QStringLiteral("success")) {
            if (errorMessage) {
                *errorMessage = QObject::tr("tmpfiles.org rejected the upload.");
            }
            return {};
        }
        // tmpfiles answers with the page URL; the documented /dl/ variant no
        // longer serves the bytes (verified upstream), so the page link is used.
        return object.value(QStringLiteral("data")).toObject()
            .value(QStringLiteral("url")).toString();
    }

    if (providerId == QStringLiteral("pixeldrain")) {
        const QJsonObject object = QJsonDocument::fromJson(response).object();
        const QString id = object.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            if (errorMessage) {
                const QString detail = object.value(QStringLiteral("value")).toString();
                *errorMessage = detail.isEmpty()
                    ? QObject::tr("Pixeldrain rejected the upload.")
                    : QObject::tr("Pixeldrain rejected the upload: %1").arg(detail);
            }
            return {};
        }
        return QStringLiteral("https://pixeldrain.com/u/%1").arg(id);
    }

    if (providerId == QStringLiteral("gofile")) {
        const QJsonObject object = QJsonDocument::fromJson(response).object();
        const QJsonObject data = object.value(QStringLiteral("data")).toObject();
        QString link = data.value(QStringLiteral("downloadPage")).toString();
        if (link.isEmpty()) {
            if (errorMessage) {
                const QString status = object.value(QStringLiteral("status")).toString();
                *errorMessage = QObject::tr("Gofile rejected the upload (%1).").arg(status);
            }
            return {};
        }
        return link;
    }

    if (providerId == QStringLiteral("custom")) {
        if (text.startsWith(QStringLiteral("http://")) || text.startsWith(QStringLiteral("https://"))) {
            return text;
        }
        // Some endpoints answer with a small JSON document instead of the URL.
        const QJsonObject object = QJsonDocument::fromJson(response).object();
        for (const char* key : { "url", "link", "download", "downloadPage", "message" }) {
            const QString candidate = object.value(QString::fromLatin1(key)).toString();
            if (candidate.startsWith(QStringLiteral("http"))) {
                return candidate;
            }
        }
        if (errorMessage) {
            *errorMessage = QObject::tr("The server did not return a download link.");
        }
        return {};
    }

    // catbox, litterbox, temp.sh and x0.at reply with the plain URL.
    if (!text.startsWith(QStringLiteral("http://")) && !text.startsWith(QStringLiteral("https://"))) {
        if (errorMessage) {
            *errorMessage = text.isEmpty()
                ? QObject::tr("The server did not return a download link.")
                : QObject::tr("The host refused the upload: %1").arg(text.left(200));
        }
        return {};
    }
    return text;
}

/// Queries the Gofile server list (at most once every ten seconds) and returns
/// the name of a store server.
inline QString gofileServer(QString* errorMessage)
{
    static QString cached;
    static QElapsedTimer timer;

    // Gofile asks not to query the server list more than once every ten seconds;
    // caching it for half an hour keeps uploads fast and the API host relaxed.
    if (!cached.isEmpty() && timer.isValid() && timer.elapsed() < 30 * 60 * 1000) {
        return cached;
    }

    QByteArray config = "silent\nshow-error\nfail-with-body\nconnect-timeout = 20\n";
    config += "url = \"https://api.gofile.io/servers\"\n";
    config += "header = \"Accept: application/json\"\n";

    QByteArray response;
    if (!SendHttpHelper::run(config, errorMessage, &response)) {
        return {};
    }

    const QJsonObject object = QJsonDocument::fromJson(response).object();
    const QJsonArray servers = object.value(QStringLiteral("data")).toObject()
                                   .value(QStringLiteral("servers")).toArray();
    if (servers.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Gofile did not report any upload server.");
        }
        return {};
    }

    cached = servers.first().toObject().value(QStringLiteral("name")).toString();
    timer.start();
    return cached;
}

/// Uploads the archive and reports the share link.
inline Result upload(const QString& providerId, const QVariantMap& config,
                     const QString& archivePath, int timeoutMs)
{
    Result result;
    const QFileInfo info(archivePath);
    if (!info.exists() || !info.isFile()) {
        result.message = QObject::tr("The archive file no longer exists.");
        return result;
    }

    QVariantMap effectiveConfig = config;
    if (providerId == QStringLiteral("gofile")) {
        const QString cachedServer = config.value(QStringLiteral("cachedServer")).toString().trimmed();
        if (cachedServer.isEmpty()) {
            QString lookupError;
            const QString server = gofileServer(&lookupError);
            if (server.isEmpty()) {
                result.message = lookupError.isEmpty()
                    ? QObject::tr("Could not find a Gofile upload server.")
                    : lookupError;
                return result;
            }
            effectiveConfig.insert(QStringLiteral("cachedServer"), server);
        }
    }

    QString error;
    const QByteArray curlConfigText = curlConfig(providerId, effectiveConfig, archivePath, &error);
    if (curlConfigText.isEmpty()) {
        result.message = error;
        return result;
    }

    QByteArray response;
    if (!SendHttpHelper::run(curlConfigText, &error, &response, timeoutMs)) {
        result.message = error;
        return result;
    }

    const QString link = parseResponse(providerId, response, &error);
    if (link.isEmpty()) {
        result.message = error;
        return result;
    }

    result.success = true;
    result.link = link;
    return result;
}

} // namespace WebShareUpload

#endif // WEBSHAREUPLOAD_H