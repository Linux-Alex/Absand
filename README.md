# Absand

[![Build installers and publish release](https://github.com/Linux-Alex/Absand/actions/workflows/release.yml/badge.svg)](https://github.com/Linux-Alex/Absand/actions/workflows/release.yml)

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

## Versions and automatic releases

The current application and package version is stored in the root [`VERSION`](VERSION) file. Absand uses semantic versions in `MAJOR.MINOR.PATCH` form, such as `1.2.3`. CMake reads this file for the application version and Linux package metadata.

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

git add VERSION CMakeLists.txt src/main.cpp README.md \
  .github/workflows/release.yml packaging scripts
git commit -m "chore: release Absand 1.0.1"
git tag -a v1.0.1 -m "Absand 1.0.1"

git push origin main
git push origin v1.0.1
```

The tag must exactly equal `v` followed by the content of `VERSION`; a mismatch intentionally fails the release. You can test installer generation without publishing a release by opening **Actions → Build installers and publish release → Run workflow** on GitHub. Manually triggered runs keep both installers as workflow artifacts.

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

version="$(tr -d '[:space:]' < VERSION)"
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
