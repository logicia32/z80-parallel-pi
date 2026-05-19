/* irq_host.c — drives the IM2 interrupt-driven receive demo.
 *
 * One z80.h core runs isrdemo.ihx (crt0_irq.s arms IM2 + EI). This host
 * is the "NIC": it delivers K bytes to the core PURELY by raising /INT.
 * The core's main() never polls the NIC — only the hand-written IM2 ISR
 * reads it. We verify two things, so the claim cannot be faked:
 *   1. the checksum the Z80 reports == the checksum we sent, and
 *   2. exactly K interrupts were actually accepted (core.int_taken).
 *
 *   ./irq_host [isrdemo.ihx]   -> prints PASS/FAIL, exit 0 on PASS
 */
#include <stdio.h>
#include <string.h>
#include "z80.h"
#include "machine.h"

long g_out_calls = 0;
int  g_out_last_port = -1;
int  g_out_last_val = -1;

static int hexb(const char *s){int h=s[0]<='9'?s[0]-'0':(s[0]|32)-'a'+10,
 l=s[1]<='9'?s[1]-'0':(s[1]|32)-'a'+10;return(h<<4)|l;}

static int load_ihx(const char *path, unsigned char *mem)
{
    char line[600]; FILE *f = fopen(path, "r"); int i;
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        int n, addr, type;
        if (line[0] != ':') continue;
        n = hexb(line+1); addr=(hexb(line+3)<<8)|hexb(line+5);
        type = hexb(line+7);
        if (type == 1) break;
        if (type) continue;
        for (i = 0; i < n; i++) mem[(addr+i)&0xffff]=hexb(line+9+i*2);
    }
    fclose(f); return 1;
}

#define SETTLE 4000L          /* let the ISR RETI and main consume */

static long run_until(CORE *c, int (*done)(CORE *), long cap)
{
    long t = 0;
    while (!done(c) && t < cap) { core_tick(c); t++; }
    return t;
}
static int is_ready(CORE *c)  { return c->ready; }
static int is_taken(CORE *c)  { return c->nic_taken; }
static int is_halted(CORE *c) { return c->halted; }

int main(int argc, char **argv)
{
    static unsigned char img[65536];
    static BUS bus;
    static CORE core;
    const char *ihx = argc > 1 ? argv[1] : "isrdemo.ihx";
    const unsigned char payload[4] = { 0x11, 0x22, 0x33, 0x44 };
    int K = (int)sizeof payload, i, expect = 0;
    long t;

    if (!load_ihx(ihx, img)) { fprintf(stderr, "load %s\n", ihx); return 2; }
    for (i = 0; i < K; i++) expect = (expect + payload[i]) & 0xff;

    memset(&bus, 0, sizeof bus);
    bus.lock_owner = -1; bus.last_owner = -1;
    memset(&core, 0, sizeof core);
    core.id = 0; core.size = 1; core.bus = &bus;
    core.int_vec = 0x00;                 /* table @ (I=0x02)<<8 | 0 = 0x0200 */
    memcpy(core.priv, img, SHARED_BASE);
    core.pins = z80_init(&core.cpu);

    /* phase 1: wait until the Z80 has armed IM2 and said "go" */
    t = run_until(&core, is_ready, 2000000L);
    if (!core.ready) { printf("IRQ DEMO: FAIL (core never armed IM2)\n");
                       return 1; }

    /* phase 2: deliver each byte purely by interrupt */
    for (i = 0; i < K; i++) {
        core.nic_mbox  = payload[i];
        core.nic_taken = 0;
        core.int_acked = 0;
        core.int_req   = 1;             /* raise /INT (level) */
        run_until(&core, is_taken, 1000000L);
        core.int_req   = 0;             /* device serviced */
        core.int_acked = 0;
        if (!core.nic_taken) {
            printf("IRQ DEMO: FAIL (byte %d never taken by ISR)\n", i);
            return 1;
        }
        t = 0; while (t < SETTLE && !core.halted) { core_tick(&core); t++; }
    }

    /* phase 3: let main finish and report */
    run_until(&core, is_halted, 1000000L);

    printf("IRQ DEMO (IM2, hand-written ISR):\n");
    printf("  bytes delivered by interrupt : %d\n", K);
    printf("  interrupts actually taken    : %ld\n", core.int_taken);
    printf("  checksum  expected=0x%02X  z80-reported=0x%02X\n",
           expect, core.result & 0xff);
    if (core.int_taken == K && (core.result & 0xff) == expect
        && core.halted) {
        printf("  -> PASS (received only via the IM2 ISR)\n");
        return 0;
    }
    printf("  -> FAIL\n");
    return 1;
}
