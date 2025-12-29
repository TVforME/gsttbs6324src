#!/bin/bash
# Strip script to remove debug symbols from release builds

SO_FILE="$1"
if [ -z "$SO_FILE" ]; then
    echo "Usage: $0 <shared_object_file>"
    exit 1
fi

# Get the install directory
INSTALL_DIR="${MESON_INSTALL_DESTDIR_PREFIX}"
if [ -z "$INSTALL_DIR" ]; then
    INSTALL_DIR="$DESTDIR"
fi

# Full path to installed file
FULL_PATH="$INSTALL_DIR/$MESON_INSTALL_DESTDIR_BASEDIR/$SO_FILE"

# Strip the file
if [ -f "$FULL_PATH" ]; then
    echo "Stripping $FULL_PATH..."
    strip --strip-unneeded "$FULL_PATH"
    echo "Stripped successfully"
else
    echo "Warning: $FULL_PATH not found"
fi