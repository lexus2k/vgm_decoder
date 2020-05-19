# This is Makefile for ESP32 IDF

COMPONENT_ADD_INCLUDEDIRS := ./include
COMPONENT_SRCDIRS := ./src ./src/formats ./src/chips
CPPFLAGS += -DVGM_DECODER_LOGGER=0

# COMPONENT_DEPENDS := spibus
