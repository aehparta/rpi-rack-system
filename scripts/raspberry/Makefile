
R=$(shell grep 'dtoverlay=gpio-poweroff' /boot/config.txt > /dev/null; echo $$?)
G=$(shell grep 'dtoverlay=pwm-2chan' /boot/config.txt > /dev/null; echo $$?)
B=$(shell grep 'gpio=26=' /boot/config.txt > /dev/null; echo $$?)

all:
	$(info Run 'make install' to install)

install: check red green blue services
	cp rpi-rack-leds-setup.sh /usr/bin/rpi-rack-leds-setup
	chmod +x /usr/bin/rpi-rack-leds-setup

# check that this is run as root
check:
ifneq ($(shell id -u),0)
	$(error You are not root, run this as root please)
endif
ifeq ($(wildcard /boot/config.txt),)
	$(error /boot/config.txt is missing, not a Raspberry Pi system?)
endif

# set red to be on when system has shutdown
red:
ifneq ($(R),0)
	echo 'dtoverlay=gpio-poweroff,gpiopin=13' >> /boot/config.txt
endif

# green led using pwm
green:
ifneq ($(G),0)
	echo 'dtoverlay=pwm-2chan' >> /boot/config.txt
endif

# blue to be lit on bootup
blue:
ifneq ($(B),0)
	echo 'gpio=26=op,dh' >> /boot/config.txt
endif

# install services
services:
	cp rpi-rack-system-booting.service /lib/systemd/system/rpi-rack-system-booting.service
	cp rpi-rack-system-running.service /lib/systemd/system/rpi-rack-system-running.service
	cp rpi-rack-system-shutdown.service /lib/systemd/system/rpi-rack-system-shutdown.service
	cp rpi-rack-system-poweroff.service /lib/systemd/system/rpi-rack-system-poweroff.service
	systemctl enable rpi-rack-system-booting.service
	systemctl enable rpi-rack-system-running.service
	systemctl enable rpi-rack-system-shutdown.service
	systemctl enable rpi-rack-system-poweroff.service
