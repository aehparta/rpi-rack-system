
TARGET = OPI
DEBUG = 1
USE = GPIO SPI LOG LOG_COLORS
LDFLAGS = -lmicrohttpd -lb64 -lcrypto -lpcre
DEFINES = SLOT_COUNT=1

# init
include $(LIBE_PATH)/init.mk

# our own sources etc
BUILD_BINS = racksm
PROGRAM_BIN = racksm
racksm_SRC = main.c slot.c opt.c httpd.c $(libe_SRC)

# build
include $(LIBE_PATH)/build.mk

# programmable targets
include $(LIBE_PATH)/flash/program.mk

-include local.mk
