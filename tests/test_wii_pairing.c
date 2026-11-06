/* End-to-end PIN selection through discovery, name lookup and authentication. */
#include "sim.h"
#include "../src/host.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int failed;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); failed++; } } while (0)
static void check_pair(const char *name, int no_name, int name_status, int legacy, int wii, int no_host_address,
                       int unanswered_pages)
{
    static const unsigned char host_pin[6] = {0x66, 0, 0x44, 0x33, 0x22, 0x11};
    sim_pad pad = {0};
    host_events events = {0};
    unsigned char pin[23];
    sim_t *sim;
    host_t *host;
    long end;
    unlink("build/test_wii_pads.db");
    pad.mode = SIM_WAIT_PAIRING;
    pad.addr[0] = 0xfd; pad.addr[1] = 0x4e; pad.addr[2] = 0x7e;
    pad.addr[3] = 0xaa; pad.addr[4] = 0x22;
    pad.cod = 0x002504;
    pad.vid = legacy == 1 ? 0x057e : 0x054c;
    pad.pid = legacy == 1 ? 0x0306 : 0x05c4;
    pad.remote_name = name; pad.no_name_reply = no_name;
    pad.no_host_address = no_host_address;
    pad.name_status = name_status; pad.legacy_pin = legacy;
    pad.unanswered_pages = unanswered_pages;
    sim = sim_new(&pad);
    host = host_open(sim_hci(sim), "build/test_wii_pads.db", &events);
    CHECK(host != NULL);
    if (!host) { sim_free(sim); return; }
    host_pair(host, 60);
    /* Long enough for a cancelled name request (10 s + 2 s) or one page
     * left unanswered (20 s), then a fresh discovery and pairing. */
    end = sim_now() + 45000;
    while (sim_now() < end && (no_host_address || sim_auth_successes(sim) == 0)) host_poll(host, 5);
    if (no_host_address) CHECK(sim_auth_successes(sim) == 0);
    else CHECK(sim_auth_successes(sim) >= 1);
    if (legacy && !no_host_address) {
        CHECK(sim_pin_replies(sim, pin) >= 1);
        CHECK(!memcmp(pin, pad.addr, 6));
        CHECK(pin[6] == (wii ? 6 : 4));
        CHECK(!memcmp(pin + 7, wii ? host_pin : (const unsigned char *)"0000", wii ? 6 : 4));
        for (int i = 7 + (wii ? 6 : 4); i < 23; i++) CHECK(pin[i] == 0);
    } else CHECK(sim_pin_replies(sim, pin) == 0);
    /* Never page over a name request or a page still running. */
    CHECK(sim_refused(sim) == 0);
    if (no_name) CHECK(sim_name_cancels(sim) == 1);
    else CHECK(sim_name_cancels(sim) == 0);
    CHECK(sim_page_cancels(sim) == unanswered_pages);
    host_close(host); sim_free(sim);
    unlink("build/test_wii_pads.db");
}
int main(void)
{
    check_pair("Nintendo RVL-CNT-01", 0, 0, 1, 1, 0, 0);
    check_pair("Nintendo RVL-CNT-01-TR", 0, 0, 1, 1, 0, 0);
    check_pair("Nintendo RVL-CNT-01-UC", 0, 0, 2, 0, 0, 0);
    check_pair("Other legacy controller", 0, 0, 2, 0, 0, 0);
    check_pair("Nintendo RVL-CNT-01", 1, 0, 2, 0, 0, 0);
    check_pair("Nintendo RVL-CNT-01", 0, 1, 2, 0, 0, 0);
    check_pair("DualShock 4", 0, 0, 0, 0, 0, 0);
    check_pair("Nintendo RVL-CNT-01", 0, 0, 1, 1, 1, 0);
    /* The first page goes unanswered: cancelled at the timeout, the next
     * discovery pages again and the red-Sync PIN still applies. */
    check_pair("Nintendo RVL-CNT-01", 0, 0, 1, 1, 0, 1);
    printf(failed ? "%d Wii pairing checks failed\n" : "all Wii legacy pairing checks passed\n", failed);
    return failed != 0;
}
