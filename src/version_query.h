/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PADBRIDGE_VERSION_QUERY_H
#define PADBRIDGE_VERSION_QUERY_H
#include "hci.h"
#include <stdint.h>
enum {
    VERSION_QUERY_OK, VERSION_QUERY_INVALID, VERSION_QUERY_SEND_FAILED,
    VERSION_QUERY_TRANSPORT_LOST, VERSION_QUERY_CONTROLLER_ERROR,
    VERSION_QUERY_MALFORMED, VERSION_QUERY_TIMEOUT
};
typedef struct {
    unsigned command_attempts, commands, pumps, events;
    uint8_t status, hci_version, lmp_version;
    uint16_t hci_revision, manufacturer, lmp_subversion;
} version_query_result;
/* One1001 command, no retry; at most60x50ms pumps and64 events per pump.
 * The caller owns transport setup and cleanup. No ACL or host initialization. */
int version_query(hci_t hci, version_query_result *result);
#endif
