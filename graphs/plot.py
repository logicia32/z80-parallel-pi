#!/usr/bin/env python3
"""グラフ生成: 速度向上(P=1..4)と 解法ごとの精度。

ここに埋め込んだ数値は、このリポジトリの harness を SDCC 4.5.24 +
ucsim_z80 0.9.9(同梱の lab-z80-uart-cosim/tools/sdcc)で実際に走らせて
採った実測値。SDCC の版が変わると Z80 のクロック数は数 % 動くが、
曲線の「形」(②はほぼ線形・①は頭打ちかつ不安定・Newton は 1.0)は
版に依らない。再現手順:

  分散メモリ ①:  cd parallel/distributed-memory && make run   (×数回)
  共有メモリ ②:  cd parallel/shared-memory     && make run
  精度(解法別): cd methods && for n in 4 8 16 32 64 256 1024; do
                    make -s N=$n run-trapezoid run-simpson; done

② は決定論的シミュレータなので何回走らせても同じ数字。
① は各 ucsim_z80 と仲介ルータがホスト上のプロセスで、配り方の
タイミングが毎回変わるため crit-path が大きく振れる。下の① の数値は
観測した代表値と散らばりの「目安」で、厳密な上下限ではない(走るたび・
環境ごとに変わる——それ自体が論点)。この「振れる/振れない」こそが
分散と共有の性格差で、記事の芯。
出力: speedup.{png,svg} / accuracy.{png,svg}
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

P = [1, 2, 3, 4]

# --- ② 共有メモリ(自作マルチコア): crit-path cycles, N=64 -------------
#     parallel/shared-memory/t63 np2_lock.ihx P の最大コア cycles。
#     決定論的: 何回走らせてもこの値。
s_crit = [1205949, 620377, 422281, 323096]
s_speedup = [s_crit[0] / c for c in s_crit]          # 1.00 1.94 2.86 3.73

# --- ① 分散メモリ(メッセージパッシング): crit-path Z80 clocks, N=64 ---
#     parallel/distributed-memory/router.py を複数回。P=1 は単一プロセス
#     なので決定論的に 1324206 (毎回不変)。P>=2 は rank0 の集約待ちが
#     ホスト側スケジューリング次第で大きく振れ、走るたび・環境ごとに
#     値が変わる。下は観測した代表値と散らばりの「目安」で、厳密な
#     上下限ではない(P=3 で 1.7〜2.5、P=4 で 1.8〜2.9 などを観測):
d_speedup_med = [1.00, 1.75, 2.20, 2.30]             # 代表値(中央あたり)
d_speedup_lo  = [1.00, 1.60, 1.70, 1.80]             # 観測した下振れ目安
d_speedup_hi  = [1.00, 1.80, 2.50, 2.90]             # 観測した上振れ目安

ideal = P[:]                                          # 線形(理想)上限

# --- ニュートン法: 反復は逐次の鎖。分割しようがなく、他コアはただ
#     待つだけ = 直列部分の割合 s≈1。アムダール上限 1/s≈1。これは
#     解析的に自明(測るまでもない)。実測の台形/シンプソンとの対比用。
newton_speedup = [1.0, 1.0, 1.0, 1.0]                # 解析的(非実測)

# --- 精度: 単一ノードで N を振った pi 推定値(methods/、実測) --------
acc_N      = [4, 8, 16, 32, 64, 256, 1024]
acc_trap   = [3.1311, 3.1389, 3.1408, 3.1413, 3.1414, 3.1415, 3.1416]
acc_simp   = [3.1415, 3.1415, 3.1415, 3.1415, 3.1415, 3.1415, 3.1416]
PI_TRUE    = 3.14159265

# ===================== Figure 1: 速度向上 =====================
fig, ax = plt.subplots(figsize=(7.2, 5.0))
ax.plot(P, ideal, "k--", label="ideal (linear, = P)", linewidth=1)
ax.plot(P, s_speedup, "o-", color="#1f77b4",
        label="(2) shared memory — deterministic")
# ① はノイズ帯(min..max)を塗り、中央値を線で
ax.fill_between(P, d_speedup_lo, d_speedup_hi, color="#ff7f0e", alpha=0.18)
ax.plot(P, d_speedup_med, "s-", color="#ff7f0e",
        label="(1) message passing — typical, band=observed spread (host-noisy)")
ax.plot(P, newton_speedup, "^-", color="#999999",
        label="Newton's method — analytic (serial chain, s≈1)")
for x, y in zip(P, s_speedup):
    ax.annotate(f"{y:.2f}x", (x, y), textcoords="offset points",
                xytext=(6, 5), fontsize=8, color="#1f77b4")
for x, y in zip(P, d_speedup_med):
    ax.annotate(f"{y:.2f}x", (x, y), textcoords="offset points",
                xytext=(6, -13), fontsize=8, color="#d2691e")
ax.set_xticks(P)
ax.set_xlabel("number of Z80 cores / nodes  P")
ax.set_ylabel("speedup  (T_1 / T_P, crit-path)")
ax.set_title("Parallel pi on Z80 — speedup vs P  (trapezoid, N=64, Q14)")
ax.set_ylim(0.8, 4.3)
ax.grid(True, alpha=0.3)
ax.legend(loc="upper left", fontsize=8)
fig.tight_layout()
fig.savefig("speedup.png", dpi=140)
fig.savefig("speedup.svg")
plt.close(fig)

# ===================== Figure 2: 解法ごとの精度 =====================
fig, ax = plt.subplots(figsize=(7.2, 5.0))
err_trap = [abs(v - PI_TRUE) for v in acc_trap]
err_simp = [abs(v - PI_TRUE) for v in acc_simp]
ax.loglog(acc_N, err_trap, "s-", color="#ff7f0e",
          label="trapezoid  (error ~ h^2)")
ax.loglog(acc_N, err_simp, "o-", color="#1f77b4",
          label="Simpson    (error ~ h^4)")
ax.axhline(1.0 / (1 << 14), color="#999999", linestyle=":",
           label="Q14 quantization floor (1 LSB)")
ax.set_xticks(acc_N)
ax.set_xticklabels([str(n) for n in acc_N])
ax.set_xlabel("number of panels  N  (single node)")
ax.set_ylabel("| pi_est - pi |")
ax.set_title("Accuracy by method — Simpson reaches the Q14 floor "
             "by N=4")
ax.grid(True, which="both", alpha=0.3)
ax.legend(loc="lower left", fontsize=9)
fig.tight_layout()
fig.savefig("accuracy.png", dpi=140)
fig.savefig("accuracy.svg")
plt.close(fig)

print("wrote speedup.{png,svg} and accuracy.{png,svg}")
print(f"  (2) speedup P=1..4 : {[round(x, 2) for x in s_speedup]}  (deterministic)")
print(f"  (1) speedup P=1..4 : median {d_speedup_med}  range "
      f"{list(zip(d_speedup_lo, d_speedup_hi))}  (host-noisy)")
print("  Newton            : 1.00 flat (analytic; serial recurrence)")
