#!/bin/sh
# Self-heal Qualcomm QUP I2C4 (990000.i2c) GPI DMA stalls for AW8624 haptics.
# Triggered via systemd-run from udev on HAPTICS_I2C_TIMEOUT=1 from aw8624.ko.
exec 9>/run/lisa-haptics-recover.lock
flock -n 9 || exit 0

if [ -w /sys/bus/platform/drivers/geni_i2c/unbind ]; then
	echo 990000.i2c > /sys/bus/platform/drivers/geni_i2c/unbind 2>/dev/null || true
	sleep 0.2
	echo 990000.i2c > /sys/bus/platform/drivers/geni_i2c/bind 2>/dev/null || true
	sleep 0.3
	[ -w /sys/devices/platform/soc@0/9c0000.geniqup/990000.i2c/power/control ] && \
		echo on > /sys/devices/platform/soc@0/9c0000.geniqup/990000.i2c/power/control 2>/dev/null || true
	# Let feedbackd re-open the fresh aw8624_haptic event node on next D-Bus call
	pkill feedbackd 2>/dev/null || true
fi
exit 0
