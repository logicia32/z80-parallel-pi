# methods/ — 単一ノードで pi(と平方根)を求める 3 つの数値解法
#
#   make               ... trapezoid / simpson / newton をビルド
#   make run-trapezoid ... 台形公式を ucsim_z80 で実行
#   make run-simpson   ... シンプソン公式を実行
#   make run-newton    ... ニュートン法(平方根)を実行
#   make run           ... 3 つを順に実行
#   make N=64 ...       ... 分割数 N を変えて再ビルド(台形/シンプソン)
#   make clean
#
# SDCC ドライバが compile -> assemble -> link -> .ihx を一括で行う。
# simif はメモリではなく I/O ポート 0x80 に出す (-I if=outputs[0x80])。
# メモリに置くと既定 crt0 の LD SP,#0 で降りるスタックに踏まれるため。
#
# SDCC / ucsim_z80 は各自インストールし PATH に通す(既定はそれを使う)。
# 別の場所のものを使う場合: make SDCC=/path/to/sdcc UCSIM=/path/to/ucsim_z80
SDCC   ?= sdcc
UCSIM  ?= ucsim_z80
# -I.. so trapezoid.c can #include the single shared ../pi_kernel.h
CFLAGS  = -mz80 -I..
SIMIF   = -I if=outputs[0x80]

# N は台形/シンプソンの分割数。コマンドラインで上書き可: make N=256
N      ?= 1024

all: trapezoid.ihx simpson.ihx newton.ihx

console.rel: console.c console.h
	$(SDCC) $(CFLAGS) -c console.c -o console.rel

# N はマクロなので make の依存解決では追えない。FORCE を付けて毎回
# 再ビルドし、make N=64 run-trapezoid が常に正しい N で走るようにする。
trapezoid.ihx: trapezoid.c console.rel FORCE
	$(SDCC) $(CFLAGS) -DN=$(N)UL -c trapezoid.c -o trapezoid.rel
	$(SDCC) $(CFLAGS) trapezoid.rel console.rel -o trapezoid.ihx

simpson.ihx: simpson.c console.rel FORCE
	$(SDCC) $(CFLAGS) -DN=$(N)UL -c simpson.c -o simpson.rel
	$(SDCC) $(CFLAGS) simpson.rel console.rel -o simpson.ihx

newton.ihx: newton.c console.rel FORCE
	$(SDCC) $(CFLAGS) -c newton.c -o newton.rel
	$(SDCC) $(CFLAGS) newton.rel console.rel -o newton.ihx

FORCE:

run-trapezoid: trapezoid.ihx
	printf 'run\nquit\n' | $(UCSIM) -t Z80 $(SIMIF) trapezoid.ihx

run-simpson: simpson.ihx
	printf 'run\nquit\n' | $(UCSIM) -t Z80 $(SIMIF) simpson.ihx

run-newton: newton.ihx
	printf 'run\nquit\n' | $(UCSIM) -t Z80 $(SIMIF) newton.ihx

run: run-trapezoid run-simpson run-newton

clean:
	rm -f *.ihx *.asm *.lst *.rel *.sym *.map *.noi *.lk *.bin

.PHONY: all run run-trapezoid run-simpson run-newton clean FORCE
