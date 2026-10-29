PROJECT_ROOT := $(abspath ../../..)
SHORT_NAME := AA
FULL_NAME := Agent Armstrong
CONFIG ?= Release
APP_SOURCES := $(wildcard $(PROJECT_ROOT)/src/*.c)
APP_DEFINES := -DWND_TITLE='"$(FULL_NAME)"' -DWND_WIDTH=960 -DWND_HEIGHT=720 -DFIELD_RATE=50 -DXPORT_SPU_WRITE_MAX_BYTES=0x7eff0u

include ../../../../xport/src/platform/web/Makefile
