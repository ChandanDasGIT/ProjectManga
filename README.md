# ProjectManga

A manga and novel reader for Linux, Android and Windows, with a Qt 6 / QML interface and a pure
C++ backend.

- **Discover**: search MangaDex, browse shelves and tags, and open a series' details and chapters
- **Library**: the series you follow, with a badge when new chapters come out (checked daily, and
  at the estimated release time when one can be guessed)
- **Reader**: page-by-page or scrolling, full screen, with a page-curl effect; picks up where you
  left off
- **Novels**: books from Standard Ebooks and Project Gutenberg, read in the app with adjustable
  text settings
- **Add Site**: add any other site by the address of its chapter-list page; the app guesses the
  title, chapters and pages from the HTML
- Optional **sync** of the library and reading places with your own server, or directly between
  devices on the same Wi-Fi

## Sync (optional)

Sync needs a ProjectManga sync server (a separate program you host yourself) and its secret key.
Give both when configuring; without them the app works on its own and sync stays off.

- `PROJECTMANGA_SYNC_SERVER`: the server's address, `https://host`
- `PROJECTMANGA_SYNC_KEY`: the server's secret key

Both are built into the app, never into the source. Don't share your builds (APK, binaries): the
key is inside them. Use the same values for every device you want to keep in step.

## Build requirements

ProjectManga needs **Qt 6.10 or newer** (Quick, Network, Shader Tools, and Image Formats for WebP
covers), CMake 3.21+, Ninja and a C++20 compiler. HTTPS uses the system's OpenSSL.

### Fedora

```sh
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel \
                 qt6-qtshadertools-devel qt6-qtimageformats
```

### Arch Linux

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-shadertools \
                        qt6-imageformats
```

### Ubuntu

Ubuntu's own Qt is too old (Ubuntu 26.04 has Qt 6.9), so install Qt with the
[Qt Online Installer](https://www.qt.io/download-qt-installer-oss) (free Qt account): choose
Qt 6.10 or newer, *Desktop* (gcc 64-bit), and under *Additional Libraries* *Qt Shader Tools* and
*Qt Image Formats*. Then the build tools and the libraries Qt needs:

```sh
sudo apt install build-essential cmake ninja-build libgl1-mesa-dev libxkbcommon-dev \
                 libxcb-cursor0 libssl3
```

and point CMake at that Qt before configuring (adjust the version):

```sh
export CMAKE_PREFIX_PATH=$HOME/Qt/6.11.2/gcc_64
```

### Debian

Debian's own Qt is too old as well (Debian 13 has Qt 6.8). Install Qt with the
[Qt Online Installer](https://www.qt.io/download-qt-installer-oss) exactly as for Ubuntu above,
then:

```sh
sudo apt install build-essential cmake ninja-build libgl1-mesa-dev libxkbcommon-dev \
                 libxcb-cursor0 libssl3t64
```

(on Debian 12 the last package is `libssl3`), and point CMake at that Qt:

```sh
export CMAKE_PREFIX_PATH=$HOME/Qt/6.11.2/gcc_64
```

## Building and installing

### Linux

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DPROJECTMANGA_SYNC_SERVER=https://<your server> -DPROJECTMANGA_SYNC_KEY=<your key>
cmake --build build --target appProjectManga
./build/appProjectManga
```

(Leave out the two sync values for no sync. In fish, leave out the `\` line breaks and write the
values without `<` `>`.)

To install it for your user, with a launcher entry and icon (no root needed), configure
`build-release/` once with your sync values, then run the install script:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DPROJECTMANGA_SYNC_SERVER=https://<your server> -DPROJECTMANGA_SYNC_KEY=<your key>
packaging/linux/install.sh
```

It installs `~/.local/bin/projectmanga`; `packaging/linux/uninstall.sh` removes it again. Your
library stays in `~/.local/share/ProjectManga`. Later installs reuse the values in
`build-release/`.

### Android

Requirements: Qt 6.10+ for *Android arm64-v8a* and *Desktop* from the Qt Online Installer (with
*Qt Shader Tools* and *Qt Image Formats*); the Android SDK (platform 36, build-tools 36) and NDK
(27) from Android Studio's SDK Manager; a JDK (17 or newer). The OpenSSL libraries the APK needs
are already in `third_party/openssl/arm64-v8a`.

1. Configure and build (adjust the paths to where Qt and the Android tools are):

   ```sh
   ~/Qt/6.11.2/android_arm64_v8a/bin/qt-cmake -S . -B build-android -G Ninja \
       -DCMAKE_BUILD_TYPE=Release -DQT_HOST_PATH=$HOME/Qt/6.11.2/gcc_64 \
       -DANDROID_SDK_ROOT=$HOME/Android/Sdk -DANDROID_NDK_ROOT=$HOME/Android/Sdk/ndk/27.2.12479018 \
       -DPROJECTMANGA_SYNC_SERVER=https://<your server> -DPROJECTMANGA_SYNC_KEY=<your key>
   cmake --build build-android --target appProjectManga
   ```

2. Make the APK:

   ```sh
   ~/Qt/6.11.2/gcc_64/bin/androiddeployqt \
       --input build-android/android-appProjectManga-deployment-settings.json \
       --output build-android/android-build --gradle
   ```

   The APK is `build-android/android-build/build/outputs/apk/debug/android-build-debug.apk`.
   It is a debug build, signed with Android's debug key. After rebuilding, copy
   `build-android/libappProjectManga_arm64-v8a.so` into
   `build-android/android-build/libs/arm64-v8a/` before running `androiddeployqt` again, or the
   APK keeps the old copy.

3. Install it: with the phone connected over USB (USB debugging on),
   `adb install -r build-android/android-build/build/outputs/apk/debug/android-build-debug.apk`;
   or copy the APK to the phone, open it and allow installing from that app.

The library lives in the app's private storage. For server sync, the phone must reach your server
(for example Tailscale on the phone).

### Windows

Not tested yet. Install Qt 6.10+ with the Qt Online Installer: *MinGW 64-bit* (with *Qt Shader
Tools* and *Qt Image Formats*), plus *MinGW*, *CMake* and *Ninja* under Developer and Designer
Tools. In the "Qt 6.x (MinGW 64-bit)" command prompt, from the project folder:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DPROJECTMANGA_SYNC_SERVER=https://<your server> -DPROJECTMANGA_SYNC_KEY=<your key>
cmake --build build --target appProjectManga
windeployqt --qmldir . build\appProjectManga.exe
build\appProjectManga.exe
```

`windeployqt` copies the Qt libraries next to the app, so the `build` folder then runs on its own.
HTTPS needs OpenSSL 3 (`libssl-3-x64.dll`, `libcrypto-3-x64.dll`) next to the app or on `PATH`;
the Qt Online Installer offers it under Developer and Designer Tools. The library lives in
`%LOCALAPPDATA%\ProjectManga`.

## Tests

```sh
cmake --build build --target backendtests wrappertests
./build/backendtests            # every backend test; ./build/backendtests library runs one group
./build/wrappertests             # the Qt wrappers against a local test server
```

The tests need no internet and use throwaway folders.

## Layout

The code has three tiers, with the same folder names in each:

- `backend/`: plain C++20, no Qt (sources, library, update checks, sync), built as the
  `mangacore` library
- `wrapper/`: Qt C++ that hands the backend to QML
- `frontend/`: the QML interface

`third_party/openssl/` holds OpenSSL (Apache 2.0) for Android's HTTPS.

## Licence

GPL-3.0; see [LICENSE](LICENSE).
