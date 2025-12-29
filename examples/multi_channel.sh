#!/bin/bash
# Capture from all 4 TBS6324 encoders simultaneously

OUTPUT_DIR="multi_capture_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTPUT_DIR"

echo "Capturing from all 4 encoders to $OUTPUT_DIR/"
echo "Press Ctrl+C to stop all captures"

# Capture from all 4 encoders in background
for encoder in {0..3}; do
    OUTPUT_FILE="$OUTPUT_DIR/channel_$encoder.ts"
    echo "Starting encoder $encoder..."
    gst-launch-1.0 tbs6324src encoder=$encoder ! filesink location=$OUTPUT_FILE &
done

# Wait for all background processes
wait

echo "All captures saved to $OUTPUT_DIR/"