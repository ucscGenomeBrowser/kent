#!/usr/bin/env python3
"""
posSweep.py - run the bench at multiple named positions on the same hub.

The N sweep (nSweep.py) varies hub-wide feature count by rebuilding testHub
at each N. This sweep keeps the hub fixed and varies the *viewed window*, so
each row measures quickLift overhead at a specific in-window feature density.
At the default testHub (N=5000 features uniformly distributed across
chr22:15M-50M at 7kb stride), the canonical positions exercise:

  sparse       chr22:1M-2M           0 features in window (overhead floor)
  narrow_dense chr22:25M-25.1M       ~14 features (100kb)
  medium       chr22:20M-25M         ~714 features (5Mb)
  wide         chr22:15M-50M         ~5000 features (full region)

For each position:
  1. cases.yaml is loaded; every hub variant's `position` field is overridden
     with the swept position. Saved-session variants are skipped (their
     position is fixed in the saved session and can't be overridden).
  2. quickLiftBench.py runs against the rewritten config.
  3. The per-position results.tsv is read back, prepended with
     position_name + position columns, and appended to a single sweep.tsv.

Refs Redmine #37445.
"""

import argparse
import copy
import csv
import os
import statistics
import subprocess
import sys
from datetime import datetime

import yaml


HERE = os.path.dirname(os.path.abspath(__file__))
BENCH = os.path.join(HERE, "quickLiftBench.py")
DEFAULT_CASES_FILE = os.path.join(HERE, "cases.yaml")
DEFAULT_CASES = "mode_b_bb,mode_b_bw,mode_c_hs1_bb,mode_c_hs1_bw"

# Canonical positions for the default testHub (N=5000, stride=7000bp,
# features in chr22:15M-50M). Each tuple is (name, position, note).
DEFAULT_POSITIONS = [
    ("sparse",       "chr22:1000000-2000000",   "1Mb outside feature region (0 features)"),
    ("narrow_dense", "chr22:25000000-25100000", "100kb inside region (~14 features at N=5000)"),
    ("medium",       "chr22:20000000-25000000", "5Mb inside region (~714 features at N=5000)"),
    ("wide",         "chr22:15000000-50000000", "full 35Mb feature region (~5000 features at N=5000)"),
]


def parse_args():
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "--positions",
        default=None,
        help="Comma-separated list of name:chr:start-end entries. Default: "
             "built-in canonical set (sparse, narrow_dense, medium, wide).",
    )
    p.add_argument(
        "--cases",
        default=DEFAULT_CASES,
        help=f"Comma-separated case ids (default: {DEFAULT_CASES})",
    )
    p.add_argument(
        "--config",
        default=DEFAULT_CASES_FILE,
        help="Base cases.yaml whose hub variant positions are rewritten",
    )
    p.add_argument(
        "--iterations",
        type=int,
        default=10,
        help="Iterations per (position, variant) (default: 10)",
    )
    p.add_argument(
        "--warmup",
        type=int,
        default=1,
        help="Warmup iterations per (position, variant) (default: 1)",
    )
    p.add_argument(
        "--out",
        default=None,
        help="Output dir (default: results/possweep-<timestamp>/)",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the plan and exit without running",
    )
    return p.parse_args()


def parse_positions(raw):
    """raw is name:chr:start-end,name2:chr:start-end. Returns list of tuples
    (name, "chr:start-end", note=""). Splits only on the first two colons so
    chrom strings containing further punctuation survive."""
    out = []
    for entry in raw.split(","):
        entry = entry.strip()
        if not entry:
            continue
        parts = entry.split(":", 2)
        if len(parts) != 3:
            sys.exit(f"bad --positions entry {entry!r}: want name:chr:start-end")
        name, chrom, span = parts
        out.append((name, f"{chrom}:{span}", ""))
    return out


def rewrite_positions(base_cfg, position, case_ids):
    """Filter cases.yaml to selected ids and rewrite each hub variant's
    `position` field. Saved-session variants are kept as-is (their position
    lives in the session). Returns the new cfg dict and a list of warnings."""
    warnings = []
    out_cases = []
    for case in base_cfg.get("cases") or []:
        if case["id"] not in case_ids:
            continue
        new_case = copy.deepcopy(case)
        for vname, vraw in (new_case.get("variants") or {}).items():
            if isinstance(vraw, dict) and "position" in vraw:
                vraw["position"] = position
            elif isinstance(vraw, str):
                warnings.append(
                    f"case {case['id']!r} variant {vname!r}: saved-session "
                    f"variant — position is fixed in the session, sweep "
                    f"will measure that session's window, not {position}"
                )
        out_cases.append(new_case)
    out = dict(base_cfg)
    out["cases"] = out_cases
    return out, warnings


def median_or_none(xs):
    xs = [x for x in xs if x is not None]
    if not xs:
        return None
    return int(statistics.median(xs))


def p90(xs):
    xs = sorted(x for x in xs if x is not None)
    if not xs:
        return None
    if len(xs) == 1:
        return xs[0]
    k = max(0, int(round(0.9 * (len(xs) - 1))))
    return xs[k]


def to_int(v):
    if v == "" or v is None:
        return None
    try:
        return int(v)
    except ValueError:
        return None


def write_summary(merged_path, summary_path):
    """Per (position_name, case, variant): median/p90 of total/load/draw.
    Then per (position_name, case): native vs lifted ratio of total medians."""
    rows = []
    with open(merged_path) as f:
        reader = csv.DictReader(f, delimiter="\t")
        for r in reader:
            rows.append(r)

    by_group = {}
    for r in rows:
        if r.get("error"):
            continue
        key = (r["position_name"], r["position"], r["case"], r["variant"])
        by_group.setdefault(key, []).append(r)

    fields = [
        "position_name", "position", "case", "variant",
        "n_ok",
        "total_median", "total_p90",
        "load_sum_median", "load_sum_p90",
        "draw_sum_median", "draw_sum_p90",
    ]
    per_key = {}
    with open(summary_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, delimiter="\t")
        w.writeheader()
        for key in sorted(by_group):
            group = by_group[key]
            total = [to_int(r["total_ms"]) for r in group]
            load = [to_int(r["load_ms_sum"]) for r in group]
            draw = [to_int(r["draw_ms_sum"]) for r in group]
            stats = {
                "position_name": key[0], "position": key[1],
                "case": key[2], "variant": key[3],
                "n_ok": len(group),
                "total_median": median_or_none(total),
                "total_p90": p90(total),
                "load_sum_median": median_or_none(load),
                "load_sum_p90": p90(load),
                "draw_sum_median": median_or_none(draw),
                "draw_sum_p90": p90(draw),
            }
            w.writerow({k: ("" if v is None else v) for k, v in stats.items()})
            per_key[key] = stats

        f.write("\n# Pairwise ratio per (position_name, case): lifted/native\n")
        pair_fields = [
            "position_name", "position", "case",
            "native_total_median", "lifted_total_median",
            "ratio_total", "ratio_load_sum", "ratio_draw_sum",
        ]
        wp = csv.DictWriter(f, fieldnames=pair_fields, delimiter="\t")
        wp.writeheader()
        seen = set((pn, pos, c) for (pn, pos, c, _v) in per_key)
        for pn, pos, c in sorted(seen):
            ns = per_key.get((pn, pos, c, "native"))
            ls = per_key.get((pn, pos, c, "lifted"))
            if not ns or not ls:
                continue
            def ratio(num, denom):
                if num is None or denom is None or denom == 0:
                    return ""
                return f"{num / denom:.2f}"
            wp.writerow({
                "position_name": pn, "position": pos, "case": c,
                "native_total_median": "" if ns["total_median"] is None else ns["total_median"],
                "lifted_total_median": "" if ls["total_median"] is None else ls["total_median"],
                "ratio_total": ratio(ls["total_median"], ns["total_median"]),
                "ratio_load_sum": ratio(ls["load_sum_median"], ns["load_sum_median"]),
                "ratio_draw_sum": ratio(ls["draw_sum_median"], ns["draw_sum_median"]),
            })


def main():
    args = parse_args()

    case_ids = [c.strip() for c in args.cases.split(",") if c.strip()]
    if not case_ids:
        sys.exit("must supply at least one case id")

    positions = (
        parse_positions(args.positions) if args.positions else list(DEFAULT_POSITIONS)
    )
    if not positions:
        sys.exit("must supply at least one position")

    with open(args.config) as f:
        base_cfg = yaml.safe_load(f)
    known_ids = {c["id"] for c in base_cfg.get("cases") or []}
    unknown = set(case_ids) - known_ids
    if unknown:
        sys.exit(f"unknown case id(s): {sorted(unknown)} (known: {sorted(known_ids)})")

    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = args.out or os.path.join(HERE, "results", f"possweep-{timestamp}")
    os.makedirs(out_dir, exist_ok=True)
    sweep_path = os.path.join(out_dir, "sweep.tsv")
    summary_path = os.path.join(out_dir, "sweep_summary.tsv")
    print(f"posSweep output -> {out_dir}", file=sys.stderr)
    print(
        f"plan: {len(positions)} positions x {args.iterations} iterations "
        f"x {len(case_ids)} cases x 2 variants",
        file=sys.stderr,
    )
    for name, pos, note in positions:
        suffix = f"  ({note})" if note else ""
        print(f"  {name}: {pos}{suffix}", file=sys.stderr)
    if args.dry_run:
        return

    merged_writer = None
    with open(sweep_path, "w", newline="") as merged_fp:
        for name, pos, _note in positions:
            point_cfg, warns = rewrite_positions(base_cfg, pos, case_ids)
            for w in warns:
                print(f"  warning: {w}", file=sys.stderr)

            cfg_path = os.path.join(out_dir, f"cases_{name}.yaml")
            with open(cfg_path, "w") as f:
                yaml.safe_dump(point_cfg, f, sort_keys=False)

            point_out = os.path.join(out_dir, name)
            print(f"\n=== bench position={name} ({pos})", file=sys.stderr)
            subprocess.run(
                [
                    sys.executable, BENCH,
                    "--config", cfg_path,
                    "--iterations", str(args.iterations),
                    "--warmup", str(args.warmup),
                    "--out", point_out,
                ],
                check=True,
            )

            per_point_results = os.path.join(point_out, "results.tsv")
            with open(per_point_results) as rf:
                reader = csv.DictReader(rf, delimiter="\t")
                if merged_writer is None:
                    fields = ["position_name", "position"] + reader.fieldnames
                    merged_writer = csv.DictWriter(
                        merged_fp, fieldnames=fields, delimiter="\t"
                    )
                    merged_writer.writeheader()
                for row in reader:
                    row["position_name"] = name
                    row["position"] = pos
                    merged_writer.writerow(row)
            merged_fp.flush()

    write_summary(sweep_path, summary_path)
    print(f"\nmerged: {sweep_path}", file=sys.stderr)
    print(f"summary: {summary_path}", file=sys.stderr)


if __name__ == "__main__":
    main()
