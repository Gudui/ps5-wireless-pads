#include "usb_desc.h"

#include <string.h>

#define DT_INTERFACE 0x04
#define DT_ENDPOINT  0x05

int usb_config_complete(const uint8_t *cfg, int len)
{
    int off;
    if (!cfg || len < 9 || cfg[0] != 9 || cfg[1] != 2 ||
        (cfg[2] | cfg[3] << 8) != len) return 0;
    for (off = 0; off < len;) {
        int size, type;
        if (len - off < 2) return 0;
        size = cfg[off]; type = cfg[off + 1];
        if (size < 2 || size > len - off) return 0;
        if ((type == DT_INTERFACE && size < 9) ||
            (type == DT_ENDPOINT && size < 7) || (type == 2 && off != 0)) return 0;
        off += size;
    }
    return 1;
}

int usb_find_hci_functions(const uint8_t *cfg, int len, hci_function *out)
{
    int off = 0, n = 0, cur = -1;      /* cur: index in out of the interface being read */
    int in_class = 0;

    while (off + 2 <= len) {
        int dl = cfg[off];
        const uint8_t *d = cfg + off;

        if (dl < 2 || off + dl > len) break;            /* malformed: stop */

        if (d[1] == DT_INTERFACE && dl >= 9) {
            /* Only alternate setting 0 of a Bluetooth-class interface. */
            in_class = d[5] == 0xE0 && d[6] == 0x01 && d[7] == 0x01 && d[3] == 0;
            cur = -1;
            if (in_class && n < HCI_FUNCTIONS_MAX) {
                cur = n++;
                memset(&out[cur], 0, sizeof out[cur]);
                out[cur].iface = d[2];
            }
        } else if (d[1] == DT_ENDPOINT && dl >= 7 && in_class && cur >= 0) {
            uint8_t addr = d[2], attr = d[3] & 3;
            uint16_t mps = (uint16_t)(d[4] | d[5] << 8);
            hci_function *f = &out[cur];

            if (attr == 3 && (addr & 0x80) && !f->ep_events) {
                f->ep_events = addr;
                f->mps_events = mps;
            } else if (attr == 2 && (addr & 0x80) && !f->ep_acl_in) {
                f->ep_acl_in = addr;
                f->mps_acl_in = mps;
            } else if (attr == 2 && !(addr & 0x80) && !f->ep_acl_out) {
                f->ep_acl_out = addr;
                f->mps_acl_out = mps;
            } else if (f->n_other < (int)sizeof f->other) {
                f->other[f->n_other++] = addr;
            }
        }
        off += dl;
    }

    /* Keep only the interfaces that really are an HCI function: the voice
     * interface of the same class has isochronous endpoints and none of these. */
    {
        int i, k = 0;
        for (i = 0; i < n; i++) {
            if (out[i].ep_events && out[i].ep_acl_in && out[i].ep_acl_out) {
                if (k != i) out[k] = out[i];
                k++;
            }
        }
        return k;
    }
}

int usb_ugen_external_name(const char *name)
{
    unsigned part[2] = {0, 0};
    int i;
    if (!name || strncmp(name, "ugen", 4) != 0) return 0;
    name += 4;
    for (i = 0; i < 2; i++) {
        const char *start = name;
        if (*name < '0' || *name > '9') return 0;
        while (*name >= '0' && *name <= '9') {
            part[i] = part[i] * 10 + (unsigned)(*name++ - '0');
            if (part[i] > 255) return 0;
        }
        if (name - start > 1 && *start == '0') return 0;
        if (i == 0 && *name++ != '.') return 0;
    }
    return !*name && part[1] > 0 && part[1] <= 127 &&
           !(part[0] == 0 && part[1] == 2);
}
