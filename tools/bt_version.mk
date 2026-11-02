include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
BUILD_ID ?= unknown
CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -Isrc -DPADBRIDGE_BUILD_ID=\"$(BUILD_ID)\"
SOURCES := tools/bt_version.c src/version_query.c src/hci_usb.c src/usb_desc.c src/evstream.c src/log.c src/util.c
dist/bt-version.elf: $(SOURCES)
	@mkdir -p dist
	$(CC) $(CFLAGS) -o $@ $(SOURCES)
