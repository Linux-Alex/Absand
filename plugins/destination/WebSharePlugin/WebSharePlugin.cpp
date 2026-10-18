// SPDX-License-Identifier: MPL-2.0

#include "WebSharePlugin.h"

#include "core/WebShareUpload.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

QString defaultSaveFolder()
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (folder.isEmpty()) {
        folder = QDir::homePath();
    }
    return folder;
}

QVariantMap providerDefaults()
{
    QVariantMap defaults;
    defaults.insert(QStringLiteral("provider"), QStringLiteral("gofile"));
    defaults.insert(QStringLiteral("accountToken"), QString());
    defaults.insert(QStringLiteral("expiry"), QStringLiteral("72h"));
    defaults.insert(QStringLiteral("customUrl"), QString());
    defaults.insert(QStringLiteral("cachedServer"), QString());
    defaults.insert(QStringLiteral("saveFolder"), defaultSaveFolder());
    return defaults;
}

} // namespace

WebSharePlugin::WebSharePlugin(QObject* parent)
    : QObject(parent)
{
}

QVariantMap WebSharePlugin::metadata() const
{
    return {
        { "name", name() },
        { "version", version() },
        { "description", description() },
        { "author", "Absand Team" },
        { "channel", channelName() }
    };
}

QVariantMap WebSharePlugin::defaultConfiguration() const
{
    return providerDefaults();
}

ConfigSchema WebSharePlugin::configSchema() const
{
    // Values stay provider ids so a translated label never changes the setting.
    QStringList providerValues;
    QStringList providerLabels;
    for (const WebShareUpload::Provider& provider : WebShareUpload::providers()) {
        providerValues.append(provider.id);
        providerLabels.append(provider.note.isEmpty()
                                  ? provider.label
                                  : QStringLiteral("%1 — %2").arg(provider.label, provider.note));
    }

    return {
        { QStringLiteral("provider"), tr("Provider"), tr("Where the archive is uploaded"), QString(),
          ConfigField::Type::Choice, ConfigField::Browse::None, providerValues,
          QStringLiteral("gofile"), ConfigField::Required, providerLabels },
        { QStringLiteral("accountToken"), tr("Account token"),
          tr("Pixeldrain requires an API key; Gofile and catbox accept an optional account token"),
          QString(), ConfigField::Type::Password, ConfigField::Browse::None, {}, QVariant() },
        { QStringLiteral("expiry"), tr("Expiry"),
          tr("Only used by Litterbox, which deletes the upload afterwards"), QString(),
          ConfigField::Type::Choice, ConfigField::Browse::None,
          { QStringLiteral("1h"), QStringLiteral("12h"), QStringLiteral("24h"), QStringLiteral("72h") },
          QStringLiteral("72h"), ConfigField::Advanced },
        { QStringLiteral("customUrl"), tr("Custom endpoint"),
          tr("Used with the custom provider; a multipart POST with a 'file' field"),
          QStringLiteral("https://example.com/upload"), ConfigField::Type::Text,
          ConfigField::Browse::None, {}, QVariant(), ConfigField::Advanced },
        { QStringLiteral("cachedServer"), tr("Upload server"),
          tr("Gofile upload server; leave empty to let Absand choose one"),
          QStringLiteral("store1"), ConfigField::Type::Text, ConfigField::Browse::None, {},
          QVariant(), ConfigField::Advanced },
        { QStringLiteral("saveFolder"), tr("Default save folder"),
          tr("Where the archive is written before it is uploaded"), QString(),
          ConfigField::Type::Text, ConfigField::Browse::Directory, {},
          defaultSaveFolder(), ConfigField::Required }
    };
}

bool WebSharePlugin::validateConfiguration(const QVariantMap& config, QString* error) const
{
    const QString providerId = config.value(QStringLiteral("provider"), QStringLiteral("gofile")).toString();
    if (!WebShareUpload::providerIds().contains(providerId)) {
        if (error) *error = tr("Please choose a web file host.");
        return false;
    }

    if (WebShareUpload::requiresToken(providerId)
        && config.value(QStringLiteral("accountToken")).toString().trimmed().isEmpty()) {
        if (error) *error = tr("%1 needs an API key in the account token field.")
                                .arg(WebShareUpload::providerById(providerId).label);
        return false;
    }

    if (providerId == QStringLiteral("custom")) {
        const QString url = config.value(QStringLiteral("customUrl")).toString().trimmed();
        if (!url.startsWith(QStringLiteral("http://")) && !url.startsWith(QStringLiteral("https://"))) {
            if (error) *error = tr("Please enter the upload endpoint URL.");
            return false;
        }
    }

    if (config.value(QStringLiteral("saveFolder")).toString().trimmed().isEmpty()) {
        if (error) *error = tr("Please choose where the archive is written locally.");
        return false;
    }

    return true;
}

QString WebSharePlugin::suggestedLocalSaveFolder(const QVariantMap& config) const
{
    return config.value("saveFolder", defaultSaveFolder()).toString();
}

bool WebSharePlugin::transferArchive(const QString& archivePath,
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

    const QString providerId = config.value(QStringLiteral("provider"), QStringLiteral("gofile")).toString();
    const WebShareUpload::Provider provider = WebShareUpload::providerById(providerId);

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

    const WebShareUpload::Result result = WebShareUpload::upload(providerId, config, archivePath, 30 * 60 * 1000);
    if (!result.success) {
        return report(false, result.message.isEmpty()
                                 ? tr("%1 rejected the upload.").arg(provider.label)
                                 : result.message,
                      QString());
    }

    return report(true, tr("Uploaded to %1: %2").arg(provider.label, result.link), result.link);
}
