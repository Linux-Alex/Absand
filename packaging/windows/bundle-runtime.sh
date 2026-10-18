#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 STAGED_BIN_DIRECTORY" >&2
    exit 2
fi

bin_dir=$(cd "$1" && pwd)
app="$bin_dir/Absand.exe"

if [[ ! -f "$app" ]]; then
    echo "Missing staged executable: $app" >&2
    exit 1
fi

# --qmldir makes windeployqt scan the QML sources so the Qt Quick modules the
# Kirigami front end imports are deployed too.
qml_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../src/qml" && pwd)"
windeployqt.exe --release --compiler-runtime --qmldir "$qml_dir" --dir "$bin_dir" "$app"

# Absand invokes curl at runtime for network destinations and password channels.
cp -f /ucrt64/bin/curl.exe "$bin_dir/curl.exe"

# The Kirigami front end imports org.kde.kirigami at runtime; ship those QML
# modules next to the Qt ones that windeployqt copied.
for _kiri_qml in /ucrt64/share/qt6/qml/org/kde /ucrt64/lib/qt6/qml/org/kde; do
    if [[ -d "$_kiri_qml" ]]; then
        mkdir -p "$bin_dir/qml/org"
        cp -ru "$_kiri_qml" "$bin_dir/qml/org/"
        break
    fi
done

# Copy the complete transitive UCRT64 dependency set for the executable, Qt,
# plugins, 7-Zip runtime, libzip, and curl. Repeat because copied DLLs can add
# another dependency layer.
for _pass in 1 2 3 4 5; do
    while IFS= read -r -d '' binary; do
        while IFS= read -r dependency; do
            [[ -f "$dependency" ]] || continue
            cp -u "$dependency" "$bin_dir/$(basename "$dependency")"
        done < <(ldd "$binary" 2>/dev/null | awk '$3 ~ /^\/ucrt64\/bin\// { print $3 }')
    done < <(find "$bin_dir" -type f \( -iname '*.exe' -o -iname '*.dll' \) -print0)
done

certificate_bundle=$(find /ucrt64/etc -type f \
    \( -name 'ca-bundle.crt' -o -name 'ca-certificates.crt' \) -print -quit 2>/dev/null || true)
if [[ -n "$certificate_bundle" ]]; then
    cp -f "$certificate_bundle" "$bin_dir/curl-ca-bundle.crt"
fi

if find "$bin_dir" -type f \( -iname '*.exe' -o -iname '*.dll' \) -print0 \
    | xargs -0 -n1 ldd 2>/dev/null | grep -F 'not found'; then
    echo "One or more Windows runtime dependencies are missing." >&2
    exit 1
fi

echo "Windows runtime bundle prepared in $bin_dir"