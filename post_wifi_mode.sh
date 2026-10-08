#!/bin/zsh
curl -X POST http://enlatadora-s3.local/config/wifi/set \
  -H "Content-Type: application/json" \
  -d "{\"start_ap\":$1}"
