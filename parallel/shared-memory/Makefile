# parallel/shared-memory — 共有メモリ(自作マルチコア)版
#
# 1 個の検証済み Z80 コア(chips z80.h, Andre Weissflog, zlib)を N 個
# インスタンス化し、バス調停器(mc.c)・ハードウェアセマフォ・バリアを
# 自前で足した「1 つの SMP マシン」。reduction は 0xC000 の共有 RAM に
# 置いた 32bit アキュムレータの read-modify-write。t63 はホスト側 C で、
# その中で N コアを時分割で回す。Z80 側は SDCC でビルド。
#
#   make            ... ホスト harness と Z80 binary 一式をビルド
#   make run        ... ロック版(正解)/無ロック版(lost-update)/割り込みデモ
#   make clean
#
# ホスト C コンパイラは $(CC)。Z80 側は SDCC。
#   make SDCC=/path/to/sdcc CC=gcc
SDCC    ?= sdcc
SDASZ80 ?= sdasz80
CC      ?= cc
CFLAGS   = -O2 -I.
# -I../.. so node_pi2.c can #include the single shared pi_kernel.h
ZFLAGS   = -mz80 -DN=64UL -I../..

all: t63 np2_lock.ihx np2_race.ihx irq_host isrdemo.ihx

# ホスト側: chips コア実体(CHIPS_IMPL)+ 調停器 + ピン配線 + harness
t63: t63.c mc.c cpu.c z80impl.c machine.h z80.h
	$(CC) $(CFLAGS) t63.c mc.c cpu.c z80impl.c -o t63

# 割り込みデモ用ホスト("NIC" 役。/INT だけで K バイト配送)
irq_host: irq_host.c mc.c cpu.c z80impl.c machine.h z80.h
	$(CC) $(CFLAGS) irq_host.c mc.c cpu.c z80impl.c -o irq_host

# Z80 側ワークロード。USE_LOCK 有り=HW セマフォで共有RAMの RMW を保護。
np2_lock.ihx: node_pi2.c ../../pi_kernel.h
	$(SDCC) $(ZFLAGS) -DUSE_LOCK node_pi2.c -o np2_lock.ihx

# USE_LOCK 無し=ロック無しで共有RAMの 32bit 値を read-modify-write。
# バリアで全コアを揃えてから競らせるので lost update が顕在化する。
np2_race.ihx: node_pi2.c ../../pi_kernel.h
	$(SDCC) $(ZFLAGS) node_pi2.c -o np2_race.ihx

# 割り込みデモの Z80 側。--no-std-crt0 なので _CODE/_DATA 配置は自前。
# 指定しないと再配置コードが crt0 の絶対領域(0x0000/0x0200/0x0300)と
# 衝突し、IM2 ベクタ表が壊れる(隣ラボで踏んだ罠)。
crt0_irq.rel: crt0_irq.s
	$(SDASZ80) -l -o -s crt0_irq.s

isrdemo.ihx: crt0_irq.rel isrdemo.c
	$(SDCC) -mz80 --no-std-crt0 --code-loc 0x0400 --data-loc 0x8000 \
		crt0_irq.rel isrdemo.c -o isrdemo.ihx

run: all
	@echo "=== (2) shared-RAM reduction, locked (HW semaphore) : P=1..4 ==="
	@for p in 1 2 3 4; do \
	  ./t63 np2_lock.ihx $$p | grep -E 'cycles=|shared acc=|lock:|MATCH|MISMATCH'; \
	  echo; done
	@echo "=== (2) NO lock : the classic lost update (measured, not 0) ==="
	@for p in 2 3 4; do ./t63 np2_race.ihx $$p | grep -E 'shared acc=|MATCH|MISMATCH'; done
	@echo
	@echo "=== interrupt-driven receive (IM2, hand-written ISR) ==="
	@./irq_host isrdemo.ihx

clean:
	rm -f t63 irq_host *.ihx *.asm *.lst *.rel *.sym *.map *.noi *.lk *.o

.PHONY: all run clean
