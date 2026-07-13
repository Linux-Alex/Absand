# Release License Checklist

Use this checklist for every public source or binary release. It is operational guidance, not legal advice.

## Source release

- [ ] Include the root `LICENSE` and `THIRD_PARTY_NOTICES.md` files.
- [ ] Keep the license files inside `libs/bit7z`, `libs/7zip`, and `libs/QArchive` unchanged.
- [ ] Ensure Absand-owned source files identify `MPL-2.0` and third-party files have not been relabeled.
- [ ] Record exact dependency versions in the release notes or a software bill of materials.
- [ ] Remove `libs/QArchive` if it remains unused and is not intended to be distributed.
- [ ] Run a license/SBOM scanner over the release archive and manually review its results.

## Binary release

- [ ] Start with the generated `licenses/` directory beside the executable; do not remove any file from it.
- [ ] Add the full license bundle shipped with the exact Qt SDK/package used to create the release.
- [ ] Host or ship complete corresponding source for the exact LGPL Qt libraries being distributed, including patches. Keep the offer under the distributor's control.
- [ ] Keep Qt dynamically linked and verify that users can replace the Qt shared libraries. If using static Qt, obtain legal review or use an appropriate commercial Qt license.
- [ ] Include installation information needed to run a legitimately relinked/replaced LGPL library build where LGPL-3.0 requires it.
- [ ] Include bit7z source corresponding exactly to the binary and identify where recipients can obtain it.
- [ ] Include 7-Zip's complete license summary, LGPL text and unRAR restriction, and make corresponding source available as required.
- [ ] Include libzip's BSD notice in the installer/application documentation.
- [ ] If curl is bundled, include the license bundle for that exact curl build and all bundled TLS, SSH, compression and internationalized-domain dependencies.
- [ ] Run `ldd` (Linux), `objdump`/`ntldd` (MinGW), or an equivalent dependency inspection tool against the executable and every plugin.
- [ ] Review Qt platform, image-format, TLS and other plugins copied by deployment tools; add their notices and corresponding-source information.
- [ ] Confirm that the installer/EULA does not prohibit reverse engineering when it is necessary to debug modifications to LGPL libraries.

## Suggested release artifacts

- `Absand-<version>-source.tar.xz`
- `Absand-<version>-third-party-sources.tar.xz` or a durable corresponding-source URL
- `Absand-<version>-SBOM.spdx.json`
- platform binary archive or installer containing `licenses/`

Keep source offers and download locations available for the period required by the applicable licenses.
