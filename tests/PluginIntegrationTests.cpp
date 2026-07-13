// SPDX-License-Identifier: MPL-2.0

#include <QtTest>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <zip.h>
#include <archive.h>
#include <archive_entry.h>

#include <bit7z/bit7z.hpp>
#include <bit7z/bitabstractarchivecreator.hpp>

#include "core/PluginManager.h"
#include "core/SevenZipRuntime.h"
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
        QCOMPARE(manager.sendPlugins().size(), 4);
        QVERIFY(manager.destinationPlugins().size() >= 4);
        QVERIFY(manager.archivePluginByName("zip archive"));
        QVERIFY(manager.sendPluginByName("CLIPBOARD"));
        QCOMPARE(manager.availableArchiveFormats(), QStringList({"7z", "zip"}));
        QCOMPARE(manager.availableSendChannels(), QStringList({"clipboard", "matrix", "teams", "telegram"}));
        QVERIFY(manager.sendPluginByName("Matrix"));
        QVERIFY(manager.sendPluginByName("Microsoft Teams"));
        QVERIFY(manager.sendPluginByName("Telegram"));
        QVERIFY(manager.destinationPluginByName("LOCAL FOLDER"));
    }

    void loadingTwiceDoesNotDuplicatePlugins()
    {
        QVERIFY(!manager.loadAllPlugins());
        QCOMPARE(manager.archivePlugins().size(), 2);
        QCOMPARE(manager.sendPlugins().size(), 4);
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
