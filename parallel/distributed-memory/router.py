#!/usr/bin/env python3
"""Distributed-memory (message-passing) scaling harness for parallel pi.

This Python process IS the network: it spawns P independent ucsim_z80
nodes (no shared memory between them) and routes their framed messages.
Each node runs the SAME binary (SPMD); the router hands rank 0 the
others' partial sums to reduce.

Runs P = 1,2,3,4. Each node is driven (no -G) with the monitor script
"run\\nstate\\nquit\\n": `run` blocks until the node's simif-stop, then
`state` prints the Z80 clock count, then `quit`. We parse per-rank clks
and the result, then print a speedup / accuracy table.

ucsim_z80 is taken from $UCSIM, else from PATH.
"""
import os
import re
import select
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
UCSIM = os.environ.get("UCSIM", "ucsim_z80")
IHX = "node_pi.ihx"
QBITS = 14
PI = 3.14159265
DEADLINE_S = 90.0
CLK_RE = re.compile(r"Total time since last reset=.*\((\d+)\s+clks\)")


def make_fifo(p):
    if os.path.exists(p):
        os.unlink(p)
    os.mkfifo(p)


def run_once(P):
    fd_in, fd_out, procs, bufs = {}, {}, {}, {}
    clks = {}
    piline = {}

    def drain(proc, r):
        for raw in iter(proc.stdout.readline, b""):
            s = raw.decode("latin1", "replace").rstrip("\n")
            if not s:
                continue
            if os.environ.get("DBG"):
                print(f"  [n{r}] {s}")
            m = CLK_RE.search(s)
            if m:
                clks[r] = int(m.group(1))
            if s.startswith("pi~="):
                piline[r] = s

    for r in range(P):
        fin, fout, cfg = f"n{r}.in", f"n{r}.out", f"n{r}.cfg"
        make_fifo(fin)
        make_fifo(fout)
        with open(cfg, "w") as f:
            f.write(f'set hardware simif fin "{fin}"\n')
            f.write(f'set hardware simif fout "{fout}"\n')
        fd_in[r] = os.open(fin, os.O_RDWR)
        fd_out[r] = os.open(fout, os.O_RDWR)
        bufs[r] = b""

    for r in range(P):
        p = subprocess.Popen(
            [UCSIM, "-t", "Z80", "-P", "-I", "if=rom[0x4000]",
             "-C", f"n{r}.cfg", IHX],
            cwd=HERE, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        procs[r] = p
        threading.Thread(target=drain, args=(p, r), daemon=True).start()

    for r in range(P):
        os.write(fd_in[r], bytes([r, P]))                 # hello
        # NOTE: do NOT send "quit" yet. `run` blocks until the node's
        # simif-stop, then `state` prints the clk count. If we queued
        # "quit" here it races the drain thread and the clk line is
        # sometimes lost (=> "?"). We send "quit" only AFTER the clk
        # has been captured (or a grace timeout), below.
        procs[r].stdin.write(b"run\nstate\n")
        procs[r].stdin.flush()

    report = None
    deadline = time.time() + DEADLINE_S
    fds = [fd_out[r] for r in range(P)]
    rev = {fd_out[r]: r for r in range(P)}
    while report is None and time.time() < deadline:
        rr, _, _ = select.select(fds, [], [], max(0.05, deadline - time.time()))
        for fd in rr:
            r = rev[fd]
            chunk = os.read(fd, 256)
            if not chunk:
                continue
            bufs[r] += chunk
            buf = bufs[r]
            i = 0
            while len(buf) - i >= 2:
                dst, ln = buf[i], buf[i + 1]
                if len(buf) - i < 2 + ln:
                    break
                payload = bytes(buf[i + 2:i + 2 + ln])
                i += 2 + ln
                if dst == 0xFE:
                    report = payload
                else:
                    os.write(fd_in[dst], bytes([dst, ln]) + payload)
            bufs[r] = buf[i:]

    # the result is in; now wait for every node's clk line to be drained
    # (deterministic for P=1; this removes the "?" capture race), then
    # tell the nodes to quit.
    grace = time.time() + 5.0
    while time.time() < grace and not all(r in clks for r in range(P)):
        time.sleep(0.02)
    for r in range(P):
        try:
            procs[r].stdin.write(b"quit\n")
            procs[r].stdin.flush()
        except (BrokenPipeError, ValueError, OSError):
            pass

    for r in range(P):
        try:
            procs[r].wait(timeout=5)
        except subprocess.TimeoutExpired:
            procs[r].kill()
        os.close(fd_in[r])
        os.close(fd_out[r])
        for e in ("in", "out"):
            q = f"n{r}.{e}"
            if os.path.exists(q):
                os.unlink(q)

    val = None
    if report and len(report) == 4:
        u = report[0] | (report[1] << 8) | (report[2] << 16) | (report[3] << 24)
        val = u / float(1 << QBITS)
    return clks, val


def main():
    os.chdir(HERE)
    rows = []
    base = None
    Ps = (int(sys.argv[1]),) if len(sys.argv) > 1 else (1, 2, 3, 4)
    for P in Ps:
        clks, val = run_once(P)
        per = [clks.get(r) for r in range(P)]
        have = [c for c in per if c is not None]
        crit = max(have) if have else None
        tot = sum(have) if have else None
        if P == 1:
            base = crit
        sp = (base / crit) if (crit and base) else None
        rows.append((P, per, crit, tot, val, sp))

    print()
    print(f"  trapezoidal pi = integral_0^1 4/(1+x^2) dx, Q{QBITS}, N=64, "
          f"true pi = {PI}")
    print("  " + "-" * 78)
    print(f"  {'P':>2} | {'per-rank Z80 clks':<34} | {'crit-path':>10} | "
          f"{'total':>9} | {'pi~=':>7} | {'speedup':>7}")
    print("  " + "-" * 78)
    for P, per, crit, tot, val, sp in rows:
        pr = " ".join(f"{c}" if c is not None else "?" for c in per)
        sps = f"{sp:.2f}x" if sp else "-"
        vs = f"{val:.5f}" if val is not None else "FAIL"
        cs = f"{crit}" if crit is not None else "?"
        ts = f"{tot}" if tot is not None else "?"
        print(f"  {P:>2} | {pr:<34} | {cs:>10} | {ts:>9} | "
              f"{vs:>7} | {sps:>7}")
    print("  " + "-" * 78)
    print("  note: rank0 carries reduce+recv+printf; N fixed so per-rank")
    print("        compute ~ N/P but coordination does not shrink "
          "=> sublinear.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
