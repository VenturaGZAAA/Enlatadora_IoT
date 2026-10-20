#!/bin/zsh
pio run

curl -# -X POST \
  -u admin:admin \
  -F "firmware=@.pio/build/esp32-s3-devkitm-1/firmware.bin" \
  http://enlatadora-s3.local/firmware
