#!/bin/bash

# initialize red led
if [ ! -d "/sys/class/gpio/gpio17" ]; then
    echo 17 > /sys/class/gpio/export
    echo "out" > /sys/class/gpio/gpio17/direction
    echo 0 > /sys/class/gpio/gpio17/value
fi

# initialize green led
if [ ! -d "/sys/class/gpio/gpio27" ]; then
    echo 27 > /sys/class/gpio/export
    echo "out" > /sys/class/gpio/gpio27/direction
    echo 0 > /sys/class/gpio/gpio27/value
fi

# initialize blue led
if [ ! -d "/sys/class/gpio/gpio22" ]; then
    echo 22 > /sys/class/gpio/export
    echo "out" > /sys/class/gpio/gpio22/direction
    echo 0 > /sys/class/gpio/gpio22/value
fi

case "$1" in
    booting)
        echo 0 > /sys/class/gpio/gpio17/value
        echo 1 > /sys/class/gpio/gpio27/value
        echo 0 > /sys/class/gpio/gpio22/value
    ;;
    running)
        echo 0 > /sys/class/gpio/gpio17/value
        echo 0 > /sys/class/gpio/gpio22/value
        while true; do
            echo 1 > /sys/class/gpio/gpio27/value
            sleep 0.01
            echo 0 > /sys/class/gpio/gpio27/value
            sleep 1
            # send temperature
            if [ -c "/dev/ttyS0" ] && [ -f "/sys/class/thermal/thermal_zone0/temp" ]; then
                T=`cat /sys/class/thermal/thermal_zone0/temp`
                T=$((T/1000))
                sequence=`printf "\\20\\%o" $T`
                printf "$sequence" > /dev/ttyS0
            fi
        done
    ;;
    shutdown)
        echo 1 > /sys/class/gpio/gpio17/value
        echo 0 > /sys/class/gpio/gpio27/value
        echo 1 > /sys/class/gpio/gpio22/value
    ;;
    poweroff)
        echo 0 > /sys/class/gpio/gpio17/value
        echo 0 > /sys/class/gpio/gpio27/value
        echo 0 > /sys/class/gpio/gpio22/value
    ;;
    *)
    ;;
esac

