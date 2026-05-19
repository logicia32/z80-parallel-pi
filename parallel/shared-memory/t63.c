/* #63 (chips z80.h): the IDEAL -- the EXACT same SDCC Q14 trapezoid
 * binary as Part (1), run on N z80.h cores, reducing into a shared
 * accumulator. Locked build must match Part (1): raw Q14 51470 =
 * 3.14148. Race build (no lock) = lost updates.
 *   ./t63 <ihx> [P]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "z80.h"
#include "machine.h"

long g_out_calls = 0;
int  g_out_last_port = -1;
int  g_out_last_val = -1;

#define N        64UL
#define EXPECT   51470UL
#define MAXCORES 8
#define PRIV_SP  0xBF00
#define QUANTUM  32

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

int main(int argc, char **argv)
{
    static unsigned char img[65536];
    static BUS bus;
    static CORE core[MAXCORES];
    const char *ihx = argc > 1 ? argv[1] : "np2_lock.ihx";
    int P = argc > 2 ? atoi(argv[2]) : 4;
    int i, q, allh;
    long iter = 0;
    unsigned long val, raw;

    if (P < 1 || P > MAXCORES) P = 4;
    if (!load_ihx(ihx, img)) { fprintf(stderr,"load %s\n",ihx); return 2; }

    if (img[0x100]==0x31 && img[0x101]==0 && img[0x102]==0) {
        img[0x101]=PRIV_SP&0xff; img[0x102]=(PRIV_SP>>8)&0xff;
        printf("  [crt0 SP -> 0x%04X private]\n", PRIV_SP);
    } else {
        printf("  [WARN crt0 pattern @0x100: %02x %02x %02x]\n",
               img[0x100],img[0x101],img[0x102]);
    }

    memset(&bus,0,sizeof bus);
    bus.lock_owner=-1; bus.last_owner=-1;
    bus.barrier_target = P;        /* cores converge before the RMW */

    for (i = 0; i < P; i++) {
        core[i].id=i; core[i].size=P; core[i].halted=0; core[i].cycles=0;
        core[i].bus=&bus; core[i].int_vec=0; core[i].int_req=0;
        core[i].int_acked=0;
        memcpy(core[i].priv, img, SHARED_BASE);
        core[i].pins = z80_init(&core[i].cpu);
    }

    do {
        allh = 1;
        for (i = 0; i < P; i++) {
            if (core[i].halted) continue;
            allh = 0;
            for (q = 0; q < QUANTUM && !core[i].halted; q++)
                core_tick(&core[i]);
        }
        iter++;
    } while (!allh && iter < 4000000L);

    /* the accumulator is the first 4 bytes of the shared-RAM window
     * (address 0xC000); read it straight out of shared RAM. */
    val = (unsigned long)bus.shared[0] | ((unsigned long)bus.shared[1]<<8)
        | ((unsigned long)bus.shared[2]<<16) | ((unsigned long)bus.shared[3]<<24);
    raw = val / N;

    printf("  ihx=%s P=%d\n", ihx, P);
    for (i = 0; i < P; i++)
        printf("    core%d cycles=%ld\n", i, core[i].cycles);
    printf("  shared acc=%lu  raw=acc/N=%lu  pi~=%.5f\n",
           val, raw, raw/16384.0);
    printf("  lock: acquires=%ld spins=%ld   bus(acc): reads=%ld "
           "writes=%ld contention=%ld\n",
           bus.lock_acquires, bus.lock_spins,
           bus.reads, bus.writes, bus.contention);
    printf("  expected raw=%lu (Part (1))  -> %s\n",
           EXPECT, raw==EXPECT ? "MATCH (1)" : "MISMATCH (race?)");
    return raw==EXPECT ? 0 : 1;
}
