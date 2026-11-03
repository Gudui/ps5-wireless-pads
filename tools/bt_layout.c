/* SPDX-License-Identifier: GPL-3.0-or-later
 * Cached descriptors only. Never open endpoint nodes or issue bus requests,
 * USB_FS transfers, HCI commands, pairing or credential changes. */
#include "usb_desc.h"
#include <sys/ioctl.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include <errno.h>
#ifdef BT_LAYOUT_EXTERNAL
#include <dirent.h>
#endif
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef PADBRIDGE_BUILD_ID
#define PADBRIDGE_BUILD_ID "unknown"
#endif
#define NODE "/dev/ugen0.2"
#ifdef BT_LAYOUT_EXTERNAL
#define LOG_PATH "/data/prospero-win/wii-bt-adapter-layout.log"
#define TAG "BT_ADAPTER_LAYOUT "
#define EXPECT_VID 0x2357
#define EXPECT_PID 0x0604
#else
#define LOG_PATH "/data/prospero-win/wii-bt-layout.log"
#define TAG "BT_LAYOUT "
#define EXPECT_VID 0x0e8d
#define EXPECT_PID 0x3603
#endif
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

#ifdef BT_LAYOUT_EXTERNAL
/* Cached identity copy-outs only; never touch the internal radio node. */
static int open_adapter(char path[64])
{
    DIR *dir = opendir("/dev");
    struct dirent *entry;
    int chosen = -1, matches = 0, entries = 0, nodes = 0, failed = 0;
    if (!dir) { emit(TAG "discover operation=opendir errno=%d", errno); return -1; }
    for (;;) {
        struct usb_device_descriptor dd;
        char candidate[64];
        int fd;
        errno = 0;
        entry = readdir(dir);
        if (!entry) { if (errno) failed = 1; break; }
        if (++entries > 4096) { failed = 1; break; }
        if (!usb_ugen_external_name(entry->d_name)) continue;
        if (++nodes > 32) { failed = 1; break; }
        snprintf(candidate, sizeof candidate, "/dev/%s", entry->d_name);
        fd = open(candidate, O_RDONLY | O_NONBLOCK);
        if (fd < 0) { emit(TAG "candidate node=%s open_errno=%d", candidate, errno); continue; }
        memset(&dd, 0, sizeof dd);
        if (ioctl(fd, USB_GET_DEVICE_DESC, &dd) != 0) {
            emit(TAG "candidate node=%s device_errno=%d", candidate, errno);
            close(fd); continue;
        }
        emit(TAG "candidate node=%s vid=%04x pid=%04x", candidate,
             UGETW(dd.idVendor), UGETW(dd.idProduct));
        if (UGETW(dd.idVendor) == EXPECT_VID && UGETW(dd.idProduct) == EXPECT_PID) {
            matches++;
            emit(TAG "match node=%s revision=%04x", candidate, UGETW(dd.bcdDevice));
            if (chosen < 0) { chosen = fd; snprintf(path, 64, "%s", candidate); }
            else close(fd);
        } else close(fd);
    }
    if (closedir(dir) != 0) failed = 1;
    emit(TAG "discover candidates=%d matches=%d bounded=%d", nodes, matches, !failed);
    if (failed || matches != 1) { if (chosen >= 0) close(chosen); return -1; }
    return chosen;
}
#endif

int main(void)
{
    struct usb_device_descriptor device;
    struct usb_gen_descriptor request;
    uint8_t config[4096];
    hci_function functions[HCI_FUNCTIONS_MAX];
    int fd = -1, result = 1, count, i, log_error;
#ifdef BT_LAYOUT_EXTERNAL
    char selected_node[64] = "unselected";
    const char *node = selected_node;
#else
    const char *node = NODE;
#endif
    record = fopen(LOG_PATH, "w");
    log_error = errno;
    emit(TAG "begin source=%s pid=%d node=%s cached_only=1",
         PADBRIDGE_BUILD_ID, (int)getpid(), node);
    if (!record) emit(TAG "log errno=%d path=%s", log_error, LOG_PATH);
#ifdef BT_LAYOUT_EXTERNAL
    fd = open_adapter(selected_node);
    if (fd < 0) { emit(TAG "result=inconclusive reason=no-unique-adapter"); goto done; }
#else
    fd = open(node, O_RDONLY | O_NONBLOCK);
#endif
    if (fd < 0) { emit(TAG "result=inconclusive operation=open errno=%d", errno); goto done; }
    memset(&device, 0, sizeof device);
    if (ioctl(fd, USB_GET_DEVICE_DESC, &device) != 0) {
        emit(TAG "result=inconclusive operation=device errno=%d", errno); goto done;
    }
    emit(TAG "identity vid=%04x pid=%04x class=%02x/%02x/%02x",
         UGETW(device.idVendor), UGETW(device.idProduct), device.bDeviceClass,
         device.bDeviceSubClass, device.bDeviceProtocol);
    if (UGETW(device.idVendor) != EXPECT_VID || UGETW(device.idProduct) != EXPECT_PID) {
        emit(TAG "result=inconclusive reason=identity-mismatch"); goto done;
    }
    memset(&request, 0, sizeof request);
    memset(config, 0, sizeof config);
    request.ugd_data = config; request.ugd_maxlen = sizeof config;
    request.ugd_config_index = 0xff; /* current configuration, cached copy-out */
    if (ioctl(fd, USB_GET_FULL_DESC, &request) != 0) {
        emit(TAG "result=inconclusive operation=config errno=%d", errno); goto done;
    }
    if (request.ugd_actlen > sizeof config ||
        !usb_config_complete(config, (int)request.ugd_actlen)) {
        emit(TAG "result=inconclusive reason=incomplete-config bytes=%u",
             (unsigned)request.ugd_actlen); goto done;
    }
    emit(TAG "config bytes=%u", (unsigned)request.ugd_actlen);
    for (i = 0; i < (int)request.ugd_actlen; i += 24) {
        char line[24 * 3 + 1]; int k, bytes = (int)request.ugd_actlen - i;
        if (bytes > 24) bytes = 24;
        for (k = 0; k < bytes; k++) snprintf(line + k * 3, 4, "%02x ", config[i + k]);
        emit(TAG "cfg offset=%u hex=%s", (unsigned)i, line);
    }
    count = usb_find_hci_functions(config, (int)request.ugd_actlen, functions);
    for (i = 0; i < count; i++) {
        const hci_function *f = &functions[i];
        emit(TAG "hci interface=%d events=%02x/%u acl_in=%02x/%u acl_out=%02x/%u",
             f->iface, f->ep_events, f->mps_events, f->ep_acl_in, f->mps_acl_in,
             f->ep_acl_out, f->mps_acl_out);
    }
    emit(TAG "result=%s hci_functions=%d radio_commands=0 transfers=0",
         count ? "ok" : "no-hci-function", count);
    result = count ? 0 : 2;
done:
    if (fd >= 0 && close(fd) != 0) {
        emit(TAG "result=inconclusive cleanup=close errno=%d", errno); result = 1;
    }
    if (record) fclose(record);
    return result;
}
