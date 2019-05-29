#!/bin/bash

# blue led is free to use for user stuff, connected to GPIO 26

# initialize blue led
if [ ! -d "/sys/class/gpio/gpio26" ]; then
	echo 26 > /sys/class/gpio/export
	echo "out" > /sys/class/gpio/gpio26/direction
	echo 0 > /sys/class/gpio/gpio26/value
fi

# initialize green led to use pwm
if [ ! -d "/sys/class/pwm/pwmchip0/pwm1" ]; then
	echo 1 > /sys/class/pwm/pwmchip0/export
	echo 0 > /sys/class/pwm/pwmchip0/pwm1/enable
	echo 0 > /sys/class/pwm/pwmchip0/pwm1/duty_cycle
fi

case "$1" in
	booting)
		echo 0 > /sys/class/pwm/pwmchip0/pwm1/enable
		echo 1000000000 > /sys/class/pwm/pwmchip0/pwm1/period
		echo 800000000 > /sys/class/pwm/pwmchip0/pwm1/duty_cycle
		echo 1 > /sys/class/pwm/pwmchip0/pwm1/enable
		;;
	running)
		echo 0 > /sys/class/pwm/pwmchip0/pwm1/enable
		echo 1000000000 > /sys/class/pwm/pwmchip0/pwm1/period
		echo 5000000 > /sys/class/pwm/pwmchip0/pwm1/duty_cycle
		echo 1 > /sys/class/pwm/pwmchip0/pwm1/enable
		;;
	shutdown)
		echo 0 > /sys/class/pwm/pwmchip0/pwm1/enable
		echo 1000000000 > /sys/class/pwm/pwmchip0/pwm1/period
		echo 200000000 > /sys/class/pwm/pwmchip0/pwm1/duty_cycle
		echo 1 > /sys/class/pwm/pwmchip0/pwm1/enable
		;;
	*)
		;;
esac

