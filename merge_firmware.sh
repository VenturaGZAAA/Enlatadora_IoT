#!/usr/bin/env bash

# Exit immediately on error
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

ENV_NAME="esp32-s3-devkitc-1"
BUILD_DIR=".pio/build/${ENV_NAME}"
OUTPUT_BIN="${1:-merged-firmware.bin}"

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    echo "Usage: ./merge_firmware.sh [output_filename.bin]"
    echo "Merges ESP32-S3 bootloader, partition table, otadata, and firmware into a single flashable binary."
    exit 0
fi

# Ensure build binaries exist; compile if missing
if [[ ! -f "${BUILD_DIR}/firmware.bin" || ! -f "${BUILD_DIR}/bootloader.bin" || ! -f "${BUILD_DIR}/partitions.bin" ]]; then
    echo "Build artifacts missing in ${BUILD_DIR}. Compiling project first..."
    pio run -e "${ENV_NAME}"
fi

# Locate esptool
ESPTOOL_BIN=""
if command -v esptool.py &>/dev/null; then
    ESPTOOL_BIN="esptool.py"
elif command -v esptool &>/dev/null; then
    ESPTOOL_BIN="esptool"
elif [[ -f "${HOME}/.platformio/penv/bin/esptool.py" ]]; then
    ESPTOOL_BIN="${HOME}/.platformio/penv/bin/esptool.py"
elif compgen -G "${HOME}/.platformio/packages/tool-esptoolpy*/esptool.py" > /dev/null; then
    ESPTOOL_BIN=$(ls -1 "${HOME}"/.platformio/packages/tool-esptoolpy*/esptool.py | head -n 1)
fi

if [[ -z "$ESPTOOL_BIN" ]]; then
    echo "Error: esptool.py not found. Please install esptool or PlatformIO."
    exit 1
fi

# Locate boot_app0.bin (OTA initial boot data)
BOOT_APP0_BIN=""
if [[ -f "${BUILD_DIR}/boot_app0.bin" ]]; then
    BOOT_APP0_BIN="${BUILD_DIR}/boot_app0.bin"
elif compgen -G "${HOME}/.platformio/packages/framework-arduinoespressif32*/tools/partitions/boot_app0.bin" > /dev/null; then
    BOOT_APP0_BIN=$(ls -1 "${HOME}"/.platformio/packages/framework-arduinoespressif32*/tools/partitions/boot_app0.bin | head -n 1)
fi

echo "Merging binaries using ${ESPTOOL_BIN}..."

# Build arguments list for merge_bin
MERGE_ARGS=(
    --chip esp32s3
    merge_bin
    --output "${OUTPUT_BIN}"
    --flash_mode keep
    --flash_freq keep
    --flash_size 8MB
    0x0 "${BUILD_DIR}/bootloader.bin"
    0x8000 "${BUILD_DIR}/partitions.bin"
)

if [[ -n "$BOOT_APP0_BIN" && -f "$BOOT_APP0_BIN" ]]; then
    MERGE_ARGS+=(0xe000 "${BOOT_APP0_BIN}")
fi

MERGE_ARGS+=(0x10000 "${BUILD_DIR}/firmware.bin")

python3 "${ESPTOOL_BIN}" "${MERGE_ARGS[@]}" || "${ESPTOOL_BIN}" "${MERGE_ARGS[@]}"

echo ""
echo "================================================================="
echo "Success! Merged binary created: ${OUTPUT_BIN}"
echo "================================================================="
echo "You can flash this single binary to an ESP32-S3 at offset 0x0:"
echo ""
echo "  esptool.py --chip esp32s3 write_flash 0x0 ${OUTPUT_BIN}"
echo ""
echo "Or using PlatformIO / web-flashing tools with base address 0x0."
echo "================================================================="
