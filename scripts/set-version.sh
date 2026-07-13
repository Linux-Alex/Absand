#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

if [[ $# -ne 1 || ! $1 =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Usage: $0 MAJOR.MINOR.PATCH" >&2
    exit 2
fi

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
printf '%s\n' "$1" > "$repo_root/VERSION"
echo "Absand version set to $1"
echo "Release tag must be: v$1"
