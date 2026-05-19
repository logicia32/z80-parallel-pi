#ifndef MACHINE_H
#define MACHINE_H
#include "z80.h"

/* Self-built multicore on chips z80.h:
 *   0x0000..0xBFFF  per-core PRIVATE RAM (code / data / stack)
 *   0xC000..0xFFFF  the ONE SHARED-RAM window, via the bus arbiter
 * The 32-bit pi accumulator lives at 0xC000 (shared[0..3]); every core
 * read-modify-writes it there. A HW semaphore + a barrier (I/O ports)
 * are sync devices around that shared-RAM reduction. Each shared access
 * is metered (count + ownership handover = contention). */
#define SHARED_BASE 0xC000
#define SHARED_SIZE 0x4000

typedef struct BUS {
    unsigned char shared[SHARED_SIZE];    /* shared[0..3] = acc @0xC000 */
    long reads, writes, contention;
    int  last_owner;
    int  lock_owner;
    long lock_acquires, lock_spins;
    int  barrier_count, barrier_target;   /* sync before the shared RMW */
} BUS;

typedef struct CORE {
    int           id;
    int           size;
    int           halted;
    long          cycles;
    unsigned char priv[SHARED_BASE];
    BUS          *bus;
    int           out_port;
    int           nic_mbox;    /* NIC mailbox byte (read by the ISR)   */
    int           nic_taken;   /* set when the ISR has read the mbox   */
    int           int_vec;     /* IM2 vector this core's NIC supplies  */
    int           int_req;     /* harness: assert /INT until serviced  */
    int           int_acked;   /* set when the int-ack cycle occurred  */
    long          int_taken;   /* count of IRQs actually accepted      */
    int           ready;       /* Z80 OUT P_READY: IM2 armed           */
    int           result;      /* Z80 OUT P_RESULT: received checksum  */
    int           bar_arrived; /* counted once at the barrier          */
    z80_t         cpu;
    uint64_t      pins;
} CORE;

unsigned char mc_rb(CORE *c, int addr);
void          mc_wb(CORE *c, int addr, unsigned char v);
unsigned char io_in(CORE *c, int port);
void          io_out(CORE *c, int port, unsigned char v);
void          core_tick(CORE *c);

extern long g_out_calls;
extern int  g_out_last_port;
extern int  g_out_last_val;
extern long g_io_log;
#endif
