
TARGET = AVR
MCU_AVR = atmega328p
F_CPU_AVR = 11059200
LDFLAGS_AVR += -Wl,-u,vfprintf -lprintf_flt

USE = GPIO LOG NVM
DEFINES = LOG_BAUD=38400

# avrdude
AVRDUDE_PROGRAMMER = linuxgpio

# init
include $(LIBE_PATH)/init.mk

# our own sources etc
BUILD_BINS = test
PROGRAM_BIN = test
test_SRC = main.c $(libe_SRC)
test_ASRC = asm.S

# build
include $(LIBE_PATH)/build.mk

# programmable targets
include $(LIBE_PATH)/flash/program.mk

-include local.mk
