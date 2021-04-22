
TARGET = AVR
MCU_AVR = atmega328p
F_CPU_AVR = 11059200
LDFLAGS_AVR += -Wl,-u,vfprintf -lprintf_flt

USE = GPIO LOG NVM
DEFINES = LOG_BAUD=57600

# avrdude
AVRDUDE_PROGRAMMER = linuxspi
AVRDUDE_PORT = /dev/spidev0.0
AVRDUDE_SUDO = 1

# init
include $(LIBE_PATH)/init.mk

# our own sources etc
BUILD_BINS = keeper-slot keeper-fans
PROGRAM_BIN = keeper-fans
keeper-slot_SRC = main-slot.c $(libe_SRC)
keeper-slot_ASRC = main-slot-asm.S
keeper-fans_SRC = main-fans.c $(libe_SRC)
keeper-fans_ASRC = main-fans-asm.S

# build
include $(LIBE_PATH)/build.mk

# programmable targets
include $(LIBE_PATH)/flash/program.mk

-include local.mk
