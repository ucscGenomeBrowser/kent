#!/usr/bin/env python3
"""Analyze a trackCheckRobot.py log: group failures and (optionally) fetch the
actual HGERROR text for each unique (assembly, track) pair.

The robot log records every bad hgc URL prefixed with "Error: <asm>.<track>:".
URLs in the log include an hgsid that's almost certainly stale by the time you
re-run analysis, so this script re-primes a fresh hgTracks session for each
failure (hideAll, then enable <track>=full, sharing a cookie jar) before
fetching the hgc URL again.

Usage:
  trackCheckAnalyze.py LOGFILE              # group, print counts + one sample URL per pattern
  trackCheckAnalyze.py LOGFILE --fetch      # also re-fetch each URL and print the real error message
  trackCheckAnalyze.py LOGFILE --fetch \\
       --only hg38.netHprc                  # regex filter on assembly.track keys

The default run is fast (no HTTP). --fetch is the slow path; it hits hgwbeta
~3x per unique failure.
"""
import argparse
import html
import http.cookiejar
import re
import sys
import urllib.request
from collections import OrderedDict

HTTP_TIMEOUT = 30


def extract_error_text(body):
    """Return a list of user-visible error strings found in an hgc page body.

    hgc emits errors two ways:
      - newer JS: `warnList.innerHTML += '<li>...</li>';` (escaped \\xNN bytes + HTML entities)
      - older static: `<!-- HGERROR-START --> free text <!-- HGERROR-END -->`
    """
    msgs = []
    for m in re.finditer(r"warnList\.innerHTML\s*\+?=\s*'((?:\\'|[^'])*)'", body):
        s = m.group(1)
        s = re.sub(r"\\x([0-9a-fA-F]{2})", lambda mm: chr(int(mm.group(1), 16)), s)
        s = html.unescape(s)
        msgs.append(re.sub(r"<[^>]+>", "", s).strip())
    for m in re.finditer(r"<!-- HGERROR-START -->(.*?)<!-- HGERROR-END -->", body, re.DOTALL):
        msgs.append(re.sub(r"<[^>]+>", "", m.group(1)).strip())
    return [m for m in msgs if m]


def fetch_with_priming(assembly, track, hgc_url):
    """Prime an hgTracks session (hideAll then enable track), then fetch hgc."""
    jar = http.cookiejar.CookieJar()
    opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))
    opener.addheaders = [("Accept-Encoding", "identity")]

    m = re.search(r"[?&]c=([^&]+)", hgc_url)
    l = re.search(r"[?&]l=(\d+)", hgc_url)
    r = re.search(r"[?&]r=(\d+)", hgc_url)
    server_m = re.match(r"(https?)://([^/]+)", hgc_url)
    if not (m and l and r and server_m):
        return "(cannot parse hgc URL)"
    chrom, start, end = m.group(1), l.group(1), r.group(1)
    proto, server = server_m.group(1), server_m.group(2)
    pos = f"{chrom}:{start}-{end}"
    base = f"{proto}://{server}/cgi-bin/hgTracks"

    try:
        opener.open(f"{base}?db={assembly}&position={pos}&hgt.hideAll=yes&pix=1200",
                    timeout=HTTP_TIMEOUT).read()
        opener.open(f"{base}?db={assembly}&position={pos}&{track}=full&pix=1200",
                    timeout=HTTP_TIMEOUT).read()
        body = opener.open(hgc_url, timeout=HTTP_TIMEOUT).read().decode("utf-8", errors="replace")
    except Exception as e:
        return f"(fetch failed: {e})"

    msgs = extract_error_text(body)
    return msgs[0] if msgs else "(no HGERROR / warnList found on reshow — may have been fixed)"


PATTERN_COLLAPSES = [
    (re.compile(r"^netHprcGCA_[0-9]+v[0-9]+$"), "netHprcGCA_*"),
    (re.compile(r"^netDro[A-Za-z]+[0-9]+$"),     "netDro*"),
    (re.compile(r"^chainHprcGCA_[0-9]+v[0-9]+$"),"chainHprcGCA_*"),
]


def collapse(track):
    for rx, label in PATTERN_COLLAPSES:
        if rx.match(track):
            return label
    return track


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log", help="Path to a v*.TrackCheck.py.log file")
    ap.add_argument("--fetch", action="store_true",
                    help="Re-fetch each unique failure to show the actual error message")
    ap.add_argument("--only", default=None,
                    help="Regex filter on assembly.track keys (e.g. 'hg38.netHprc')")
    args = ap.parse_args()

    uniq = OrderedDict()
    with open(args.log) as f:
        for line in f:
            m = re.match(r"^Error: ([^.]+)\.([^:]+):\s*(.*)", line)
            if not m:
                continue
            asm, track, rest = m.group(1), m.group(2), m.group(3)
            key = (asm, track)
            if key in uniq:
                continue
            url_m = re.search(r"https://\S+", rest)
            url = url_m.group(0).rstrip() if url_m else None
            if url:
                url = url.replace("&amp;", "&")
                url = re.sub(r"hgsid=[^&]*&", "", url)
            kind = rest.split(" at")[0].strip() if " at" in rest else rest.strip()
            uniq[key] = (kind, url)

    only_re = re.compile(args.only) if args.only else None

    pattern_counts = {}
    for (asm, track), _ in uniq.items():
        key = f"{asm}.{collapse(track)}"
        pattern_counts[key] = pattern_counts.get(key, 0) + 1

    print(f"Unique (assembly, track) failures: {len(uniq)}")
    print()
    print("=== Collapsed pattern counts ===")
    for key, n in sorted(pattern_counts.items(), key=lambda kv: -kv[1]):
        print(f"  {n:5d}  {key}")
    print()

    print("=== Individual failures ===")
    for (asm, track), (kind, url) in uniq.items():
        tag = f"{asm}.{track}"
        if only_re and not only_re.search(tag):
            continue
        if not url or "fetch failed" in kind or "timed out" in kind:
            print(f"  {tag}: {kind}")
            continue
        if not args.fetch:
            print(f"  {tag}: (use --fetch to show error text)  {url}")
            continue
        err = fetch_with_priming(asm, track, url)
        print(f"  {tag}: {err}")


if __name__ == "__main__":
    main()
