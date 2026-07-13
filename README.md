# Absand

Absand is a cross-platform Qt desktop application for creating encrypted ZIP and 7z archives, transferring them to configurable destinations, and delivering the archive password through a separate channel.

## Features

- ZIP and 7z archive creation, browsing, and optional encryption
- local-folder, network-share, FTP, and SFTP destination plugins
- password delivery through clipboard, Telegram, Matrix, or Microsoft Teams
- configurable credentials, recipients, and message templates for every delivery plugin
- English and Slovenian UI with a live language selector
- Windows Explorer and Linux/KDE file-manager integration

Passwords are sent only when archive creation succeeds and encryption is enabled. Network delivery uses the `curl` executable. Absand stores plugin settings as JSON in the current user's application-config directory; bot tokens, access tokens, FTP passwords, and webhook URLs are therefore not encrypted at rest. Protect that account and config directory, and use test credentials while developing.

## Password delivery setup

Select a channel beside **Send password via**, then choose **Configure channels...**.

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

## Repository layout

- `src/` — application, Widgets UI, models, interfaces, and core helpers
- `plugins/archive/` — ZIP and 7z plugins
- `plugins/destination/` — archive destination plugins
- `plugins/send/` — clipboard, Telegram, Matrix, and Teams plugins
- `translations/` — English and Slovenian Qt Linguist catalogs
- `libs/bit7z/` and `libs/7zip/` — bundled archive dependencies
- `tests/` — plugin integration tests

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
  /usr/local/bin/translations \
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
/ucrt64/bin/windeployqt.exe --release --compiler-runtime deploy/bin/Absand.exe
cp /ucrt64/bin/libzip.dll deploy/bin/
```

Use `ldd deploy/bin/Absand.exe` and `ldd deploy/bin/plugins/*/*.dll` from the same shell to identify any remaining MSYS2 runtime DLLs (commonly compression, TLS, or curl dependencies) and copy them into `deploy/bin`. Confirm that these remain present:

- `deploy/bin/Absand.exe`
- `deploy/bin/7z.dll`
- `deploy/bin/plugins/archive/*.dll`, `plugins/destination/*.dll`, and `plugins/send/*.dll`
- `deploy/bin/translations/absand_sl_SI.qm`

Run `deploy/bin/Absand.exe` outside MSYS2 as the final smoke test. CMake installation registers the “Send with Absand” Explorer context menu for the current user. An installer can package the whole `deploy/bin` tree; it should remove those registry entries during uninstall.

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
