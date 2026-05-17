#!/usr/bin/env python3
"""
nSweep.py - rebuild testHub at multiple (N, BW_STEP) sizes and run
quickLiftBench.py at each size. Tags every row of every per-point results.tsv
with the sweep's N and BW_STEP so downstream plotting can isolate
"render time vs feature count" or "render time vs bin count" curves.

For each (N, BW_STEP) combination:
  1. buildTestHub.sh is run with N + BW_STEP env vars into a per-point hub
     directory under --hub-dest-base (default
     ~/public_html/quickLiftBench/sweep/N{N}_S{S}/).
  2. cases.yaml is loaded and the Mode B / Mode C hub URLs are rewritten to
     point at the per-point hub. The rewritten config is dropped into the
     output dir.
  3. quickLiftBench.py runs with --config <rewritten> --cases <selected>.
  4. The resulting results.tsv is read back, prepended with N + bw_step
     columns, and appended to sweep.tsv in the output dir.

Refs Redmine #37445.
"""

import argparse
import csv
import os
import shutil
import statistics
import subprocess
import sys
from datetime import datetime

import yaml


HERE = os.path.dirname(os.path.abspath(__file__))
BUILDER = os.path.join(HERE, "testHub", "buildTestHub.sh")
BENCH = os.path.join(HERE, "quickLiftBench.py")
DEFAULT_CASES_FILE = os.path.join(HERE, "cases.yaml")
DEFAULT_CASES = "mode_b_bb,mode_b_bw,mode_c_hs1_bb,mode_c_hs1_bw"


def parse_args():
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "--n-values",
        default="500,1000,5000,10000,20000",
        help="Comma-separated feature counts (default: 500,1000,5000,10000,20000)",
    )
    p.add_argument(
        "--bw-step-values",
        default="1000",
        help="Comma-separated bigWig step sizes in bp (default: 1000)",
    )
    p.add_argument(
        "--cases",
        default=DEFAULT_CASES,
        help="Comma-separated bench case ids to run at each point "
             f"(default: {DEFAULT_CASES})",
    )
    p.add_argument(
        "--config",
        default=DEFAULT_CASES_FILE,
        help="Base cases.yaml whose hub URLs are rewritten per point",
    )
    p.add_argument(
        "--iterations",
        type=int,
        default=10,
        help="Iterations per (point, variant) (default: 10). Sweep defaults "
             "lower than the standalone bench since many points are run.",
    )
    p.add_argument(
        "--warmup",
        type=int,
        default=1,
        help="Warmup iterations per (point, variant) (default: 1)",
    )
    p.add_argument(
        "--region-start",
        type=int,
        default=15_000_000,
        help="REGION_START passed to buildTestHub.sh (default: 15000000)",
    )
    p.add_argument(
        "--region-end",
        type=int,
        default=50_000_000,
        help="REGION_END passed to buildTestHub.sh (default: 50000000)",
    )
    p.add_argument(
        "--feature-w",
        type=int,
        default=None,
        help="Override FEATURE_W per point (default: auto-pick from N + region "
             "span so features fit without overlap; clamped to [50, 5000])",
    )
    p.add_argument(
        "--hub-dest-base",
        default=os.path.expanduser("~/public_html/quickLiftBench/sweep"),
        help="Parent dir for per-point hub builds. The URL the bench uses is "
             "inherited from cases.yaml -- this script rewrites the testHub "
             "segment to sweep/<point>, so --hub-dest-base must be served at "
             "the equivalent URL.",
    )
    p.add_argument(
        "--out",
        default=None,
        help="Output dir (default: results/nsweep-<timestamp>/)",
    )
    p.add_argument(
        "--clean-builds",
        action="store_true",
        help="Delete per-point hub dirs after the run (default: keep them)",
    )
    p.add_argument(
        "--skip-existing",
        action="store_true",
        help="Skip hub rebuild if the per-point dir already has hub_hs1.txt",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the plan and exit without building or benching",
    )
    return p.parse_args()


def pick_feature_w(n, region_span, override):
    """Pick a FEATURE_W that lets N features fit in region_span without
    overlap. buildTestHub.sh uses stride = max(span/N, W), so N fits when
    W <= span/N. Clamp to [50, 5000]: small N keeps the original 5000bp
    baseline; very large N still produces a valid BED12 with >=50bp width.
    At N = region_span/50 = 700k (with default 35M region) the region is
    fully packed; beyond that the builder warns and the point is mislabeled."""
    if override is not None:
        return override
    if n <= 0:
        return 5000
    target = region_span // n
    return max(50, min(5000, target))


def rewrite_cases(base_cfg, point_subdir, case_ids):
    """Filter cases.yaml to the selected ids and rewrite each hub variant's
    hubUrl from .../testHub/... to .../sweep/<point_subdir>/... so each
    sweep point hits its own freshly-built hub."""
    src_marker = "/quickLiftBench/testHub/"
    dst_marker = f"/quickLiftBench/sweep/{point_subdir}/"
    out_cases = []
    for case in base_cfg.get("cases") or []:
        if case["id"] not in case_ids:
            continue
        new_case = dict(case)
        new_variants = {}
        for vname, vraw in (case.get("variants") or {}).items():
            if isinstance(vraw, dict) and "hubUrl" in vraw:
                v = dict(vraw)
                v["hubUrl"] = v["hubUrl"].replace(src_marker, dst_marker)
                new_variants[vname] = v
            else:
                new_variants[vname] = vraw
        new_case["variants"] = new_variants
        out_cases.append(new_case)
    out = dict(base_cfg)
    out["cases"] = out_cases
    return out


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
    """Group sweep.tsv rows by (N, bw_step, case, variant) and emit median /
    p90 of total_ms, load_ms_sum, draw_ms_sum. Then a second section with
    per (N, bw_step, case) ratio of lifted/native total medians (sorted by N)
    -- the headline curve the paper plots."""
    rows = []
    with open(merged_path) as f:
        reader = csv.DictReader(f, delimiter="\t")
        for r in reader:
            rows.append(r)

    by_group = {}
    for r in rows:
        if r.get("error"):
            continue
        key = (int(r["N"]), int(r["bw_step"]), r["case"], r["variant"])
        by_group.setdefault(key, []).append(r)

    fields = [
        "N", "bw_step", "case", "variant",
        "n_ok",
        "total_median", "total_p90",
        "load_sum_median", "load_sum_p90",
        "draw_sum_median", "draw_sum_p90",
    ]
    per_key_stats = {}
    with open(summary_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, delimiter="\t")
        w.writeheader()
        for key in sorted(by_group):
            group = by_group[key]
            total = [to_int(r["total_ms"]) for r in group]
            load = [to_int(r["load_ms_sum"]) for r in group]
            draw = [to_int(r["draw_ms_sum"]) for r in group]
            stats = {
                "N": key[0], "bw_step": key[1],
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
            per_key_stats[key] = stats

        f.write("\n# Pairwise ratio per (N, bw_step, case): lifted/native\n")
        pair_fields = [
            "N", "bw_step", "case",
            "native_total_median", "lifted_total_median",
            "ratio_total", "ratio_load_sum", "ratio_draw_sum",
        ]
        wp = csv.DictWriter(f, fieldnames=pair_fields, delimiter="\t")
        wp.writeheader()
        seen_cases = set((n, s, c) for (n, s, c, _v) in per_key_stats)
        for n, s, c in sorted(seen_cases):
            ns = per_key_stats.get((n, s, c, "native"))
            ls = per_key_stats.get((n, s, c, "lifted"))
            if not ns or not ls:
                continue
            def ratio(num, denom):
                if num is None or denom is None or denom == 0:
                    return ""
                return f"{num / denom:.2f}"
            wp.writerow({
                "N": n, "bw_step": s, "case": c,
                "native_total_median": "" if ns["total_median"] is None else ns["total_median"],
                "lifted_total_median": "" if ls["total_median"] is None else ls["total_median"],
                "ratio_total": ratio(ls["total_median"], ns["total_median"]),
                "ratio_load_sum": ratio(ls["load_sum_median"], ns["load_sum_median"]),
                "ratio_draw_sum": ratio(ls["draw_sum_median"], ns["draw_sum_median"]),
            })


def main():
    args = parse_args()

    try:
        n_values = [int(x) for x in args.n_values.split(",") if x.strip()]
        bw_step_values = [int(x) for x in args.bw_step_values.split(",") if x.strip()]
    except ValueError as e:
        sys.exit(f"--n-values / --bw-step-values must be ints: {e}")
    case_ids = [c.strip() for c in args.cases.split(",") if c.strip()]
    if not n_values or not bw_step_values or not case_ids:
        sys.exit("must supply at least one N, one BW_STEP, and one case id")

    with open(args.config) as f:
        base_cfg = yaml.safe_load(f)
    known_ids = {c["id"] for c in base_cfg.get("cases") or []}
    unknown = set(case_ids) - known_ids
    if unknown:
        sys.exit(f"unknown case id(s): {sorted(unknown)} (known: {sorted(known_ids)})")

    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = args.out or os.path.join(HERE, "results", f"nsweep-{timestamp}")
    os.makedirs(out_dir, exist_ok=True)
    sweep_path = os.path.join(out_dir, "sweep.tsv")
    summary_path = os.path.join(out_dir, "sweep_summary.tsv")
    print(f"sweep output -> {out_dir}", file=sys.stderr)

    region_span = args.region_end - args.region_start
    if region_span <= 0:
        sys.exit("--region-end must be greater than --region-start")

    points = []
    for n in n_values:
        for s in bw_step_values:
            w = pick_feature_w(n, region_span, args.feature_w)
            points.append((n, s, w))

    print(
        f"plan: {len(points)} points, "
        f"{args.iterations} iterations x {len(case_ids)} cases x 2 variants, "
        f"region={args.region_start}-{args.region_end} ({region_span}bp)",
        file=sys.stderr,
    )
    for n, s, w in points:
        print(f"  N={n} BW_STEP={s} FEATURE_W={w}", file=sys.stderr)
    if args.dry_run:
        return

    merged_writer = None
    built_dirs = []
    with open(sweep_path, "w", newline="") as merged_fp:
        for n, s, w in points:
            point_subdir = f"N{n}_S{s}_W{w}"
            hub_dir = os.path.join(args.hub_dest_base, point_subdir)
            os.makedirs(hub_dir, exist_ok=True)
            built_dirs.append(hub_dir)

            hub_marker = os.path.join(hub_dir, "hub_hs1.txt")
            if args.skip_existing and os.path.exists(hub_marker):
                print(f"\n=== reuse hub N={n} BW_STEP={s} W={w} ({hub_dir})", file=sys.stderr)
            else:
                print(f"\n=== build hub N={n} BW_STEP={s} W={w} -> {hub_dir}", file=sys.stderr)
                env = os.environ.copy()
                env["N"] = str(n)
                env["BW_STEP"] = str(s)
                env["FEATURE_W"] = str(w)
                env["REGION_START"] = str(args.region_start)
                env["REGION_END"] = str(args.region_end)
                subprocess.run(["bash", BUILDER, hub_dir], env=env, check=True)

            point_cfg = rewrite_cases(base_cfg, point_subdir, case_ids)
            cfg_path = os.path.join(out_dir, f"cases_{point_subdir}.yaml")
            with open(cfg_path, "w") as f:
                yaml.safe_dump(point_cfg, f, sort_keys=False)

            point_out = os.path.join(out_dir, point_subdir)
            print(f"=== bench N={n} BW_STEP={s}", file=sys.stderr)
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
                    fields = ["N", "bw_step"] + reader.fieldnames
                    merged_writer = csv.DictWriter(
                        merged_fp, fieldnames=fields, delimiter="\t"
                    )
                    merged_writer.writeheader()
                for row in reader:
                    row["N"] = n
                    row["bw_step"] = s
                    merged_writer.writerow(row)
            merged_fp.flush()

    if args.clean_builds:
        for d in built_dirs:
            shutil.rmtree(d, ignore_errors=True)

    write_summary(sweep_path, summary_path)
    print(f"\nmerged: {sweep_path}", file=sys.stderr)
    print(f"summary: {summary_path}", file=sys.stderr)


if __name__ == "__main__":
    main()
