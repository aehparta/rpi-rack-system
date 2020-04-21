
TARGET = OPI
USE = GPIO LOG SPI

# init
include $(LIBE_PATH)/init.mk

# our own sources etc
BUILD_BINS = test
PROGRAM_BIN = test
test_SRC = main.c slot.c $(libe_SRC)

# build
include $(LIBE_PATH)/build.mk

# programmable targets
include $(LIBE_PATH)/flash/program.mk

-include local.mk
