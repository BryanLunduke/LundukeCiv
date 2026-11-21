#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Bryan Lunduke
# Assemble a self-contained Windows x86_64 folder and zip for Lunduke Civ.
#
#   packaging/windows/package.sh BUILD_DIR SYSROOT OUT_DIR
#
# OUT_DIR/lunduke-civ/
#   bin/lunduke-civ.exe + every DLL it needs (found by walking PE imports)
#   etc/fonts/fonts.conf    fontconfig setup that only uses share/fonts
#   share/fonts/            DejaVu Sans / Sans Mono
#   share/licenses/         licenses of everything shipped
# and OUT_DIR/lunduke-civ-windows-x86_64.zip
set -eu
build=$(cd "$1" && pwd); sysroot=$(cd "$2" && pwd)/mingw64; out=$3
here=$(cd "$(dirname "$0")" && pwd); src=$(cd "$here/../.." && pwd)
objdump=${OBJDUMP:-x86_64-w64-mingw32-objdump}
strip=${STRIP:-x86_64-w64-mingw32-strip}
name=lunduke-civ
root=$out/$name

rm -rf "$root"; mkdir -p "$root/bin" "$root/etc/fonts" "$root/share/fonts" "$root/share/licenses/$name"
cp "$build/src/$name.exe" "$root/bin/"
"$strip" --strip-unneeded "$root/bin/$name.exe"

# DLL closure. Anything not in the sysroot must be a Windows system DLL.
queue="$root/bin/$name.exe"
while [ -n "$queue" ]; do
  next=""
  for f in $queue; do
    for dll in $("$objdump" -p "$f" | sed -n 's/^[[:space:]]*DLL Name: //p' | tr -d '\r'); do
      [ -e "$root/bin/$dll" ] && continue
      if [ -e "$sysroot/bin/$dll" ]; then
        cp "$sysroot/bin/$dll" "$root/bin/"
        next="$next $root/bin/$dll"
      fi
    done
  done
  queue=$next
done
for dll in "$root"/bin/*.dll; do "$strip" --strip-unneeded "$dll"; done

cp "$here/fonts.conf" "$root/etc/fonts/fonts.conf"
for f in DejaVuSans.ttf DejaVuSans-Bold.ttf DejaVuSans-Oblique.ttf DejaVuSans-BoldOblique.ttf \
         DejaVuSansMono.ttf DejaVuSansMono-Bold.ttf; do
  cp "$sysroot/share/fonts/TTF/$f" "$root/share/fonts/"
done

cp "$src/COPYING" "$src/NOTICE" "$src/README.md" "$root/share/licenses/$name/"
for pkgdir in "$sysroot/share/licenses"/*; do
  pkg=$(basename "$pkgdir")
  case $pkg in python-packaging|tzdata|wineditline|vulkan-loader|lzo2) continue ;; esac
  cp -r "$pkgdir" "$root/share/licenses/$pkg"
done
[ -f "$sysroot/../packages.json" ] && cp "$sysroot/../packages.json" "$root/share/licenses/msys2-packages.json"

cat > "$root/README-windows.txt" <<'TXT'
Lunduke Civ for Windows (x86_64)

Run bin\lunduke-civ.exe. Keep the folder together: the program finds its
DLLs in bin\ and its fonts through etc\fonts\fonts.conf.

Saved games go to %APPDATA%\Lunduke\Lunduke Civ.
TXT

(
  cd "$out" && rm -f "$name-windows-x86_64.zip"
  if command -v zip >/dev/null 2>&1; then
    zip -qr9X "$name-windows-x86_64.zip" "$name"
  else
    python3 -m zipfile -c "$name-windows-x86_64.zip" "$name"
  fi
)
echo "packaged $root"
ls -l "$out/$name-windows-x86_64.zip"
