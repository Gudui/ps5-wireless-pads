# Diagnostic ELF only: does not link the Bluetooth host, virtual pads or web UI.
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
BUILD_ID ?= unknown
CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -Isrc -DPADBRIDGE_BUILD_ID=\"$(BUILD_ID)\"
dist/bt-layout.elf: tools/bt_layout.c src/usb_desc.c src/usb_desc.h
	@mkdir -p dist
	$(CC) $(CFLAGS) -o $@ tools/bt_layout.c src/usb_desc.c
