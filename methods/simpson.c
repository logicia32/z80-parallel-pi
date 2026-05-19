/* simpson.c — シンプソン公式で pi を求める(単一ノード版・Q14 固定小数点)
 *
 *   pi = ∫_0^1 4/(1+x^2) dx
 *
 * 台形公式が各小区間を「直線」で近似するのに対し、シンプソン公式は
 * 2 小区間を 1 組にして「放物線(2 次曲線)」で近似する。同じ分割数なら
 * 台形よりずっと精度が高い(誤差が h^4 で減る。台形は h^2)。
 *
 *   ∫ ≈ (h/3) [ f0 + fN + 4*(f1+f3+...) + 2*(f2+f4+...) ],  h = 1/N
 *
 * ここでは「2 小区間 = 1 ペア」に分けて
 *   ペア j の寄与 = f_{2j} + 4 f_{2j+1} + f_{2j+2}
 * を足し込み、最後に S/(3N) で割る等価な形にしている。ペアごとに独立
 * なので、並列版では「ペアの範囲」をコアに配るだけで分割でき、しかも
 * 整数和なので分割の仕方に依らずビット単位で同じ答えになる。
 *
 * Q14・32bit に収まる確認(N=1024 のとき):
 *   f_fp <= 4*SCALE = 2^16 ;  ペア寄与 <= 6*2^16 ;
 *   ペア数 = N/2 = 512 ;  総和 S <= 512*6*2^16 < 2^28  (< 2^32)
 *
 * 注意: N は必ず偶数(ペアに割り切れる)にすること。
 */
#include <stdio.h>
#include "console.h"

#define QBITS 14
#define SCALE (1UL << QBITS)        /* 16384 : Q14 の 1.0 */
#ifndef N
#define N     1024UL                /* 分割数(偶数, -DN= で上書き可) */
#endif
#if (N % 2u) != 0u
#error "Simpson needs an even N (2 小区間で 1 ペア)。make N=<even>"
#endif

/* f(x) = 4/(1+x^2)。x も戻り値も Q14。 */
static unsigned long fx(unsigned long x_fp)
{
    unsigned long xx  = (x_fp * x_fp) >> QBITS;   /* x^2     Q14 */
    unsigned long den = SCALE + xx;               /* 1+x^2   Q14 */
    unsigned long num = 4UL * SCALE * SCALE;      /* = 2^30      */
    return num / den;                             /* 4/(1+x^2) Q14 */
}

void main(void)
{
    unsigned long j, pairs = N / 2;
    unsigned long sum = 0, fa, fm, fb, total, ip, fp, frac4;

    putchar('g'); putchar('o'); putchar('\n');    /* 生存確認 */

    for (j = 0; j < pairs; j++) {
        unsigned long a = 2UL * j;                 /* 左端の点番号 */
        fa = fx((SCALE * a)        / N);           /* f_{2j}       */
        fm = fx((SCALE * (a + 1))  / N);           /* f_{2j+1}     */
        fb = fx((SCALE * (a + 2))  / N);           /* f_{2j+2}     */
        sum += fa + 4UL * fm + fb;                 /* ペア寄与     */
    }
    total = sum / (3UL * N);                       /* * (h/3)      */
    ip    = total >> QBITS;
    fp    = total & (SCALE - 1);
    frac4 = (fp * 10000UL) / SCALE;

    printf("simpson:   pi~=%lu.%04lu (Q%u, N=%lu)\n",
           ip, frac4, (unsigned)QBITS, (unsigned long)N);
    sim_exit();
    for (;;) ;
}
