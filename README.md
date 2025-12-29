# TBS6324 GStreamer Plugin

A GStreamer source element for the TBS6324 Quad 3G-SDI PCIe Encoder card.

## Overview

The TBS6324 is a hardware encoder that captures 3G-SDI input and outputs MPEG-TS streams. This plugin provides a GStreamer source element (`tbs6324src`) that interfaces with the card's DVB driver API.

**Important Implementation Note**: The TBS6324 card uses a somewhat unconventional driver implementation. It presents itself as a DVB device (using the Linux DVB API), but the card is actually a **hardware encoder**, not a DTV tuner. The driver uses "dummy" DVB5 API calls to configure the hardware encoder's parameters (bitrate, resolution, codec, etc.) via custom IOCTLs (`FE_24CXX_READ/WRITE`). This is a workaround to integrate the card with existing Linux video infrastructure, allowing it to work with standard streaming applications and media servers that expect DVB devices.

The card can also be configured using the official **TBS6324SDITool** application from the manufacturer, which provides a GUI for testing and configuring encoder settings.

## Features

-   **4 Independent Encoders**: Each encoder (0-3) presents as a separate DVB adapter
-   **Hardware H.264/H.265 Encoding**: Up to 1080p@60fps
-   **MPEG-TS Output**: Complete muxed stream including video, audio, and PSI tables
-   **Automatic PID Assignment**: Encoder-specific PIDs to prevent conflicts
-   **Card Detection**: LSPCI-based hardware verification
-   **Firmware Version Reading**: Queries encoder for version information
-   **Build Information**: Embedded build date and time in plugin metadata
-   **DVB API Compatibility**: Uses dummy DVB5 API calls to interface with streaming applications
-   **Standard Device Interface**: Works with ffmpeg, OBS, Wowza, and other streaming software

## Project Structure

```
gsttbs6324/
├── src/                    # Source files
│   ├── gsttbs6324.c       # Main plugin implementation
│   ├── gsttbs6324.h       # Header file
│   └── config.h.in        # Config template (for build date)
├── build/                 # Build output directory (Make)
│   ├── gsttbs6324.o      # Object file
│   ├── config.h          # Generated config with build info
│   └── libgsttbs6324.so  # Compiled plugin
├── docs/                  # Documentation
│   ├── TBS6324SDItool.zip # Configuration tool archive
│   └── TBS6324_User_Manual.pdf # User manual
├── examples/              # Example scripts and programs
│   ├── capture_from_tbs.sh      # Capture to file
│   ├── multi_channel.sh         # Multi-channel capture
│   ├── network_stream.sh       # Network streaming
│   ├── generate_smpte_pattern.sh # SMPTE pattern capture
│   └── test_offsets.c          # Register offset test program
├── Makefile               # GNU Make build system
├── meson.build            # Meson build system
├── strip.sh              # Strip script for release builds
└── README.md             # This file
```

## Building

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential libgstreamer1.0-dev \
    libglib2.0-dev pciutils

# For Meson build (optional)
sudo apt-get install meson ninja-build
```

### Building with Make

Make is the recommended build system as it works out-of-the-box.

#### Default Build (Optimized)

```bash
cd gsttbs6324
make clean && make
```

This creates:

-   `build/libgsttbs6324.so` (36KB with debug symbols)

#### Release Build (Stripped & Optimized)

```bash
make clean && make release
```

This creates:

-   `build/libgsttbs6324.so` (31KB stripped, production-ready)

#### Debug Build

```bash
make clean && make debug
```

This creates:

-   `build/libgsttbs6324.so` (~100KB with full debug symbols)

#### Other Make Targets

```bash
make info        # Show build information
make check-deps  # Check build dependencies
make help        # Show all available targets
```

### Building with Meson

Meson is a modern build system with better cross-platform support.

#### Setup and Build

```bash
cd gsttbs6324
meson setup builddir
meson compile -C builddir
```

#### Build Type Options

```bash
# Debug build (default) - No optimization, full debug symbols
meson setup builddir --buildtype=debug
meson compile -C builddir

# Release build - Optimized, stripped (recommended for production)
meson setup builddir --buildtype=release
meson compile -C builddir

# Minimum size build - Optimized for size
meson setup builddir --buildtype=minsize
meson compile -C builddir

# Debug optimized - Optimized with debug symbols
meson setup builddir --buildtype=debugoptimized
meson compile -C builddir
```

#### Reconfigure Existing Build

```bash
meson configure builddir -Dbuildtype=release
meson compile -C builddir
```

## Installation

### Install from Make Build

```bash
sudo make install
```

### Install from Meson Build

```bash
sudo meson install -C builddir
```

### Uninstall

```bash
# From Make
sudo make uninstall

# From Meson
sudo meson uninstall -C builddir
```

### Installation Directory

Both build systems install to the system GStreamer plugin directory:

```bash
/usr/lib/x86_64-linux-gnu/gstreamer-1.0/
```

To verify installation:

```bash
gst-inspect-1.0 tbs6324src
```

## Build Comparison

| Build System | Debug Size | Release Size | Build Time | Features |
| --- | --- | --- | --- | --- |
| **Make** | ~100KB | 31KB | Fast | Simple, no dependencies |
| **Meson** | ~100KB | 31KB | Moderate | Modern, cross-platform |

**Note**: Release builds automatically strip debug symbols for smaller size.

## Build Information

The plugin includes build date and time information that can be viewed:

```bash
gst-inspect-1.0 tbs6324src
```

Output includes:

```
Description: Captures video from TBS6324 Quad 3G-SDI PCIe Encoder
Built: 2025-12-28 13:58:XX
```

This information is automatically generated during the build process.

## Usage

### Using Example Scripts

The `examples/` directory contains ready-to-use scripts:

#### Capture to File

```bash
cd examples
./capture_from_tbs.sh
```

#### Multi-Channel Capture

```bash
cd examples
./multi_channel.sh
```

#### Network Streaming

```bash
cd examples
# Edit network_stream.sh to set destination IP/port
./network_stream.sh
```

#### Test Register Offsets

```bash
cd examples
gcc -o test_offsets test_offsets.c
./test_offsets 0  # Test encoder 0
```

### Basic Pipeline

Capture encoder 0 to file:

```bash
gst-launch-1.0 tbs6324src encoder=0 ! filesink location=output.ts
```

### Network Streaming

Stream via UDP:

```bash
gst-launch-1.0 tbs6324src encoder=0 ! udpsink host=192.168.1.100 port=5000
```

### Decode and Display

Live preview:

```bash
gst-launch-1.0 tbs6324src encoder=0 ! tsdemux ! h264parse ! \
  avdec_h264 ! videoconvert ! autovideosink
```

### Multi-Channel

Stream all 4 encoders simultaneously:

```bash
# Channel 0
gst-launch-1.0 tbs6324src encoder=0 ! filesink location=ch0.ts &

# Channel 1
gst-launch-1.0 tbs6324src encoder=1 ! filesink location=ch1.ts &

# Channel 2
gst-launch-1.0 tbs6324src encoder=2 ! filesink location=ch2.ts &

# Channel 3
gst-launch-1.0 tbs6324src encoder=3 ! filesink location=ch3.ts &
```

## Properties

### Video Encoding

| Property | Type | Default | Range | Description |
| --- | --- | --- | --- | --- |
| `encoder` | int | 0 | 0-3 | Encoder channel number |
| `encoder-type` | string | "H264" | H264, H265 | Video codec |
| `width` | int | 1920 | 0-1920 | Output width (0 = auto) |
| `height` | int | 1080 | 0-1080 | Output height (0 = auto) |
| `fps` | int | 60 | 5-60 | Frame rate |
| `bitrate` | int | 6000 | 160-20000 | Video bitrate (kbps) |
| `bitrate-mode` | string | "cbr" | cbr, vbr, evbr | Bitrate control mode |
| `gop-interval` | int | 30 | 1-180 | GOP key interval |
| `profile` | string | "main" | baseline, main, high | H.264 profile |

### MPEG-TS

| Property | Type | Default | Range | Description |
| --- | --- | --- | --- | --- |
| `pmt-pid` | int | varies | 16-7936 | PMT PID (auto per encoder) |
| `video-pid` | int | varies | 16-3840 | Video PID (auto per encoder) |
| `audio-pid` | int | varies | 16-3840 | Audio PID (auto per encoder) |
| `service-id` | int | varies | 1-65535 | Service ID (auto per encoder) |
| `program-name` | string | varies | \- | Program name (auto per encoder) |

### Audio

| Property | Type | Default | Range | Description |
| --- | --- | --- | --- | --- |
| `audio-codec` | string | "AAC" | AAC, MP2 | Audio codec |
| `audio-bitrate` | int | 128000 | 24000-256000 | Audio bitrate (bps) |
| `audio-type` | string | "LC-AAC" | LC-AAC, HE-AAC | AAC type |
| `audio-channel` | int | 0 | 0-3 | SDI audio channel pair |
| `audio-resample` | int | 0 | 0, 32000, 44100 | Resample rate (0 = off) |

## Encoder-Specific Defaults

Each encoder automatically receives unique default values:

| Encoder | PMT PID | Video PID | Audio PID | Service ID | Program Name |
| --- | --- | --- | --- | --- | --- |
| 0 | 1024 | 512 | 768 | 256 | TBS-SDI-1 |
| 1 | 1034 | 513 | 769 | 257 | TBS-SDI-2 |
| 2 | 1044 | 514 | 770 | 258 | TBS-SDI-3 |
| 3 | 1054 | 515 | 771 | 259 | TBS-SDI-4 |

## Architecture

### Device Model

The TBS6324 presents itself as DVB devices using a "dummy" DVB5 API implementation. This is an unconventional approach where:

-   **Frontend Device** (`/dev/dvb/adapterN/frontend0`): Used for encoder configuration via custom IOCTLs
    -   **NOT a real DVB tuner** - no DTV/DVB tuning is performed
    -   Uses `FE_24CXX_READ/WRITE` IOCTLs for encoder configuration
    -   Appears as a DVB frontend to maintain compatibility with streaming software
-   **DVR Device** (`/dev/dvb/adapterN/dvr0`): Used for reading the MPEG-TS stream data
    -   Outputs muxed MPEG-TS with video, audio, and PSI tables
    -   Standard DVB DVR interface that works with most streaming applications

**Why This Approach?** This "ugly" workaround allows the TBS6324 to:

-   Integrate with existing Linux media frameworks
-   Work with standard streaming servers (OBS, ffmpeg, Wowza, etc.)
-   Be recognized by video capture applications that expect DVB devices
-   Use standard Linux device permissions (video group)
-   Benefit from existing DVB driver infrastructure

**Alternative Configuration Tool**: The manufacturer provides `TBS6324SDITool`, a GUI application that can configure the encoder outside of this plugin. This tool can be useful for:

-   Testing hardware functionality
-   Verifying encoder settings
-   Troubleshooting driver issues
-   Configuring settings before using with streaming applications

### IOCTL Protocol

The plugin uses custom `FE_24CXX_READ/WRITE` IOCTLs to configure the hardware encoder:

1.  Write command type (SET\_STREAM1 or GET\_EXT)
2.  Write data parameters (bitrate, resolution, PIDs, etc.)
3.  Send command via COMMAND register
4.  Read response (for GET\_EXT)

**Technical Details**:

-   These are custom IOCTLs specific to the TBS6324 driver
-   They are NOT standard DVB API calls
-   The driver implements a "dummy" DVB frontend to expose these IOCTLs
-   This is the only way to configure the hardware encoder's parameters
-   Without these IOCTLs, the encoder cannot be programmed

### Streaming

After configuration, the plugin reads MPEG-TS packets from the DVR device in a loop via the `gst_tbs6324_src_create()` function (inherited from GstPushSrc).

## Troubleshooting

### Card Not Detected

**Error**: `TBS6324 card not found`

**Solution**:

```bash
# Verify hardware detection
lspci -nn | grep 6324

# Should show: [10e6:6324]
```

### Device Access Denied

**Error**: `Could not open frontend/dvr device`

**Solution**:

```bash
# Add user to video group
sudo usermod -aG video $USER

# Or run as root
sudo gst-launch-1.0 tbs6324src ...
```

### No Data from DVR

**Error**: `End of stream` or `Read error`

**Solution**:

-   Verify SDI input is connected and active
-   Check encoder configuration with TBS6324SDITool
-   Ensure proper video signal on SDI input

## Make Targets Reference

```bash
make              # Build plugin (optimized, not stripped)
make clean        # Remove build files
make release      # Build release (optimized + stripped)
make debug        # Build with debug symbols
make install      # Install to system
make uninstall    # Remove from system
make check-deps   # Check dependencies
make info         # Show build information
make help         # Show all targets
```

## Meson Build Reference

```bash
meson setup builddir              # Setup build directory
meson compile -C builddir          # Compile
meson install -C builddir          # Install
meson uninstall -C builddir        # Uninstall
meson configure builddir -Dbuildtype=release  # Reconfigure
```

## Testing with TBS6324SDITool

The manufacturer provides `TBS6324SDITool`, a GUI application for testing and configuring the encoder:

```bash
# Get firmware version
./TBS6324SDITool_x64 -a 0 -v

# Configure encoder
./TBS6324SDITool_x64 -a 0 -c 1920x1080 -b 6000

# Launch GUI (if available)
./TBS6324SDITool_x64
```

This tool can be used to:

-   Verify hardware detection and functionality
-   Test encoder with different settings
-   Read firmware version and card information
-   Configure encoders independently of the GStreamer plugin

**Note**: Settings configured via TBS6324SDITool may persist and affect the plugin's behavior. It's recommended to either use the tool or the plugin for configuration, not both simultaneously.

## Development

### Adding Build Information

The plugin automatically includes build date and time in the metadata. This is generated during the build process from `src/config.h.in`.

### Debugging

For debugging builds, use:

```bash
make debug
GST_DEBUG=3 gst-launch-1.0 tbs6324src encoder=0 ! ...
```

## License

LGPL (GNU Lesser General Public License)

## Author

Plugin Author

## GitHub Repository

This project is hosted on GitHub at: [https://github.com/TVforMe/Gstreamer/gsttbs6324src](https://github.com/TVforMe/Gstreamer/gsttbs6324src)