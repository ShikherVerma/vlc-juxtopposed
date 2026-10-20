# Build the player

This repository contains the full modified VLC source. The interface lives in [`modules/gui/qt/`](../modules/gui/qt/). It uses Qt 6 and QML, not a web wrapper.

```sh
git clone https://github.com/ShikherVerma/vlc-juxtopposed.git
cd vlc-juxtopposed
git checkout v1
```

VLC has substantial native dependencies. Follow the included [upstream build notes](../INSTALL) and platform recipes for toolchain setup. First tools/contrib builds can take hours. Use a dedicated build directory and retain contrib checksums and configuration logs.

## Linux

Use a development environment with Qt 6, Qt Quick/QML, Qt SVG, VLC medialibrary, FFmpeg, FreeType, libass, libplacebo and XCB development dependencies. Our build environment uses VideoLAN's Debian unstable image `registry.videolan.org/vlc-debian-unstable:20260611225331` with medialibrary 0.13.2 built statically.

From the source root, with dependencies available:

```sh
meson setup build-linux --buildtype=debugoptimized \
  -Dqt=enabled -Dmedialibrary=enabled \
  -Davcodec=enabled -Davformat=enabled -Dfreetype=enabled \
  -Dlibass=enabled -Dlibplacebo=enabled -Dxcb=enabled
ninja -C build-linux -j8
./build-linux/vlc
```

These are the source-tree build commands, not an AppImage packaging recipe. Desktop GPU/Wayland integration depends on the host environment.

## Windows x64

Use VideoLAN's UCRT cross-build environment on Linux, including `registry.videolan.org/vlc-debian-llvm-ucrt:20260611225331`. The upstream entry point in that environment is:

```sh
export CONFIGFLAGS='--enable-medialibrary'
extras/package/win32/build.sh -a x86_64 -u -r
```

Limit the build environment to 4 CPUs and 8 GiB. See the script's options and [`extras/package/win32/`](../extras/package/win32/) for out-of-tree build and packaging details. Preserve DLL/plugin layout and generate the plugin cache using the exact native Windows payload; the cross-build Wine cache has failed to load an optional plugin in our environment and must not be treated as a complete native cache.

## macOS Apple Silicon

Use an Apple Silicon Mac with Xcode and command-line tools. This fork enables the shared Qt interface on macOS; a Cocoa-only upstream build does not produce the redesign. Our configuration includes:

```sh
export MACOSX_DEPLOYMENT_TARGET=12.0
export VLC_MACOSX_DEPLOYMENT_TARGET=12.0
export VLC_FORCE_KERNELVERSION=19
export VLC_CONTRIB_ISOLATE_NATIVE_MAC=1
export CONTRIBFLAGS='--enable-qt --enable-qtdeclarative --enable-qtsvg'
export VLC_EXTRA_CONTRIBS='.qt .qtdeclarative .qtsvg'
export VLC_CONFIGURE_ARGS='--enable-qt --disable-debug --disable-macosx-ui-shaders --with-macosx-version-min=12.0'
```

Build from a separate directory using [`extras/package/macosx/build.sh`](../extras/package/macosx/build.sh), with `-a aarch64 -j8`; use its source-contrib option `-c` if the exact matching prebuilt contrib is unavailable. Provide a current GNU make with jobserver support and Python 3.10 or newer; the original build used project-local tools rather than replacing system tools. Set `QMAKE6` to the contrib's native `bin/qmake6` when configure cannot discover it.

Normal app assembly must use the included branded plist and Qt-default packaging. Keep the new `firstrunwizard_macos.mm` in the Objective-C++ build. The configured deployment flags must also apply to Objective-C/Objective-C++ objects and linking.

Developer ID signing and Apple notarization require your own credentials. No credentials or maintainer-machine signing scripts are included. Do not change Gatekeeper or remove quarantine to work around a rejected download.

## Source identity

See [SOURCE.md](SOURCE.md) for the upstream base and exact product commit. Platform runtime coverage is tracked separately in [STATUS.md](STATUS.md).
