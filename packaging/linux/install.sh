#!/usr/bin/env bash
# Builds ProjectManga and installs it for the current user, without sudo:
#   ~/.local/bin/projectmanga                                          the app
#   ~/.local/share/icons/hicolor/scalable/apps/<app id>.svg            its icon
#   ~/.local/share/applications/<app id>.desktop                       its launcher entry
# PREFIX=/some/folder packaging/linux/install.sh installs under that folder instead of ~/.local.
# The app needs Qt 6 and OpenSSL (for HTTPS) from the system at run time.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
prefix="${PREFIX:-$HOME/.local}"
id="io.github.chandandasgit.ProjectManga"
build="$root/build-release"

echo "Building ProjectManga (Release) in $build ..."
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$build" --target appProjectManga

echo "Installing under $prefix ..."
install -Dm755 "$build/appProjectManga" "$prefix/bin/projectmanga"
install -Dm644 "$root/resources/icons/icon.svg" "$prefix/share/icons/hicolor/scalable/apps/$id.svg"
# The launcher runs the installed app by its full path, so ~/.local/bin need not be on PATH.
entry="$(mktemp)"
trap 'rm -f "$entry"' EXIT
sed "s|^Exec=.*|Exec=$prefix/bin/projectmanga|" "$here/$id.desktop" > "$entry"
install -Dm644 "$entry" "$prefix/share/applications/$id.desktop"

if command -v update-desktop-database > /dev/null; then
    update-desktop-database -q "$prefix/share/applications" || true
fi
if command -v gtk-update-icon-cache > /dev/null; then
    gtk-update-icon-cache -q -t "$prefix/share/icons/hicolor" || true
fi

echo "Installed. Start it from your launcher (ProjectManga) or run $prefix/bin/projectmanga"
