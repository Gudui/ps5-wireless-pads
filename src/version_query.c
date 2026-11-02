/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "version_query.h"
#include <string.h>
static uint16_t word(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }
int version_query(hci_t hci, version_query_result *r)
{
    unsigned char event[HCI_PKT_MAX];
    unsigned round, j;
    if (!r) return VERSION_QUERY_INVALID;
    memset(r, 0, sizeof *r);
    if (!hci.ops || !hci.ops->cmd || !hci.ops->pump || !hci.ops->next_event)
        return VERSION_QUERY_INVALID;
    r->command_attempts = 1;
    if (!hci.ops->cmd(hci.ctx, 0x1001, NULL, 0)) return VERSION_QUERY_SEND_FAILED;
    r->commands = 1;
    for (round = 0; round < 60; round++) {
        r->pumps++;
        if (hci.ops->pump(hci.ctx, 50) < 0) return VERSION_QUERY_TRANSPORT_LOST;
        for (j = 0; j < 64; j++) {
            int len = hci.ops->next_event(hci.ctx, event, sizeof event);
            if (len == 0) break;
            if (len < 0 || len > (int)sizeof event) return VERSION_QUERY_TRANSPORT_LOST;
            r->events++;
            if (len >= 5 && event[0] == 0x0e && word(event + 3) == 0x1001) {
                if (len < 6 || event[1] != len - 2) return VERSION_QUERY_MALFORMED;
                r->status = event[5];
                if (r->status) return VERSION_QUERY_CONTROLLER_ERROR;
                if (len != 14) return VERSION_QUERY_MALFORMED;
                r->hci_version = event[6]; r->hci_revision = word(event + 7);
                r->lmp_version = event[9]; r->manufacturer = word(event + 10);
                r->lmp_subversion = word(event + 12);
                return VERSION_QUERY_OK;
            }
            if (len >= 6 && event[0] == 0x0f && word(event + 4) == 0x1001) {
                if (len != 6 || event[1] != 4) return VERSION_QUERY_MALFORMED;
                r->status = event[2];
                if (r->status) return VERSION_QUERY_CONTROLLER_ERROR;
            }
        }
    }
    return VERSION_QUERY_TIMEOUT;
}
