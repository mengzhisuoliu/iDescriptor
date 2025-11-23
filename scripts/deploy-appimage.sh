#!/bin/bash
# if you get errors try
# QMAKE=/usr/lib/qt6/bin/qmake  NO_STRIP=1 ./scripts/deploy-appimage.sh v1.0.0
# or even more explicit
#export QT_HOME=~/Qt/$YOUR_QT_VERSION/gcc_64
#export PATH="$QT_HOME/bin:$PATH"
#export LD_LIBRARY_PATH="$QT_HOME/lib"
#export QML2_IMPORT_PATH="$QT_HOME/qml"
#export QT_PLUGIN_PATH="$QT_HOME/plugins"
#QMAKE="$QT_HOME/bin/qmake6" ./scripts/deploy-appimage.sh v0.1.0

set -e
VERSION=$1
if [ -z "$VERSION" ]; then
    echo "No version specified"
    exit 1
fi

export VERSION=$VERSION
export APPDIR=$PWD/AppDir
export GSTREAMER_VERSION=1.0

# Download linuxdeploy and linuxdeploy-plugin-qt if not already present
if [ ! -f linuxdeploy-x86_64.AppImage ]; then
    wget -c -nv "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod a+x linuxdeploy-x86_64.AppImage 
fi

if [ ! -f linuxdeploy-plugin-qt-x86_64.AppImage ]; then
    wget -c -nv "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod a+x linuxdeploy-plugin-qt-x86_64.AppImage 
fi

# Ensure patchelf is installed
if ! command -v patchelf &> /dev/null; then
    echo "ERROR: patchelf not found. Please install it with: sudo apt install patchelf"
    exit 1
fi

# Prepare AppDir structure
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# Copy executable and icon
cp build/iDescriptor "$APPDIR/usr/bin/"
cp resources/icons/app-icon/icon.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/iDescriptor.png"


# Copy ifuse
cp /usr/local/bin/ifuse "$APPDIR/usr/bin"

# Copy iproxy
cp /usr/local/bin/iproxy "$APPDIR/usr/bin"

# Bundle GStreamer plugins and helpers
plugins_target_dir="$APPDIR/usr/lib/gstreamer-$GSTREAMER_VERSION"

# Detect plugin dirs based on architecture
if [ -d /usr/lib/$(uname -m)-linux-gnu/gstreamer-$GSTREAMER_VERSION ]; then
    plugins_dir="/usr/lib/$(uname -m)-linux-gnu/gstreamer-$GSTREAMER_VERSION"
else
    plugins_dir="/usr/lib/gstreamer-$GSTREAMER_VERSION"
fi

mkdir -p "$plugins_target_dir"

plugins=(
    "libgstalsa.so"
    "libgstpulse.so"
    "libgstpipewire.so"
    "libgstjack.so"
    "libgstaudioconvert.so"
    "libgstaudioresample.so"
    "libgstvolume.so"
    "libgstlevel.so"
    "libgstcoreelements.so"
    "libgstdecodebin.so"
    "libgstplayback.so"
    "libgstwavparse.so"
    "libgstmpg123.so"
    "libgstvorbis.so"
    "libgstogg.so"
    "libgstopus.so"
    "libgstflac.so"
    "libgstfaad.so"
    "libgstfdkaac.so"
    "libgstmatroska.so" 
    "libgstlibav.so"
    "libgstapp.so"
    "libgstautodetect.so"
    "libgstaudioresample.so"
)

for i in "${plugins[@]}"; do
    plugin_target_path="$plugins_target_dir/$i"
    plugin_path="$plugins_dir/$i"
    if [ -f "$plugin_path" ]; then
        echo "Copying plugin: $i"
        cp "$plugin_path" "$plugins_target_dir"
        echo "Manually setting RPATH for $plugin_target_path"
        patchelf --set-rpath '$ORIGIN/..:$ORIGIN' "$plugin_target_path"
    else
        echo "Warning: Plugin $i not found in $plugins_dir"
    fi
done

# Copy gst-plugin-scanner and gst-ptp-helper by searching for them
scanner_path=$(find /usr/lib -name gst-plugin-scanner 2>/dev/null | head -n 1)
if [ -n "$scanner_path" ] && [ -f "$scanner_path" ]; then
    echo "Copying gst-plugin-scanner from $scanner_path"
    cp "$scanner_path" "$plugins_target_dir/"
else
    echo "Warning: gst-plugin-scanner could not be found on the system."
fi

helper_path=$(find /usr/lib -name gst-ptp-helper 2>/dev/null | head -n 1)
if [ -n "$helper_path" ] && [ -f "$helper_path" ]; then
    echo "Copying gst-ptp-helper from $helper_path"
    cp "$helper_path" "$plugins_target_dir/"
else
    echo "Warning: gst-ptp-helper could not be found on the system."
fi

mkdir -p "$APPDIR/apprun-hooks"

cat <<'EOF' > "$APPDIR/apprun-hooks/linuxdeploy-plugin-env.sh"
#!/bin/bash

export GST_REGISTRY_REUSE_PLUGIN_SCANNER="no"
export GST_PLUGIN_SYSTEM_PATH_1_0="${APPDIR}/usr/lib/gstreamer-1.0"
export GST_PLUGIN_PATH_1_0="${APPDIR}/usr/lib/gstreamer-1.0"

export GST_PLUGIN_SCANNER_1_0="${APPDIR}/usr/lib/gstreamer-1.0/gst-plugin-scanner"
export GST_PTP_HELPER_1_0="${APPDIR}/usr/lib/gstreamer-1.0/gst-ptp-helper"

export IPROXY_BIN_APPIMAGE="${APPDIR}/usr/bin/iproxy"
export IFUSE_BIN_APPIMAGE="${APPDIR}/usr/bin/ifuse"
EOF

chmod +x "$APPDIR/apprun-hooks/linuxdeploy-plugin-env.sh"

# .desktop file
cp iDescriptor.desktop "$APPDIR/usr/share/applications/"

# Manually deploy geoservices plugins (workaround for linuxdeploy-plugin-qt not finding them)
if [ -n "$Qt6_DIR" ] && [ -d "$Qt6_DIR/plugins/geoservices" ]; then
    echo "Manually deploying geoservices plugins from $Qt6_DIR/plugins/geoservices"
    mkdir -p "$APPDIR/usr/plugins/geoservices"
    cp -v "$Qt6_DIR/plugins/geoservices"/*.so "$APPDIR/usr/plugins/geoservices/" || echo "Warning: Could not copy geoservices plugins"

    echo "Setting RPATH for geoservices plugins"
    for plugin in "$APPDIR/usr/plugins/geoservices"/*.so; do
        if [ -f "$plugin" ]; then
            echo "Setting rpath for $plugin"
            patchelf --set-rpath '$ORIGIN/../../lib' "$plugin"
        fi
    done
else
    echo "Warning: Could not find geoservices plugins directory"
    echo "Qt6_DIR=$Qt6_DIR"
    echo "QT_HOME=$QT_HOME"
fi

export LD_LIBRARY_PATH="$APPDIR/usr/local/lib:$LD_LIBRARY_PATH"
export LINUXDEPLOY_EXCLUDED_LIBRARIES="*sql*"
export QML_SOURCES_PATHS="./qml"
export EXTRA_QT_MODULES="geoservices;position"


 ./linuxdeploy-x86_64.AppImage \
            --appdir ./AppDir \
            --desktop-file AppDir/usr/share/applications/iDescriptor.desktop \
            --executable "$APPDIR/usr/lib/gstreamer-1.0/gst-plugin-scanner" \
            --executable "$APPDIR/usr/lib/gstreamer-1.0/gst-ptp-helper" \
            --plugin qt \
            --exclude-library libGL,libGLX,libEGL,libOpenGL,libdrm,libva,libvdpau,libxcb,libxcb-glx,libxcb-dri2,libxcb-dri3,libX11,libXext,libXrandr,libXrender,libXfixes,libXau,libXdmcp,libqsqlmimer,libmysqlclient,libmysqlclient \
            --output appimage

# Find the generated AppImage and rename it
APPIMAGE_FILE=$(find . -maxdepth 1 -name "iDescriptor*.AppImage")
if [ -n "$APPIMAGE_FILE" ]; then
    mv "$APPIMAGE_FILE" "iDescriptor-${VERSION}-Linux_x86_64.AppImage"
    chmod +x "iDescriptor-${VERSION}-Linux_x86_64.AppImage"
    zip -r "iDescriptor-${VERSION}-Linux_x86_64.AppImage.zip" "iDescriptor-${VERSION}-Linux_x86_64.AppImage"
    echo "AppImage created and zipped: iDescriptor-${VERSION}-Linux_x86_64.AppImage.zip"
else
    echo "Error: Could not find generated AppImage file."
    exit 1
fi
