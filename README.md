# Absand

[![Build installers and publish release](https://github.com/Linux-Alex/Absand/actions/workflows/release.yml/badge.svg)](https://github.com/Linux-Alex/Absand/actions/workflows/release.yml)

Absand is a cross-platform Qt desktop application for creating encrypted ZIP and 7z archives, transferring them to configurable destinations, and delivering the archive password through a separate channel.

## Features

- ZIP and 7z archive creation, browsing, and optional encryption
- local-folder, network-share, FTP, SFTP, e-mail (SMTP), and anonymous **web share** destination plugins
- password delivery through clipboard, e-mail (SMTP), SMS, ntfy push, Telegram, Matrix,
  Microsoft Teams, Discord, or any custom webhook (Slack, Google Chat, Mattermost, Gotify, ...)
- several named profiles per channel, for example one per mail server or recipient
- the recipient (address, phone number, chat, topic, folder) is chosen in the main window,
  so a different destination needs no trip through the settings
- configurable credentials, recipients, and message templates for every delivery plugin
- English and Slovenian UI with a live language selector
- Kirigami/QML user interface following KDE standards, with the same settings rendered
  from plugin schemas as the classic widget dialog
- Windows Explorer and Linux/KDE file-manager integration

Passwords are sent only when archive creation succeeds and encryption is enabled. Network delivery uses the `curl` executable: Absand prefers a curl binary placed next to the application (the Windows installer ships one), falls back to `curl` from `PATH`, and needs curl 7.76 or newer for the Telegram, Matrix, and Teams channels. Transfers and delivery requests have timeouts, so an unreachable server can never hang the window forever; the progress dialog keeps the window repainting while a large archive is created.

Absand stores plugin settings as JSON in the current user's application-config directory; bot tokens, access tokens, FTP passwords, and webhook URLs are therefore not encrypted at rest. Protect that account and config directory, and use test credentials while developing.

## Web share (anonymous file hosts)

The **Web share** destination uploads the finished archive to an anonymous file host and reports
the download link; the link is shown in the summary, can be copied, and is available to password
messages through the `{link}` placeholder:

```text
Archive password: {password}
Download: {link}
```

Choose one provider per profile:

| Provider | Limit | Notes |
|---|---|---|
| Gofile | no published limit | default, keyless JSON API; the upload server is cached to keep the API host relaxed |
| Litterbox | 1 GB | temporary, deleted after the chosen expiry (1–72 h) |
| temp.sh | 4 GB | temporary, three day expiry, largest anonymous limit |
| x0.at | 1 GB | temporary, keeps the original file name |
| catbox.moe | 200 MB | permanent store |
| tmpfiles.org | 100 MB | short lived (1–48 h) |
| pixeldrain | 20 GB | needs an API key in the account token field |
| Custom endpoint | — | any host that accepts a multipart POST with a `file` field |

The archive is written to the usual save folder first, so a failed upload never loses it, and the
size limits are checked before the upload starts.

## Password delivery setup

Select a channel beside **Send password via**, pick one of its **profiles**, and type the
**recipient** (address, phone number, chat, topic, or folder) directly in the main window.
Credentials and server settings live in the profiles, so the same recipient field can point at
a colleague for one archive and at yourself for the next one.

Profiles are managed with **Configure channels...** (delivery) and **Configure...** (destination):
each tab lists the profiles of that plugin and offers **New**, **Rename**, and **Delete**.
A plugin that has never been configured keeps a single profile called *Default*.

All network channels run through the `curl` executable and need curl 7.76 or newer.

### E-mail (SMTP)

Works with any mail server and is the most universal channel; it is available both as a
delivery channel and as a **destination** that mails the finished archive as an attachment.

1. Enter the server (for example `smtp.gmail.com`), the port (587 for STARTTLS, 465 for
   implicit TLS), and the security mode.
2. Enter the username (usually the full address) and a password. Gmail, Outlook, and most
   providers require an **app-specific password** when two-factor authentication is enabled.
3. Fill in the sender address and, in the main window, the recipient. Several recipients can be
   separated with commas.

For the destination plugin the attachment size limit is 20 MB by default because most mail
servers reject larger messages.

### SMS

SMS is the only channel that usually costs money; every provider below is reachable with the
same configuration form:

- **Textbelt** – the free tier allows one text per day (`key = textbelt`), suitable for trying.
- **Twilio** – account SID plus auth token, sender number optional; trial credit covers testing.
- **Vonage** – API key plus API secret.
- **Custom URL** – any HTTP gateway, with `{recipient}` and `{message}` placeholders that
  Absand percent-encodes for you.

There is no reliable permanently free SMS API; carrier e-mail-to-SMS gateways have been shut
down by most providers, so Absand does not pretend otherwise.

### ntfy push

[ntfy](https://ntfy.sh) needs no account: install the app, subscribe to a topic, and put the
same topic into the profile. Choose a **long random topic**, because anybody who knows it can
read the message. Self-hosted servers are supported by changing the server URL.

### Discord

Create a channel webhook (channel settings → Integrations → Webhooks), paste the URL into the
profile, and the bot name if you like. The message template supports `{password}`.

### Webhook (Slack, Google Chat, Mattermost, Gotify, custom)

One plugin covers every service that accepts an incoming webhook: pick the service to choose
the JSON format (Slack/Mattermost and Google Chat use `text`, Discord uses `content`,
Gotify uses `message`), paste the URL, and optionally add an extra header such as
`Authorization: Bearer ...`. The **Custom** service lets you write the request body yourself
with `{message}` and `{password}` placeholders.

### Telegram

1. Create a bot with BotFather and copy its bot token.
2. Start a conversation with the bot or add it to the target group.
3. Obtain the numeric chat ID (for example with the Bot API `getUpdates` method).
4. Enter the bot token, chat ID, and a message template containing `{password}`.

The plugin calls Telegram's `sendMessage` Bot API endpoint.

### Matrix

Enter the homeserver URL, an access token belonging to a user that can post in the target room, and the room ID (normally beginning with `!`). The message template must contain `{password}`. The plugin uses the Matrix Client-Server API `m.room.message` endpoint.

### Microsoft Teams

The Teams plugin uses a **Workflows incoming webhook**, which works well in a Microsoft 365 developer tenant:

1. In Teams, add the Workflows app and create the “Post to a channel when a webhook request is received” workflow.
2. Choose the test team/channel and copy the generated HTTPS webhook URL.
3. Paste it into Absand and set a template containing `{password}`.

Treat the webhook URL as a secret. This implementation does not require an Entra application registration or tenant/client secrets.

## User interface

Absand ships two front ends that share the same core, plugins, and plugin schemas:

- **Kirigami/QML** (`src/qml/`, `src/app/AppBackend.*`, `src/app/ArchiveBrowser.*`) — the modern
  KDE interface: an application window with a global drawer, Breeze-styled cards, an archive
  browser page, and settings pages that are generated from the `ConfigField` schemas the
  plugins declare. Archive creation, transfer, and password delivery report progress inline.
- **Widgets** (`src/ui/`) — the classic dialog, used automatically when no matching KF6
  Kirigami is installed.

Select one with `-DABSAND_FRONTEND=auto|widgets|kirigami` (default `auto`). `auto` enables
the Kirigami front end only when KF6 Kirigami ships QML modules for the very Qt installation
Absand is compiled against, so a plain Qt build keeps working.

Kirigami build requirements:

```bash
# Debian/Ubuntu
sudo apt install libkf6kirigami-dev qml6-module-org-kde-kirigami qt6-declarative-dev

# MSYS2/UCRT64
pacman -S --needed mingw-w64-ucrt-x86_64-qt6-declarative mingw-w64-ucrt-x86_64-kirigami
```

The QML files are compiled into the executable, so nothing extra is installed; the runtime
needs the Kirigami QML module of the platform (the Debian package depends on
`qml6-module-org-kde-kirigami`, and `packaging/windows/bundle-runtime.sh` copies
`org/kde` into the deployment directory).

For headless smoke testing, `Absand --qml-selftest` instantiates the window and both settings
pages and exits with a status; the release workflow runs it on Linux and Windows.

## Repository layout

- `src/` — application, front ends (`qml`, `ui`), models, interfaces, and core helpers
- `plugins/archive/` — ZIP and 7z plugins
- `plugins/destination/` — local folder, network share, FTP, SFTP, and e-mail plugins
- `plugins/send/` — clipboard, e-mail, SMS, ntfy, Telegram, Matrix, Teams, Discord, and webhook plugins

Plugins describe their settings as a schema (`ConfigField`); the settings dialog renders that
schema today and the planned Kirigami/QML frontend renders the same schema with Qt Quick
controls, so a plugin never ships UI code for a specific front end.
- `translations/` — English and Slovenian Qt Linguist catalogs
- `libs/bit7z/` and `libs/7zip/` — bundled archive dependencies
- `tests/` — plugin integration tests

## Versions and automatic releases

The current application and package version is stored in the root [`VERSION.txt`](VERSION.txt) file. Absand uses semantic versions in `MAJOR.MINOR.PATCH` form, such as `1.2.3`. CMake reads this file for the application version and Linux package metadata.

Pushing a matching tag such as `v1.2.3` starts the GitHub Actions release workflow. It builds and tests both platforms, produces these installers, generates SHA-256 checksums, and publishes them on the GitHub Releases page:

```text
Absand-1.2.3-linux-amd64.deb
Absand-1.2.3-windows-x64-setup.exe
SHA256SUMS.txt
```

To publish a new release, replace `1.0.1` below with the version you want and run the complete block from the repository root:

```bash
set -e

./scripts/set-version.sh 1.0.1

git add VERSION.txt CMakeLists.txt src/main.cpp README.md \
  .github/workflows/release.yml packaging scripts
git commit -m "chore: release Absand 1.0.1"
git tag -a v1.0.1 -m "Absand 1.0.1"

git push origin main
git push origin v1.0.1
```

The tag must exactly equal `v` followed by the content of `VERSION.txt`; a mismatch intentionally fails the release. You can test installer generation without publishing a release by opening **Actions → Build installers and publish release → Run workflow** on GitHub. Manually triggered runs keep both installers as workflow artifacts.

The generated Windows installer is not code-signed. Windows SmartScreen may therefore warn users until you add a trusted code-signing certificate to the release workflow.

## Linux: build, run, and deploy

The commands below target Ubuntu/Debian and use the distribution's Qt libraries. Open a terminal in the Absand repository, then copy and paste the complete block.

### Install dependencies, build, test, and install system-wide

```bash
set -e

sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  dpkg-dev \
  qt6-base-dev \
  qt6-tools-dev-tools \
  libzip-dev \
  libarchive-dev \
  curl \
  desktop-file-utils

cmake -S . -B build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local

cmake --build build-release --parallel "$(nproc)"
ctest --test-dir build-release --output-on-failure

sudo cmake --install build-release
cp build-release/install_manifest.txt build-release/install_manifest.system.txt
sudo ldconfig
sudo update-desktop-database /usr/local/share/applications

if command -v kbuildsycoca6 >/dev/null 2>&1; then
  kbuildsycoca6
fi

/usr/local/bin/Absand
```

The first CMake configure may download bit7z's pinned CPM bootstrap dependency. After installation, Absand is available from the desktop application menu, the KDE/Dolphin service menu, and `/usr/local/bin/Absand`.

To archive files immediately from the command line:

```bash
/usr/local/bin/Absand "$HOME/Documents/file-one.pdf" "$HOME/Documents/folder-to-archive"
```

### Create and install a Debian package locally

After installing the dependencies above, this complete block builds the same `.deb` format as GitHub Actions and installs it with APT:

```bash
set -e

version="$(tr -d '[:space:]' < VERSION.txt)"
rm -rf build-deb release-artifacts

cmake -S . -B build-deb -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-deb --parallel "$(nproc)"
ctest --test-dir build-deb --output-on-failure

mkdir -p release-artifacts
cpack --config build-deb/CPackConfig.cmake -G DEB -B release-artifacts
package="$(find release-artifacts -maxdepth 1 -type f -name '*.deb' -print -quit)"
output="release-artifacts/Absand-${version}-linux-amd64.deb"
mv "$package" "$output"

sudo apt install -y "./$output"
Absand
```

Remove the packaged installation later with:

```bash
sudo apt remove absand
```

### Upgrade an existing installation

From the updated Absand repository:

```bash
set -e

cmake -S . -B build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local

cmake --build build-release --parallel "$(nproc)"
ctest --test-dir build-release --output-on-failure
sudo cmake --install build-release
cp build-release/install_manifest.txt build-release/install_manifest.system.txt
sudo ldconfig
sudo update-desktop-database /usr/local/share/applications

if command -v kbuildsycoca6 >/dev/null 2>&1; then
  kbuildsycoca6
fi
```

### Create a package staging directory

This produces a clean filesystem tree suitable for inspection or use by a Debian/RPM/AppImage packaging tool. It is not a dependency-free portable bundle.

```bash
set -e

rm -rf "$PWD/AppDir"
cmake --install build-release --prefix "$PWD/AppDir/usr"

find "$PWD/AppDir" -type f -print | sort
tar -C "$PWD/AppDir" -czf "$PWD/Absand-1.0.0-linux-system-qt.tar.gz" .

echo "Created: $PWD/Absand-1.0.0-linux-system-qt.tar.gz"
```

Test the staged application on the build machine with:

```bash
"$PWD/AppDir/usr/bin/Absand"
```

### Uninstall the `/usr/local` installation

Run this from the same repository. The installation commands above preserve the system-install manifest as `build-release/install_manifest.system.txt`, so creating a staging package later cannot overwrite it.

```bash
set -e

test -f build-release/install_manifest.system.txt

while IFS= read -r installed_file; do
  sudo rm -v -- "$installed_file"
done < build-release/install_manifest.system.txt

sudo rmdir --ignore-fail-on-non-empty \
  /usr/local/bin/licenses \
  /usr/local/share/absand/translations \
  /usr/local/share/absand \
  /usr/local/share/doc/absand/licenses \
  /usr/local/share/doc/absand \
  /usr/local/bin/plugins/archive \
  /usr/local/bin/plugins/destination \
  /usr/local/bin/plugins/send \
  /usr/local/bin/plugins \
  /usr/local/share/kio/servicemenus \
  /usr/local/share/kio \
  /usr/local/share/applications 2>/dev/null || true

sudo ldconfig
sudo update-desktop-database /usr/local/share/applications 2>/dev/null || true

if command -v kbuildsycoca6 >/dev/null 2>&1; then
  kbuildsycoca6
fi
```

## Windows: build, run, and deploy

The simplest supported toolchain is 64-bit MinGW because the bundled 7-Zip runtime is built with its GNU makefile. Keep Qt, libzip, and the compiler on the same architecture and ABI. The following example uses an MSYS2 UCRT64 shell:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-tools \
  mingw-w64-ucrt-x86_64-libzip mingw-w64-ucrt-x86_64-libarchive \
  mingw-w64-ucrt-x86_64-curl pkgconf make

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Run from the UCRT64 shell with:

```bash
./build/Absand.exe
```

To stage a distributable directory:

```bash
cmake --install build --prefix "$PWD/deploy"
bash packaging/windows/bundle-runtime.sh "$PWD/deploy/bin"
```

The bundling script runs `windeployqt`, copies curl and its transitive MSYS2 DLL dependencies, and adds the CA certificate bundle. Confirm that these remain present:

- `deploy/bin/Absand.exe`
- `deploy/bin/7z.dll`
- `deploy/bin/plugins/archive/*.dll`, `plugins/destination/*.dll`, and `plugins/send/*.dll`
- `deploy/bin/translations/absand_sl_SI.qm`

Run `deploy/bin/Absand.exe` outside MSYS2 as the final smoke test. The generated Inno Setup installer registers the “Send with Absand” Explorer context menu for the current user and removes those entries during uninstall.

The Windows build is a windows-subsystem (GUI) executable, so starting it never opens a console window. Because there is therefore no console to print to, Absand appends its diagnostics to `%LOCALAPPDATA%\Absand\logs\absand.log` (rotated at 1 MB); attach that file when reporting a problem.

## Updating translations

After changing user-visible strings, refresh the catalogs with Qt Linguist and review the Slovenian translations before committing:

```bash
lupdate src plugins -no-obsolete \
  -ts translations/absand_en_US.ts translations/absand_sl_SI.ts
linguist translations/absand_sl_SI.ts
```

CMake compiles both catalogs to `.qm` files automatically.

## License

Absand-owned source code is licensed under the [Mozilla Public License 2.0](LICENSE).

Third-party components retain their original licenses and are not relicensed under MPL-2.0. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the dependency inventory, attribution, source locations, linking model, and redistribution obligations. Maintainers producing binaries should complete the [release license checklist](docs/RELEASE_LICENSE_CHECKLIST.md).

The build and install steps place the available project and bundled-dependency license texts in a `licenses/` directory beside the executable. Platform packagers must additionally include the license bundle and corresponding-source information for the exact Qt, curl, and system-library binaries they choose to distribute.