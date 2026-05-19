/* newton.c — ニュートン法(= ヘロンの平方根)で sqrt(S) を求める
 *            単一ノード版・Q14 固定小数点
 *
 * ニュートン法は g(x)=0 の根を反復で詰める:
 *   x_{n+1} = x_n - g(x_n)/g'(x_n)
 * g(x) = x^2 - S に当てはめると、平方根の有名な漸化式になる:
 *   x_{n+1} = ( x_n + S / x_n ) / 2          (ヘロンの方法)
 *
 * この記事群で newton を「pi の計算」ではなく「並列にしても速くならない
 * 計算」の見本として置いている。理由はコードそのものに出ている:
 *   x_{n+1} を計算するには x_n が要る。さらに x_{n+2} には x_{n+1} が要る。
 * つまり反復は 1 本の鎖(逐次の依存関係)で、途中の輪を別のコアに渡せない。
 * 1 回の f / f' の評価(ここでは掛け算と割り算 1 個ずつ)を分割しても
 * 割に合わない。アムダールの法則でいう「直列部分の割合 ≈ 1」で、
 * Z80 を何個ぶら下げても速度向上は 1 倍に張り付く。
 * 一方 trapezoid / simpson は区間(ペア)ごとに独立なので素直に割れる。
 * この対比が 04 の主題。
 *
 * Q14・32bit に収まる確認:
 *   S/x を Q14 で得るには S * 2^28 / x_fp。
 *   S=2 のとき 2*2^28 = 2^29 < 2^32。S は小さく保つこと(既定 2)。
 */
#include <stdio.h>
#include "console.h"

#define QBITS 14
#define SCALE (1UL << QBITS)        /* 16384 : Q14 の 1.0 */
#ifndef S
#define S     2UL                   /* sqrt(S) を求める(-DS= で上書き) */
#endif
#ifndef ITERS
#define ITERS 8U                    /* 反復回数(-DITERS= で上書き) */
#endif

static void show(unsigned k, unsigned long x_fp)
{
    unsigned long ip   = x_fp >> QBITS;
    unsigned long fp   = x_fp & (SCALE - 1);
    unsigned long frac = (fp * 100000UL) / SCALE;     /* 小数 5 桁 */
    printf("  iter %u: x=%lu.%05lu\n", k, ip, frac);
}

void main(void)
{
    unsigned k;
    unsigned long x = SCALE;                  /* 初期値 x0 = 1.0 (Q14) */

    putchar('g'); putchar('o'); putchar('\n');

    printf("newton: sqrt(%lu) by x_{n+1}=(x_n + S/x_n)/2  (Q%u)\n",
           (unsigned long)S, (unsigned)QBITS);
    show(0, x);
    for (k = 1; k <= ITERS; k++) {
        /* q = S / x を Q14 で。S*2^28 / x_fp = (S/x)*2^14 */
        unsigned long q = (S * SCALE * SCALE) / x;
        x = (x + q) >> 1;                     /* x_{n+1} は x_n に依存 */
        show(k, x);
    }
    printf("newton: done (this recurrence is strictly sequential)\n");
    sim_exit();
    for (;;) ;
}
