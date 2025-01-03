#!/bin/bash

# Define macros
SWAPPINESS_VALUE=200          # Set system swappiness value
ZRAM_SIZE="4G"                # Set zRAM size
COMPRESSION_ALGORITHM="lz4"   # Set zRAM compression algorithm
SWAP_PRIORITY=150             # Set swap partition priority

# Print current operation
echo "Setting system swappiness to ${SWAPPINESS_VALUE}..."
sysctl vm.swappiness=${SWAPPINESS_VALUE}

# Verify swappiness setting
CURRENT_SWAPPINESS=$(sysctl vm.swappiness | awk '{print $3}')
if [ "${CURRENT_SWAPPINESS}" -eq "${SWAPPINESS_VALUE}" ]; then
	echo "Swappiness set successfully. Current value: ${CURRENT_SWAPPINESS}."
else
	echo "Error: Failed to set swappiness. Current value: ${CURRENT_SWAPPINESS}."
	exit 1
fi

# Find available zRAM device number
ZRAM_DEV_NUM=0
while [ -e "/dev/zram${ZRAM_DEV_NUM}" ]; do
	ZRAM_DEV_NUM=$((ZRAM_DEV_NUM + 1))
done

# Load zRAM module if not loaded
if ! lsmod | grep -q zram; then
	echo "Loading zRAM module..."
	modprobe zram num_devices=$((ZRAM_DEV_NUM + 1))
fi

ZRAM_DEV="/dev/zram${ZRAM_DEV_NUM}"

echo "Creating zRAM device ${ZRAM_DEV}..."

# Set compression algorithm
echo "${COMPRESSION_ALGORITHM}" > /sys/block/zram${ZRAM_DEV_NUM}/comp_algorithm

# Verify compression algorithm
CURRENT_ALGO=$(cat /sys/block/zram${ZRAM_DEV_NUM}/comp_algorithm)
if echo "${CURRENT_ALGO}" | grep -qw "\[${COMPRESSION_ALGORITHM}\]"; then
	echo "zRAM compression algorithm set to: [${COMPRESSION_ALGORITHM}]."
else
	echo "Error: Failed to set compression algorithm. Current value: ${CURRENT_ALGO}."
	exit 1
fi

# Set zRAM size
echo "${ZRAM_SIZE}" > /sys/block/zram${ZRAM_DEV_NUM}/disksize

# Verify zRAM size
CURRENT_SIZE=$(cat /sys/block/zram${ZRAM_DEV_NUM}/disksize)
if [ "${CURRENT_SIZE}" -eq "$(numfmt --from=iec ${ZRAM_SIZE})" ]; then
	echo "zRAM size set to: ${ZRAM_SIZE}."
else
	echo "Error: Failed to set zRAM size. Current value: $(numfmt --to=iec ${CURRENT_SIZE})."
	exit 1
fi

# Create and enable swap space
mkswap ${ZRAM_DEV}
swapon -p ${SWAP_PRIORITY} ${ZRAM_DEV}

# Verify swap activation
if swapon --show | grep -q "${ZRAM_DEV}"; then
	echo "zRAM swap space enabled. Device: ${ZRAM_DEV}, Priority: ${SWAP_PRIORITY}."
else
	echo "Error: Failed to enable zRAM swap space."
	exit 1
fi

# Verify swap priority
CURRENT_PRIORITY=$(swapon --show | grep "${ZRAM_DEV}" | awk '{print $5}')
if [ "${CURRENT_PRIORITY}" -eq "${SWAP_PRIORITY}" ]; then
	echo "Swap priority set to: ${CURRENT_PRIORITY}."
else
	echo "Error: Failed to set swap priority. Current value: ${CURRENT_PRIORITY}."
	exit 1
fi

echo "All configurations have been successfully applied."
