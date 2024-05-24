#!/usr/bin/bash

echo 125 > /sys/devices/platform/abs_platform_device.0/abs_value;
OUTPUT=$( cat /sys/devices/platform/abs_platform_device.0/abs_value );
if [ "$OUTPUT" != "value:125" ]; then
echo "Failed to write to abs_value";
fi

echo 125 > /sys/devices/platform/abs_platform_device.0/abs_address;
echo 0 > /sys/devices/platform/abs_platform_device.0/abs_value;
OUTPUT=$( cat /sys/devices/platform/abs_platform_device.0/abs_value );
if [ "$OUTPUT" != "value:0" ]; then
echo "Failed to write to abs_value";
fi

echo 0 > /sys/devices/platform/abs_platform_device.0/abs_address;
OUTPUT=$( cat /sys/devices/platform/abs_platform_device.0/abs_value );
if [ "$OUTPUT" != "value:125" ]; then
echo "Failed to write to abs_value";
fi

echo 125 > /sys/devices/platform/abs_platform_device.0/abs_address;
OUTPUT=$( cat /sys/devices/platform/abs_platform_device.0/abs_value );
if [ "$OUTPUT" != "value:0" ]; then
echo "Failed to write to abs_value";
fi