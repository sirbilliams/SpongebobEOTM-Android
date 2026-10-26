#!/bin/bash
# Builds the minimal LGPL FFmpeg that plays the game's Bink movies (game/bink), as shared libraries.
#
#   build-ffmpeg.sh <ffmpeg-src> <build-dir> <install-prefix> [android <ndk-toolchain-bin> <api>]
#
# Only the Bink demuxer, the Bink video decoder and the two Bink audio decoders are compiled in
# (--disable-everything). No GPL parts, no external libraries, no assembler (--disable-x86asm).
set -euo pipefail
SRC=$(realpath "$1"); BUILD=$2; PREFIX=$3; TARGET=${4:-host}
mkdir -p "$BUILD" "$PREFIX"; BUILD=$(realpath "$BUILD"); PREFIX=$(realpath "$PREFIX")
[ -f "$PREFIX/.built-$(git -C "$SRC" rev-parse HEAD)-$TARGET" ] && { echo "ffmpeg already built"; exit 0; }

OPTS=(--prefix="$PREFIX" --disable-everything --disable-programs --disable-doc --disable-debug
  --disable-network --disable-autodetect --disable-gpl --disable-nonfree --disable-version3
  --disable-static --enable-shared --disable-x86asm --disable-avdevice --disable-avfilter
  --disable-swscale --disable-swresample --disable-postproc --enable-pic
  --enable-demuxer=bink --enable-decoder=bink --enable-decoder=binkaudio_rdft --enable-decoder=binkaudio_dct
  --enable-protocol=file --enable-small --disable-iconv --disable-zlib --disable-bzlib --disable-lzma)

cd "$BUILD"
if [ "$TARGET" = android ]; then
  TC=$5; API=${6:-24}
  "$SRC/configure" "${OPTS[@]}" --enable-cross-compile --target-os=android --arch=aarch64 --cpu=armv8-a \
    --cc="$TC/aarch64-linux-android$API-clang" --cxx="$TC/aarch64-linux-android$API-clang++" \
    --ar="$TC/llvm-ar" --ranlib="$TC/llvm-ranlib" --strip="$TC/llvm-strip" --nm="$TC/llvm-nm" \
    --extra-cflags="-fPIC" --pkg-config=false
  # Android can't load versioned sonames (libavcodec.so.61) from an APK: plain names instead.
  sed -i 's/^SLIBNAME_WITH_MAJOR=.*/SLIBNAME_WITH_MAJOR=$(SLIBNAME)/;s/^SLIB_INSTALL_NAME=.*/SLIB_INSTALL_NAME=$(SLIBNAME)/;s/^SLIB_INSTALL_LINKS=.*/SLIB_INSTALL_LINKS=/' ffbuild/config.mak
else
  "$SRC/configure" "${OPTS[@]}" --pkg-config=false
fi
make -j2
make install
touch "$PREFIX/.built-$(git -C "$SRC" rev-parse HEAD)-$TARGET"
