#ifndef CONSOLE_H
#define CONSOLE_H

/*
 * console.h — ucsim_z80 への文字出力をつなぐ最小コンソール
 *
 * 素の Z80 には UART も画面も無い。SDCC の stdio (printf) は putchar() を
 * 呼ぶだけで、その先の「どこへ出すか」はターゲット側の責務になる。
 *
 * ここでは ucsim_z80 のシミュレータ・インターフェース (simif) を出口に
 * 使う。simif は -I if=outputs[0x80] で「I/O ポート 0x80」に現れる擬似
 * デバイス。OUT (0x80),A でコマンド 1 バイトを書くと、ホスト側で対応
 * する動作が起きる:
 *   0x70 'p' : 次に書いた 1 バイトをホスト stdout へ出す
 *   0x73 's' : シミュレーションを正常停止する
 *
 * 重要: simif を「メモリ」ではなく「I/O 空間」に置く。SDCC の既定 crt0
 * は LD SP,#0 でスタックを 0xFFFF から下方向に伸ばすため、コンソールを
 * 0xFFF0 等のメモリに置くと、再帰や引数の多い printf でスタックが device
 * を踏み潰す。実機の UART と同じく I/O ポートに逃がすのが正解。
 */

/* printf/puts が内部で呼ぶ。SDCC ライブラリ側に putchar の実体は無い
 * ので、ここで与える(これがターゲット移植の最小単位)。 */
int  putchar(int c);

/* main から戻る代わりにシミュレーションを畳む。HALT より後始末が綺麗。 */
void sim_exit(void);

#endif /* CONSOLE_H */
