# parallel/distributed-memory — 分散メモリ(メッセージパッシング)版
#
# 各 Z80 ノードは独立した ucsim_z80 プロセス。ノード間に共有メモリは
# 無く、やり取りは全部「フレーム」。ホスト側の router.py が配線(=
# ネットワーク)そのもので、rank0 に他ランクの部分和を渡して集約する。
#
#   make            ... node_pi.ihx をビルド(SPMD: 全ノード同じ binary)
#   make run        ... P=1,2,3,4 を走らせ速度向上/精度の表を出す
#   make clean
#
# SDCC / ucsim_z80 は各自インストールし PATH に通す。別の場所のものは
#   make SDCC=/path/to/sdcc UCSIM=/path/to/ucsim_z80
SDCC   ?= sdcc
UCSIM  ?= ucsim_z80
# -I../.. so node_pi.c can #include the single shared pi_kernel.h
CFLAGS  = -mz80 -I../..

all: node_pi.ihx

# SPMD ノード本体 + メモリマップ simif バックエンド(0x4000)。
# putchar も mpi_rx/mpi_send_frame も console_mm.c が供給する。
# .c -> .rel を別々に行い、最後に .rel をまとめてリンクする
# (複数 .c を 1 コマンドで渡すと外部参照が解決されないことがある)。
node_pi.rel: node_pi.c console.h mpi.h ../../pi_kernel.h
	$(SDCC) $(CFLAGS) -c node_pi.c -o node_pi.rel

console_mm.rel: console_mm.c console.h mpi.h
	$(SDCC) $(CFLAGS) -c console_mm.c -o console_mm.rel

node_pi.ihx: node_pi.rel console_mm.rel
	$(SDCC) $(CFLAGS) node_pi.rel console_mm.rel -o node_pi.ihx

run: node_pi.ihx
	UCSIM="$(UCSIM)" python3 router.py

clean:
	rm -f *.ihx *.asm *.lst *.rel *.sym *.map *.noi *.lk *.bin n*.cfg

.PHONY: all run clean
