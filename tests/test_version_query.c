/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "version_query.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    unsigned commands, pumps, events;
    int send_error, lost, flood, repeat;
    unsigned char event[32]; int len;
} fake;
static int command(void *ctx, unsigned op, const void *params, int len)
{
    fake *f = ctx;
    assert(op == 0x1001 && !params && len == 0);
    f->commands++; return !f->send_error;
}
static int pump(void *ctx, int timeout)
{
    fake *f = ctx; assert(timeout == 50); f->pumps++;
    return f->lost ? -1 : 1;
}
static int next(void *ctx, unsigned char *out, int max)
{
    fake *f = ctx;
    if (f->flood) { out[0] = 0xff; out[1] = 0; f->events++; return 2; }
    if (!f->len || (f->events && !f->repeat)) return 0;
    assert(max >= f->len); memcpy(out, f->event, (size_t)f->len);
    f->events++; return f->len;
}
static const hci_ops ops = { command, NULL, pump, next, NULL, NULL, NULL };
static const unsigned char complete[] = {0x0e,12,1,1,0x10,0, 9,0x34,0x12,9,0x46,0,0x78,0x56};
static void setup(fake *f) { memset(f, 0, sizeof *f); memcpy(f->event, complete, sizeof complete); f->len = sizeof complete; }
int main(void)
{
    fake f; version_query_result r; hci_t h = {&ops, &f}; int len;
    setup(&f); assert(version_query(h, &r) == VERSION_QUERY_OK);
    assert(f.commands == 1 && r.commands == 1 && r.pumps == 1);
    assert(r.hci_version == 9 && r.hci_revision == 0x1234 &&
           r.lmp_version == 9 && r.manufacturer == 0x46 && r.lmp_subversion == 0x5678);
    for (len = 0; len < (int)sizeof complete; len++) {
        setup(&f); f.len = len;
        assert(version_query(h, &r) != VERSION_QUERY_OK && f.commands == 1);
    }
    setup(&f); f.event[1]--; assert(version_query(h, &r) == VERSION_QUERY_MALFORMED);
    setup(&f); f.event[5] = 1; f.len = 6; f.event[1] = 4;
    assert(version_query(h, &r) == VERSION_QUERY_CONTROLLER_ERROR && r.status == 1);
    setup(&f); f.event[3] = 2; f.repeat = 1;
    assert(version_query(h, &r) == VERSION_QUERY_TIMEOUT && f.commands == 1);
    setup(&f); f.flood = 1;
    assert(version_query(h, &r) == VERSION_QUERY_TIMEOUT);
    assert(f.commands == 1 && f.pumps == 60 && f.events == 60 * 64);
    setup(&f); f.send_error = 1;
    assert(version_query(h, &r) == VERSION_QUERY_SEND_FAILED && f.pumps == 0 && r.commands == 0);
    setup(&f); f.lost = 1;
    assert(version_query(h, &r) == VERSION_QUERY_TRANSPORT_LOST && f.commands == 1);
    setup(&f); f.event[0]=0x0f; f.event[1]=4; f.event[2]=5; f.event[3]=1;
    f.event[4]=1; f.event[5]=0x10; f.len=6;
    assert(version_query(h,&r)==VERSION_QUERY_CONTROLLER_ERROR && r.status==5);
    setup(&f); h.ops = NULL;
    assert(version_query(h, &r) == VERSION_QUERY_INVALID && f.commands == 0);
    puts("all bounded version query checks passed"); return 0;
}
