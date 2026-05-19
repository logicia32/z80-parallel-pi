/* bus arbiter: 0x0000..0xBFFF is per-core private RAM, 0xC000..0xFFFF is
 * the ONE shared-RAM window every core sees (the 32-bit pi accumulator
 * lives there). Every shared access is metered: a read/write plus a
 * "contention" tick whenever ownership of the bus changes hands. The
 * I/O ports here are only sync devices (HW semaphore, barrier, done) --
 * the reduced *data* is in shared RAM, not in a port. */
#include <stdio.h>
#include "machine.h"

unsigned char mc_rb(CORE *c, int addr)
{
    addr &= 0xffff;
    if (addr < SHARED_BASE)
        return c->priv[addr];
    {
        BUS *b = c->bus;
        b->reads++;
        if (b->last_owner != c->id) { b->contention++; b->last_owner = c->id; }
        return b->shared[addr - SHARED_BASE];
    }
}

void mc_wb(CORE *c, int addr, unsigned char v)
{
    addr &= 0xffff;
    if (addr < SHARED_BASE) { c->priv[addr] = v; return; }
    {
        BUS *b = c->bus;
        b->writes++;
        if (b->last_owner != c->id) { b->contention++; b->last_owner = c->id; }
        b->shared[addr - SHARED_BASE] = v;
    }
}

#define P_ID 0x01
#define P_SIZE 0x02
#define P_LOCK 0x03
#define P_UNLOCK 0x04
#define P_DONE 0x08
#define P_BARRIER 0x09   /* IN: 1 once all cores have arrived */
#define P_READY 0x0A     /* OUT: Z80 says "IM2 armed, deliver now" */
#define P_RESULT 0x0B    /* OUT: Z80 reports the received checksum */
#define P_NIC 0x10       /* IN: NIC mailbox (interrupt-driven receive) */

long g_io_log = 0;

unsigned char io_in(CORE *c, int port)
{
    BUS *b = c->bus;
    if (g_io_log > 0) { g_io_log--;
        fprintf(stderr, "  IN  core%d port=0x%02x\n", c->id, port & 0xff); }
    switch (port & 0xff) {
    case P_ID:   return (unsigned char) c->id;
    case P_SIZE: return (unsigned char) c->size;
    case P_LOCK:
        if (b->lock_owner < 0) { b->lock_owner = c->id; b->lock_acquires++;
                                 return 0; }
        b->lock_spins++;
        return 1;
    case P_BARRIER:
        if (!c->bar_arrived) { c->bar_arrived = 1; b->barrier_count++; }
        return (unsigned char)(b->barrier_count >= b->barrier_target);
    case P_NIC:  c->nic_taken = 1;     /* ISR consumed the mailbox  */
                 return (unsigned char) c->nic_mbox;
    default:     return (unsigned char) c->id;
    }
}

void io_out(CORE *c, int port, unsigned char v)
{
    BUS *b = c->bus;
    g_out_calls++;
    g_out_last_port = port & 0xff;
    g_out_last_val = v;
    if (g_io_log > 0) { g_io_log--;
        fprintf(stderr, "  OUT core%d port=0x%02x v=0x%02x\n",
                c->id, port & 0xff, v); }
    switch (port & 0xff) {
    case P_UNLOCK:
        if (b->lock_owner == c->id) b->lock_owner = -1;
        return;
    case P_READY:
        c->ready = 1;              /* IM2 armed; host may deliver */
        return;
    case P_RESULT:
        c->result = v;             /* received checksum */
        return;
    case P_DONE:
        c->halted = 1;
        return;
    default:
        c->out_port = v;
        return;
    }
}
