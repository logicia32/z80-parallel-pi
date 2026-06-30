# z80-parallel-pi — computing π in parallel on an 8-bit Z80 with no FPU

**English** | [日本語](#日本語)

> Experiment notes for making a 1970s 8-bit **Z80** compute π using **fixed-point only**
> (it has no floating-point hardware), then wiring **1–4 Z80s** together to *measure*
> whether sharing the work actually makes it faster — or doesn't. I put this together
> hoping it could be hands-on teaching material for a university course on numerical or
> parallel computing. Every reproduction step is in this README.

The subject is the standby that sits at the very front of every parallel-computing textbook:

$$\int_{0}^{1}\frac{4}{1+x^{2}}\,dx=\pi$$

The right-hand side is exactly π, so numerically integrating the left-hand side gives you π.
We run this for real with **three numerical methods** and **two parallel models**, and graph it.

Write-ups:
**English** → https://logicia32.hashnode.dev/four-z80s-computing-pi-in-parallel ·
**日本語** → https://zenn.dev/logicia32/articles/2026-05-19-z80-parallel-pi

> Note: the source comments are in Japanese. The code itself is standard C / Z80, so it
> reads fine either way; only the prose comments are untranslated.

---

## What this teaches (conclusions first)

| Aspect | What you see |
|---|---|
| Trapezoidal rule | Just split the interval, so it **parallelizes straightforwardly and speeds up with machine count** |
| Simpson's rule | At the same slice count, **accuracy is orders of magnitude higher** (error ~h⁴ vs ~h²) |
| Newton's method | The iteration is a sequential chain, so **adding machines doesn't help** (analytically obvious = the graph's line is analytic, not measured) |
| ① Distributed memory | Send-and-gather. Speeds up, but **tops out around 2× and swings every run** (rank-0 reduction is serial + host-dependent) |
| ② Shared memory | Contend-for-it. **3.73× at four machines**, and **deterministic — identical every run**. Forget the lock and the answer doesn't go to 0; it **quietly drifts to an under-counted value** (π≈0.94 at P=4) |
| Common | One kernel `pi_kernel.h` (single source). The fixed-point integer sum is **bit-for-bit identical regardless of how you split it, which model, or how many machines**: π=3.14148 |

The single biggest aim of this material is to show — with Newton's method, in machine-equivalent
clock counts — that "there are calculations that don't speed up under parallelism, and here's *why*."

---

## Directory layout

```
z80-parallel-pi/
├── pi_kernel.h               ★ the single trapezoid kernel (the 3 below #include it)
├── methods/                  single-node, 3 methods (start here)
│   ├── trapezoid.c             trapezoid (uses pi_kernel.h)
│   ├── simpson.c               Simpson's rule (also Q14, even N)
│   ├── newton.c                Newton's method = Heron's square root (a sequential chain)
│   ├── console.c / console.h   char output to ucsim_z80 (I/O port 0x80)
│   └── Makefile
├── parallel/
│   ├── distributed-memory/   ① message-passing model
│   │   ├── node_pi.c           SPMD node body (uses pi_kernel.h)
│   │   ├── console_mm.c        memory-mapped simif backend
│   │   ├── router.py           host-side router (= the network itself)
│   │   └── Makefile
│   └── shared-memory/        ② shared-memory model (custom multicore)
│       ├── z80.h               single-header Z80 core (third party, zlib, see NOTICE)
│       ├── z80impl.c           one line that instantiates z80.h (original, MIT)
│       ├── machine.h mc.c cpu.c bus arbiter + pin wiring + IM2 interrupts
│       ├── node_pi2.c          Z80-side workload (pi_kernel.h, lock on/off)
│       ├── t63.c               host harness running N cores (pi reduction)
│       ├── crt0_irq.s isrdemo.c Z80 side of the interrupt demo (IM2, hand-written ISR)
│       ├── irq_host.c          host side of the interrupt demo ("NIC" role, verification)
│       └── Makefile
├── graphs/                   speedup / accuracy graph generation (matplotlib)
├── writeups/                 explanations (long reads, 01–04)
├── LICENSE                   the original parts of this repo (MIT)
└── NOTICE                    attribution for third-party code (z80.h only, zlib)
```

> `pi_kernel.h` is **a single file**. With `grep -rn pi_kernel.h` you can confirm with your
> own eyes that the single-node version, ①, and ② all just `#include` the same kernel — i.e.
> "same calculation, only the way it's bundled differs."

---

## Requirements

- **SDCC** (the C compiler for Z80) and **ucsim_z80** (the bundled simulator). On most
  distros ucsim_z80 ships inside the SDCC package.
- **make**, **python3**, and **matplotlib** if you want the graphs.

Check:

```sh
sdcc --version          # the z80 target should be included
ucsim_z80 -t Z80 -h     # should start
python3 -c "import matplotlib"   # only if you're drawing graphs
```

If your toolchain lives outside `PATH`, pass `SDCC=/path/to/sdcc UCSIM=/path/to/ucsim_z80`
to each `make`.

> The **absolute clock counts** in this repo are measured with **SDCC 4.5.24 + ucsim_z80 0.9.9**.
> Other versions build and run too, but the clocks shift by a few percent. What **does not**
> depend on the version is the *shape and the conclusions*: **π=3.14148; "① swings every run /
> ② is deterministic"; and what tops out vs. what doesn't under parallelism.** That's what I
> want you to confirm by hand — not the individual numbers, but those.

---

## 1. First, the 3 methods on a single node (methods/)

```sh
cd methods
make run                       # runs the three in order
```

Expected output (excerpt):

```
trapezoid: pi~=3.1416 (Q14, N=1024)
simpson:   pi~=3.1416 (Q14, N=1024)
newton: sqrt(2) by x_{n+1}=(x_n + S/x_n)/2  (Q14)
  iter 0: x=1.00000
  iter 1: x=1.50000
  iter 2: x=1.41662
  iter 3: x=1.41418
  ...
```

You can vary the slice count `N` to watch how accuracy behaves (the trapezoid–Simpson gap shows up):

```sh
make N=4   run-trapezoid run-simpson     # trapezoid=3.1311 / Simpson=3.1415
make N=64  run-trapezoid run-simpson     # trapezoid=3.1414 / Simpson=3.1415
```

> Simpson reaches **the accuracy of the trapezoid at N=256 already at N=4**. After that it hits
> the quantization floor of Q14 (= 1/16384 ≈ 6×10⁻⁵) and won't sharpen further. It's a felt
> lesson that "choosing a better formula" beats "slicing finer."

Newton's method needs `x_n` to produce `x_{n+1}`. **Without the previous value you can't compute
the next** = a sequential chain. This pays off later in the "doesn't speed up under parallelism" story.

---

## 2-①. Distributed memory: send and gather (parallel/distributed-memory)

Each Z80 is an independent ucsim_z80 process. There is no shared memory between nodes; all exchange
is via "frames." `router.py` is that wiring (= the network) itself.

```sh
cd parallel/distributed-memory
make run                       # runs P=1,2,3,4 and prints a table (do it a few times)
```

A measured example (critical path = clock count of the slowest node). **P=1 is deterministic at
1324206. For P≥2, waiting on rank-0's reduction swings widely depending on the host's scheduling.**
Below is **one single observation**; the speedup column is that row's `1324206 ÷ crit-path` (this
sample's own value). The band in parentheses is a rough indicator of the spread observed across
other trials too — not strict bounds; it changes every run and per environment (the graph
`speedup.png` draws the band's median as a line):

```
 P | per-rank Z80 clks            | crit-path |   pi~= | speedup (=1324206/crit) | observed band (rough)
 1 | 1324206                      |   1324206 | 3.14148|   1.00x                 |   —
 2 | 752287 608568                |    752287 | 3.14148|   1.76x                 |   1.6–1.8
 3 | 539105 413728 423173         |    539105 | 3.14148|   2.46x                 |   1.7–2.5
 4 | 681382 321060 319924 312529  |    681382 | 3.14148|   1.94x                 |   1.8–2.9
```

In this one run, note that P=4 is *slower* than P=3 (2.46x → 1.94x, an **inversion**). More machines,
yet slower — this is exactly the live face of "① swings every run and you can't read it"; on another
run P=4 stretches to 2.9x. The non-rank-0 compute clocks are deterministic (identical every time).
Only the rank-0 reducer moves, and it doesn't shrink as you add machines (Amdahl) **and** swings
every run — "fast but unreadable" is the true character of ①. The contrast with ②'s determinism
(next section) is the core.

## 2-②. Shared memory: contend for it (parallel/shared-memory)

A verified single-header Z80 core, instantiated N times, with a bus arbiter, hardware semaphore,
barrier, and IM2 interrupts added by hand — "one SMP machine." The Z80-side workload is **the same
kernel `pi_kernel.h`** as part 1 (a different binary that only bundles things differently). The
reduction is an **RMW of a 32-bit value placed in the shared-RAM window `0xC000`**.

```sh
cd parallel/shared-memory
make run    # locked version (P=1..4 agree) / lock-free version (under-counts) / interrupt demo
```

A measured example (the custom simulator is deterministic = these exact values every time):

```
=== (2) shared-RAM reduction, locked (HW semaphore) : P=1..4 ===
  P=1  shared acc=3294113 raw=51470 pi~=3.14148  MATCH  (crit 1205949)
  P=4  shared acc=3294113 raw=51470 pi~=3.14148  MATCH  (crit  323096, 3.73x)
       lock spins: P=2:9  P=3:23  P=4:46
=== (2) NO lock : the classic lost update (measured, not 0) ===
  P=2 acc=1349508 pi~=1.28699   P=3 acc=1106018 pi~=1.05475
  P=4 acc= 988371 pi~=0.94257   (correct is 3.14148)  ... MISMATCH
=== interrupt-driven receive (IM2, hand-written ISR) ===
  bytes by interrupt=4  interrupts taken=4  checksum 0xAA  -> PASS
```

The locked version is **about 3.73× at four machines**, and it doesn't move by a single clock no
matter how many times you run it (the opposite of ①). Remove the lock and the read-modify-write on
shared RAM races, so **the answer doesn't go to 0 — it quietly drifts to an under-counted value**
(π≈0.94 at P=4, deterministically the same value every time). Not "a lost update becomes 0" but
"**it becomes a hard-to-notice, halfway value**" — the thing you learn in class, in real numbers.
The interrupt demo is the real thing: a NIC raises `/INT`, received via IM2 + a hand-written ISR;
`main` never polls the NIC at all, and it reconciles the count of accepted interrupts against a
checksum to print PASS/FAIL.

---

## 3. Graphs (graphs/)

```sh
cd graphs
make                           # speedup.{png,svg} / accuracy.{png,svg}
# to use a different python: make PYTHON=/path/to/python3
```

- `speedup.png` … ② is near-linear and deterministic (measured, identical every run); ① is a
  representative value + the observed spread band (host-dependent, changes every run; the band is
  an indicator, not strict bounds); Newton is the horizontal line at 1.00 (**analytic**: a chain
  can't be split, so there's nothing to measure — shown for contrast with the measured ②).
- `accuracy.png` … per-method error. Simpson hits the Q14 floor early.

The numbers in `graphs/plot.py` are the measured results of the harnesses above (SDCC 4.5.24 +
ucsim_z80 0.9.9). ② is deterministic, so fixed values. ① is host-dependent and swings, so a
representative value + the observed spread band (trial- and environment-dependent, not strict
bounds). Only Newton's line is analytic (there is no parallel implementation), as stated in the
docstring. Change the SDCC version and the clocks move a few percent, but the "shape" is invariant.

---

## Why doesn't Newton's method speed up under parallelism?

Trapezoid and Simpson are "independent per interval (pair)," so you split them simply by handing
ranges to cores. Addition is associative, so however you split it the answer is bit-for-bit identical.

Newton's method is different.

$$x_{n+1}=\frac{1}{2}\left(x_{n}+\frac{S}{x_{n}}\right)$$

To compute `x_{n+1}` you need `x_n`, and `x_{n+2}` needs `x_{n+1}`. The iteration is one chain (a
sequential dependency); you can't hand a middle link to another core. Splitting a single evaluation
(one multiply, one divide) isn't worth it. In Amdahl's-law terms the "serial fraction ≈ 1," so you
can hang four machines off it and only one actually works while the rest just wait = the speedup
sticks at 1× (the gray line in `speedup.png`; because it's obvious from being unsplittable, it's
drawn as an **analytic** value, for contrast with the measured trapezoid/Simpson curves).

**"Whether parallelism helps is decided not by the content of the calculation but by the shape of
its dependencies"** — that's the one thing I most want you to take home from this material.

---

## License and acknowledgments

- The original parts of this repo (`pi_kernel.h`, the bus arbiter and multicore harness, the IM2
  interrupt layer and interrupt demo, the router, the build scripts, the graphs, and the one-line
  wrapper `z80impl.c`) are **MIT** (`LICENSE`).
- **Only** `parallel/shared-memory/z80.h` is a third-party single-header Z80 emulator, under the
  **zlib/libpng license** (Andre Weissflog). See `NOTICE` for attribution. Thanks for publishing
  such a wonderful core.

Longer background and reading are in `writeups/` (parts 1–4).

---

## 日本語

# z80-parallel-pi — FPU の無い 8 ビット Z80 で円周率を、並列に求めてみる

> 1970 年代の 8 ビット CPU **Z80** に、浮動小数点を一切使わず（FPU が無いので）
> 固定小数点だけで円周率 π を計算させ、さらに Z80 を **1〜4 台**つないで
> 「分担すると速くなるのか／ならないのか」を**実測**するための実験ノートです。
> 大学の数値計算・並列計算の授業で、手を動かして確かめる教材になればと思って
> まとめました。再現手順はこの README に全部書いてあります。

題材は並列計算の教科書でいちばん最初に出てくる定番です。

$$\int_{0}^{1}\frac{4}{1+x^{2}}\,dx=\pi$$

右辺がきっかり π になるので、左辺を数値積分すれば円周率が出ます。これを
3 つの数値解法と 2 つの並列方式で実際に走らせ、グラフにします。

解説記事 — 英語: <https://logicia32.hashnode.dev/four-z80s-computing-pi-in-parallel> ／
日本語: <https://zenn.dev/logicia32/articles/2026-05-19-z80-parallel-pi>

---

## 何が分かる教材か（先に結論）

| 観点 | 見えること |
|---|---|
| 台形公式 | 区間を分けるだけなので**素直に並列化でき、台数で速くなる** |
| シンプソン公式 | 同じ分割数でも**精度が桁違いに高い**（誤差 ~h⁴ 対 ~h²） |
| ニュートン法 | 反復が逐次の鎖なので**並列にしても速くならない**（解析的に自明＝グラフの線は実測でなく解析値） |
| ①分散メモリ | 送って持ち寄る型。速くなるが **2倍そこそこで頭打ち、しかも走るたびに振れる**（rank0 集約が直列＋ホスト依存） |
| ②共有メモリ | 取り合う型。**4 台で 3.73 倍**、しかも**決定論的に毎回同じ**。ロックを忘れると答えは 0 ではなく**過小集計に静かに狂う**（P=4 で π≈0.94） |
| 共通 | 同じカーネル `pi_kernel.h`（単一ソース）。固定小数点の整数和は**分割の仕方・方式・台数に依らずビット単位で同じ答え** π=3.14148 |

「並列にしても速くならない計算がある。それは ～ だから」を、ニュートン法を
題材に実機相当のクロック数で示すのが、この教材のいちばんの狙いです。

---

## ディレクトリ構成

```
z80-parallel-pi/
├── pi_kernel.h               ★ 単一の台形カーネル(下記3つが #include)
├── methods/                  単一ノードの 3 解法（まずここ）
│   ├── trapezoid.c             台形(pi_kernel.h を使う)
│   ├── simpson.c               シンプソン公式（同じく Q14・偶数N）
│   ├── newton.c                ニュートン法 = ヘロンの平方根（逐次の鎖）
│   ├── console.c / console.h   ucsim_z80 への文字出力（I/O ポート 0x80）
│   └── Makefile
├── parallel/
│   ├── distributed-memory/   ① メッセージパッシング型
│   │   ├── node_pi.c           SPMD ノード本体（pi_kernel.h を使う）
│   │   ├── console_mm.c        メモリマップ simif バックエンド
│   │   ├── router.py           ホスト側ルータ（= ネットワークそのもの）
│   │   └── Makefile
│   └── shared-memory/        ② 共有メモリ型（自作マルチコア）
│       ├── z80.h               1 ヘッダ Z80 コア（第三者・zlib、NOTICE 参照）
│       ├── z80impl.c           z80.h を実体化する1行(原作・MIT)
│       ├── machine.h mc.c cpu.c バス調停器 + ピン配線 + IM2 割り込み
│       ├── node_pi2.c          Z80 側ワークロード(pi_kernel.h・ロック有/無)
│       ├── t63.c               N コアを回すホスト harness（pi 集約）
│       ├── crt0_irq.s isrdemo.c 割り込みデモの Z80 側(IM2・手書きISR)
│       ├── irq_host.c          割り込みデモのホスト("NIC"役・検証)
│       └── Makefile
├── graphs/                   速度向上・精度のグラフ生成（matplotlib）
├── writeups/                 解説（読み物・01〜04）
├── LICENSE                   本リポジトリのオリジナル部分（MIT）
└── NOTICE                    第三者コードの帰属（z80.h のみ・zlib）
```

> `pi_kernel.h` は**ただ 1 ファイル**。`grep -rn pi_kernel.h` で、単一
> ノード版・①・② が同じカーネルを `#include` しているだけ＝「同じ
> 計算、束ね方だけ違う」を自分の目で確認できます。

---

## 必要なもの

- **SDCC**（Z80 向け C コンパイラ）と **ucsim_z80**（同梱のシミュレータ）
  多くのディストリで SDCC パッケージに ucsim_z80 が同梱されます。
- **make**、**python3**、グラフを描くなら **matplotlib**

確認:

```sh
sdcc --version          # z80 ターゲットが含まれること
ucsim_z80 -t Z80 -h     # 起動すること
python3 -c "import matplotlib"   # グラフを描く場合のみ
```

処理系を PATH 以外に置いている場合は、各 `make` に
`SDCC=/path/to/sdcc UCSIM=/path/to/ucsim_z80` を渡せます。

> 本リポジトリの**絶対クロック数**は **SDCC 4.5.24 + ucsim_z80 0.9.9**
> での実測値です。別の版でもビルド・実行できますが、クロックは数 %
> ずれます。**π＝3.14148・「①は走るたびに振れる／②は決定論的」・
> 並列で頭打ちする/しない、という"形と結論"は版に依りません**——
> 手元で確かめてほしいのは、個々の数字ではなくそこです。

---

## 1. まず単一ノードで 3 解法（methods/）

```sh
cd methods
make run                       # 3 つを順に実行
```

期待される出力（抜粋）:

```
trapezoid: pi~=3.1416 (Q14, N=1024)
simpson:   pi~=3.1416 (Q14, N=1024)
newton: sqrt(2) by x_{n+1}=(x_n + S/x_n)/2  (Q14)
  iter 0: x=1.00000
  iter 1: x=1.50000
  iter 2: x=1.41662
  iter 3: x=1.41418
  ...
```

分割数 `N` を変えて精度の出方を見られます（台形とシンプソンの差が出ます）:

```sh
make N=4   run-trapezoid run-simpson     # 台形=3.1311 / シンプソン=3.1415
make N=64  run-trapezoid run-simpson     # 台形=3.1414 / シンプソン=3.1415
```

> シンプソンは **N=4 の時点で台形の N=256 相当の精度**に達します。あとは
> Q14（= 1/16384 ≒ 6×10⁻⁵）の量子化の床に当たって、それ以上は詰まりません。
> 「もっと細かく刻む」より「いい公式を選ぶ」ほうが効く、という体験です。

ニュートン法は `x_{n+1}` を出すのに `x_n` が要ります。**前の値が無いと次が
計算できない**＝逐次の鎖。これが後で「並列にしても速くならない」話に効きます。

---

## 2-①. 分散メモリ：送って持ち寄る（parallel/distributed-memory）

各 Z80 は独立した ucsim_z80 プロセス。ノード間に共有メモリは無く、やり取りは
全部「フレーム」。`router.py` がその配線（= ネットワーク）そのものです。

```sh
cd parallel/distributed-memory
make run                       # P=1,2,3,4 を走らせ表を出す（数回どうぞ）
```

実測例（クリティカルパス = 一番遅いノードのクロック数基準）。**P=1 は
決定論的に 1324206。P≥2 は rank0 の集約待ちがホストのスケジューリング
次第で大きく振れます**。下は**ある 1 回の観測**で、speedup 列はその行の
`1324206 ÷ crit-path`（このサンプル自身の値）。カッコの帯は別の試行も
含めて観測した散らばりの目安で、厳密な上下限ではなく走るたび・環境ごと
に変わります（グラフ `speedup.png` は帯の中央値を線で描きます）:

```
 P | per-rank Z80 clks            | crit-path |   pi~= | speedup (=1324206/crit) | 観測帯の目安
 1 | 1324206                      |   1324206 | 3.14148|   1.00x                 |   —
 2 | 752287 608568                |    752287 | 3.14148|   1.76x                 |   1.6〜1.8
 3 | 539105 413728 423173         |    539105 | 3.14148|   2.46x                 |   1.7〜2.5
 4 | 681382 321060 319924 312529  |    681382 | 3.14148|   1.94x                 |   1.8〜2.9
```

この 1 回では P=4 が P=3 より遅い（2.46x → 1.94x と**逆転**している）こと
に注目してください。台数を増やしたのに遅くなる——これこそ「①は走るたび
に振れて読めない」の生の姿で、別の回では P=4 が 2.9x まで伸びることも
あります。非 rank0 の計算クロックは決定論的（毎回同じ）。動くのは集約
担当 rank0 だけで、台数を増やしても縮まない（アムダール）うえに**走る
たびに振れる**——「速いが読めない」のが①の正体です。②（次節）の決定論
との対比が芯。

## 2-②. 共有メモリ：取り合う（parallel/shared-memory）

検証済みの 1 ヘッダ Z80 コアを N 個インスタンス化し、バス調停器・ハードウェア
セマフォ・バリア・IM2 割り込みを自前で足した「1 つの SMP マシン」。Z80 側の
ワークロードは**第1回と同じカーネル `pi_kernel.h`**（束ね方が違うだけの
別 binary）。集約は**共有 RAM 窓 `0xC000` に置いた 32bit 値の RMW**です。

```sh
cd parallel/shared-memory
make run    # ロック版(P=1..4一致) / 無ロック版(過小集計) / 割り込みデモ
```

実測例（自作シミュレータは決定論的＝毎回この値）:

```
=== (2) shared-RAM reduction, locked (HW semaphore) : P=1..4 ===
  P=1  shared acc=3294113 raw=51470 pi~=3.14148  MATCH  (crit 1205949)
  P=4  shared acc=3294113 raw=51470 pi~=3.14148  MATCH  (crit  323096, 3.73x)
       lock spins: P=2:9  P=3:23  P=4:46
=== (2) NO lock : the classic lost update (measured, not 0) ===
  P=2 acc=1349508 pi~=1.28699   P=3 acc=1106018 pi~=1.05475
  P=4 acc= 988371 pi~=0.94257   （正解は 3.14148）  ... MISMATCH
=== interrupt-driven receive (IM2, hand-written ISR) ===
  bytes by interrupt=4  interrupts taken=4  checksum 0xAA  -> PASS
```

ロック有りは **4 台で約 3.73 倍**、しかも何回走らせても 1 clk も動きません
（①と対照的）。ロックを外すと共有 RAM の read-modify-write が競合して
**答えは 0 ではなく過小集計に静かに狂う**（P=4 で π≈0.94。決定論的に毎回
同じ値）。「ロストアップデートは 0 になる」ではなく「**気づきにくい中途半端
な値になる**」——授業で習うやつを実数で。割り込みデモは NIC が `/INT` を
上げ、IM2＋手書き ISR で受ける本物で、main は NIC を一切ポーリングせず、
受理割り込み回数とチェックサムを突き合わせて PASS/FAIL を出します。

---

## 3. グラフ（graphs/）

```sh
cd graphs
make                           # speedup.{png,svg} / accuracy.{png,svg}
# 別の python を使う場合: make PYTHON=/path/to/python3
```

- `speedup.png` … ②が線形に近く決定論的（実測・毎回同じ）、①は代表値
  ＋観測した散らばり帯（host 依存で走るたび変わる。帯は目安で厳密境界
  ではない）、ニュートンは 1.00 の水平線（**解析値**：鎖は分割不能なので
  測るまでもない。実測の②との対比用）。
- `accuracy.png` … 解法ごとの誤差。シンプソンが早々に Q14 の床に達する。

`graphs/plot.py` の数値は上記 harness の実測（SDCC 4.5.24 + ucsim_z80
0.9.9）。②は決定論的なので固定値。①は host 依存で振れるため代表値＋
観測した散らばり帯（試行・環境依存で、厳密な上下限ではない）。
ニュートンの線だけは解析値（並列実装は無い）と docstring に明記。
SDCC の版が変わるとクロックは数 % 動くが「形」は不変。

---

## なぜニュートン法は並列にしても速くならないのか

台形・シンプソンは「区間（ペア）ごとに独立」なので、範囲をコアに配るだけで
分割できます。足し算は結合的なので、どう分けてもビット単位で同じ答え。

ニュートン法は違います。

$$x_{n+1}=\frac{1}{2}\left(x_{n}+\frac{S}{x_{n}}\right)$$

`x_{n+1}` を計算するには `x_n` が要り、`x_{n+2}` には `x_{n+1}` が要る。
反復は 1 本の鎖（逐次の依存関係）で、途中の輪を別のコアに渡せません。
1 回ぶんの評価（掛け算・割り算 1 個ずつ）を分割しても割に合わない。
アムダールの法則でいう「直列部分の割合 ≈ 1」で、4 台ぶら下げても実際に
働けるのは 1 台・残りはただ待つだけ＝速度向上は 1 倍に張り付きます
（`speedup.png` の灰色の線。これは分割不能ゆえ自明なので**解析値**として
描いてあり、台形/シンプソンの実測曲線との対比用です）。

**「並列化が効くかどうかは、計算の中身ではなく依存関係の形で決まる」** ——
この教材でいちばん持ち帰ってほしいのはここです。

---

## ライセンスと謝辞

- 本リポジトリのオリジナル部分（`pi_kernel.h`、バス調停器・マルチコア
  harness、IM2 割り込み層・割り込みデモ、ルータ、ビルドスクリプト、
  グラフ、`z80impl.c` の 1 行ラッパー）は **MIT**（`LICENSE`）。
- `parallel/shared-memory/z80.h` **のみ**が第三者の 1 ヘッダ Z80
  エミュレータで、**zlib/libpng ライセンス**（Andre Weissflog）。帰属は
  `NOTICE` を参照してください。素晴らしいコアを公開してくださっている
  ことに感謝します。

詳しい背景と読み物は `writeups/` にあります（第1〜4回）。
