#!/bin/bash
# Generate SMPTE color bars for TBS6324 testing
# This script requires an external video source connected to SDI input

ENCODER=0
OUTPUT_FILE="smpte_test_$(date +%Y%m%d_%H%M%S).ts"

echo "Capturing SMPTE pattern from encoder $ENCODER to $OUTPUT_FILE..."
echo "Note: Ensure SMPTE color bars are being fed to the SDI input"
echo "Press Ctrl+C to stop"

gst-launch-1.0 tbs6324src encoder=$ENCODER ! filesink location=$OUTPUT_FILE

echo "Capture saved to $OUTPUT_FILE"
echo "You can verify the pattern by playing the file with:"
echo "gst-play-1.0 $OUTPUT_FILE"