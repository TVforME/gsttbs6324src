# Makefile for gst-tbs6324 GStreamer plugin
# Alternative build system to meson

# Compiler and flags
CC = gcc
CFLAGS = -Wall -I$(BUILD_DIR) -Wextra -O2 -fPIC -DHAVE_CONFIG_H $(shell pkg-config --cflags gstreamer-1.0 gstreamer-base-1.0)
LDFLAGS = $(shell pkg-config --libs gstreamer-1.0 gstreamer-base-1.0)
INSTALL = install
DESTDIR =

# GStreamer plugin directory
GST_PLUGIN_DIR = $(shell pkg-config --variable=pluginsdir gstreamer-1.0)

# Directories
SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)

# Source files
SOURCES = $(SRC_DIR)/gsttbs6324.c
HEADERS = $(SRC_DIR)/gsttbs6324.h
OBJECTS = $(OBJ_DIR)/gsttbs6324.o

# Plugin name and location
PLUGIN = $(BUILD_DIR)/libgsttbs6324.so

# Version
VERSION = 0.1.0

# Get build date and time
BUILD_DATE := $(shell date +%Y-%m-%d)
BUILD_TIME := $(shell date +%H:%M:%S)

# Default target
all: $(BUILD_DIR)/config.h $(PLUGIN)

# Create build directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Generate config.h
$(BUILD_DIR)/config.h: src/config.h.in | $(BUILD_DIR)
	@echo "Generating config.h..."
	@sed -e "s/@BUILD_DATE@/$(BUILD_DATE)/g" \
	    -e "s/@BUILD_TIME@/$(BUILD_TIME)/g" \
	    -e "s/@VERSION@/$(VERSION)/g" \
	    -e "s/@PACKAGE@/gst-tbs6324/g" \
	    $< > $@

# Build plugin
$(PLUGIN): $(BUILD_DIR) $(OBJECTS)
	@echo "Linking $(PLUGIN)..."
	$(CC) -shared -o $@ $(OBJECTS) $(LDFLAGS)
	@echo "Build complete: $(PLUGIN)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Install plugin
install: $(PLUGIN)
	@echo "Installing $(PLUGIN) to $(GST_PLUGIN_DIR)..."
	$(INSTALL) -d $(DESTDIR)$(GST_PLUGIN_DIR)
	$(INSTALL) -m 644 $(PLUGIN) $(DESTDIR)$(GST_PLUGIN_DIR)/
	@echo "Installation complete"

# Uninstall plugin
uninstall:
	@echo "Removing $(PLUGIN) from $(GST_PLUGIN_DIR)..."
	rm -f $(DESTDIR)$(GST_PLUGIN_DIR)/$(notdir $(PLUGIN))
	@echo "Uninstall complete"

# Clean build files
clean:
	@echo "Cleaning build files..."
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Distclean (remove all generated files)
distclean: clean

# Check dependencies
check-deps:
	@echo "Checking build dependencies..."
	@which gcc > /dev/null || (echo "Error: gcc not found"; exit 1)
	@pkg-config --exists gstreamer-1.0 || (echo "Error: gstreamer-1.0 not found"; exit 1)
	@pkg-config --exists gstreamer-base-1.0 || (echo "Error: gstreamer-base-1.0 not found"; exit 1)
	@echo "All dependencies satisfied"

# Show plugin info
info:
	@echo "Plugin Information:"
	@echo "  Name: gst-tbs6324"
	@echo "  Version: $(VERSION)"
	@echo "  Source: $(SOURCES)"
	@echo "  Plugin: $(PLUGIN)"
	@echo "  Install dir: $(GST_PLUGIN_DIR)"
	@echo ""
	@echo "Build System:"
	@echo "  CC: $(CC)"
	@echo "  CFLAGS: $(CFLAGS)"
	@echo "  LDFLAGS: $(LDFLAGS)"

# Help
help:
	@echo "Available targets:"
	@echo "  all          - Build the plugin (default)"
	@echo "  install      - Install the plugin to system directory"
	@echo "  uninstall    - Remove the plugin from system directory"
	@echo "  clean        - Remove build files"
	@echo "  distclean    - Remove all generated files"
	@echo "  check-deps   - Check if build dependencies are met"
	@echo "  info         - Show plugin and build information"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Installation options:"
	@echo "  sudo make install               - Install to system directory"
	@echo "  make install DESTDIR=/path       - Install to custom directory"

# Test plugin (requires installation)
# Build release version
release: CFLAGS += -O3 -DNDEBUG
release: clean all
	@echo "Release build complete"
	@echo "Stripping debug symbols..."
	@strip --strip-unneeded $(PLUGIN)
	@echo "Strip complete"

.PHONY: all install uninstall clean distclean check-deps info help test debug release
