#include "console.h"

/*
 * __sfr __at(0x80) で「I/O ポート 0x80」を 1 バイトのレジスタとして
 * 宣言する。SIMIF への代入は Z80 の OUT (0x80),A 1 命令になる。
 * ucsim 側は -I if=outputs[0x80] でここに simif をぶら下げる。
 */
__sfr __at (0x80) SIMIF;

#define SIMIF_PRINT 0x70   /* 'p' */
#define SIMIF_STOP  0x73   /* 's' */

/*
 * putchar の実体。'p' を書いてから 1 バイトを書くと、その文字が
 * ucsim_z80 のホスト stdout に出る。printf("%d") のような書式変換は
 * SDCC のライブラリが担い、最終的にこの 1 文字出力に落ちてくる。
 */
int putchar(int c)
{
    SIMIF = SIMIF_PRINT;          /* 'p' : これから 1 文字出す */
    SIMIF = (unsigned char)c;     /* その 1 文字              */
    return c;
}

void sim_exit(void)
{
    SIMIF = SIMIF_STOP;           /* 's' : シミュレーション正常停止 */
}
