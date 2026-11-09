#!/bin/zsh
pio run

curl -# -X POST \
  -u admin:admin \
  -H "Content-Type: application/octet-stream" \
  --data-binary @.pio/build/esp32-s3-devkitc-1/firmware.bin \
  http://enlatadora-master.local/firmware
