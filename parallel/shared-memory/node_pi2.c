/* shared-memory workload: trapezoidal pi via a SHARED-RAM accumulator.
 *
 * The compute kernel is ../../pi_kernel.h — the SAME single source the
 * single-node and message-passing builds include. (2)'s only difference
 * is the reduction: every core read-modify-writes ONE 32-bit accumulator
 * that lives in the shared-RAM window (0xC000), reached over the bus
 * arbiter. The stack is private (crt0 SP forced to 0xBF00 by the host,
 * below the 0xC000 shared window), so the shared accumulator and the
 * stack never collide.
 *
 *   USE_LOCK : take a hardware semaphore (P_LOCK/P_UNLOCK) around the
 *              read-modify-write -> correct, equals Part (1)/(2-(1)).
 *   (no lock): the read-modify-write races -> the classic lost update.
 *              Whatever value it lands on is reported as measured; it is
 *              NOT asserted to be any particular number.
 *
 * P_ID/P_SIZE/P_BARRIER/P_DONE are harness sync devices (a real SMP has
 * HW mailboxes/barriers too); the *data* reduction is the shared-RAM
 * accumulator, which is the point of "shared memory".
 */
#ifndef N
#define N     64UL
#endif
#include "pi_kernel.h"          /* SAME kernel as single-node & dist-mem */

__sfr __at (0x01) P_ID;
__sfr __at (0x02) P_SIZE;
__sfr __at (0x03) P_LOCK;     /* IN: 0=acquired, 1=busy (HW semaphore) */
__sfr __at (0x04) P_UNLOCK;   /* OUT: release the semaphore            */
__sfr __at (0x08) P_DONE;     /* OUT: host stops scheduling this core   */
__sfr __at (0x09) P_BARRIER;  /* IN: 1 once every core has arrived      */

/* THE shared datum. __at(0xC000) is inside the shared-RAM window, so
 * every access is a real bus load/store the arbiter serves from shared
 * RAM (and meters as contention) — not an I/O port. */
static volatile unsigned long __at (0xC000) g_acc;

void main(void)
{
    unsigned char rank = P_ID;
    unsigned char size = P_SIZE;
    unsigned long start, end, partial, acc;

    start = ((unsigned long)rank * N) / size;
    end   = (rank == (unsigned char)(size - 1))
            ? N : (((unsigned long)rank + 1) * N) / size;

    partial = trap_panels(start, end);   /* same kernel as Part (1) */

    while (!P_BARRIER)         /* all cores converge, then contend */
        ;
#ifdef USE_LOCK
    while (P_LOCK)             /* spin until the HW semaphore is ours */
        ;
#endif
    acc  = g_acc;             /* READ   shared accumulator (shared RAM) */
    acc += partial;           /* MODIFY                                 */
    g_acc = acc;              /* WRITE  back to shared RAM              */
#ifdef USE_LOCK
    P_UNLOCK = 0;
#endif

    P_DONE = 0;
    for (;;)
        ;
}
