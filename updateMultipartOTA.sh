#!/bin/zsh
pio run

curl -# -X POST \
  -u admin:admin \
  -F "firmware=@.pio/build/esp32-s3-devkitc-1/firmware.bin" \
  http://enlatadora-master.local/firmware
