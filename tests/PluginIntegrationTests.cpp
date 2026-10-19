// SPDX-License-Identifier: MPL-2.0

#include <QtTest>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <zip.h>
#include <archive.h>
#include <archive_entry.h>

#include <bit7z/bit7z.hpp>
#include <bit7z/bitabstractarchivecreator.hpp>

#include "app/AppBackend.h"
#include "app/ArchiveBrowser.h"
#include "core/PluginManager.h"
#include "core/SevenZipRuntime.h"
#include "core/TransferHelpers.h"
#include "core/WebShareUpload.h"
#include "core/SendHttpHelper.h"
#include "core/SmtpMessage.h"
#include "core/UserConfig.h"
#include "interfaces/ArchivePluginInterface.h"
#include "interfaces/DestinationPluginInterface.h"
#include "interfaces/SendPluginInterface.h"

class PluginIntegrationTests : public QObject
{
    Q_OBJECT

private:
    PluginManager manager;

    static QString writeFile(const QString& path, const QByteArray& contents)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size())
            return {};
        return path;
    }

    static QByteArray readZipEntry(const QString& archivePath, const QString& entry,
                                   const QByteArray& password = {})
    {
        int error = 0;
        zip_t* archive = zip_open(QFile::encodeName(archivePath).constData(), ZIP_RDONLY, &error);
        if (!archive)
            return {};
        zip_file_t* file = password.isEmpty()
            ? zip_fopen(archive, entry.toUtf8().constData(), 0)
            : zip_fopen_encrypted(archive, entry.toUtf8().constData(), 0, password.constData());
        QByteArray result;
        if (file) {
            char buffer[4096];
            zip_int64_t count;
            while ((count = zip_fread(file, buffer, sizeof(buffer))) > 0)
                result.append(buffer, count);
            zip_fclose(file);
        }
        zip_close(archive);
        return result;
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(manager.loadAllPlugins(), "No plugins loaded from the build output");
        QCOMPARE(manager.archivePlugins().size(), 2);
        QCOMPARE(manager.sendPlugins().size(), 9);
        QCOMPARE(manager.destinationPlugins().size(), 13);
        QVERIFY(manager.archivePluginByName("zip archive"));
        QVERIFY(manager.sendPluginByName("CLIPBOARD"));
        QCOMPARE(manager.availableArchiveFormats(), QStringList({"7z", "zip"}));
        QCOMPARE(manager.availableSendChannels(),
                 QStringList({"clipboard", "discord", "email", "matrix", "ntfy", "sms", "teams", "telegram", "webhook"}));
        QVERIFY(manager.sendPluginByName("Matrix"));
        QVERIFY(manager.sendPluginByName("Microsoft Teams"));
        QVERIFY(manager.sendPluginByName("Telegram"));
        QVERIFY(manager.sendPluginByName("E-mail (SMTP)"));
        QVERIFY(manager.sendPluginByName("SMS"));
        QVERIFY(manager.sendPluginByName("ntfy push"));
        QVERIFY(manager.sendPluginByName("Discord"));
        QVERIFY(manager.sendPluginByName("Webhook"));
        QVERIFY(manager.destinationPluginByName("LOCAL FOLDER"));
        QVERIFY(manager.destinationPluginByName("E-mail (SMTP)"));
        for (const char* host : { "GOFILE", "LITTERBOX", "TEMP.SH", "X0.AT", "CATBOX.MOE",
                                  "TMPFILES.ORG", "PIXELDRAIN" }) {
            QVERIFY2(manager.destinationPluginByName(QString::fromLatin1(host)), host);
        }
        QVERIFY(manager.destinationPluginByName("Web share (custom endpoint)"));
    }

    void loadingTwiceDoesNotDuplicatePlugins()
    {
        QVERIFY(!manager.loadAllPlugins());
        QCOMPARE(manager.archivePlugins().size(), 2);
        QCOMPARE(manager.sendPlugins().size(), 9);
    }

    void sevenZipRuntimeIsDiscoverable()
    {
        const QString libraryPath = resolveSevenZipLibraryPath();
        QVERIFY2(!libraryPath.isEmpty(), "Bundled 7-Zip runtime could not be found");
        QVERIFY2(QFileInfo::exists(libraryPath), qPrintable(libraryPath));
    }

    void zipCreatesReadableArchiveAndReportsProgress()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString input = writeFile(temp.filePath("hello.txt"), "hello absand");
        QVERIFY(!input.isEmpty());
        const QString output = temp.filePath("result.zip");
        QList<int> progress;
        auto* plugin = manager.archivePluginByName("ZIP Archive");
        QVERIFY(plugin->createArchive({input}, output, {}, [&](int value) { progress << value; }));
        QCOMPARE(readZipEntry(output, "hello.txt"), QByteArray("hello absand"));
        QCOMPARE(progress.first(), 0);
        QCOMPARE(progress.last(), 100);
    }

    void zipPreservesDirectoryTree()
    {
        QTemporaryDir temp;
        QDir root(temp.path());
        QVERIFY(root.mkpath("folder/nested"));
        QVERIFY(!writeFile(temp.filePath("folder/nested/data.bin"), "payload").isEmpty());
        const QString output = temp.filePath("tree.zip");
        auto* plugin = manager.archivePluginByName("ZIP Archive");
        QVERIFY(plugin->createArchive({temp.filePath("folder")}, output, {}));
        QCOMPARE(readZipEntry(output, "folder/nested/data.bin"), QByteArray("payload"));
    }

    void zipEncryptionRequiresCorrectPassword()
    {
        QTemporaryDir temp;
        const QString input = writeFile(temp.filePath("secret.txt"), "classified");
        const QString output = temp.filePath("secret.zip");
        auto* plugin = manager.archivePluginByName("ZIP Archive");
        QVERIFY(plugin->createArchive({input}, output, "correct horse"));
        QVERIFY(readZipEntry(output, "secret.txt", "wrong").isEmpty());
        QCOMPARE(readZipEntry(output, "secret.txt", "correct horse"), QByteArray("classified"));
    }

    void sevenZipCreatesReadableArchive()
    {
        QTemporaryDir temp;
        const QString input = writeFile(temp.filePath("hello.txt"), "seven zip payload");
        const QString output = temp.filePath("result.7z");
        auto* plugin = manager.archivePluginByName("7z Archive");
        QVERIFY(plugin->createArchive({input}, output, {}));

        archive* reader = archive_read_new();
        archive_read_support_format_7zip(reader);
        QVERIFY(archive_read_open_filename(reader, QFile::encodeName(output).constData(), 10240) == ARCHIVE_OK);
        archive_entry* entry = nullptr;
        QVERIFY(archive_read_next_header(reader, &entry) == ARCHIVE_OK);
        QCOMPARE(QString::fromUtf8(archive_entry_pathname(entry)), QString("hello.txt"));
        QByteArray data(64, '\0');
        const la_ssize_t size = archive_read_data(reader, data.data(), data.size());
        QVERIFY(size >= 0);
        data.resize(size);
        QCOMPARE(data, QByteArray("seven zip payload"));
        archive_read_free(reader);
    }

    void archivePluginsRejectInvalidRequests()
    {
        QTemporaryDir temp;
        for (ArchivePluginInterface* plugin : manager.archivePlugins()) {
            const QString output = temp.filePath("invalid." + plugin->defaultExtension());
            QVERIFY(!plugin->createArchive({}, output, {}));
            QVERIFY(!QFileInfo::exists(output));
            QVERIFY(!plugin->createArchive({temp.filePath("missing")}, output, {}));
            QVERIFY(!QFileInfo::exists(output));
        }
    }

    void sevenZipCreatesEncryptedArchiveAndRequiresPassword()
    {
        QTemporaryDir temp;
        const QString input = writeFile(temp.filePath("secret.txt"), "secret");
        const QString output = temp.filePath("secret.7z");
        auto* plugin = manager.archivePluginByName("7z Archive");

        QVERIFY(plugin->createArchive({input}, output, "must-not-be-ignored"));
        QVERIFY(QFileInfo::exists(output));

        const QString libraryPath = resolveSevenZipLibraryPath();
        QVERIFY2(!libraryPath.isEmpty(), "Missing 7-Zip shared library for encrypted archive test");

        bit7z::Bit7zLibrary library{ libraryPath.toStdString() };
        bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::SevenZip };

        QTemporaryDir wrongDir;
        QVERIFY(wrongDir.isValid());
        extractor.setPassword("wrong");
        QVERIFY_EXCEPTION_THROWN(
            extractor.extract(output.toStdString(), wrongDir.path().toStdString()),
            bit7z::BitException
        );

        QTemporaryDir extractDir;
        QVERIFY(extractDir.isValid());
        extractor.setPassword("must-not-be-ignored");
        extractor.extract(output.toStdString(), extractDir.path().toStdString());
        QFile extractedFile(extractDir.filePath("secret.txt"));
        QVERIFY(extractedFile.open(QIODevice::ReadOnly));
        QCOMPARE(extractedFile.readAll(), QByteArray("secret"));
    }

    void zipHandlesNonAsciiPathsAndEntryNames()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QDir root(temp.path());
        QVERIFY(root.mkpath(QString::fromUtf8("gradivo/\xC5\xA1ola")));

        const QString input = writeFile(temp.filePath(QString::fromUtf8("gradivo/\xC5\xA1ola/\xC4\x8Drke.txt")),
                                        "unicode payload");
        QVERIFY(!input.isEmpty());

        // Non-ASCII characters in the archive path itself broke ZIP creation on
        // Windows, because libzip receives a narrow path from the C runtime.
        const QString output = temp.filePath(QString::fromUtf8("arhiv \xC4\x8D.png \xC5\xBEiga.zip"));
        auto* plugin = manager.archivePluginByName("ZIP Archive");
        QVERIFY(plugin->createArchive({temp.filePath(QString::fromUtf8("gradivo"))}, output, {}));
        QVERIFY(QFileInfo::exists(output));

        QCOMPARE(readZipEntry(output, QString::fromUtf8("gradivo/\xC5\xA1ola/\xC4\x8Drke.txt")),
                 QByteArray("unicode payload"));
    }

    void zipWritesThroughMemoryBufferAsAFallback()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QDir root(temp.path());
        QVERIFY(root.mkpath("folder/nested"));

        const QString input = writeFile(temp.filePath("folder/nested/data.txt"), "buffered payload");
        QVERIFY(!input.isEmpty());
        const QString output = temp.filePath("memory.zip");

        // This is the fallback Windows uses for paths outside the ANSI code
        // page; force it here so the write-out path is covered on every host.
        qputenv("ABSAND_ZIP_MEMORY_BUFFER", "1");
        auto* plugin = manager.archivePluginByName("ZIP Archive");
        const bool created = plugin->createArchive({ temp.filePath("folder") }, output, {});
        qunsetenv("ABSAND_ZIP_MEMORY_BUFFER");

        QVERIFY(created);
        QVERIFY(QFileInfo::exists(output));
        QCOMPARE(readZipEntry(output, "folder/nested/data.txt"), QByteArray("buffered payload"));

        // Encryption must work through the buffer as well.
        qputenv("ABSAND_ZIP_MEMORY_BUFFER", "1");
        const QString secret = temp.filePath("memory-secret.zip");
        const bool encrypted = plugin->createArchive({ input }, secret, "buffered-pass");
        qunsetenv("ABSAND_ZIP_MEMORY_BUFFER");

        QVERIFY(encrypted);
        QCOMPARE(readZipEntry(secret, "data.txt", "buffered-pass"), QByteArray("buffered payload"));
    }

    void zipStoresEmptyDirectories()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QDir root(temp.path());
        QVERIFY(root.mkpath("folder/empty"));
        QVERIFY(!writeFile(temp.filePath("folder/keep.txt"), "kept").isEmpty());

        const QString output = temp.filePath("empties.zip");
        auto* plugin = manager.archivePluginByName("ZIP Archive");
        QVERIFY(plugin->createArchive({temp.filePath("folder")}, output, {}));

        int error = 0;
        zip_t* archive = zip_open(QFile::encodeName(output).constData(), ZIP_RDONLY, &error);
        QVERIFY(archive);
        QVERIFY(zip_name_locate(archive, "folder/empty/", ZIP_FL_NOCASE) >= 0);
        QVERIFY(zip_name_locate(archive, "folder/keep.txt", ZIP_FL_NOCASE) >= 0);
        zip_close(archive);
    }

    void sevenZipOverwritesExistingArchive()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString input = writeFile(temp.filePath("overwrite.txt"), "second version");
        const QString output = temp.filePath("existing.7z");
        QVERIFY(!writeFile(output, "not an archive").isEmpty());

        auto* plugin = manager.archivePluginByName("7z Archive");
        // 7-Zip refuses to replace an existing file unless the overwrite mode is set.
        QVERIFY(plugin->createArchive({input}, output, {}));

        archive* reader = archive_read_new();
        archive_read_support_format_7zip(reader);
        QCOMPARE(archive_read_open_filename(reader, QFile::encodeName(output).constData(), 10240), ARCHIVE_OK);
        archive_entry* entry = nullptr;
        QCOMPARE(archive_read_next_header(reader, &entry), ARCHIVE_OK);
        archive_read_free(reader);
    }

    void targetUrlsArePercentEncoded()
    {
        QCOMPARE(TransferHelpers::buildTargetUrl("ftp", "example.com", 21, "/uploads", "arhiv 2024.zip"),
                 QString("ftp://example.com:21/uploads/arhiv%202024.zip"));
        QCOMPARE(TransferHelpers::buildTargetUrl("sftp", "example.com", 22, "/odd folder",
                                                 QString::fromUtf8("\xC5\xBEiga.7z")),
                 QString("sftp://example.com:22/odd%20folder/%C5%BEiga.7z"));
        // A target copied from a browser must not produce "ftp://ftp://...".
        QCOMPARE(TransferHelpers::buildTargetUrl("ftp", "ftp://example.com:2021/", 21, "", "a.zip"),
                 QString("ftp://example.com:2021/a.zip"));
        // An invalid port falls back to the scheme default instead of ":0".
        QCOMPARE(TransferHelpers::buildTargetUrl("ftp", "example.com", 0, "/", "a.zip"),
                 QString("ftp://example.com:21/a.zip"));
        QCOMPARE(TransferHelpers::buildTargetUrl("sftp", "example.com", 70000, "/", "a.zip"),
                 QString("sftp://example.com:22/a.zip"));
    }

    void copyFileToFolderReplacesDestinationAndKeepsSource()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString source = writeFile(temp.filePath("archive.zip"), "fresh");
        QVERIFY(!source.isEmpty());

        const QString target = temp.filePath("delivered/2024");
        QVERIFY(TransferHelpers::copyFileToFolder(source, target, nullptr));
        QFile delivered(QDir(target).filePath("archive.zip"));
        QVERIFY(delivered.open(QIODevice::ReadOnly));
        QCOMPARE(delivered.readAll(), QByteArray("fresh"));
        delivered.close();

        QVERIFY(writeFile(source, "newer") == source);
        QVERIFY(TransferHelpers::copyFileToFolder(source, target, nullptr));
        QVERIFY(delivered.open(QIODevice::ReadOnly));
        QCOMPARE(delivered.readAll(), QByteArray("newer"));
        delivered.close();

        // Copying onto itself must succeed without deleting the source.
        QVERIFY(TransferHelpers::copyFileToFolder(source, temp.path(), nullptr));
        QVERIFY(QFileInfo::exists(source));
        QVERIFY(QDir(target).entryList(QDir::Files).contains("archive.zip"));
    }

    void localFolderDestinationDeliversArchive()
    {
        auto* destination = manager.destinationPluginByName("Local Folder");
        QVERIFY(destination);

        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString archive = writeFile(temp.filePath("deliver.zip"), "payload");
        QVERIFY(!archive.isEmpty());

        const QString target = temp.filePath("delivered/today");
        const QVariantMap config{ { QStringLiteral("targetFolder"), target } };
        QVERIFY(destination->validateConfiguration(config, nullptr));

        bool succeeded = false;
        QString message;
        QString shareLink;
        QVERIFY(destination->transferArchive(archive, config,
                                             [&succeeded, &message, &shareLink](bool ok, QString text, QString link) {
                                                 succeeded = ok;
                                                 message = text;
                                                 shareLink = link;
                                             }));
        QVERIFY(shareLink.isEmpty());
        QVERIFY(succeeded);
        QVERIFY(!message.isEmpty());
        QVERIFY(QFileInfo::exists(QDir(target).filePath("deliver.zip")));
    }

    void curlUploadReportsFailureForUnreachableServer()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("curl")).isEmpty()) {
            QSKIP("curl is not installed");
        }

        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString payload = writeFile(temp.filePath("payload.zip"), "x");
        QVERIFY(!payload.isEmpty());

        QString error;
        const bool uploaded = TransferHelpers::uploadWithCurl(
            { QStringLiteral("-T"), payload, QStringLiteral("ftp://127.0.0.1:1/payload.zip") },
            &error, 15000);
        QVERIFY(!uploaded);
        QVERIFY(!error.isEmpty());
    }

    void copyFileToFolderReportsMissingSource()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QString error;
        QVERIFY(!TransferHelpers::copyFileToFolder(temp.filePath("missing.zip"), temp.path(), &error));
        QVERIFY(!error.isEmpty());
    }

    void pluginSchemasDescribeTheirDefaults()
    {
        // Every declared field must have a default, and every configuration key a
        // plugin returns must be described by its schema: the settings UI (and the
        // QML frontend) render exactly the schema.
        for (SendPluginInterface* plugin : manager.sendPlugins()) {
            const ConfigSchema schema = plugin->configSchema();
            if (schema.isEmpty()) {
                continue;
            }

            const QVariantMap defaults = plugin->defaultConfiguration();
            for (const ConfigField& field : schema) {
                QVERIFY2(!field.key.isEmpty(), qPrintable(plugin->name()));
                QVERIFY2(!field.label.isEmpty(), qPrintable(field.key));
                QVERIFY2(defaults.contains(field.key),
                         qPrintable(QStringLiteral("%1: missing default for %2").arg(plugin->name(), field.key)));

                const QVariant defaultValue = defaults.value(field.key);
                switch (field.type) {
                case ConfigField::Type::Integer:
                    QVERIFY2(defaultValue.canConvert<int>(), qPrintable(field.key));
                    break;
                case ConfigField::Type::Bool:
                    QVERIFY2(defaultValue.canConvert<bool>(), qPrintable(field.key));
                    break;
                case ConfigField::Type::Choice:
                    QVERIFY2(field.choices.contains(defaultValue.toString()),
                             qPrintable(QStringLiteral("%1: default %2 is not one of the choices")
                                            .arg(field.key, defaultValue.toString())));
                    break;
                default:
                    break;
                }
            }

            for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
                bool described = false;
                for (const ConfigField& field : schema) {
                    if (field.key == it.key()) {
                        described = true;
                        break;
                    }
                }
                QVERIFY2(described,
                         qPrintable(QStringLiteral("%1: key %2 is not in the schema").arg(plugin->name(), it.key())));
            }
        }

        for (DestinationPluginInterface* plugin : manager.destinationPlugins()) {
            const ConfigSchema schema = plugin->configSchema();
            const QVariantMap defaults = plugin->defaultConfiguration();
            for (const ConfigField& field : schema) {
                QVERIFY2(defaults.contains(field.key),
                         qPrintable(QStringLiteral("%1: missing default for %2").arg(plugin->name(), field.key)));
            }
        }
    }

    void profilesAreIndependentPerPlugin()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        UserConfig::setConfigRootOverrideForTests(temp.path());

        const auto kind = UserConfig::ConfigKind::Send;
        const QString key = QStringLiteral("email");

        UserConfig::saveProfile(kind, key, QStringLiteral("marinic.eu"),
                                { { "host", "smtp.marinic.eu" }, { "recipient", "aleks@marinic.eu" } });
        UserConfig::saveProfile(kind, key, QStringLiteral("arcont.si"),
                                { { "host", "smtp.arcont.si" }, { "recipient", "dcs@marinic.eu" } });

        QCOMPARE(UserConfig::profileNames(kind, key),
                 QStringList({ QStringLiteral("arcont.si"), QStringLiteral("marinic.eu") }));

        // The last saved profile becomes the active one.
        QCOMPARE(UserConfig::activeProfile(kind, key), QStringLiteral("arcont.si"));
        QCOMPARE(UserConfig::loadProfile(kind, key).value("host").toString(), QStringLiteral("smtp.arcont.si"));

        UserConfig::setActiveProfile(kind, key, QStringLiteral("marinic.eu"));
        QCOMPARE(UserConfig::loadSendConfig(key).value("recipient").toString(), QStringLiteral("aleks@marinic.eu"));

        UserConfig::renameProfile(kind, key, QStringLiteral("arcont.si"), QStringLiteral("work"));
        QVERIFY(UserConfig::profileNames(kind, key).contains(QStringLiteral("work")));
        QVERIFY(!UserConfig::profileNames(kind, key).contains(QStringLiteral("arcont.si")));

        UserConfig::deleteProfile(kind, key, QStringLiteral("work"));
        QCOMPARE(UserConfig::profileNames(kind, key), QStringList({ QStringLiteral("marinic.eu") }));

        // The last remaining profile cannot be deleted.
        UserConfig::deleteProfile(kind, key, QStringLiteral("marinic.eu"));
        QCOMPARE(UserConfig::profileNames(kind, key), QStringList({ QStringLiteral("marinic.eu") }));

        UserConfig::clearConfigRootOverrideForTests();
    }

    void legacySingleConfigurationIsMigratedToADefaultProfile()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        UserConfig::setConfigRootOverrideForTests(temp.path());

        const QString legacyPath = QDir(temp.path()).filePath("send/telegram.json");
        QVERIFY(QDir().mkpath(QFileInfo(legacyPath).absolutePath()));
        const QString legacy = writeFile(legacyPath, R"({"botToken":"legacy-token","chatId":"42","messageTemplate":"{password}"})");
        QVERIFY(!legacy.isEmpty());

        const auto kind = UserConfig::ConfigKind::Send;
        QCOMPARE(UserConfig::profileNames(kind, QStringLiteral("telegram")),
                 QStringList({ UserConfig::defaultProfileName() }));
        QCOMPARE(UserConfig::loadSendConfig(QStringLiteral("telegram")).value("chatId").toString(), QStringLiteral("42"));

        // Saving migrates the file to the profile layout.
        UserConfig::saveProfile(kind, QStringLiteral("telegram"),
                                UserConfig::defaultProfileName(),
                                { { "botToken", "new-token" }, { "chatId", "7" }, { "messageTemplate", "{password}" } });
        QCOMPARE(UserConfig::loadSendConfig(QStringLiteral("telegram")).value("botToken").toString(),
                 QStringLiteral("new-token"));

        UserConfig::clearConfigRootOverrideForTests();
    }

    void newDeliveryChannelsValidateTheirSettings()
    {
        QString error;

        auto* email = manager.sendPluginByName("E-mail (SMTP)");
        QVERIFY(email);
        QVERIFY(!email->validateConfiguration({}, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(email->validateConfiguration({ { "host", "smtp.example.com" }, { "port", 587 },
                                              { "recipient", "a@example.com; b@example.com" },
                                              { "messageTemplate", "Password: {password}" } }, &error));
        QVERIFY(!email->validateConfiguration({ { "host", "smtp.example.com" }, { "port", 587 },
                                                { "recipient", "not-an-address" },
                                                { "messageTemplate", "{password}" } }, &error));

        auto* sms = manager.sendPluginByName("SMS");
        QVERIFY(sms);
        QVERIFY(!sms->validateConfiguration({}, &error));
        QVERIFY(sms->validateConfiguration({ { "provider", "Textbelt" }, { "recipient", "+38640123456" },
                                            { "messageTemplate", "{password}" } }, &error));
        QVERIFY(!sms->validateConfiguration({ { "provider", "Twilio" }, { "recipient", "+38640123456" },
                                              { "messageTemplate", "{password}" } }, &error));

        auto* ntfy = manager.sendPluginByName("ntfy push");
        QVERIFY(ntfy);
        QVERIFY(ntfy->validateConfiguration({ { "serverUrl", "https://ntfy.sh" }, { "topic", "abc123" },
                                              { "messageTemplate", "{password}" } }, &error));
        QVERIFY(!ntfy->validateConfiguration({ { "serverUrl", "https://ntfy.sh" }, { "messageTemplate", "{password}" } },
                                             &error));

        auto* discord = manager.sendPluginByName("Discord");
        QVERIFY(discord);
        QVERIFY(discord->validateConfiguration({ { "webhookUrl", "https://discord.com/api/webhooks/1/x" },
                                                 { "messageTemplate", "{password}" } }, &error));
        QVERIFY(!discord->validateConfiguration({ { "webhookUrl", "ftp://nope" },
                                                  { "messageTemplate", "{password}" } }, &error));

        auto* webhook = manager.sendPluginByName("Webhook");
        QVERIFY(webhook);
        QVERIFY(webhook->validateConfiguration({ { "url", "https://example.com/hook" },
                                                 { "messageTemplate", "{password}" } }, &error));
        QVERIFY(!webhook->validateConfiguration({ { "messageTemplate", "{password}" } }, &error));

        auto* emailDestination = manager.destinationPluginByName("E-mail (SMTP)");
        QVERIFY(emailDestination);
        QVERIFY(emailDestination->validateConfiguration({ { "host", "smtp.example.com" }, { "port", 587 },
                                                          { "recipient", "a@example.com" } }, &error));
        QVERIFY(!emailDestination->validateConfiguration({ { "recipient", "a@example.com" } }, &error));
    }

    void mailHelpersSplitAddressesAndEncodeHeaders()
    {
        QCOMPARE(SendHttpHelper::splitAddresses("a@example.com; b@example.com, c@example.com"),
                 QStringList({ "a@example.com", "b@example.com", "c@example.com" }));
        QCOMPARE(SendHttpHelper::splitAddresses("  ,, "), QStringList());
        QCOMPARE(SendHttpHelper::splitAddresses("a@example.com\na@example.com"),
                 QStringList({ "a@example.com" }));

        // Header injection must be impossible through the subject.
        const QString sanitized = SendHttpHelper::sanitizedHeaderValue("Hello\r\nBcc: evil@example.com");
        QVERIFY(!sanitized.contains('\n'));
        QVERIFY(!sanitized.contains('\r'));

        // Non-ASCII subjects are encoded as an RFC 2047 word.
        const QString subject = SmtpMessage::encodedSubject(QString::fromUtf8("\xC5\xBDiga geslo"));
        QVERIFY(subject.startsWith("=?UTF-8?B?"));
        QVERIFY(subject.endsWith("?="));

        const QByteArray wrapped = SmtpMessage::wrappedBase64(QByteArray(200, 'a'));
        const QList<QByteArray> lines = wrapped.split('\n');
        for (const QByteArray& line : lines) {
            QVERIFY(line.size() <= 77);
        }
    }

    void webShareProvidersCoverTheFreeHosts()
    {
        const QStringList ids = WebShareUpload::providerIds();
        for (const char* expected : { "gofile", "litterbox", "tempsh", "x0at", "catbox", "tmpfiles",
                                      "pixeldrain", "custom" }) {
            QVERIFY2(ids.contains(QString::fromLatin1(expected)), expected);
        }

        QCOMPARE(WebShareUpload::providerById("litterbox").limitMb, 1024);
        QCOMPARE(WebShareUpload::providerById("tempsh").limitMb, 4096);
        QCOMPARE(WebShareUpload::providerById("x0at").limitMb, 1024);
        QCOMPARE(WebShareUpload::providerById("catbox").limitMb, 200);
        QCOMPARE(WebShareUpload::providerById("tmpfiles").limitMb, 100);
        QCOMPARE(WebShareUpload::providerById("pixeldrain").limitMb, 20480);
        QVERIFY(WebShareUpload::requiresToken("pixeldrain"));
        QVERIFY(!WebShareUpload::requiresToken("gofile"));
    }

    void webShareCurlConfigsTargetTheRightEndpoints()
    {
        const QString archive = QStringLiteral("/tmp/arhiv test.zip");

        QString error;
        QVERIFY(WebShareUpload::curlConfig("pixeldrain", {}, archive, &error).isEmpty());
        QVERIFY(!error.isEmpty());

        error.clear();
        QVERIFY(WebShareUpload::curlConfig("custom", {}, archive, &error).isEmpty());
        QVERIFY(!error.isEmpty());

        const QByteArray gofile = WebShareUpload::curlConfig("gofile", {}, archive, &error);
        QVERIFY(gofile.contains("https://store1.gofile.io/contents/uploadfile"));
        QVERIFY(gofile.contains("form = \"file=@\\\"/tmp/arhiv test.zip\\\""));

        const QByteArray litterbox = WebShareUpload::curlConfig(
            "litterbox", { { "expiry", "24h" } }, archive, &error);
        QVERIFY(litterbox.contains("litterbox.catbox.moe/resources/internals/api.php"));
        QVERIFY(litterbox.contains("form = \"time=24h\""));
        QVERIFY(litterbox.contains("form = \"reqtype=fileupload\""));

        QVERIFY(WebShareUpload::curlConfig("tempsh", {}, archive, &error).contains("https://temp.sh/upload"));
        QVERIFY(WebShareUpload::curlConfig("x0at", {}, archive, &error).contains("https://x0.at/"));
        QVERIFY(WebShareUpload::curlConfig("catbox", {}, archive, &error).contains("catbox.moe/user/api.php"));
        QVERIFY(WebShareUpload::curlConfig("tmpfiles", {}, archive, &error).contains("tmpfiles.org/api/v1/upload"));

        const QByteArray pixeldrain = WebShareUpload::curlConfig(
            "pixeldrain", { { "accountToken", "secret-key" } }, QStringLiteral("/tmp/a.zip"), &error);
        QVERIFY(pixeldrain.contains("https://pixeldrain.com/api/file/a.zip"));
        QVERIFY(pixeldrain.contains("user = \":secret-key\""));
        QVERIFY(pixeldrain.contains("request = \"PUT\""));

        const QByteArray custom = WebShareUpload::curlConfig(
            "custom", { { "customUrl", "https://files.example/upload" } }, archive, &error);
        QVERIFY(custom.contains("https://files.example/upload"));
    }

    void webShareParsesProviderResponses()
    {
        QString error;

        QCOMPARE(WebShareUpload::parseResponse(
                     QStringLiteral("gofile"),
                     QByteArray("{\"status\":\"ok\",\"data\":{\"downloadPage\":\"https://gofile.io/d/abc123\"}}"),
                     &error),
                 QString("https://gofile.io/d/abc123"));

        // tmpfiles hands out its page link; the /dl/ variant no longer serves bytes.
        QCOMPARE(WebShareUpload::parseResponse(
                     QStringLiteral("tmpfiles"),
                     QByteArray("{\"status\":\"success\",\"data\":{\"url\":\"https://tmpfiles.org/1234/arhiv.zip\"}}"),
                     &error),
                 QString("https://tmpfiles.org/1234/arhiv.zip"));

        QCOMPARE(WebShareUpload::parseResponse(
                     QStringLiteral("pixeldrain"),
                     QByteArray("{\"id\":\"xyz789\",\"name\":\"arhiv.zip\"}"), &error),
                 QString("https://pixeldrain.com/u/xyz789"));

        for (const char* provider : { "catbox", "litterbox", "tempsh", "x0at" }) {
            QCOMPARE(WebShareUpload::parseResponse(QString::fromLatin1(provider),
                                                   QByteArray("https://host.example/arhiv.zip\n"), &error),
                     QString("https://host.example/arhiv.zip"));
        }

        // Custom endpoints may answer with plain text or a small JSON document.
        QCOMPARE(WebShareUpload::parseResponse("custom", "https://example.com/f/1", &error),
                 QString("https://example.com/f/1"));
        QCOMPARE(WebShareUpload::parseResponse(QStringLiteral("custom"),
                                              QByteArray("{\"url\":\"https://example.com/f/2\"}"), &error),
                 QString("https://example.com/f/2"));
        QVERIFY(WebShareUpload::parseResponse("custom", "nope", &error).isEmpty());
        QVERIFY(!error.isEmpty());

        error.clear();
        QVERIFY(WebShareUpload::parseResponse(QStringLiteral("tmpfiles"),
                                              QByteArray("{\"status\":\"error\"}"), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void webShareDestinationsExposeOneEntryPerHost()
    {
        QString error;

        // Every host is its own destination with its own profile store.
        auto* gofile = manager.destinationPluginByName("Gofile");
        QVERIFY(gofile);
        QCOMPARE(gofile->configKey(), QString("gofile"));
        QVERIFY(gofile->validateConfiguration(gofile->defaultConfiguration(), &error));

        auto* pixeldrain = manager.destinationPluginByName("pixeldrain");
        QVERIFY(pixeldrain);
        QCOMPARE(pixeldrain->configKey(), QString("pixeldrain"));
        QVERIFY(!pixeldrain->validateConfiguration(pixeldrain->defaultConfiguration(), &error));
        QVERIFY(!error.isEmpty());
        QVariantMap withKey = pixeldrain->defaultConfiguration();
        withKey.insert("accountToken", "api-key");
        QVERIFY(pixeldrain->validateConfiguration(withKey, &error));

        auto* custom = manager.destinationPluginByName("Web share (custom endpoint)");
        QVERIFY(custom);
        QVERIFY(!custom->validateConfiguration(custom->defaultConfiguration(), &error));
        QVariantMap withUrl = custom->defaultConfiguration();
        withUrl.insert("customUrl", "https://files.example/upload");
        QVERIFY(custom->validateConfiguration(withUrl, &error));

        auto* litterbox = manager.destinationPluginByName("Litterbox");
        QVERIFY(litterbox);
        QVERIFY(litterbox->validateConfiguration(litterbox->defaultConfiguration(), &error));
        QVERIFY(litterbox->defaultConfiguration().contains("expiry"));
        QCOMPARE(litterbox->defaultConfiguration().value("expiry").toString(), QString("72h"));
    }

    void messageTemplateSupportsTheShareLink()
    {
        const QVariantMap config{ { "messageTemplate", "Password: {password}\nDownload: {link}" },
                                  { "shareLink", "https://gofile.io/d/abc" } };
        QCOMPARE(SendHttpHelper::messageFromTemplate(config, "s3cret"),
                 QString("Password: s3cret\nDownload: https://gofile.io/d/abc"));

        // Without a share link the placeholder simply disappears.
        QCOMPARE(SendHttpHelper::messageFromTemplate({ { "messageTemplate", "Password: {password}{link}" } }, "x"),
                 QString("Password: x"));
    }

    void backendCreatesArchivesThroughTheQmlFacade()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        UserConfig::setConfigRootOverrideForTests(temp.path());

        const QString input = writeFile(temp.filePath("report.txt"), "quarterly numbers");
        QVERIFY(!input.isEmpty());

        AppBackend backend;
        backend.addFiles({ input });
        QCOMPARE(backend.inputCount(), 1);
        QVERIFY(backend.inputSummary().contains("quarterly") || backend.inputSummary().contains("report"));

        const QString outputFolder = temp.filePath("output");
        backend.setDestinationFolder(outputFolder);
        backend.setEncrypt(false);

        QVERIFY(backend.startArchive());
        QVERIFY(!backend.busy());

        const QStringList created = QDir(outputFolder).entryList(QDir::Files);
        QVERIFY(!created.isEmpty());
        QVERIFY(created.first().endsWith(".zip"));

        QVERIFY(backend.resultVisible());
        QVERIFY(backend.resultDetails().contains(created.first()));
        QVERIFY(backend.resultShareLink().isEmpty());
        backend.dismissResult();

        UserConfig::clearConfigRootOverrideForTests();
    }

    void backendRemembersPerProfileRecipients()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        UserConfig::setConfigRootOverrideForTests(temp.path());

        AppBackend backend;

        // Pick the e-mail channel and give it two profiles with different recipients.
        const QVariantList channels = backend.channels();
        int emailIndex = -1;
        for (int index = 0; index < channels.size(); ++index) {
            if (channels.at(index).toMap().value("key").toString() == "email") {
                emailIndex = index;
                break;
            }
        }
        QVERIFY(emailIndex > 0);

        backend.setChannelIndex(emailIndex);
        QVERIFY(backend.channelHasRecipient());

        QCOMPARE(backend.createProfile(true, QStringLiteral("email"), QStringLiteral("work"),
                                       backend.channelProfile()),
                 QString());
        backend.setChannelProfile(QStringLiteral("work"));
        backend.setChannelRecipient(QStringLiteral("work@example.com"));

        QCOMPARE(backend.createProfile(true, QStringLiteral("email"), QStringLiteral("private"),
                                       backend.channelProfile()),
                 QString());
        backend.setChannelProfile(QStringLiteral("private"));
        backend.setChannelRecipient(QStringLiteral("private@example.com"));

        backend.setChannelProfile(QStringLiteral("work"));
        QCOMPARE(backend.channelRecipient(), QString("work@example.com"));

        backend.setChannelProfile(QStringLiteral("private"));
        QCOMPARE(backend.channelRecipient(), QString("private@example.com"));

        // Deleting is allowed while another profile remains.
        QCOMPARE(backend.deleteProfile(true, QStringLiteral("email"), QStringLiteral("private")), QString());
        QVERIFY(UserConfig::profileNames(UserConfig::ConfigKind::Send, QStringLiteral("email"))
                    .contains(QStringLiteral("work")));
        QVERIFY(!UserConfig::profileNames(UserConfig::ConfigKind::Send, QStringLiteral("email"))
                     .contains(QStringLiteral("private")));

        UserConfig::clearConfigRootOverrideForTests();
    }

    void backendExposesPluginSchemasForTheSettingsPages()
    {
        AppBackend backend;

        const QVariantList sendSchemas = backend.pluginSettings(true);
        QVERIFY(sendSchemas.size() >= 8);

        bool sawEmailSchema = false;
        for (const QVariant& entry : sendSchemas) {
            const QVariantMap map = entry.toMap();
            QVERIFY(!map.value("key").toString().isEmpty());
            QVERIFY(!map.value("name").toString().isEmpty());
            QVERIFY(map.contains("values"));
            QVERIFY(map.contains("profiles"));

            if (map.value("key").toString() != "email") {
                continue;
            }
            sawEmailSchema = true;

            const QVariantList schema = map.value("schema").toList();
            QVERIFY(!schema.isEmpty());

            bool sawRecipient = false;
            for (const QVariant& fieldValue : schema) {
                const QVariantMap field = fieldValue.toMap();
                QVERIFY(!field.value("key").toString().isEmpty());
                QVERIFY(!field.value("type").toString().isEmpty());
                if (field.value("recipient").toBool()) {
                    sawRecipient = true;
                }
            }
            QVERIFY(sawRecipient);
        }
        QVERIFY(sawEmailSchema);

        const QVariantList destinationSchemas = backend.pluginSettings(false);
        QVERIFY(destinationSchemas.size() >= 12);

        // Saving values through the facade validates them.
        const QString error = backend.saveProfileValues(
            true, QStringLiteral("email"), QStringLiteral("broken"),
            QVariantMap{ { "host", "" }, { "messageTemplate", "Password: {password}" } });
        QVERIFY(!error.isEmpty());
    }

    void localFolderDestinationShowsASingleFolder()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        UserConfig::setConfigRootOverrideForTests(temp.path());

        AppBackend backend;

        // Local folder is the default destination and must not ask for a second
        // target: the save folder is the destination.
        QCOMPARE(backend.destinationName(), QString("Local Folder"));
        QVERIFY(!backend.destinationHasTarget());

        const QString folder = temp.filePath("chosen-folder");
        backend.setDestinationFolder(folder);
        QCOMPARE(backend.destinationFolder(), folder);

        // The choice is stored in the profile, so the settings dialog agrees.
        const QVariantMap stored = UserConfig::loadProfile(UserConfig::ConfigKind::Destination,
                                                           QStringLiteral("local_folder"));
        QCOMPARE(stored.value("targetFolder").toString(), folder);

        // Other destinations keep their own target field.
        const QVariantList destinations = backend.destinations();
        for (int index = 0; index < destinations.size(); ++index) {
            const QString key = destinations.at(index).toMap().value("key").toString();
            backend.setDestinationIndex(index);
            if (key == "local_folder") {
                QVERIFY(!backend.destinationHasTarget());
            } else if (key == "ftp" || key == "sftp" || key == "email_destination") {
                QVERIFY(backend.destinationHasTarget());
            }
        }

        UserConfig::clearConfigRootOverrideForTests();
    }

    void archiveBrowserDetectsEncryptionInsteadOfAlwaysAsking()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        // Plain archive: no password needed, so the browser must not ask.
        const QString plainInput = writeFile(temp.filePath("plain.txt"), "plain payload");
        QVERIFY(!plainInput.isEmpty());
        const QString plainArchive = temp.filePath("plain.zip");
        auto* zipPlugin = manager.archivePluginByName("ZIP Archive");
        QVERIFY(zipPlugin->createArchive({ plainInput }, plainArchive, {}));

        ArchiveBrowser browser;
        QCOMPARE(browser.openArchive(plainArchive, QString()), QString());
        QVERIFY(browser.archiveOpen());
        QVERIFY(!browser.archiveEncrypted());
        QVERIFY(!browser.passwordRequired());
        browser.closeArchive();

        // Encrypted archive: the entries open without a password but extraction
        // needs one, and the browser reports that.
        const QString secretInput = writeFile(temp.filePath("secret.txt"), "classified payload");
        QVERIFY(!secretInput.isEmpty());
        const QString secretArchive = temp.filePath("secret.zip");
        QVERIFY(zipPlugin->createArchive({ secretInput }, secretArchive, "browser-pass"));

        QCOMPARE(browser.openArchive(secretArchive, QString()), QString());
        QVERIFY(browser.archiveOpen());
        QVERIFY(browser.archiveEncrypted());
        QVERIFY(browser.passwordRequired());

        const QString extractFolder = temp.filePath("out");
        const QString failed = browser.extractPaths({ QStringLiteral("secret.txt") }, extractFolder);
        QVERIFY2(!failed.isEmpty(), "extraction without a password must be reported");
        {
            // A partial file may have been created, but never the plaintext.
            QFile leaked(QDir(extractFolder).filePath("secret.txt"));
            if (leaked.open(QIODevice::ReadOnly)) {
                QVERIFY(leaked.readAll() != QByteArray("classified payload"));
            }
        }

        // Supplying the password makes the same browser instance usable.
        QCOMPARE(browser.openArchive(secretArchive, QStringLiteral("browser-pass")), QString());
        QVERIFY(browser.archiveEncrypted());
        QVERIFY(!browser.passwordRequired());
        // extractPaths() reports success with a human readable message.
        QVERIFY(!browser.extractPaths({ QStringLiteral("secret.txt") }, extractFolder).isEmpty());
        {
            QFile extracted(QDir(extractFolder).filePath("secret.txt"));
            QVERIFY(extracted.open(QIODevice::ReadOnly));
            QCOMPARE(extracted.readAll(), QByteArray("classified payload"));
        }

        // WinZip AES does not encrypt the central directory, so a wrong password
        // still lists the entries; the failure is reported when extracting and
        // the browser then asks for the password again.
        QVERIFY(browser.openArchive(secretArchive, QStringLiteral("wrong")).isEmpty());
        QVERIFY(browser.archiveEncrypted());
        const QString wrongExtraction =
            browser.extractPaths({ QStringLiteral("secret.txt") }, temp.filePath("wrong-out"));
        QVERIFY(!wrongExtraction.isEmpty());
        QVERIFY(browser.passwordRequired());
    }

    void archiveBrowserListsFiltersAndExtractsEntries()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const QString input = writeFile(temp.filePath("hello.txt"), "browser payload");
        QVERIFY(!input.isEmpty());
        const QString output = temp.filePath("browser.zip");

        auto* plugin = manager.archivePluginByName("ZIP Archive");
        QVERIFY(plugin->createArchive({ input }, output, {}));

        ArchiveBrowser browser;
        QCOMPARE(browser.openArchive(output, QString()), QString());
        QVERIFY(browser.archiveOpen());
        QVERIFY(browser.itemCount() >= 1);

        browser.setFilterText(QStringLiteral("hello"));
        QVERIFY(browser.matchingCount() >= 1);
        browser.setFilterText(QStringLiteral("nothing-matches-this"));
        QCOMPARE(browser.matchingCount(), 0);
        browser.setFilterText(QString());

        const QString extractFolder = temp.filePath("extracted");
        const QString extraction = browser.extractPaths({ QStringLiteral("hello.txt") }, extractFolder);
        QVERIFY2(QFileInfo::exists(QDir(extractFolder).filePath("hello.txt")), qPrintable(extraction));
        {
            QFile extracted(QDir(extractFolder).filePath("hello.txt"));
            QVERIFY(extracted.open(QIODevice::ReadOnly));
            QCOMPARE(extracted.readAll(), QByteArray("browser payload"));
        }

        const QString extra = writeFile(temp.filePath("extra.txt"), "more");
        QCOMPARE(browser.addFiles({ extra }), QString());
        QVERIFY(browser.itemCount() >= 2);

        browser.closeArchive();
        QVERIFY(!browser.archiveOpen());
        QCOMPARE(browser.itemCount(), 0);
    }

    void clipboardSenderReportsSuccessAndRejectsEmptyPassword()
    {
        auto* plugin = manager.sendPluginByName("Clipboard");
        bool success = false;
        QString message;
        plugin->send("s3cret", {}, [&](bool ok, QString text) { success = ok; message = text; });
        QVERIFY(success);
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("s3cret"));
        plugin->send({}, {}, [&](bool ok, QString text) { success = ok; message = text; });
        QVERIFY(!success);
        QVERIFY(!message.isEmpty());
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("s3cret"));
    }

    void destinationConfigRoundTrips()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        UserConfig::setConfigRootOverrideForTests(temp.path());

        const QString pluginKey = "ftp";
        QVariantMap config;
        config["host"] = "example.com";
        config["port"] = 2121;
        config["username"] = "tester";
        config["password"] = "secret";
        config["remotePath"] = "/uploads";
        config["saveFolder"] = temp.path();

        UserConfig::saveDestinationConfig(pluginKey, config);
        const QVariantMap loaded = UserConfig::loadDestinationConfig(pluginKey);
        QCOMPARE(loaded.value("host").toString(), QString("example.com"));
        QCOMPARE(loaded.value("port").toInt(), 2121);
        QCOMPARE(loaded.value("username").toString(), QString("tester"));
        QCOMPARE(loaded.value("password").toString(), QString("secret"));
        QCOMPARE(loaded.value("remotePath").toString(), QString("/uploads"));
        QCOMPARE(loaded.value("saveFolder").toString(), temp.path());

        UserConfig::clearConfigRootOverrideForTests();
    }

    void sendConfigRoundTripsAndPluginsValidateRequiredFields()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        UserConfig::setConfigRootOverrideForTests(temp.path());

        const QVariantMap telegram{{"botToken", "test-token"}, {"chatId", "123"},
                                   {"messageTemplate", "Password: {password}"}};
        UserConfig::saveSendConfig("telegram", telegram);
        QCOMPARE(UserConfig::loadSendConfig("telegram"), telegram);

        QString error;
        QVERIFY(!manager.sendPluginByName("Telegram")->validateConfiguration({}, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(manager.sendPluginByName("Telegram")->validateConfiguration(telegram, &error));

        const QVariantMap matrix{{"homeserver", "https://matrix.example"}, {"accessToken", "token"},
                                 {"roomId", "!room:example"}, {"messageTemplate", "{password}"}};
        QVERIFY(manager.sendPluginByName("Matrix")->validateConfiguration(matrix, &error));

        const QVariantMap teams{{"webhookUrl", "https://example.invalid/webhook"},
                                {"messageTemplate", "{password}"}};
        QVERIFY(manager.sendPluginByName("Microsoft Teams")->validateConfiguration(teams, &error));

        UserConfig::saveLanguage("sl_SI");
        QCOMPARE(UserConfig::language(), QString("sl_SI"));
        UserConfig::clearConfigRootOverrideForTests();
    }
};

QTEST_MAIN(PluginIntegrationTests)
#include "PluginIntegrationTests.moc"