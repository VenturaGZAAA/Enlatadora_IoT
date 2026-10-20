#!/bin/zsh
pio run

curl -# -X POST \
  -u admin:admin \
  -H "Content-Type: application/octet-stream" \
  --data-binary @.pio/build/esp32-s3-devkitm-1/firmware.bin \
  http://enlatadora-s3.local/firmware
