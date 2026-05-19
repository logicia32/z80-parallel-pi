/* #58 SPMD node: parallel fixed-point trapezoidal pi.
 *   integral_0^1 4/(1+x^2) dx = pi, Q14, 32-bit only.
 * Boot: read [rank,size]. Each rank sums its panel range; non-zero ranks
 * send the partial sum to rank 0; rank 0 reduces (+own), * h, prints,
 * and reports the result frame (DST 0xFE) for the router to verify.
 *
 * Panels split by *panel* (not point): boundary points are shared by the
 * two adjacent panels, so global endpoint 1/2-weights come out right
 * automatically. The naive point-split-and-halve is the war-story bug.
 */
#include <stdio.h>
#include "console.h"
#include "mpi.h"

#ifndef N
#define N     64UL                 /* total trapezoid panels */
#endif
#include "pi_kernel.h"          /* SAME kernel as single-node & shared-mem */

void main(void)
{
    unsigned char rank = mpi_rx();
    unsigned char size = mpi_rx();
    unsigned long start, end, partial;

    start = ((unsigned long)rank * N) / size;
    end   = (rank == (unsigned char)(size - 1))
            ? N : (((unsigned long)rank + 1) * N) / size;

    partial = trap_panels(start, end);         /* this rank's panels */

    if (rank != 0) {
        unsigned char b[4];
        b[0] = partial & 0xff;       b[1] = (partial >> 8) & 0xff;
        b[2] = (partial >> 16) & 0xff; b[3] = (partial >> 24) & 0xff;
        mpi_send_frame(0, b, 4);
    } else {
        unsigned long total = partial, r, ip, frac4;
        unsigned char k, pb[8], rb[4];
        for (k = 1; k < size; k++) {
            (void)mpi_recv_frame(pb);
            total += (unsigned long)pb[0] | ((unsigned long)pb[1] << 8)
                   | ((unsigned long)pb[2] << 16) | ((unsigned long)pb[3] << 24);
        }
        r     = total / N;                      /* * h, h = 1/N */
        ip    = r >> QBITS;
        frac4 = ((r & (SCALE - 1)) * 10000UL) / SCALE;
        printf("pi~=%lu.%04lu (Q%u,N=%lu,P=%u)\n",
               ip, frac4, (unsigned)QBITS, (unsigned long)N, (unsigned)size);
        rb[0] = r & 0xff;       rb[1] = (r >> 8) & 0xff;
        rb[2] = (r >> 16) & 0xff; rb[3] = (r >> 24) & 0xff;
        mpi_send_frame(0xFE, rb, 4);            /* report to router */
    }
    sim_exit();
    for (;;) ;
}
