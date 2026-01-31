#!/bin/bash

set -euo pipefail

ARCH="${1:-x86_64}"
VERSION="${2:-dev}"
BUILD_DIR="build"
APP_PATH="${BUILD_DIR}/iDescriptor.app"


echo "Deploying iDescriptor DMG for ${ARCH} architecture (version: ${VERSION})"

# Determine the platform-specific suffix for the DMG name based on architecture
PLATFORM_SUFFIX=""
if [ "${ARCH}" == "x86_64" ]; then
  PLATFORM_SUFFIX="Apple_Intel"
elif [ "${ARCH}" == "arm64" ]; then
  PLATFORM_SUFFIX="Apple_Silicon"
else
  echo "Error: Unsupported architecture '${ARCH}'."
  exit 1
fi

# Ensure the app exists
if [ ! -d "${APP_PATH}" ]; then
  echo "Error: ${APP_PATH} not found."
  exit 1
fi


GST_PLUGIN_DIR="${APP_PATH}/Contents/Frameworks/gstreamer"
mkdir -p "${GST_PLUGIN_DIR}"

PLUGINS=(
  "libgstapp"
  "libgstaudioconvert"
  "libgstaudioresample"
  "libgstautodetect"
  "libgstavi"
  "libgstcoreelements"
  "libgstimagefreeze"
  "libgstjpeg"
  "libgstlevel"
  "libgstlibav"
  "libgstosxaudio"
  "libgstplayback"
  "libgstvideobox"
  "libgstvideofilter"
  "libgstvideoparsersbad"
  "libgstvolume"
  "libgstvideoconvertscale"
  "libgstvideorate"
)

BREW_PREFIX="$(brew --prefix)"

# Copy GStreamer plugins
for plugin in "${PLUGINS[@]}"; do
  cp "${BREW_PREFIX}/lib/gstreamer-1.0/${plugin}.dylib" "${GST_PLUGIN_DIR}/"
done

# Copy gst-plugin-scanner
cp "$(brew --prefix gstreamer)/libexec/gstreamer-1.0/gst-plugin-scanner" "${APP_PATH}/Contents/Frameworks/"

# Bundle libjxl_cms
# For some reason libjxl_cms is not bundled by macdeployqt, so we do it manually
cp "${BREW_PREFIX}/lib/libjxl_cms.0.11.dylib" "${APP_PATH}/Contents/Frameworks/"
install_name_tool -id "@rpath/libjxl_cms.0.11.dylib" "${APP_PATH}/Contents/Frameworks/libjxl_cms.0.11.dylib"

# Add RPATH to main executable
install_name_tool -add_rpath "@executable_path/../Frameworks" "${APP_PATH}/Contents/MacOS/iDescriptor"


# Copy GStreamer + GLib core libraries
GST_LIBS=(
  "libgstreamer-1.0.0.dylib"
  "libgstbase-1.0.0.dylib"
  "libgstaudio-1.0.0.dylib"
  "libgstvideo-1.0.0.dylib"
  "libgstapp-1.0.0.dylib"
  "libgstpbutils-1.0.0.dylib"
  "libgsttag-1.0.0.dylib"
  "libgstriff-1.0.0.dylib"
  "libgstcodecparsers-1.0.0.dylib"
  "libgstcodecs-1.0.0.dylib"
  "libgstrtp-1.0.0.dylib"
  "libgstsdp-1.0.0.dylib"
  "libglib-2.0.0.dylib"
  "libgobject-2.0.0.dylib"
  "libgmodule-2.0.0.dylib"
  "libgio-2.0.0.dylib"
  "libgthread-2.0.0.dylib"
)

FRAMEWORKS_DIR="${APP_PATH}/Contents/Frameworks"

for lib in "${GST_LIBS[@]}"; do
  if [ -f "${BREW_PREFIX}/lib/${lib}" ]; then
    cp "${BREW_PREFIX}/lib/${lib}" "${FRAMEWORKS_DIR}/"
    install_name_tool -id "@rpath/${lib}" "${FRAMEWORKS_DIR}/${lib}"
    echo "Fixed rpath for ${lib}"
  fi
done

# Copy FFmpeg libraries
FFMPEG_LIB_DIR="$(brew --prefix ffmpeg)/lib"
FFMPEG_LIBS=(
  "libavformat"
  "libavcodec"
  "libavutil"
  "libswscale"
  "libavfilter"
)

for lib_base in "${FFMPEG_LIBS[@]}"; do
  # Use find to get the full versioned filename
  lib_path=$(find "${FFMPEG_LIB_DIR}" -name "${lib_base}.*.dylib" -print -quit)
  if [ -f "$lib_path" ]; then
    lib_name=$(basename "$lib_path")
    cp "$lib_path" "${FRAMEWORKS_DIR}/"
    #These maybe unneeded, macdeployqt already does this but just in case
    install_name_tool -id "@rpath/${lib_name}" "${FRAMEWORKS_DIR}/${lib_name}"
    echo "Fixed rpath for ${lib_name}"
  else
    echo "Warning: ${lib_base} library not found in ${FFMPEG_LIB_DIR}"
  fi
done

echo "Bundling iproxy..."
IPROXY_PATH="$(which iproxy)"
if [ -z "${IPROXY_PATH}" ]; then
  echo "Error: iproxy not found in PATH"
  exit 1
fi

cp "${IPROXY_PATH}" "${APP_PATH}/Contents/MacOS/"
chmod +x "${APP_PATH}/Contents/MacOS/iproxy"

macdeployqt "${APP_PATH}" -qmldir=qml -verbose=2

codesign --force --deep -s - "${APP_PATH}"

DMG_NAME="iDescriptor-${VERSION}-${PLATFORM_SUFFIX}.dmg"

create-dmg \
  --volname "iDescriptor" \
  --volicon "resources/icons/app-icon/icon.icns" \
  --window-pos 200 120 \
  --window-size 600 400 \
  --icon-size 100 \
  --icon "iDescriptor.app" 175 190 \
  --hide-extension "iDescriptor.app" \
  --app-drop-link 425 190 \
  "${BUILD_DIR}/${DMG_NAME}" \
  "${APP_PATH}"
