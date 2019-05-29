
all:
	$(info run 'make install' to install)

install:
ifneq ($(shell id -u),0)
	$(error "You are not root, run this as root please")
endif
	cp rpi-rack-leds-setup.sh /usr/bin/rpi-rack-leds-setup
	chmod +x /usr/bin/rpi-rack-leds-setup

