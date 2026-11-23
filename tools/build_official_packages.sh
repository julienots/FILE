#!/usr/bin/env bash
# Builds the official packages (packages/*) into editor/resources/packages: one .agrpkg per
# package plus index.json, shipped with the editor (Packages window > Online > Official library).
#   tools/build_official_packages.sh [path/to/agr_editor]
set -euo pipefail
cd "$(dirname "$0")/.."
EDITOR="${1:-build/release/bin/agr_editor}"
OUT=editor/resources/packages
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
rm -rf "$OUT" && mkdir -p "$OUT"
for dir in packages/*/; do
    name=$(basename "$dir")
    [ -f "$dir/package.json" ] || continue
    meta() { python3 -c "import json,sys; d=json.load(open('$dir/package.json')); v=d.get('$1',''); print(','.join(v) if isinstance(v,list) else v)"; }
    proj="$TMP/$name"
    "$EDITOR" --create-project "$proj" --template empty3d >/dev/null
    cp -r "$dir/assets/." "$proj/assets/"
    includes=$(cd "$dir/assets" && find . -mindepth 2 -maxdepth 2 -type d | sed 's|^\./||' | paste -sd, -)
    "$EDITOR" --project "$proj" --create-package "$(meta name)" --version "$(meta version)" --include "$includes" \
        --depends "$(meta dependencies)" --description "$(meta description)" --author "$(meta author)" \
        --output "$OUT/$(meta name)-$(meta version).agrpkg" | grep -v '^$' | tail -1
done
"$EDITOR" --package-index "$OUT" --name "Paquets officiels AGR ENGINE" | tail -1
