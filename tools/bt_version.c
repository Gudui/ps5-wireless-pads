/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hci_usb.h"
#include "version_query.h"
#include "log.h"
#include <unistd.h>
#ifndef PADBRIDGE_BUILD_ID
#define PADBRIDGE_BUILD_ID "unknown"
#endif
int main(void)
{
    hci_t hci;
    version_query_result result;
    int status, cleanup;
    log_open("/data/prospero-win/wii-bt-version.log");
    log_line("BT_VERSION begin source=%s pid=%d opcode=1001 max_pumps=60 pump_ms=50",
             PADBRIDGE_BUILD_ID, (int)getpid());
    if (!hci_usb_open_event_query(&hci)) {
        log_line("BT_VERSION result=transport-unavailable commands=0"); log_close(); return 1;
    }
    status = version_query(hci, &result);
    if (status == VERSION_QUERY_OK)
        log_line("BT_VERSION complete opcode=1001 status=0 hci=%u revision=%04x lmp=%u manufacturer=%04x subversion=%04x",
                 result.hci_version, result.hci_revision, result.lmp_version,
                 result.manufacturer, result.lmp_subversion);
    cleanup = hci_usb_finish_event_query(&hci);
    log_line("BT_VERSION result=%s query_status=%d controller_status=%u commands=%u attempts=%u pumps=%u events=%u cleanup=%s",
             status == VERSION_QUERY_OK && cleanup ? "ok" : "inconclusive",
             status, result.status, result.commands, result.command_attempts,
             result.pumps, result.events, cleanup ? "ok" : "failed");
    log_close(); return status == VERSION_QUERY_OK && cleanup ? 0 : 1;
}
