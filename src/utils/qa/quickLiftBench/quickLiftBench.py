#!/usr/bin/env python3
"""
quickLiftBench.py - Benchmark hgTracks render times for quickLifted tracks vs
their non-lifted counterparts.

Drives a YAML-configured set of benchmark cases, hits hgTracks with
?measureTiming=1, parses per-track loadTime/drawTime out of the response, and
writes TSV results suitable for inclusion in a quickLift performance paper.

For each case, multiple variants (e.g. native vs lifted) are timed at multiple
positions across multiple iterations. Output is one TSV row per (case, variant,
position, iteration) plus a per-(case, position) summary with median/p90 and
lifted/native ratios.
"""

import argparse
import csv
import os
import re
import statistics
import sys
import time
from datetime import datetime
from urllib.parse import urlencode

import requests
import yaml


# Regex over the extracted trackTiming span text. The HTML inside looks like:
#   track, load time, draw time, total (first window)<br />
#   shortLabel, 12, 8, 20<br />
TRACK_TIMING_ROW_RE = re.compile(
    r"^\s*([^,<>]+?)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*$"
)

# <span class='trackTiming'> ... </span> -- there can be more than one (composite
# subtracks emit their own). We grab the first block, which contains the per-track
# table.
TRACK_TIMING_SPAN_RE = re.compile(
    r"<span class=['\"]trackTiming['\"]>(.*?)</span>",
    re.DOTALL | re.IGNORECASE,
)

# <span class='timing'>label: NNN millis<BR></span>
TIMING_SPAN_RE = re.compile(
    r"<span class=['\"]timing['\"]>(.+?):\s*(\d+)\s*millis<BR></span>",
    re.DOTALL | re.IGNORECASE,
)


def parse_args():
    p = argparse.ArgumentParser(
        description="Benchmark quickLift vs native track render times.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    here = os.path.dirname(os.path.abspath(__file__))
    p.add_argument(
        "--config",
        default=os.path.join(here, "cases.yaml"),
        help="YAML config file (default: cases.yaml next to script)",
    )
    p.add_argument(
        "--cases",
        default=None,
        help="Comma-separated list of case ids to run (default: all)",
    )
    p.add_argument(
        "--server-override",
        default=None,
        help="Override every variant's `server` with this server name from defaults.servers",
    )
    p.add_argument(
        "--iterations",
        type=int,
        default=None,
        help="Override defaults.iterations",
    )
    p.add_argument(
        "--warmup",
        type=int,
        default=None,
        help="Override defaults.warmup",
    )
    p.add_argument(
        "--out",
        default=None,
        help="Output directory (default: ./results/<timestamp>/)",
    )
    p.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Print per-iteration timings to stderr",
    )
    return p.parse_args()


def load_config(path):
    with open(path) as f:
        cfg = yaml.safe_load(f)
    if "cases" not in cfg or not cfg["cases"]:
        sys.exit(f"{path}: no `cases` defined")
    cfg.setdefault("defaults", {})
    cfg["defaults"].setdefault("iterations", 5)
    cfg["defaults"].setdefault("warmup", 1)
    cfg["defaults"].setdefault("timeout", 60)
    cfg["defaults"].setdefault("servers", {})
    return cfg


def resolve_variant(variant, defaults, server_override):
    """Merge defaults into a variant and resolve its server hostname."""
    merged = {}
    for k in ("db", "hubUrl", "track", "extraCgi"):
        if k in defaults:
            merged[k] = defaults[k]
    merged.update(variant)
    server_key = server_override or merged.get("server")
    if not server_key:
        raise ValueError(f"variant has no server: {variant}")
    servers = defaults.get("servers", {})
    if server_key not in servers:
        raise ValueError(
            f"server '{server_key}' not in defaults.servers ({list(servers)})"
        )
    merged["serverKey"] = server_key
    merged["serverUrl"] = servers[server_key].rstrip("/")
    return merged


def build_url(variant, position):
    """Build an hgTracks URL that isolates one named track at a given position."""
    if "track" not in variant or "db" not in variant:
        raise ValueError(f"variant missing track/db: {variant}")
    params = [
        ("db", variant["db"]),
        ("position", position),
        ("hideTracks", "1"),
        (variant["track"], "full"),
        ("hgt.trackImgOnly", "1"),
        ("hgt.reset", "1"),
        ("measureTiming", "1"),
    ]
    if variant.get("hubUrl"):
        params.append(("hubUrl", variant["hubUrl"]))
    extra = variant.get("extraCgi") or {}
    for k, v in extra.items():
        params.append((k, str(v)))
    return f"{variant['serverUrl']}/cgi-bin/hgTracks?{urlencode(params)}"


def parse_track_timing(html, want_track):
    """
    Pull per-track load/draw/total ms out of the trackTiming span.

    The HTML emits the track's shortLabel, not the trackDb track name we use
    in the URL. So we try (a) exact/case-insensitive match against the label
    first; (b) if that misses AND there's exactly one data row across all
    spans (the expected case when hideTracks=1 isolates a single track), we
    return that row. Otherwise we return None so the caller can log
    `no-track-timing` and the user can fix the case.

    Returns (load_ms, draw_ms, total_ms) or (None, None, None).
    """
    if not html:
        return None, None, None
    spans = TRACK_TIMING_SPAN_RE.findall(html)
    if not spans:
        return None, None, None

    rows = []  # (label, load, draw, total)
    for span in spans:
        text = span.replace("&nbsp;", " ")
        for raw in text.split("<br />"):
            raw = raw.strip()
            # Drop the inner red <idiv> "average for all windows" rows.
            if "<idiv" in raw or "</idiv" in raw:
                continue
            if not raw or raw.startswith("track,"):
                continue
            m = TRACK_TIMING_ROW_RE.match(raw)
            if not m:
                continue
            label, load, draw, total = m.groups()
            rows.append((label.strip(), int(load), int(draw), int(total)))

    if not rows:
        return None, None, None

    for label, load, draw, total in rows:
        if label == want_track or label.lower() == want_track.lower():
            return load, draw, total

    if len(rows) == 1:
        # Single track rendered (the hideTracks=1 case); the shortLabel didn't
        # match the trackDb name but there's no ambiguity.
        _, load, draw, total = rows[0]
        return load, draw, total

    return None, None, None


def parse_phase_timings(html):
    """Pull all <span class='timing'>label: NNN millis</span> entries."""
    if not html:
        return {}
    out = {}
    for m in TIMING_SPAN_RE.finditer(html):
        label = m.group(1).strip()
        # Strip HTML tags that sometimes sneak into labels (e.g. format strings)
        label = re.sub(r"<[^>]+>", "", label).strip()
        out[label] = int(m.group(2))
    return out


def detect_block(html):
    """Return a short error reason if the response indicates bot-block / error."""
    if html is None:
        return "no-response"
    lower = html.lower()
    if "you've been blocked" in lower or "you have been blocked" in lower:
        return "bot-blocked"
    if "<title>error" in lower or "errabort" in lower:
        return "errabort"
    return None


def run_request(session, url, timeout):
    """Fetch a URL once. Return (http_ms, status_code, html, error)."""
    start = time.monotonic()
    try:
        resp = session.get(url, timeout=timeout, verify=True)
    except requests.exceptions.RequestException as e:
        elapsed_ms = int((time.monotonic() - start) * 1000)
        return elapsed_ms, None, None, f"transport: {e.__class__.__name__}"
    elapsed_ms = int((time.monotonic() - start) * 1000)
    err = None
    if resp.status_code != 200:
        err = f"http-{resp.status_code}"
    return elapsed_ms, resp.status_code, resp.text, err


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
    # Nearest-rank p90
    k = max(0, int(round(0.9 * (len(xs) - 1))))
    return xs[k]


def fmt_ms(v):
    return "-" if v is None else str(v)


def main():
    args = parse_args()
    cfg = load_config(args.config)
    defaults = cfg["defaults"]

    iterations = args.iterations if args.iterations is not None else defaults["iterations"]
    warmup = args.warmup if args.warmup is not None else defaults["warmup"]
    timeout = defaults["timeout"]

    requested_ids = (
        set(s.strip() for s in args.cases.split(",")) if args.cases else None
    )

    cases = cfg["cases"]
    if requested_ids:
        cases = [c for c in cases if c["id"] in requested_ids]
        missing = requested_ids - {c["id"] for c in cases}
        if missing:
            sys.exit(f"unknown case id(s): {sorted(missing)}")

    if not cases:
        sys.exit("no cases selected")

    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = args.out or os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                        "results", timestamp)
    os.makedirs(out_dir, exist_ok=True)
    results_path = os.path.join(out_dir, "results.tsv")
    summary_path = os.path.join(out_dir, "summary.tsv")
    print(f"writing results to {out_dir}", file=sys.stderr)

    results_fields = [
        "case", "variant", "server", "db", "track",
        "position_label", "position", "iteration",
        "http_ms", "load_ms", "draw_ms", "total_ms",
        "status_code", "error",
    ]
    summary_fields = [
        "case", "position_label", "position", "metric",
    ]

    # Collect rows in memory for the summary pass.
    all_rows = []

    with open(results_path, "w", newline="") as rf:
        writer = csv.DictWriter(rf, fieldnames=results_fields, delimiter="\t")
        writer.writeheader()

        for case in cases:
            cid = case["id"]
            description = case.get("description", "")
            print(f"\n=== {cid}: {description}", file=sys.stderr)
            positions = case.get("positions") or []
            if not positions:
                print(f"  skipped: no positions", file=sys.stderr)
                continue
            variants = case.get("variants") or {}
            if not variants:
                print(f"  skipped: no variants", file=sys.stderr)
                continue

            # Fresh session per case to mint a new hgsid and avoid cart pollution
            # across cases.
            session = requests.Session()
            session.headers.update({"User-Agent": "quickLiftBench/1.0"})

            for vname, vraw in variants.items():
                try:
                    variant = resolve_variant(vraw, defaults, args.server_override)
                except ValueError as e:
                    print(f"  variant {vname}: {e}", file=sys.stderr)
                    continue

                for pos in positions:
                    plabel = pos["label"] if isinstance(pos, dict) else "default"
                    pvalue = pos["value"] if isinstance(pos, dict) else pos
                    url = build_url(variant, pvalue)
                    if args.verbose:
                        print(f"  URL: {url}", file=sys.stderr)

                    # Warmup - discard
                    for _ in range(warmup):
                        run_request(session, url, timeout)

                    for it in range(1, iterations + 1):
                        http_ms, code, html, err = run_request(session, url, timeout)
                        block = detect_block(html) if not err else None
                        if block and not err:
                            err = block
                        load_ms = draw_ms = total_ms = None
                        if html and not err:
                            load_ms, draw_ms, total_ms = parse_track_timing(
                                html, variant["track"]
                            )
                            if load_ms is None:
                                err = "no-track-timing"
                        row = {
                            "case": cid,
                            "variant": vname,
                            "server": variant["serverKey"],
                            "db": variant["db"],
                            "track": variant["track"],
                            "position_label": plabel,
                            "position": pvalue,
                            "iteration": it,
                            "http_ms": http_ms,
                            "load_ms": load_ms,
                            "draw_ms": draw_ms,
                            "total_ms": total_ms,
                            "status_code": code if code is not None else "",
                            "error": err or "",
                        }
                        writer.writerow(row)
                        rf.flush()
                        all_rows.append(row)
                        if args.verbose:
                            print(
                                f"    {vname:8s} {plabel:8s} it={it} "
                                f"http={fmt_ms(http_ms)} "
                                f"load={fmt_ms(load_ms)} draw={fmt_ms(draw_ms)} "
                                f"total={fmt_ms(total_ms)} {err or ''}",
                                file=sys.stderr,
                            )

    write_summary(cases, all_rows, summary_path)
    print(f"\nresults: {results_path}", file=sys.stderr)
    print(f"summary: {summary_path}", file=sys.stderr)


def write_summary(cases, rows, path):
    """
    Per (case, position): for each variant compute median + p90 of
    (http_ms, load_ms, draw_ms, total_ms). Then for each compare-pair, emit
    median ratios.
    """
    by_case_pos_var = {}
    for r in rows:
        key = (r["case"], r["position_label"], r["position"])
        by_case_pos_var.setdefault(key, {}).setdefault(r["variant"], []).append(r)

    fields = [
        "case", "position_label", "position", "variant",
        "n", "n_ok",
        "http_median", "http_p90",
        "load_median", "load_p90",
        "draw_median", "draw_p90",
        "total_median", "total_p90",
    ]
    pair_fields = [
        "case", "position_label", "position",
        "left_variant", "right_variant",
        "left_total_median", "right_total_median",
        "ratio_total", "ratio_load", "ratio_draw", "ratio_http",
    ]

    summary_lines = []  # for stdout pretty-print
    with open(path, "w", newline="") as f:
        w_v = csv.DictWriter(f, fieldnames=fields, delimiter="\t")
        w_v.writeheader()

        per_variant_stats = {}  # (case, plabel, pvalue, variant) -> dict of medians

        for (cid, plabel, pvalue), variants in sorted(by_case_pos_var.items()):
            for vname, vrows in sorted(variants.items()):
                ok = [r for r in vrows if not r["error"]]
                http = [r["http_ms"] for r in ok]
                load = [r["load_ms"] for r in ok]
                draw = [r["draw_ms"] for r in ok]
                total = [r["total_ms"] for r in ok]
                stats = {
                    "case": cid,
                    "position_label": plabel,
                    "position": pvalue,
                    "variant": vname,
                    "n": len(vrows),
                    "n_ok": len(ok),
                    "http_median": median_or_none(http),
                    "http_p90": p90(http),
                    "load_median": median_or_none(load),
                    "load_p90": p90(load),
                    "draw_median": median_or_none(draw),
                    "draw_p90": p90(draw),
                    "total_median": median_or_none(total),
                    "total_p90": p90(total),
                }
                w_v.writerow({k: ("" if v is None else v) for k, v in stats.items()})
                per_variant_stats[(cid, plabel, pvalue, vname)] = stats

        f.write("\n# Pairwise comparisons (right/left ratio of medians)\n")
        w_p = csv.DictWriter(f, fieldnames=pair_fields, delimiter="\t")
        w_p.writeheader()

        for case in cases:
            cid = case["id"]
            pairs = case.get("compare")
            variants = list((case.get("variants") or {}).keys())
            if not pairs:
                # default: compare every other variant against the first
                if len(variants) < 2:
                    continue
                pairs = [[variants[0], v] for v in variants[1:]]
            positions = case.get("positions") or []
            for pos in positions:
                plabel = pos["label"] if isinstance(pos, dict) else "default"
                pvalue = pos["value"] if isinstance(pos, dict) else pos
                for left, right in pairs:
                    ls = per_variant_stats.get((cid, plabel, pvalue, left))
                    rs = per_variant_stats.get((cid, plabel, pvalue, right))
                    if not ls or not rs:
                        continue
                    row = {
                        "case": cid,
                        "position_label": plabel,
                        "position": pvalue,
                        "left_variant": left,
                        "right_variant": right,
                        "left_total_median": ls["total_median"] if ls["total_median"] is not None else "",
                        "right_total_median": rs["total_median"] if rs["total_median"] is not None else "",
                        "ratio_total": ratio(rs["total_median"], ls["total_median"]),
                        "ratio_load": ratio(rs["load_median"], ls["load_median"]),
                        "ratio_draw": ratio(rs["draw_median"], ls["draw_median"]),
                        "ratio_http": ratio(rs["http_median"], ls["http_median"]),
                    }
                    w_p.writerow(row)
                    summary_lines.append(
                        f"  {cid:30s} {plabel:8s}  {left}->{right}  "
                        f"total {ls['total_median']}->{rs['total_median']} ms  "
                        f"ratio={row['ratio_total']}"
                    )

    if summary_lines:
        print("\nPairwise summary (lifted/native ratio of total render time):",
              file=sys.stderr)
        for line in summary_lines:
            print(line, file=sys.stderr)


def ratio(num, denom):
    if num is None or denom is None or denom == 0:
        return ""
    return f"{num / denom:.2f}"


if __name__ == "__main__":
    main()
