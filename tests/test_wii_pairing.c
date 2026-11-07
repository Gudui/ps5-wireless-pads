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
    /* Long enough for a cancelled name request (3 s + 1 s) or one page
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
    /* The name is asked over the link, never by paging the pad for it. */
    CHECK(sim_names(sim) >= 1);
    CHECK(sim_names_unlinked(sim) == 0);
    if (no_name) CHECK(sim_name_cancels(sim) == 1);
    else CHECK(sim_name_cancels(sim) == 0);
    CHECK(sim_page_cancels(sim) == unanswered_pages);
    host_close(host); sim_free(sim);
    unlink("build/test_wii_pads.db");
}
typedef struct { int connects, states; pad_info info; pad_state last; } seen;
static void on_connect(void *ud, int slot, const pad_info *info)
{
    seen *s = ud; (void)slot; s->connects++; s->info = *info;
}
static void on_state(void *ud, int slot, const pad_state *st)
{
    seen *s = ud; (void)slot; s->states++; s->last = *st;
}
/* After red-Sync pairing, the remote is set up as a Wii Remote straight
 * away: no SDP (its name gave its ids), the extension's id read, and its
 * guitar's frets and strum reaching the pad state. wii: 1 guitar, 2 none. */
static void check_wii_ready(int wii)
{
    sim_pad pad = {0};
    seen s = {0};
    host_events events = { on_connect, on_state, NULL, &s, NULL };
    sim_t *sim;
    host_t *host;
    long end;
    unlink("build/test_wii_pads.db");
    pad.mode = SIM_WAIT_PAIRING;
    pad.addr[0] = 0xfd; pad.addr[1] = 0x4e; pad.addr[2] = 0x7e;
    pad.addr[3] = 0xaa; pad.addr[4] = 0x22;
    pad.cod = 0x002504;
    pad.vid = 0x057e; pad.pid = 0x0306;
    pad.remote_name = "Nintendo RVL-CNT-01";
    pad.legacy_pin = 1;
    pad.wii = wii;
    pad.needs_output = 1;
    pad.basic[0] = 0x30; pad.basic[2] = 0x08; pad.basic_len = 3;            /* A */
    pad.report[0] = 0x32; pad.report[2] = 0x08;
    pad.report[3] = 0x20; pad.report[4] = 0x20; pad.report[5] = 0x0F; pad.report[6] = 0x10;
    pad.report[7] = (unsigned char)~0x40;                                   /* strum down */
    pad.report[8] = (unsigned char)~0x10;                                   /* green */
    pad.report_len = 11;
    sim = sim_new(&pad);
    host = host_open(sim_hci(sim), "build/test_wii_pads.db", &events);
    CHECK(host != NULL);
    if (!host) { sim_free(sim); return; }
    host_pair(host, 60);
    end = sim_now() + 20000;
    while (sim_now() < end) host_poll(host, 5);
    CHECK(s.connects == 1);
    CHECK(s.info.vid == 0x057e && s.info.pid == 0x0306);
    CHECK(s.info.profile && !strcmp(s.info.profile, "Wii Remote"));
    CHECK(sim_sdp_queries(sim) == 0);
    CHECK(s.states > 0);
    if (wii == 1) CHECK(s.last.buttons == (PAD_CROSS | PAD_DOWN));
    else CHECK(s.last.buttons == PAD_CROSS);
    CHECK(sim_last_output(sim, (unsigned char[32]){0}, 32) == 4);           /* 0xA2 0x12 0x04 0x32 */
    host_close(host); sim_free(sim);
    unlink("build/test_wii_pads.db");
}
int main(void)
{
    check_wii_ready(1);
    check_wii_ready(2);
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
