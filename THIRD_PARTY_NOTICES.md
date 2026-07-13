# Third-Party Software Notices

Absand is licensed under the Mozilla Public License 2.0. That license applies only to Absand-owned code. The components listed below remain under their respective licenses and copyright notices.

This inventory describes the default build in this repository. Packagers must review the final binary dependency tree because operating-system packages and Qt deployment tools may add further components.

## Bundled source and runtime components

### bit7z 4.1.0

- Purpose: C++ interface to the 7-Zip runtime
- Source: <https://github.com/rikyoz/bit7z>
- Bundled path: `libs/bit7z/`
- License: Mozilla Public License 2.0
- License text: `libs/bit7z/LICENSE`
- Linking: statically linked into Absand and the SevenZip archive plugin

Recipients of an executable containing bit7z must be informed where the corresponding MPL-covered source can be obtained. The bundled source in this repository provides that source for repository-built releases. Modifications to MPL-covered bit7z files must remain available under MPL-2.0.

### 7-Zip 26.02

- Purpose: archive-format runtime used through bit7z
- Source: <https://www.7-zip.org/>
- Bundled path: `libs/7zip/`
- Distributed runtime: `7z.so` on Linux or `7z.dll` on Windows
- Main license: GNU Lesser General Public License 2.1 or later
- Additional code: BSD-2-Clause, BSD-3-Clause, public-domain code, and RAR decoder code subject to the unRAR restriction
- License summary: `libs/7zip/DOC/License.txt`
- LGPL text: `libs/7zip/DOC/copying.txt`
- unRAR terms: `libs/7zip/DOC/unRarLicense.txt`

The bundled Format7zF runtime includes RAR decoder sources. Those sources may not be used to recreate the proprietary RAR compression algorithm. Redistributed binaries must reproduce the complete 7-Zip license information.

### QArchive 2.2.9

- Purpose: currently vendored but not referenced by Absand's CMake build
- Source: <https://github.com/antony-jr/QArchive>
- Bundled path: `libs/QArchive/`
- License: BSD-3-Clause
- License text: `libs/QArchive/LICENSE`

QArchive is not part of the default executable. Its copyright and license notice must nevertheless remain with redistributed copies of its source tree. It may be removed from a source distribution if the unused vendored directory is intentionally removed from the project.

## External runtime and development dependencies

### Qt 6

- Purpose: application framework and graphical interface
- Modules directly requested by the production build: Core and Widgets; Gui and platform libraries are pulled transitively
- Version used by the documented SDK build: Qt 6.11.1
- Source: <https://code.qt.io/cgit/qt/>
- Licensing information: <https://doc.qt.io/qt-6/licensing.html>
- Open-source license used by typical community builds of these modules: GNU Lesser General Public License 3.0, with Qt and its individual third-party components retaining their own notices

Absand's default build dynamically links Qt. A distributor using Qt under LGPL-3.0 must include the applicable license and copyright texts, provide the complete corresponding source for the exact Qt libraries distributed (including modifications), permit replacement/relinking and reverse engineering for debugging such modifications, and provide any required installation information. A mere link to Qt's website is not a substitute for a distributor-controlled source offer. Commercial Qt users must follow their commercial agreement instead.

Qt deployment tools can copy additional Qt plugins and third-party libraries. Their license files must be collected from the exact Qt package used for each release.

### libzip

- Purpose: ZIP creation and encryption in the ZIP archive plugin
- Source: <https://libzip.org/>
- Resolution: system/pkg-config dependency; version 1.11.4 in the verified development build
- License: BSD-3-Clause
- Reproduced license text: `third_party/licenses/libzip-BSD-3-Clause.txt`

### curl

- Purpose: external command used for FTP, SFTP, Telegram, Matrix and Teams requests
- Source: <https://curl.se/>
- Resolution: executable found on the user's `PATH`; not linked or bundled by the default CMake install
- License: curl license, plus licenses of the libraries used by the particular curl build

If a Windows or portable release bundles curl and its TLS/compression dependencies, the release must add the exact curl build's license bundle and notices.

## Test-only dependency

### libarchive

- Purpose: verification of generated 7z archives in the integration tests
- Source: <https://www.libarchive.org/>
- Resolution: system/pkg-config dependency; version 3.8.5 in the verified development build
- License: New BSD-style license, with some files under other compatible terms
- Distribution: not linked into or installed with the Absand application

## Build-only tooling

The bit7z configure step can download CPM.cmake 0.42.3. CPM.cmake is build tooling and is not installed with Absand. Compilers, CMake, Ninja/GNU Make, pkg-config and Qt Linguist are likewise build tools rather than components of the shipped application.

## No endorsement

Names and trademarks of upstream projects and contributors are used only to identify dependencies. They do not imply endorsement of Absand.
