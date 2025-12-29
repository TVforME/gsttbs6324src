#!/bin/bash
# Stream from TBS6324 encoder 0 via UDP

ENCODER=0
DEST_IP="192.168.1.100"
DEST_PORT="5000"

echo "Streaming encoder $ENCODER to $DEST_IP:$DEST_PORT via UDP..."
echo "Press Ctrl+C to stop"

gst-launch-1.0 tbs6324src encoder=$ENCODER ! udpsink host=$DEST_IP port=$DEST_PORT

echo "Stream stopped"