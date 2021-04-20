
OFF_GPIO=$(shell grep 'dtoverlay=gpio-poweroff' /boot/config.txt > /dev/null; echo $$?)
R=$(shell grep 'gpio=17=' /boot/config.txt > /dev/null; echo $$?)
G=$(shell grep 'gpio=27=' /boot/config.txt > /dev/null; echo $$?)
B=$(shell grep 'gpio=22=' /boot/config.txt > /dev/null; echo $$?)

all:
	$(info Run 'make install' to install)

install: check off_gpio red green blue services
	cp rack-leds-setup.sh /usr/bin/rack-leds-setup
	chmod +x /usr/bin/rack-leds-setup

# check that this is run as root
check:
ifneq ($(shell id -u),0)
	$(error You are not root, run this as root please)
endif
ifeq ($(wildcard /boot/config.txt),)
	$(error /boot/config.txt is missing, not a Raspberry Pi system?)
endif

# set gpio to be on when system has shutdown
off_gpio:
ifneq ($(OFF_GPIO),0)
	echo 'dtoverlay=gpio-poweroff,gpiopin=2' >> /boot/config.txt
endif

# red off on bootup
red:
ifneq ($(R),0)
	echo 'gpio=17=op,dl' >> /boot/config.txt
endif

# green off on bootup
green:
ifneq ($(G),0)
	echo 'gpio=27=op,dl' >> /boot/config.txt
endif

# blue to be lit on bootup
blue:
ifneq ($(B),0)
	echo 'gpio=22=op,dh' >> /boot/config.txt
endif

# install services
services:
	cp rack-system-booting.service /lib/systemd/system/rack-system-booting.service
	cp rack-system-running.service /lib/systemd/system/rack-system-running.service
	cp rack-system-shutdown.service /lib/systemd/system/rack-system-shutdown.service
	cp rack-system-poweroff.service /lib/systemd/system/rack-system-poweroff.service
	systemctl enable rack-system-booting.service
	systemctl enable rack-system-running.service
	systemctl enable rack-system-shutdown.service
	systemctl enable rack-system-poweroff.service
