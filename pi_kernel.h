/* pi_kernel.h — the Q14 fixed-point trapezoidal kernel (single source).
 *
 * This one file is #included verbatim by all three builds:
 *   methods/trapezoid.c                     single node
 *   parallel/distributed-memory/node_pi.c   message passing (SPMD)
 *   parallel/shared-memory/node_pi2.c       shared-memory multicore
 *
 * ONLY the reduction (how each part's partial sum is combined) differs
 * between them. The arithmetic below is byte-for-byte identical, which
 * is *why* pi comes out bit-identical across single-node, message
 * passing, and shared memory, and across P = 1..4. A reader can verify
 * the claim with one `grep -rn pi_kernel.h` — there is exactly one copy.
 *
 * N (the panel count) is the experiment's knob and MUST be #defined
 * before including this header. It is deliberately a compile-time
 * constant: the divisor in (SCALE * i) / N then stays constant, so SDCC
 * keeps it a cheap shift instead of emitting a slow 32-bit runtime
 * division. (Pass it through as a runtime argument and the Z80 clock
 * count balloons ~1.6x — measured. The performance story in the
 * articles depends on this staying honest.)
 *
 * Z80 has no FPU. SDCC's long long (64-bit) runtime is far too slow, so
 * everything is unsigned 32-bit Q14 (value = real * 2^14). Q14 is the
 * widest format whose intermediates still fit in 32 bits:
 *   x_fp          <= 16384 = 2^14
 *   x_fp^2        <= 2^28
 *   4*SCALE*SCALE  = 2^30                 (all < 2^32)
 *
 * Those bounds make fx() overflow-free unconditionally. The trapezoid
 * accumulator below carries one extra, N-dependent condition: s grows by
 * up to 4*SCALE per panel (~N*65536 total) and SCALE*(i+1) reaches
 * ~N*16384, so the 32-bit guarantee holds for N up to ~65535 — which
 * amply covers this experiment's N=64 and N=1024. A far larger N would
 * need a wider accumulator; the kernel stays within that honest range.
 */
#ifndef PI_KERNEL_H
#define PI_KERNEL_H

#ifndef N
#error "define N (panel count) before #include \"pi_kernel.h\""
#endif

#define QBITS 14
#define SCALE (1UL << QBITS)        /* 16384 : Q14 representation of 1.0 */

/* f(x) = 4 / (1 + x^2). Argument and result are both Q14. */
static unsigned long fx(unsigned long x_fp)
{
    unsigned long xx  = (x_fp * x_fp) >> QBITS;   /* x^2     Q14 */
    unsigned long den = SCALE + xx;               /* 1 + x^2 Q14 */
    unsigned long num = 4UL * SCALE * SCALE;      /* = 2^30      */
    return num / den;                             /* 4/(1+x^2) Q14 */
}

/* Sum of the trapezoid panels [start, end) of the total-N partition, Q14.
 *
 * Each panel contributes (f(left) + f(right)) / 2. Adjacent panels share
 * a boundary point, so the global endpoint 1/2-weights fall out
 * automatically and any split of [0,N) into contiguous panel ranges adds
 * up — over integers, associatively — to exactly the same total. The
 * full integral estimate is trap_panels(0, N) / N  (the * h, h = 1/N).
 *
 * Single node : trap_panels(0, N)
 * Per rank/core: trap_panels(start, end), then the partials are reduced
 *                (sent & summed, or RMW'd into shared memory).
 */
static unsigned long trap_panels(unsigned long start, unsigned long end)
{
    unsigned long i, prev, cur, s = 0;

    prev = fx((SCALE * start) / N);
    for (i = start; i < end; i++) {
        cur = fx((SCALE * (i + 1)) / N);
        s  += (prev + cur) >> 1;                  /* (f_i + f_{i+1}) / 2 */
        prev = cur;
    }
    return s;
}

#endif /* PI_KERNEL_H */
