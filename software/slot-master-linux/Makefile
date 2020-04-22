
TARGET = OPI
USE = GPIO SPI LOG LOG_COLORS

# init
include $(LIBE_PATH)/init.mk

# our own sources etc
BUILD_BINS = racksm
PROGRAM_BIN = racksm
racksm_SRC = main.c slot.c opt.c httpd.c $(libe_SRC)
LDFLAGS = -lmicrohttpd -lb64 -lcrypto

# build
include $(LIBE_PATH)/build.mk

# programmable targets
include $(LIBE_PATH)/flash/program.mk

-include local.mk
