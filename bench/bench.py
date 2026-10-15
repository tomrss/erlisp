#!/usr/bin/env python3
# erlisp benchmarker: erlisp against itself, commit after commit.
#
#   bench.py run [-n RUNS] [--binary PATH] [--timeout S] [--out DIR] SRC...
#   bench.py compare A.json B.json | --most-recent DIR
#   bench.py info FILE.json [-v]
#
# run executes every source RUNS times and writes one json for the whole
# suite, named <date>-<commit:8>-<title>-<binary:8>[-dirty].json. Every
# run is stored; the best one (min wall time) is the reference.
# Time and RSS come from wait4() on the child, no /usr/bin/time parsing.

import argparse
import datetime
import difflib
import hashlib
import json
import math
import os
import platform
import re
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def git(*args):
    return subprocess.run(["git", *args],
                          cwd=ROOT,
                          capture_output=True,
                          text=True,
                          check=True).stdout.strip()


def commit_info():
    # untracked files (new benchmarks, results) do not make the tree dirty
    dirty = git("status", "--porcelain", "--untracked-files=no") != ""
    return {
        "sha": git("rev-parse", "HEAD"),
        "title": git("log", "-1", "--format=%s"),
        "dirty": dirty,
    }


def slug(s, maxlen=30):
    s = re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")
    return s[:maxlen].strip("-")


def cpu_name():
    try:
        if sys.platform == "darwin":
            return subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"],
                                  capture_output=True,
                                  text=True).stdout.strip()
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor()


def machine_info():
    return {
        "host": socket.gethostname(),
        "os": f"{platform.system()} {platform.release()}",
        "arch": platform.machine(),
        "cpu": cpu_name(),
    }


# one execution of binary on source: stdout, exit code, wall/user/sys
# time and max RSS of the child
def measure(binary, source, timeout):
    start = time.perf_counter()
    proc = subprocess.Popen([binary, source],
                            stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE)
    killed = threading.Event()

    def kill():
        killed.set()
        proc.kill()

    timer = threading.Timer(timeout, kill)
    timer.start()
    out = proc.stdout.read()
    proc.stdout.close()
    timer.cancel()
    # wait4 instead of proc.wait(): it also gives the child's rusage
    _, status, ru = os.wait4(proc.pid, 0)
    wall = time.perf_counter() - start
    proc.returncode = os.waitstatus_to_exitcode(status)

    # ru_maxrss is in bytes on macOS, in KB on Linux
    rss = ru.ru_maxrss if sys.platform == "darwin" else ru.ru_maxrss * 1024
    return {
        "wall": wall,
        "user": ru.ru_utime,
        "sys": ru.ru_stime,
        "rss": rss,
        "exit_code": proc.returncode,
        "stdout": out.decode(errors="replace"),
        "timed_out": killed.is_set(),
    }


def cmd_run(args):
    binary = Path(args.binary).resolve()
    commit = commit_info()
    date = datetime.datetime.now()
    bin_sha = sha256_file(binary)

    tests = []
    for src in args.sources:
        name = Path(src).stem
        runs = []
        for i in range(args.runs):
            r = measure(binary, src, args.timeout)
            runs.append(r)
            mark = " TIMEOUT" if r["timed_out"] else ""
            print(f"{name} [{i + 1}/{args.runs}] {r['wall']:.3f}s{mark}",
                  file=sys.stderr)
            if r["timed_out"]:
                break
        best = min(runs, key=lambda r: r["wall"])
        tests.append({
            "name":
            name,
            "source":
            src,
            "sha256":
            sha256_file(src),
            "exit_code":
            best["exit_code"],
            "stdout":
            best["stdout"],
            "timed_out":
            best["timed_out"],
            "best": {
                k: best[k]
                for k in ("wall", "user", "sys", "rss")
            },
            "runs": [{
                k: r[k]
                for k in ("wall", "user", "sys", "rss")
            } for r in runs],
        })

    result = {
        "date": date.isoformat(timespec="seconds"),
        "machine": machine_info(),
        "commit": commit,
        "binary": {
            "path": str(binary),
            "sha256": bin_sha
        },
        "runs_per_test": args.runs,
        "timeout": args.timeout,
        "tests": tests,
    }

    name = (f"{date:%Y%m%d-%H%M%S}-{commit['sha'][:8]}-{slug(commit['title'])}"
            f"-{bin_sha[:8]}{'-dirty' if commit['dirty'] else ''}.json")
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / name
    with open(path, "w") as f:
        json.dump(result, f, indent=2)
        f.write("\n")

    print(file=sys.stderr)
    print_info(result)
    print(f"\nwritten {path}")


def load(path):
    with open(path) as f:
        return json.load(f)


def mb(nbytes):
    return f"{nbytes / (1 << 20):.1f}"


# relative spread of the wall times: (max - min) / min
def spread(test):
    walls = [r["wall"] for r in test["runs"]]
    return (max(walls) - min(walls)) / min(walls)


def median(xs):
    xs = sorted(xs)
    n = len(xs)
    return xs[n // 2] if n % 2 else (xs[n // 2 - 1] + xs[n // 2]) / 2


# ANSI colors, only on a terminal and unless NO_COLOR is set
USE_COLOR = sys.stdout.isatty() and "NO_COLOR" not in os.environ
COLORS = {"red": "31", "green": "32", "yellow": "33", "dim": "2"}

# RSS changes within this fraction are noise
RSS_THRESHOLD = 0.05


def paint(text, color):
    if not (USE_COLOR and color):
        return text
    return f"\033[{COLORS[color]}m{text}\033[0m"


# a cell is a string or a (string, color) pair
def table(headers, rows):

    def text(c):
        return c[0] if isinstance(c, tuple) else str(c)

    widths = [max(len(text(x)) for x in col) for col in zip(headers, *rows)]

    def line(cells):
        # first column left aligned, numbers right aligned. padding
        # before painting: escape codes have no width on screen
        out = []
        for i, (c, w) in enumerate(zip(cells, widths)):
            s = text(c).ljust(w) if i == 0 else text(c).rjust(w)
            out.append(paint(s, c[1]) if isinstance(c, tuple) else s)
        return "  ".join(out)

    print(line(headers))
    print("  ".join("-" * w for w in widths))
    for row in rows:
        print(line(row))


def header(data):
    c, m, b = data["commit"], data["machine"], data["binary"]
    dirty = " " + paint("DIRTY", "yellow") if c["dirty"] else ""
    print(f"date     {data['date']}")
    print(f"commit   {c['sha'][:8]} {c['title']}{dirty}")
    print(f"binary   {b['path']} {b['sha256'][:8]}")
    print(f"machine  {m['host']} · {m['os']} · {m['arch']} · {m['cpu']}")
    print(f"runs     {data['runs_per_test']} per test")


def status(test):
    if test["timed_out"]:
        return "timeout"
    return str(test["exit_code"])


def print_info(data, verbose=False):
    header(data)
    print()
    rows = []
    for t in data["tests"]:
        walls = [r["wall"] for r in t["runs"]]
        best = t["best"]
        rows.append([
            t["name"],
            f"{best['wall']:.3f}",
            f"{median(walls):.3f}",
            f"{spread(t) * 100:.1f}%",
            f"{best['user'] + best['sys']:.3f}",
            mb(best["rss"]),
            status(t),
        ])
    table([
        "test", "best (s)", "median (s)", "spread", "cpu (s)", "RSS (MB)",
        "exit"
    ], rows)
    if verbose:
        for t in data["tests"]:
            print(f"\n=== {t['name']} ({t['source']}) stdout:")
            print(t["stdout"], end="" if t["stdout"].endswith("\n") else "\n")


def cmd_info(args):
    print_info(load(args.file), args.verbose)


# the two most recent benchmarks in dir, by the date inside the json:
# (second most recent, most recent)
def most_recent(dir):
    files = sorted((load(p)["date"], p) for p in Path(dir).glob("*.json"))
    if len(files) < 2:
        sys.exit(f"compare: less than two json in {dir}")
    return files[-2][1], files[-1][1]


def cmd_compare(args):
    if args.most_recent:
        if args.a or args.b:
            sys.exit("compare: give either A B or --most-recent DIR")
        args.a, args.b = most_recent(args.most_recent)
    elif not (args.a and args.b):
        sys.exit("compare: give either A B or --most-recent DIR")
    a, b = load(args.a), load(args.b)

    print(f"A: {args.a}")
    header(a)
    print(f"\nB: {args.b}")
    header(b)

    warnings = []
    ma, mb_ = a["machine"], b["machine"]
    if (ma["host"], ma["cpu"]) != (mb_["host"], mb_["cpu"]):
        warnings.append("different machines, times are not comparable")

    ta = {t["name"]: t for t in a["tests"]}
    tb = {t["name"]: t for t in b["tests"]}
    common = [n for n in ta if n in tb]
    for n in ta:
        if n not in tb:
            warnings.append(f"{n}: only in A")
    for n in tb:
        if n not in ta:
            warnings.append(f"{n}: only in B")

    rows = []
    ratios = []
    changed_output = []
    for n in common:
        x, y = ta[n], tb[n]
        wa, wb = x["best"]["wall"], y["best"]["wall"]
        ratio = wb / wa
        delta = ratio - 1
        # a change smaller than the run to run spread is just noise
        if x["timed_out"] or y["timed_out"]:
            verdict = "timeout"
        elif abs(delta) <= max(spread(x), spread(y)):
            verdict = "noise"
        else:
            verdict = "faster" if delta < 0 else "slower"
        if verdict != "timeout":
            ratios.append(ratio)

        # no run to run spread for RSS: fixed threshold
        ra, rb = x["best"]["rss"], y["best"]["rss"]
        if ra:
            rss_delta = rb / ra - 1
            if abs(rss_delta) <= RSS_THRESHOLD:
                rss_verdict = ("noise", "dim")
            elif rss_delta < 0:
                rss_verdict = ("smaller", "green")
            else:
                rss_verdict = ("bigger", "red")
            rss_cell = (f"{rss_delta * 100:+.1f}%", rss_verdict[1])
        else:
            rss_verdict = rss_cell = "-"
        out_same = (x["stdout"] == y["stdout"]
                    and x["exit_code"] == y["exit_code"])
        if not out_same:
            changed_output.append(n)
        color = {
            "faster": "green",
            "slower": "red",
            "noise": "dim",
            "timeout": "red"
        }[verdict]
        rows.append([
            n,
            f"{wa:.3f}",
            f"{wb:.3f}",
            (f"{delta * 100:+.1f}%", color),
            (verdict, color),
            mb(ra),
            mb(rb),
            rss_cell,
            rss_verdict,
            ("same", None) if x["sha256"] == y["sha256"] else
            ("CHANGED", "yellow"),
            ("same", None) if out_same else ("DIFFERS", "red"),
        ])

    if warnings:
        print("\nwarnings:")
        for w in warnings:
            print(paint(f"  ! {w}", "yellow"))

    print()
    table([
        "test", "A (s)", "B (s)", "time Δ", "", "A RSS", "B RSS", "RSS Δ", "",
        "source", "output"
    ], rows)

    if ratios:
        geo = math.exp(sum(math.log(r) for r in ratios) / len(ratios))
        print(f"\ngeometric mean B/A: {geo:.3f} ({(geo - 1) * 100:+.1f}%)")

    for n in changed_output:
        x, y = ta[n], tb[n]
        print(paint(f"\n=== {n}: output differs", "red"))
        if x["exit_code"] != y["exit_code"]:
            print(f"exit code: A {status(x)}, B {status(y)}")
        diff = difflib.unified_diff(x["stdout"].splitlines(),
                                    y["stdout"].splitlines(),
                                    "A",
                                    "B",
                                    lineterm="")
        for line in diff:
            print(line)


def main():
    p = argparse.ArgumentParser(description="erlisp benchmarker")
    sub = p.add_subparsers(dest="command", required=True)

    r = sub.add_parser("run", help="run the benchmarks and write a json")
    r.add_argument("sources", nargs="+", help="erlisp source files")
    r.add_argument("-n",
                   "--runs",
                   type=int,
                   default=5,
                   help="runs per test (default: 5)")
    r.add_argument("--binary",
                   default=str(ROOT / "erlisp"),
                   help="erlisp executable (default: ./erlisp)")
    r.add_argument("--timeout",
                   type=float,
                   default=300,
                   help="seconds per run before killing it (default: 300)")
    r.add_argument("--out",
                   default=str(ROOT / "bench" / "results"),
                   help="output directory (default: bench/results)")
    r.set_defaults(func=cmd_run)

    c = sub.add_parser("compare", help="compare two benchmark json")
    c.add_argument("a", nargs="?", help="baseline json")
    c.add_argument("b", nargs="?", help="new json")
    c.add_argument("--most-recent",
                   metavar="DIR",
                   help="compare the two most recent json in DIR")
    c.set_defaults(func=cmd_compare)

    i = sub.add_parser("info", help="show a benchmark json")
    i.add_argument("file", help="benchmark json")
    i.add_argument("-v",
                   "--verbose",
                   action="store_true",
                   help="also print the stdout of every test")
    i.set_defaults(func=cmd_info)

    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
