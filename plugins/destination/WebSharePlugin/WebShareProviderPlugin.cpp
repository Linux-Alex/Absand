// SPDX-License-Identifier: MPL-2.0

#include "WebShareProviderPlugin.h"

#include "core/WebShareUpload.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#ifndef ABSAND_WEB_PROVIDER
#define ABSAND_WEB_PROVIDER "gofile"
#endif

namespace {

QString defaultSaveFolder()
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    return folder;
}

QString providerLabel(const QString& providerId)
{
    if (providerId == QStringLiteral("custom")) {
        return QObject::tr("Web share (custom endpoint)");
    }
    return WebShareUpload::providerById(providerId).label;
}

} // namespace

WebShareProviderPlugin::WebShareProviderPlugin(QObject* parent)
    : QObject(parent)
    , m_providerId(QString::fromLatin1(ABSAND_WEB_PROVIDER))
{
}

QString WebShareProviderPlugin::providerId() const
{
    return m_providerId;
}

QString WebShareProviderPlugin::name() const
{
    return providerLabel(m_providerId);
}

QString WebShareProviderPlugin::description() const
{
    if (m_providerId == QStringLiteral("custom")) {
        return tr("Upload the archive to any host that accepts a multipart POST and share the link");
    }
    return tr("Upload the archive to %1 and share the download link")
        .arg(WebShareUpload::providerById(m_providerId).label);
}

QVariantMap WebShareProviderPlugin::metadata() const
{
    return {
        { "name", name() },
        { "version", version() },
        { "description", description() },
        { "author", "Absand Team" },
        { "channel", channelName() }
    };
}

QString WebShareProviderPlugin::channelName() const
{
    return m_providerId == QStringLiteral("custom") ? QStringLiteral("web_share_custom") : m_providerId;
}

QString WebShareProviderPlugin::configKey() const
{
    return channelName();
}

QVariantMap WebShareProviderPlugin::defaultConfiguration() const
{
    QVariantMap config;
    config.insert(QStringLiteral("provider"), m_providerId);
    config.insert(QStringLiteral("accountToken"), QString());
    config.insert(QStringLiteral("expiry"), QStringLiteral("72h"));
    config.insert(QStringLiteral("customUrl"), QString());
    config.insert(QStringLiteral("cachedServer"), QString());
    config.insert(QStringLiteral("saveFolder"), defaultSaveFolder());
    return config;
}

ConfigSchema WebShareProviderPlugin::configSchema() const
{
    ConfigSchema schema;

    if (m_providerId == QStringLiteral("pixeldrain")) {
        schema.append({ QStringLiteral("accountToken"), tr("API key"),
                        tr("Pixeldrain requires an API key; anonymous uploads are not accepted"),
                        QString(), ConfigField::Type::Password, ConfigField::Browse::None, {},
                        QVariant(), ConfigField::Required });
    } else if (m_providerId == QStringLiteral("gofile")) {
        schema.append({ QStringLiteral("accountToken"), tr("Account token"),
                        tr("Optional: ties uploads to your own Gofile account"), QString(),
                        ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant() });
    } else if (m_providerId == QStringLiteral("catbox")) {
        schema.append({ QStringLiteral("accountToken"), tr("Account userhash"),
                        tr("Optional: uploads then belong to your catbox account"), QString(),
                        ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant() });
    }

    if (m_providerId == QStringLiteral("litterbox")) {
        schema.append({ QStringLiteral("expiry"), tr("Expiry"),
                        tr("The upload is deleted automatically after this time"), QString(),
                        ConfigField::Type::Choice, ConfigField::Browse::None,
                        { QStringLiteral("1h"), QStringLiteral("12h"),
                          QStringLiteral("24h"), QStringLiteral("72h") },
                        QStringLiteral("72h") });
    }

    if (m_providerId == QStringLiteral("custom")) {
        schema.append({ QStringLiteral("customUrl"), tr("Upload endpoint"),
                        tr("A host that accepts a multipart POST with a 'file' field"),
                        QStringLiteral("https://example.com/upload"),
                        ConfigField::Type::Text, ConfigField::Browse::None, {}, QVariant(),
                        ConfigField::Required });
    }

    if (m_providerId == QStringLiteral("gofile")) {
        schema.append({ QStringLiteral("cachedServer"), tr("Upload server"),
                        tr("Usually detected automatically; set it to pin one server"),
                        QStringLiteral("store1"), ConfigField::Type::Text,
                        ConfigField::Browse::None, {}, QVariant(), ConfigField::Advanced });
    }

    schema.append({ QStringLiteral("saveFolder"), tr("Default save folder"),
                    tr("Where the archive is written before it is uploaded"), QString(),
                    ConfigField::Type::Text, ConfigField::Browse::Directory, {},
                    defaultSaveFolder(), ConfigField::Required });

    return schema;
}

bool WebShareProviderPlugin::validateConfiguration(const QVariantMap& config, QString* error) const
{
    const WebShareUpload::Provider provider = WebShareUpload::providerById(m_providerId);

    if (WebShareUpload::requiresToken(m_providerId)
        && config.value(QStringLiteral("accountToken")).toString().trimmed().isEmpty()) {
        if (error) {
            *error = tr("%1 needs an API key in the account token field.").arg(provider.label);
        }
        return false;
    }

    if (m_providerId == QStringLiteral("custom")) {
        const QString url = config.value(QStringLiteral("customUrl")).toString().trimmed();
        if (!url.startsWith(QStringLiteral("http://")) && !url.startsWith(QStringLiteral("https://"))) {
            if (error) {
                *error = tr("Please enter the upload endpoint URL.");
            }
            return false;
        }
    }

    if (config.value(QStringLiteral("saveFolder")).toString().trimmed().isEmpty()) {
        if (error) {
            *error = tr("Please choose where the archive is written locally.");
        }
        return false;
    }

    return true;
}

QString WebShareProviderPlugin::suggestedLocalSaveFolder(const QVariantMap& config) const
{
    return config.value(QStringLiteral("saveFolder"), defaultSaveFolder()).toString();
}

bool WebShareProviderPlugin::transferArchive(const QString& archivePath,
                                             const QVariantMap& config,
                                             TransferCallback resultCallback)
{
    auto report = [resultCallback](bool ok, const QString& message, const QString& link) {
        if (resultCallback) {
            resultCallback(ok, message, link);
        }
        return ok;
    };

    QString error;
    if (!validateConfiguration(config, &error)) {
        return report(false, error, QString());
    }

    const WebShareUpload::Provider provider = WebShareUpload::providerById(m_providerId);
    const QFileInfo info(archivePath);
    if (!info.exists() || !info.isFile()) {
        return report(false, tr("The archive file no longer exists."), QString());
    }

    if (provider.limitMb > 0) {
        const qint64 limitBytes = static_cast<qint64>(provider.limitMb) * 1024 * 1024;
        if (info.size() > limitBytes) {
            return report(false,
                          tr("%1 accepts files up to %2 MB, but the archive is %3 MB.")
                              .arg(provider.label)
                              .arg(provider.limitMb)
                              .arg(QString::number(info.size() / (1024.0 * 1024.0), 'f', 1)),
                          QString());
        }
    }

    const WebShareUpload::Result result =
        WebShareUpload::upload(m_providerId, config, archivePath, 30 * 60 * 1000);

    if (!result.success) {
        return report(false,
                      result.message.isEmpty()
                          ? tr("%1 rejected the upload.").arg(provider.label)
                          : result.message,
                      QString());
    }

    return report(true, tr("Uploaded to %1: %2").arg(provider.label, result.link), result.link);
}
