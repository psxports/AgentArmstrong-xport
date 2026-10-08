PROJECT_ROOT := $(abspath ../../..)
SHORT_NAME := AA
FULL_NAME := Agent Armstrong (xport)
CONFIG ?= Release
APP_SOURCES := $(wildcard $(PROJECT_ROOT)/src/*.c)
APP_DEFINES := -DWND_TITLE='"$(FULL_NAME)"' -DWND_WIDTH=960 -DWND_HEIGHT=720 -DFIELD_RATE=50

include ../../../../xport/src/platform/web/Makefile
