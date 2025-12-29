#!/bin/bash
# Capture from TBS6324 encoder 0 to file

ENCODER=0
OUTPUT_FILE="capture_$(date +%Y%m%d_%H%M%S).ts"

echo "Capturing from TBS6324 encoder $ENCODER to $OUTPUT_FILE..."
echo "Press Ctrl+C to stop"

gst-launch-1.0 tbs6324src encoder=$ENCODER ! filesink location=$OUTPUT_FILE

echo "Capture saved to $OUTPUT_FILE"