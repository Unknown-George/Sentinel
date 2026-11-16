#!/bin/bash
set -euo pipefail

# Run after a successful Release build. No signing identity or credentials are stored here.
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${1:-$project_dir/build}"
output_dir="${2:-$project_dir/dist}"
version="${SENTINEL_VERSION:-1.0.0}"
app_source="$build_dir/Sentinel.app"
[[ -d "$app_source" ]] || { echo "Missing $app_source; build Sentinel first." >&2; exit 1; }
lipo "$app_source/Contents/MacOS/Sentinel" -verify_arch arm64
if [[ -n "${NOTARY_PROFILE:-}" && -z "${DEVELOPER_ID:-}" ]]; then
    echo "NOTARY_PROFILE requires DEVELOPER_ID." >&2
    exit 1
fi

if [[ -n "${QT_PREFIX:-}" ]]; then
    qt_prefix="$QT_PREFIX"
elif [[ -n "${QT_ROOT_DIR:-}" ]]; then
    qt_prefix="$QT_ROOT_DIR"
elif command -v brew >/dev/null; then
    qt_prefix="$(brew --prefix qtbase 2>/dev/null || brew --prefix qt)"
else
    echo "Set QT_PREFIX to the Qt installation containing bin/macdeployqt." >&2
    exit 1
fi
deploy_tool="$qt_prefix/bin/macdeployqt"
[[ -x "$deploy_tool" ]] || { echo "macdeployqt not found at $deploy_tool" >&2; exit 1; }

mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
stage="$(mktemp -d "$output_dir/.sentinel-package.XXXXXX")"
trap 'rm -rf "$stage"' EXIT
ditto "$app_source" "$stage/Sentinel.app"
app="$stage/Sentinel.app"
resources="$app/Contents/Resources"
mkdir -p "$resources/licenses"
cp "$project_dir/LICENSE" "$resources/licenses/Sentinel-MIT.txt"
cp "$project_dir/docs/THIRD-PARTY-NOTICES.md" "$resources/licenses/THIRD-PARTY-NOTICES.md"
cp "$project_dir/docs/licenses/"*.txt "$resources/licenses/"
# Retain Qt's component inventory when provided by the installed toolchain.
if [[ -d "$qt_prefix/share/qt/sbom" ]]; then
    ditto "$qt_prefix/share/qt/sbom" "$resources/licenses/qt-sbom"
fi
if [[ -d "$qt_prefix/sbom" ]]; then
    ditto "$qt_prefix/sbom" "$resources/licenses/qt-sbom"
fi
if command -v brew >/dev/null && [[ "$qt_prefix" == "$(brew --prefix qtbase 2>/dev/null || true)" ]]; then
    svg_prefix="$(brew --prefix qtsvg 2>/dev/null || true)"
    if [[ -n "$svg_prefix" && -d "$svg_prefix/share/qt/sbom" ]]; then
        ditto "$svg_prefix/share/qt/sbom" "$resources/licenses/qt-sbom"
    fi
fi

if [[ -n "${DEVELOPER_ID:-}" ]]; then
    "$deploy_tool" "$app" -always-overwrite "-sign-for-notarization=$DEVELOPER_ID"
    signing="Developer ID signed"
else
    "$deploy_tool" "$app" -always-overwrite -no-strip
    # Ad-hoc signatures allow local execution; this is not a trusted distribution identity.
    codesign --force --deep --sign - "$app"
    signing="ad-hoc signed; not notarized"
fi
python3 "$project_dir/scripts/verify_bundle.py" "$app"
codesign --verify --deep --strict "$app"

archive="$output_dir/Sentinel-$version-macOS-arm64.zip"
ditto -c -k --sequesterRsrc --keepParent "$app" "$archive"

if [[ -n "${NOTARY_PROFILE:-}" ]]; then
    xcrun notarytool submit "$archive" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$app"
    xcrun stapler validate "$app"
    ditto -c -k --sequesterRsrc --keepParent "$app" "$archive"
    signing="Developer ID signed and notarized"
fi

if [[ "${CREATE_DMG:-0}" == "1" ]]; then
    ln -s /Applications "$stage/Applications"
    hdiutil create -volname Sentinel -srcfolder "$stage" -ov -format UDZO "$output_dir/Sentinel-$version-macOS-arm64.dmg"
fi
(
    cd "$output_dir"
    shasum -a 256 "Sentinel-$version-macOS-arm64.zip" > "Sentinel-$version-macOS-arm64.zip.sha256"
)
echo "$archive ($signing)"
