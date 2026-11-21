#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Bryan Lunduke
# Cross-build Lunduke Civ for Windows x86_64 from Debian/Ubuntu.
#
#   packaging/windows/build-windows.sh [WORK_DIR]       (default: ./build-windows)
#
# Host packages: mingw-w64 (posix threads), meson, ninja-build, pkg-config,
# python3, gpg, zstd, zip, and wine for the tests. Ubuntu's wine64 package
# does not ship the wine command; the wine package does.
#
# Steps: build a signed MSYS2 sysroot (or reuse one named by SYSROOT=...),
# configure meson with the repo cross file plus a generated one for the
# sysroot, compile, run the test suite under Wine (LUNDUKE_WIN_TESTS=0 skips
# it), then package the zip.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
src=$(cd "$here/../.." && pwd)
work=$(mkdir -p "${1:-build-windows}" && cd "${1:-build-windows}" && pwd)
jobs=${JOBS:-4}

if ! command -v wine >/dev/null 2>&1 || ! command -v wineboot >/dev/null 2>&1 || ! command -v wineserver >/dev/null 2>&1; then
  echo "packaging/windows/build-windows.sh: wine, wineboot, and wineserver are required." >&2
  echo "On Ubuntu, install the wine package. wine64 alone does not provide the wine command." >&2
  exit 1
fi

if [ -n "${SYSROOT:-}" ]; then
  sysroot=$(cd "$SYSROOT" && pwd)
else
  python3 "$here/mingw64-sysroot.py" "$work/msys2" "$work/sysroot"
  sysroot=$work/sysroot
fi

cat > "$work/wine-wrap.sh" <<WRAP
#!/bin/sh
export WINEPREFIX="\${WINEPREFIX:-$work/wineprefix}" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
export WINEPATH="$sysroot/mingw64/bin"
exec wine "\$@"
WRAP
chmod +x "$work/wine-wrap.sh"

cat > "$work/sysroot.ini" <<INI
[binaries]
c = 'x86_64-w64-mingw32-gcc-posix'
cpp = 'x86_64-w64-mingw32-g++-posix'
pkg-config = 'pkg-config'
exe_wrapper = ['$work/wine-wrap.sh']

[properties]
sys_root = '$sysroot'
pkg_config_libdir = ['$sysroot/mingw64/lib/pkgconfig', '$sysroot/mingw64/share/pkgconfig']
INI

if [ ! -f "$work/build/build.ninja" ]; then
  meson setup "$work/build" "$src" --buildtype=release \
    --cross-file "$src/cross/mingw-w64-x86_64.txt" --cross-file "$work/sysroot.ini"
fi
ninja -C "$work/build" -j "$jobs"

if [ "${LUNDUKE_WIN_TESTS:-1}" = 1 ]; then
  WINEPREFIX="$work/wineprefix" WINEDEBUG=-all wineboot -i >/dev/null 2>&1 || true
  WINEPREFIX="$work/wineprefix" wineserver -w || true
  meson test -C "$work/build" --num-processes 2 -t 3 --print-errorlogs
fi

sh "$here/package.sh" "$work/build" "$sysroot" "$work/dist"
