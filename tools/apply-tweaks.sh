#!/usr/bin/env bash
# Usage: tools/apply-tweaks.sh <kernel_root> <defconfig_name>
set -euo pipefail

KROOT="${1:?kernel root}"
DEFCONFIG="${2:?nama defconfig}"
ARCH="${ARCH:-arm64}"
HERE="$(cd "$(dirname "$0")" && pwd)"
FRAG="$HERE/gaming-streaming.frag"
SC="$KROOT/scripts/config"
CFG="$KROOT/arch/$ARCH/configs/$DEFCONFIG"

[ -f "$FRAG" ] || { echo "ERROR: fragment tidak ada: $FRAG" >&2; exit 1; }
[ -f "$CFG" ]  || { echo "ERROR: defconfig tidak ada: $CFG" >&2; exit 1; }
[ -x "$SC" ]   || { echo "ERROR: scripts/config tidak ada di $KROOT" >&2; exit 1; }

while read -r op sym val; do
  case "$op" in ''|\#*) continue ;; esac
  name="${sym#CONFIG_}"
  case "$op" in
    enable)  "$SC" --file "$CFG" --enable  "$name" ;;
    disable) "$SC" --file "$CFG" --disable "$name" ;;
    set)     "$SC" --file "$CFG" --set-val "$name" "$val" ;;
    *) echo "ERROR: op tidak dikenal: $op" >&2; exit 1 ;;
  esac
done < "$FRAG"

fail=0
while read -r op sym val; do
  case "$op" in ''|\#*) continue ;; esac
  full="CONFIG_${sym#CONFIG_}"
  case "$op" in
    enable)  grep -qE "^${full}=(y|m)$" "$CFG" || { echo "GAGAL enable: $full" >&2; fail=1; } ;;
    disable) grep -qx "# ${full} is not set" "$CFG" || { echo "GAGAL disable: $full" >&2; fail=1; } ;;
    set)     grep -qx "${full}=${val}" "$CFG" || { echo "GAGAL set: $full" >&2; fail=1; } ;;
  esac
done < "$FRAG"
[ "$fail" -eq 0 ] || exit 1
echo "OK: tweak diterapkan ke $CFG"
