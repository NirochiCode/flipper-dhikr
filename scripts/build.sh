#!/usr/bin/env bash
# Build flipper_dhikr.fap against one or more SDKs and label each artifact.
#
#   scripts/build.sh              # Momentum (release) + official (release)
#   scripts/build.sh momentum     # only Momentum
#   scripts/build.sh official     # only official
#
# Each SDK is downloaded once into a project-local UFBT_HOME (.ufbt-<name>/),
# so the global ~/.ufbt is never modified. Results:
#   dist/v<app version>/<name>-<sdk version>/flipper_dhikr.fap
# (app version = DHIKR_APP_VERSION in include/dhikr_app.h), so builds of
# different app versions never overwrite each other.
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"

if [[ -x "$ROOT/.venv/bin/ufbt" ]]; then
    UFBT="$ROOT/.venv/bin/ufbt"
elif command -v ufbt >/dev/null 2>&1; then
    UFBT="$(command -v ufbt)"
else
    echo "ufbt not found. Install it with:" >&2
    echo "  python3 -m venv .venv && .venv/bin/pip install ufbt" >&2
    exit 1
fi

APP_VERSION="$(sed -n 's/^#define DHIKR_APP_VERSION *"\(.*\)"/\1/p' "$ROOT/include/dhikr_app.h")"
[[ -n "$APP_VERSION" ]] || { echo "Cannot read DHIKR_APP_VERSION" >&2; exit 1; }

MOMENTUM_INDEX="https://up.momentum-fw.dev/firmware/directory.json"

build_one() {
    local name="$1"
    local home="$ROOT/.ufbt-$name"

    if [[ ! -f "$home/current/ufbt_state.json" ]]; then
        echo "==> Downloading $name SDK (release channel) into $home"
        if [[ "$name" == "momentum" ]]; then
            UFBT_HOME="$home" "$UFBT" update --index-url="$MOMENTUM_INDEX" --channel=release
        else
            UFBT_HOME="$home" "$UFBT" update --channel=release
        fi
    fi

    local version api
    version="$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["version"])' \
        "$home/current/ufbt_state.json")"
    api="$(grep -m1 '^Version,' "$home/current/sdk_headers/f7_sdk/targets/f7/api_symbols.csv" | cut -d, -f3)"

    echo "==> Building against $name $version (API $api)"
    UFBT_HOME="$home" "$UFBT"

    local out="$ROOT/dist/v$APP_VERSION/$name-$version"
    mkdir -p "$out"
    mv "$ROOT/dist/flipper_dhikr.fap" "$out/flipper_dhikr.fap"
    printf 'app_version=%s\nsdk=%s\nversion=%s\napi=%s\nbuilt=%s\n' "$APP_VERSION" "$name" "$version" "$api" \
        "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$out/BUILD_INFO.txt"
    echo "==> $out/flipper_dhikr.fap"
}

targets=("$@")
[[ ${#targets[@]} -eq 0 ]] && targets=(momentum official)
for target in "${targets[@]}"; do
    case "$target" in
        momentum | official) build_one "$target" ;;
        *) echo "Unknown SDK '$target' (use momentum or official)" >&2; exit 2 ;;
    esac
done
