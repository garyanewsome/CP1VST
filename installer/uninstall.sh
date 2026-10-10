#!/bin/bash
# Removes CP1 from this Mac. Run as root (the Uninstall CP1 app does this).
# PREFIX can be overridden for testing; it defaults to the real filesystem root.
PREFIX="${PREFIX:-}"

# The person logged in at the screen, even when running under admin privileges.
CONSOLE_USER="${CONSOLE_USER:-$(/usr/bin/stat -f%Su /dev/console 2>/dev/null)}"
USER_HOME="${USER_HOME:-$(/usr/bin/dscl . -read "/Users/$CONSOLE_USER" NFSHomeDirectory 2>/dev/null | /usr/bin/awk '{print $2}')}"

targets=(
  "$PREFIX/Library/Audio/Plug-Ins/VST3/CP1.vst3"
  "$PREFIX/Library/Audio/Plug-Ins/Components/CP1.component"
  "$PREFIX/Applications/CP1.app"
)
if [ -n "$USER_HOME" ]; then
  targets+=(
    "$USER_HOME/Library/Audio/Plug-Ins/VST3/CP1.vst3"
    "$USER_HOME/Library/Audio/Plug-Ins/Components/CP1.component"
  )
fi

removed=0
for t in "${targets[@]}"; do
  if [ -e "$t" ]; then
    /bin/rm -rf "$t" && removed=$((removed + 1))
    echo "removed $t"
  fi
done

# Forget the installer receipt so macOS no longer thinks CP1 is installed.
if [ -z "$PREFIX" ]; then
  /usr/sbin/pkgutil --forget com.garyanewsome.CP1.pkg >/dev/null 2>&1 || true
  # Make music apps notice the AU is gone.
  /usr/bin/killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
fi

echo "done ($removed items removed). Saved presets in Application Support were kept."
exit 0
