#!/usr/bin/env python3
"""
quickLiftBench.py - Benchmark hgTracks render time for two saved sessions on
the same server.

Each case names two (or more) variants, each variant is a saved-session
reference of the form "user/sessionName". The runner loads the session, applies
a position from the case, and times the hgTracks render with measureTiming=1.

Output is one TSV row per (case, variant, position, iteration) plus a
per-(case, position) summary with median/p90 and pairwise ratios. The headline
metric is `total_ms`, taken from the "Overall total time" timing span emitted
by hgTracks. `load_ms_sum` and `draw_ms_sum` are the sums across all visible
tracks from the printTrackTiming() table.
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


# A row of the printTrackTiming() table:  shortLabel, load, draw, total
TRACK_TIMING_ROW_RE = re.compile(
    r"^\s*([^,<>]+?)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*$"
)

# <span class='trackTiming'> ... </span>. The opening span is never explicitly
# closed by printTrackTiming(); the regex's non-greedy match terminates at the
# first subsequent </span> from an unrelated span (e.g. the trailing
# 'timing' span for "Time to write and close cart").
TRACK_TIMING_SPAN_RE = re.compile(
    r"<span class=['\"]trackTiming['\"]>(.*?)</span>",
    re.DOTALL | re.IGNORECASE,
)

# <span class='timing'>label: NNN millis<BR></span> -- per-phase timings.
# Also matches the "Overall total time" footer span which uses <br /> (XHTML).
TIMING_SPAN_RE = re.compile(
    r"<span class=['\"]timing['\"]>(.+?):\s*(\d+)\s*millis(?:<br\s*/?>)?\s*</span>",
    re.DOTALL | re.IGNORECASE,
)

OVERALL_TIMING_LABEL = "Overall total time"


def parse_args():
    p = argparse.ArgumentParser(
        description=(
            "Benchmark hgTracks render time for saved sessions. Each variant "
            "is a user/sessionName reference."
        ),
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
        help="Override every case's server with this server name from defaults.servers",
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
    p.add_argument(
        "--phases",
        action="store_true",
        help="Also write phases.tsv with per-iteration phase timings parsed "
             "from <span class='timing'> markers (chromAliasSetup, "
             "trackDbLoad, parallel data fetch, etc.)",
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


def parse_session(s):
    """Split 'user/sessionName' (or '/s/user/name'). Returns (user, name)."""
    if not isinstance(s, str):
        raise ValueError(f"variant must be a 'user/sessionName' string, got: {s!r}")
    s = s.strip()
    if s.startswith("/s/"):
        s = s[3:]
    if "/" not in s:
        raise ValueError(f"variant must contain '/': {s!r}")
    user, name = s.split("/", 1)
    if not user or not name:
        raise ValueError(f"empty user or session name in: {s!r}")
    return user, name


def resolve_server(case, defaults, server_override):
    server_key = server_override or case.get("server")
    if not server_key:
        raise ValueError(f"case '{case.get('id')}' has no server")
    servers = defaults.get("servers", {})
    if server_key not in servers:
        raise ValueError(
            f"server '{server_key}' not in defaults.servers ({list(servers)})"
        )
    return server_key, servers[server_key].rstrip("/")


def build_url(server_url, user, session_name):
    """Build an hgTracks URL that loads a saved session at its saved position.

    The position is NOT overridden via URL: a native session and its
    quickLifted counterpart sit on different assemblies, so identical
    chr:start-end ranges would not be biologically equivalent. Whatever
    region the session was saved at is what gets rendered.
    """
    params = [
        ("hgS_doOtherUser", "submit"),
        ("hgS_otherUserName", user),
        ("hgS_otherUserSessionName", session_name),
        ("hgt.trackImgOnly", "1"),
        ("measureTiming", "1"),
    ]
    return f"{server_url}/cgi-bin/hgTracks?{urlencode(params)}"


def parse_track_timing_rows(html):
    """Return a list of (shortLabel, load_ms, draw_ms, total_ms) rows."""
    if not html:
        return []
    rows = []
    for span in TRACK_TIMING_SPAN_RE.findall(html):
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
    return rows


def parse_phase_timings(html):
    """Pull all <span class='timing'>label: NNN millis</span> entries."""
    if not html:
        return {}
    out = {}
    for m in TIMING_SPAN_RE.finditer(html):
        label = m.group(1).strip()
        # Strip HTML tags that sneak into labels (e.g. format strings)
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
    phases_path = os.path.join(out_dir, "phases.tsv") if args.phases else None
    print(f"writing results to {out_dir}", file=sys.stderr)

    results_fields = [
        "case", "variant", "server", "user", "session", "iteration",
        "http_ms", "load_ms_sum", "draw_ms_sum", "n_tracks", "total_ms",
        "status_code", "error",
    ]
    phases_fields = [
        "case", "variant", "server", "user", "session", "iteration",
        "phase", "ms",
    ]

    all_rows = []
    all_phase_rows = []

    with open(results_path, "w", newline="") as rf:
        writer = csv.DictWriter(rf, fieldnames=results_fields, delimiter="\t")
        writer.writeheader()

        for case in cases:
            cid = case["id"]
            description = case.get("description", "")
            print(f"\n=== {cid}: {description}", file=sys.stderr)

            try:
                server_key, server_url = resolve_server(case, defaults, args.server_override)
            except ValueError as e:
                print(f"  {e}", file=sys.stderr)
                continue

            variants = case.get("variants") or {}
            if not variants:
                print(f"  skipped: no variants", file=sys.stderr)
                continue

            # Fresh session per case to keep an independent hgsid lineage.
            session = requests.Session()
            session.headers.update({"User-Agent": "quickLiftBench/1.0"})

            for vname, vraw in variants.items():
                try:
                    user, session_name = parse_session(vraw)
                except ValueError as e:
                    print(f"  variant {vname}: {e}", file=sys.stderr)
                    continue

                url = build_url(server_url, user, session_name)
                if args.verbose:
                    print(f"  URL: {url}", file=sys.stderr)

                for _ in range(warmup):
                    run_request(session, url, timeout)

                for it in range(1, iterations + 1):
                    http_ms, code, html, err = run_request(session, url, timeout)
                    block = detect_block(html) if not err else None
                    if block and not err:
                        err = block

                    load_sum = draw_sum = total_ms = None
                    n_tracks = None
                    phases = {}
                    if html and not err:
                        rows = parse_track_timing_rows(html)
                        n_tracks = len(rows)
                        if rows:
                            load_sum = sum(r[1] for r in rows)
                            draw_sum = sum(r[2] for r in rows)
                        phases = parse_phase_timings(html)
                        total_ms = phases.get(OVERALL_TIMING_LABEL)
                        if total_ms is None and not rows:
                            err = "no-timing"

                    row = {
                        "case": cid,
                        "variant": vname,
                        "server": server_key,
                        "user": user,
                        "session": session_name,
                        "iteration": it,
                        "http_ms": http_ms,
                        "load_ms_sum": load_sum if load_sum is not None else "",
                        "draw_ms_sum": draw_sum if draw_sum is not None else "",
                        "n_tracks": n_tracks if n_tracks is not None else "",
                        "total_ms": total_ms if total_ms is not None else "",
                        "status_code": code if code is not None else "",
                        "error": err or "",
                    }
                    writer.writerow(row)
                    rf.flush()
                    all_rows.append(row)

                    if args.phases:
                        for label, ms in phases.items():
                            all_phase_rows.append({
                                "case": cid,
                                "variant": vname,
                                "server": server_key,
                                "user": user,
                                "session": session_name,
                                "iteration": it,
                                "phase": label,
                                "ms": ms,
                            })
                    if args.verbose:
                        print(
                            f"    {vname:8s} it={it} "
                            f"http={fmt_ms(http_ms)} "
                            f"load_sum={fmt_ms(load_sum)} draw_sum={fmt_ms(draw_sum)} "
                            f"tracks={fmt_ms(n_tracks)} "
                            f"total={fmt_ms(total_ms)} {err or ''}",
                            file=sys.stderr,
                        )

    write_summary(cases, all_rows, summary_path)
    print(f"\nresults: {results_path}", file=sys.stderr)
    print(f"summary: {summary_path}", file=sys.stderr)

    if phases_path:
        write_phases(all_phase_rows, phases_fields, phases_path)
        print(f"phases:  {phases_path}", file=sys.stderr)


def _to_int(v):
    if v == "" or v is None:
        return None
    return int(v)


def write_summary(cases, rows, path):
    """
    Per (case, variant): compute median + p90 of (http_ms, load_ms_sum,
    draw_ms_sum, total_ms). Then for each compare-pair, emit median ratios.
    """
    by_case_var = {}
    for r in rows:
        by_case_var.setdefault(r["case"], {}).setdefault(r["variant"], []).append(r)

    fields = [
        "case", "variant",
        "n", "n_ok",
        "http_median", "http_p90",
        "load_sum_median", "load_sum_p90",
        "draw_sum_median", "draw_sum_p90",
        "total_median", "total_p90",
    ]
    pair_fields = [
        "case",
        "left_variant", "right_variant",
        "left_total_median", "right_total_median",
        "ratio_total", "ratio_load_sum", "ratio_draw_sum", "ratio_http",
    ]

    summary_lines = []
    with open(path, "w", newline="") as f:
        w_v = csv.DictWriter(f, fieldnames=fields, delimiter="\t")
        w_v.writeheader()

        per_variant_stats = {}

        for cid, variants in sorted(by_case_var.items()):
            for vname, vrows in sorted(variants.items()):
                ok = [r for r in vrows if not r["error"]]
                http = [_to_int(r["http_ms"]) for r in ok]
                load = [_to_int(r["load_ms_sum"]) for r in ok]
                draw = [_to_int(r["draw_ms_sum"]) for r in ok]
                total = [_to_int(r["total_ms"]) for r in ok]
                stats = {
                    "case": cid,
                    "variant": vname,
                    "n": len(vrows),
                    "n_ok": len(ok),
                    "http_median": median_or_none(http),
                    "http_p90": p90(http),
                    "load_sum_median": median_or_none(load),
                    "load_sum_p90": p90(load),
                    "draw_sum_median": median_or_none(draw),
                    "draw_sum_p90": p90(draw),
                    "total_median": median_or_none(total),
                    "total_p90": p90(total),
                }
                w_v.writerow({k: ("" if v is None else v) for k, v in stats.items()})
                per_variant_stats[(cid, vname)] = stats

        f.write("\n# Pairwise comparisons (right/left ratio of medians)\n")
        w_p = csv.DictWriter(f, fieldnames=pair_fields, delimiter="\t")
        w_p.writeheader()

        for case in cases:
            cid = case["id"]
            pairs = case.get("compare")
            variants = list((case.get("variants") or {}).keys())
            if not pairs:
                if len(variants) < 2:
                    continue
                pairs = [[variants[0], v] for v in variants[1:]]
            for left, right in pairs:
                ls = per_variant_stats.get((cid, left))
                rs = per_variant_stats.get((cid, right))
                if not ls or not rs:
                    continue
                row = {
                    "case": cid,
                    "left_variant": left,
                    "right_variant": right,
                    "left_total_median": ls["total_median"] if ls["total_median"] is not None else "",
                    "right_total_median": rs["total_median"] if rs["total_median"] is not None else "",
                    "ratio_total": ratio(rs["total_median"], ls["total_median"]),
                    "ratio_load_sum": ratio(rs["load_sum_median"], ls["load_sum_median"]),
                    "ratio_draw_sum": ratio(rs["draw_sum_median"], ls["draw_sum_median"]),
                    "ratio_http": ratio(rs["http_median"], ls["http_median"]),
                }
                w_p.writerow(row)
                summary_lines.append(
                    f"  {cid:30s}  {left}->{right}  "
                    f"total {ls['total_median']}->{rs['total_median']} ms  "
                    f"ratio={row['ratio_total']}"
                )

    if summary_lines:
        print("\nPairwise summary (right/left ratio of overall total render time):",
              file=sys.stderr)
        for line in summary_lines:
            print(line, file=sys.stderr)


def ratio(num, denom):
    if num is None or denom is None or denom == 0:
        return ""
    return f"{num / denom:.2f}"


def write_phases(rows, fields, path):
    """Write per-iteration phase timings, then a per-(case, variant, phase)
    median + n summary appended below."""
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, delimiter="\t")
        w.writeheader()
        for r in rows:
            w.writerow(r)

        if not rows:
            return

        # Per-(case, variant, phase) median for quick eyeballing.
        groups = {}
        for r in rows:
            key = (r["case"], r["variant"], r["phase"])
            groups.setdefault(key, []).append(r["ms"])

        f.write("\n# Per-(case, variant, phase) median ms across iterations\n")
        summary_fields = ["case", "variant", "phase", "n", "median_ms", "p90_ms"]
        sw = csv.DictWriter(f, fieldnames=summary_fields, delimiter="\t")
        sw.writeheader()
        for (cid, vname, phase), values in sorted(groups.items()):
            sw.writerow({
                "case": cid,
                "variant": vname,
                "phase": phase,
                "n": len(values),
                "median_ms": median_or_none(values),
                "p90_ms": p90(values),
            })


if __name__ == "__main__":
    main()
