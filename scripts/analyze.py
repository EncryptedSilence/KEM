#!/usr/bin/env python3
"""Compute summary statistics from the experiment's raw CSVs.

Auto-detects KEM micro-benchmark CSVs (have keygen_ns) and network CSVs
(have tls_handshake_ns). For network CSVs only measured rows (phase==1)
with status==ok feed the timing percentiles; errors/timeouts are tallied
separately. Emits a Markdown report. Pure stdlib.

Usage: analyze.py --out results/report/stats.md FILE.csv [FILE.csv ...]
"""
import csv
import sys
import os
import statistics
from pathlib import Path


def pctile(sorted_vals, q):
    """Nearest-rank percentile; q in [0,100]."""
    if not sorted_vals:
        return 0
    if len(sorted_vals) == 1:
        return sorted_vals[0]
    k = max(1, int(round(q / 100.0 * len(sorted_vals))))
    k = min(k, len(sorted_vals))
    return sorted_vals[k - 1]


def summarize(vals):
    s = sorted(vals)
    return {
        "n": len(s),
        "median": statistics.median(s) if s else 0,
        "p95": pctile(s, 95),
        "p99": pctile(s, 99),
        "max": s[-1] if s else 0,
    }


def ns_ms(x):
    return f"{x/1e6:.3f}"


def fmt_timing_table(title, rows, cols):
    out = [f"### {title}", ""]
    out.append("| metric | n | median (ms) | p95 (ms) | p99 (ms) | max (ms) |")
    out.append("|---|---|---|---|---|---|")
    for name, vals in rows:
        st = summarize(vals)
        out.append(f"| {name} | {st['n']} | {ns_ms(st['median'])} | "
                   f"{ns_ms(st['p95'])} | {ns_ms(st['p99'])} | {ns_ms(st['max'])} |")
    out.append("")
    return "\n".join(out)


def analyze_kem(path, rows):
    kg = [int(r["keygen_ns"]) for r in rows]
    en = [int(r["encaps_ns"]) for r in rows]
    de = [int(r["decaps_ns"]) for r in rows]
    mism = sum(1 for r in rows if r.get("secret_match") == "0")
    body = fmt_timing_table(f"KEM micro-benchmark — `{os.path.basename(path)}`",
                            [("keygen", kg), ("encapsulation", en), ("decapsulation", de)],
                            None)
    body += f"\nCycles: {len(rows)}; secret mismatches: {mism}.\n"
    return body


def analyze_net(path, rows):
    measured = [r for r in rows if r.get("phase") == "1"]
    ok = [r for r in measured if r.get("status") == "ok"]
    n_ok = len(ok)
    n_err = sum(1 for r in rows if r.get("status", "").startswith("err") or r.get("status") == "connect_err")
    n_tmo = sum(1 for r in rows if r.get("status") == "timeout")
    resumed_any = any(r.get("resumed") not in ("0", None, "") for r in ok)

    def col(name):
        return [int(r[name]) for r in ok if r.get(name, "").isdigit()]

    timing_rows = [("tcp_connect", col("tcp_connect_ns")),
                   ("tls_handshake", col("tls_handshake_ns")),
                   ("key_confirm", col("keyconfirm_ns")),
                   ("cpu", col("cpu_ns"))]
    body = fmt_timing_table(f"Network — `{os.path.basename(path)}`", timing_rows, None)

    bin_med = statistics.median(col("bytes_in")) if col("bytes_in") else 0
    bout_med = statistics.median(col("bytes_out")) if col("bytes_out") else 0
    rss_max = max((int(r["peak_rss_kb"]) for r in rows if r.get("peak_rss_kb", "").isdigit()), default=0)
    group = ok[0]["group"] if ok else "-"
    ver = ok[0]["tls_version"] if ok else "-"
    cipher = ok[0]["cipher"] if ok else "-"
    body += (f"\nNegotiated: group=`{group}` version=`{ver}` cipher=`{cipher}`; "
             f"resumed(any measured)={resumed_any}.\n"
             f"Handshake bytes (median): in={bin_med} out={bout_med} "
             f"(TLS-record level, BIO counters).\n"
             f"Peak process RSS (whole process): {rss_max} KB.\n"
             f"Connections: ok={n_ok}, errors={n_err}, timeouts={n_tmo}.\n")
    return body


def main(argv):
    out_path = None
    files = []
    i = 0
    while i < len(argv):
        if argv[i] == "--out":
            out_path = argv[i + 1]
            i += 2
        else:
            files.append(argv[i])
            i += 1
    if not files:
        print("usage: analyze.py --out FILE.md CSV...", file=sys.stderr)
        return 2

    sections = ["# Experiment statistics", ""]
    for f in files:
        if not os.path.exists(f):
            sections.append(f"_missing: {f}_\n")
            continue
        with open(f, newline="") as fh:
            rows = list(csv.DictReader(fh))
        if not rows:
            sections.append(f"_empty: {f}_\n")
            continue
        header = rows[0].keys()
        if "keygen_ns" in header:
            sections.append(analyze_kem(f, rows))
        elif "tls_handshake_ns" in header:
            sections.append(analyze_net(f, rows))
        else:
            sections.append(f"_unknown CSV schema: {f}_\n")

    report = "\n".join(sections)
    if out_path:
        Path(out_path).parent.mkdir(parents=True, exist_ok=True)
        Path(out_path).write_text(report)
        print(f"wrote {out_path}")
    else:
        print(report)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
