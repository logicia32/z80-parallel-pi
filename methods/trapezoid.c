/* trapezoid.c — 台形公式で pi を求める(単一ノード版・Q14 固定小数点)
 *
 *   pi = ∫_0^1 4/(1+x^2) dx
 *
 * 計算そのもの(fx と台形パネルの和)は ../pi_kernel.h が一手に持つ。
 * この単一ノード版・分散メモリ版・共有メモリ版は、その同じ 1 ファイルを
 * #include しているだけで、違うのは「部分和をどう束ねるか」だけ。だから
 * pi はどの方式・どの台数でもビット単位で同じ値になる。
 *
 * 台形公式(複合):区間 [0,1] を N 等分し、各小区間を台形で近似する。
 *   ∫ ≈ h * ( f0/2 + f1 + ... + f_{N-1} + fN/2 ),  h = 1/N
 * 単一ノードは全パネル [0,N) を 1 台で足すだけ = trap_panels(0,N)。
 *
 * 出力は lab-z80 由来の console.c(I/O ポート 0x80 の simif)経由。
 */
#include <stdio.h>
#include "console.h"

#ifndef N
#define N     1024UL                /* 台形の分割数(-DN= で上書き可) */
#endif
#include "pi_kernel.h"             /* N を見てから読む(除数は定数のまま) */

void main(void)
{
    unsigned long sum, total, ip, fp, frac4;

    putchar('g'); putchar('o'); putchar('\n');    /* 生存確認 */

    sum   = trap_panels(0, N);                     /* 全パネルを 1 台で */
    total = sum / N;                               /* * h, h = 1/N */
    ip    = total >> QBITS;                        /* 整数部 */
    fp    = total & (SCALE - 1);                   /* 小数部(Q14) */
    frac4 = (fp * 10000UL) / SCALE;                /* 小数 4 桁へ */

    printf("trapezoid: pi~=%lu.%04lu (Q%u, N=%lu)\n",
           ip, frac4, (unsigned)QBITS, (unsigned long)N);
    sim_exit();
    for (;;) ;
}
