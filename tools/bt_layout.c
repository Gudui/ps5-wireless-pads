/* SPDX-License-Identifier: GPL-3.0-or-later
 * Cached descriptors only. Never open endpoint nodes or issue bus requests,
 * USB_FS transfers, HCI commands, pairing or credential changes. */
#include "usb_desc.h"
#include <sys/ioctl.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef PADBRIDGE_BUILD_ID
#define PADBRIDGE_BUILD_ID "unknown"
#endif
#define NODE "/dev/ugen0.2"
#define LOG_PATH "/data/prospero-win/wii-bt-layout.log"
static FILE *record;

static void emit(const char *format, ...)
{
    va_list args;
    va_start(args, format); vprintf(format, args); va_end(args); putchar('\n');
    if (record) {
        va_start(args, format); vfprintf(record, format, args); va_end(args);
        fputc('\n', record); fflush(record);
    }
}

int main(void)
{
    struct usb_device_descriptor device;
    struct usb_gen_descriptor request;
    uint8_t config[4096];
    hci_function functions[HCI_FUNCTIONS_MAX];
    int fd = -1, result = 1, count, i, log_error;
    record = fopen(LOG_PATH, "w");
    log_error = errno;
    emit("BT_LAYOUT begin source=%s pid=%d node=%s cached_only=1",
         PADBRIDGE_BUILD_ID, (int)getpid(), NODE);
    if (!record) emit("BT_LAYOUT log errno=%d path=%s", log_error, LOG_PATH);
    fd = open(NODE, O_RDONLY | O_NONBLOCK);
    if (fd < 0) { emit("BT_LAYOUT result=inconclusive operation=open errno=%d", errno); goto done; }
    memset(&device, 0, sizeof device);
    if (ioctl(fd, USB_GET_DEVICE_DESC, &device) != 0) {
        emit("BT_LAYOUT result=inconclusive operation=device errno=%d", errno); goto done;
    }
    emit("BT_LAYOUT identity vid=%04x pid=%04x class=%02x/%02x/%02x",
         UGETW(device.idVendor), UGETW(device.idProduct), device.bDeviceClass,
         device.bDeviceSubClass, device.bDeviceProtocol);
    if (UGETW(device.idVendor) != 0x0e8d || UGETW(device.idProduct) != 0x3603) {
        emit("BT_LAYOUT result=inconclusive reason=identity-mismatch"); goto done;
    }
    memset(&request, 0, sizeof request);
    memset(config, 0, sizeof config);
    request.ugd_data = config; request.ugd_maxlen = sizeof config;
    request.ugd_config_index = 0xff; /* current configuration, cached copy-out */
    if (ioctl(fd, USB_GET_FULL_DESC, &request) != 0) {
        emit("BT_LAYOUT result=inconclusive operation=config errno=%d", errno); goto done;
    }
    if (request.ugd_actlen > sizeof config ||
        !usb_config_complete(config, (int)request.ugd_actlen)) {
        emit("BT_LAYOUT result=inconclusive reason=incomplete-config bytes=%u",
             (unsigned)request.ugd_actlen); goto done;
    }
    emit("BT_LAYOUT config bytes=%u", (unsigned)request.ugd_actlen);
    for (i = 0; i < (int)request.ugd_actlen; i += 24) {
        char line[24 * 3 + 1]; int k, bytes = (int)request.ugd_actlen - i;
        if (bytes > 24) bytes = 24;
        for (k = 0; k < bytes; k++) snprintf(line + k * 3, 4, "%02x ", config[i + k]);
        emit("BT_LAYOUT cfg offset=%u hex=%s", (unsigned)i, line);
    }
    count = usb_find_hci_functions(config, (int)request.ugd_actlen, functions);
    for (i = 0; i < count; i++) {
        const hci_function *f = &functions[i];
        emit("BT_LAYOUT hci interface=%d events=%02x/%u acl_in=%02x/%u acl_out=%02x/%u",
             f->iface, f->ep_events, f->mps_events, f->ep_acl_in, f->mps_acl_in,
             f->ep_acl_out, f->mps_acl_out);
    }
    emit("BT_LAYOUT result=%s hci_functions=%d radio_commands=0 transfers=0",
         count ? "ok" : "no-hci-function", count);
    result = count ? 0 : 2;
done:
    if (fd >= 0) close(fd);
    if (record) fclose(record);
    return result;
}
