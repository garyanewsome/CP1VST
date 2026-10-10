#!/bin/bash
# Builds the CP1 macOS installer disk image:
#   dist/CP1-<version>-macOS.dmg
#     Install CP1.pkg     (system-wide VST3 + AU + standalone app)
#     Uninstall CP1.app   (double-click uninstaller)
#     READ ME FIRST.txt
#
# Usage: installer/build_installer.sh [path/to/Release/artefacts]
#   default artefacts: cmake-build-cp1/CP1_artefacts/Release
#
# Optional signing (needs an Apple Developer ID; unset = unsigned build):
#   APP_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)"
#   INSTALLER_SIGN_IDENTITY="Developer ID Installer: Name (TEAMID)"
#   NOTARY_PROFILE="name of a notarytool keychain profile"
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
ART="${1:-$ROOT/cmake-build-cp1/CP1_artefacts/Release}"
VERSION="$(sed -n 's/^project(CP1 VERSION \([0-9.]*\)).*/\1/p' "$ROOT/CMakeLists.txt")"
OUT="$ROOT/dist"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PKG_ID="com.garyanewsome.CP1.pkg"

for b in "VST3/CP1.vst3" "AU/CP1.component" "Standalone/CP1.app"; do
  [ -d "$ART/$b" ] || { echo "missing build artefact: $ART/$b" >&2; exit 1; }
done
echo "Building CP1 $VERSION installer from $ART"

# --- payload -----------------------------------------------------------
PAYLOAD="$WORK/payload"
mkdir -p "$PAYLOAD/Library/Audio/Plug-Ins/VST3" \
         "$PAYLOAD/Library/Audio/Plug-Ins/Components" \
         "$PAYLOAD/Applications"
ditto "$ART/VST3/CP1.vst3"          "$PAYLOAD/Library/Audio/Plug-Ins/VST3/CP1.vst3"
ditto "$ART/AU/CP1.component"       "$PAYLOAD/Library/Audio/Plug-Ins/Components/CP1.component"
ditto "$ART/Standalone/CP1.app"     "$PAYLOAD/Applications/CP1.app"

# Strip extended attributes so no "._" AppleDouble files end up in the package.
xattr -cr "$PAYLOAD"
export COPYFILE_DISABLE=1

if [ -n "${APP_SIGN_IDENTITY:-}" ]; then
  for b in "$PAYLOAD/Library/Audio/Plug-Ins/VST3/CP1.vst3" \
           "$PAYLOAD/Library/Audio/Plug-Ins/Components/CP1.component" \
           "$PAYLOAD/Applications/CP1.app"; do
    codesign --force --deep --options runtime --timestamp --sign "$APP_SIGN_IDENTITY" "$b"
  done
fi

# No Developer ID? Still ad-hoc sign every bundle (Apple Silicon refuses to
# run unsigned code, and a sealed bundle is what Gatekeeper's prompts expect).
if [ -z "${APP_SIGN_IDENTITY:-}" ]; then
  for b in "$PAYLOAD/Library/Audio/Plug-Ins/VST3/CP1.vst3" \
           "$PAYLOAD/Library/Audio/Plug-Ins/Components/CP1.component" \
           "$PAYLOAD/Applications/CP1.app"; do
    codesign --force --deep --sign - "$b"
  done
fi

# --- component package -------------------------------------------------
# Never let the installer "relocate" the bundles to some other copy on disk.
pkgbuild --analyze --root "$PAYLOAD" "$WORK/components.plist" >/dev/null
/usr/bin/python3 - "$WORK/components.plist" <<'PY'
import plistlib, sys
p = sys.argv[1]
d = plistlib.load(open(p, "rb"))
for e in d:
    e["BundleIsRelocatable"] = False
    e["BundleOverwriteAction"] = "upgrade"
plistlib.dump(d, open(p, "wb"))
PY

pkgbuild --root "$PAYLOAD" \
         --component-plist "$WORK/components.plist" \
         --identifier "$PKG_ID" \
         --version "$VERSION" \
         --install-location "/" \
         --ownership recommended \
         --scripts "$HERE/scripts" \
         "$WORK/CP1-component.pkg" >/dev/null

# --- product archive (the thing people double-click) -------------------
cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>CP1</title>
  <welcome file="welcome.txt"/>
  <conclusion file="conclusion.txt"/>
  <options customize="never" require-scripts="false" hostArchitectures="x86_64,arm64"/>
  <domains enable_localSystem="true"/>
  <volume-check>
    <allowed-os-versions><os-version min="14.0"/></allowed-os-versions>
  </volume-check>
  <choices-outline><line choice="default"><line choice="$PKG_ID"/></line></choices-outline>
  <choice id="default"/>
  <choice id="$PKG_ID" visible="false"><pkg-ref id="$PKG_ID"/></choice>
  <pkg-ref id="$PKG_ID" version="$VERSION" onConclusion="none">CP1-component.pkg</pkg-ref>
</installer-gui-script>
XML

STAGE="$WORK/dmg"
mkdir -p "$STAGE"
productbuild --distribution "$WORK/distribution.xml" \
             --resources "$HERE/resources" \
             --package-path "$WORK" \
             "$WORK/unsigned.pkg" >/dev/null

if [ -n "${INSTALLER_SIGN_IDENTITY:-}" ]; then
  productsign --sign "$INSTALLER_SIGN_IDENTITY" "$WORK/unsigned.pkg" "$STAGE/Install CP1.pkg"
else
  cp "$WORK/unsigned.pkg" "$STAGE/Install CP1.pkg"
fi

# --- uninstaller app ---------------------------------------------------
osacompile -o "$STAGE/Uninstall CP1.app" "$HERE/Uninstall.applescript"
cp "$HERE/uninstall.sh" "$STAGE/Uninstall CP1.app/Contents/Resources/uninstall.sh"
chmod +x "$STAGE/Uninstall CP1.app/Contents/Resources/uninstall.sh"
if [ -n "${APP_SIGN_IDENTITY:-}" ]; then
  codesign --force --deep --options runtime --timestamp --sign "$APP_SIGN_IDENTITY" "$STAGE/Uninstall CP1.app"
fi

cp "$HERE/resources/README-FIRST.txt" "$STAGE/READ ME FIRST.txt"

# --- disk image --------------------------------------------------------
mkdir -p "$OUT"
DMG="$OUT/CP1-$VERSION-macOS.dmg"
rm -f "$DMG"
hdiutil create -quiet -volname "CP1 $VERSION" -srcfolder "$STAGE" -format UDZO -ov "$DMG"

if [ -n "${NOTARY_PROFILE:-}" ]; then
  xcrun notarytool submit "$DMG" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$DMG"
fi

echo "Built: $DMG"
