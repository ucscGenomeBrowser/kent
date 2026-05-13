#!/usr/bin/env python3
"""TrackCheck robot - exercise every track on every active assembly.

For each active assembly in hgcentralbeta.dbDb:
  - Look up default position
  - For each track in that assembly's trackDb:
      * Hide all tracks (GET hgTracks with hgt.hideAll=yes) using a session cookie jar
      * Enable just this track (GET hgTracks with <track>=full)
      * Scrape hgc URLs from the rendered track image map (<AREA HREF=...>)
      * GET up to MAX_LINKS_PER_TRACK of them, check 200 and "HGERROR" in body

Replaces the Java TrackCheck.java which did the same via HttpUnit.

Usage:
  trackCheckRobot.py <propsfile-or-default>

Props file (key space value or key=value per line) keys used:
  httpProto   https (default) or http
  server      web server to test (e.g. hgwbeta.soe.ucsc.edu)
  dbSpec      "all" or a specific assembly
  table       "all" or a specific track

DB credentials come from $HGDB_CONF or ~/.hg.conf.beta (passed to hgsql).
"""
import argparse
import http.cookiejar
import os
import re
import subprocess
import sys
import time
from urllib.parse import urlencode, urljoin
import urllib.request
import urllib.error

HTTP_TIMEOUT = 30
HTTP_PIX = "1200"
MAX_LINKS_PER_TRACK = 4


class Counters:
    checked = 0
    skipped = 0
    errors = 0


def log(msg):
    print(msg, flush=True)


def err(msg):
    Counters.errors += 1
    print(f"Error: {msg}", flush=True)


def read_props(path):
    props = {
        "httpProto": "https",
        "server": "hgwbeta.soe.ucsc.edu",
        "dbSpec": "all",
        "table": "all",
    }
    if path == "default":
        return props
    with open(path) as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if "=" in line:
                k, _, v = line.partition("=")
            else:
                parts = line.split(None, 1)
                if len(parts) != 2:
                    continue
                k, v = parts
            props[k.strip()] = v.strip()
    return props


def hgsql(hgdb_conf, db, query):
    env = os.environ.copy()
    env["HGDB_CONF"] = hgdb_conf
    r = subprocess.run(
        ["hgsql", "-N", "-B", db, "-e", query],
        capture_output=True, text=True, env=env,
    )
    if r.returncode != 0:
        raise RuntimeError(f"hgsql on {db} failed: {r.stderr.strip()}")
    return [line for line in r.stdout.splitlines() if line]


def active_assemblies(hgdb_conf):
    return hgsql(hgdb_conf, "hgcentralbeta",
                 "SELECT name FROM dbDb WHERE active = 1")


def default_position(hgdb_conf, assembly):
    rows = hgsql(hgdb_conf, "hgcentralbeta",
                 f"SELECT defaultPos FROM dbDb WHERE name = '{assembly}'")
    return rows[0] if rows else None


def trackdb_tracks(hgdb_conf, assembly):
    return hgsql(hgdb_conf, assembly, "SELECT tableName FROM trackDb")


def make_opener():
    """Per-track opener with its own cookie jar so carts don't cross-contaminate."""
    jar = http.cookiejar.CookieJar()
    opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))
    opener.addheaders = [("User-Agent", "TrackCheckRobot/1.0"),
                         ("Accept-Encoding", "identity")]
    return opener


def http_get(opener, url):
    with opener.open(url, timeout=HTTP_TIMEOUT) as resp:
        return resp.status, resp.read().decode("utf-8", errors="replace")


AREA_HGC_RE = re.compile(
    r"<AREA\b[^>]*\bHREF=['\"]([^'\"]*\bhgc\b[^'\"]*)['\"]",
    re.IGNORECASE,
)


def extract_hgc_links(body, base_url):
    """Return unique hgc URLs found in track image AREA tags."""
    seen = set()
    out = []
    for m in AREA_HGC_RE.finditer(body):
        href = m.group(1).replace("&amp;", "&")
        if href in seen:
            continue
        seen.add(href)
        out.append(urljoin(base_url, href))
    return out


def check_hgc_urls(opener, urls, log_prefix):
    """GET each URL, report HTTP != 200 or 'HGERROR' in response body."""
    for url in urls:
        try:
            status, body = http_get(opener, url)
        except urllib.error.HTTPError as e:
            err(f"{log_prefix}: HTTP {e.code} for {url}")
            continue
        except Exception as e:
            err(f"{log_prefix}: fetch failed ({e}) for {url}")
            continue
        if status != 200:
            err(f"{log_prefix}: unexpected response code {status} for {url}")
            continue
        idx = body.find("HGERROR")
        if idx >= 0:
            err(f"{log_prefix}: HGERROR at {url}")


def exercise_track(http_proto, server, assembly, track, default_pos):
    """Two-step GET: hide all, then enable track. Scrape hgc links, GET them."""
    opener = make_opener()
    base = f"{http_proto}://{server}/cgi-bin/hgTracks"
    common = {"db": assembly, "position": default_pos, "pix": HTTP_PIX}

    # Step 1: hide everything in the session cart.
    url1 = f"{base}?{urlencode({**common, 'hgt.hideAll': 'yes'})}"
    try:
        status, _ = http_get(opener, url1)
    except Exception as e:
        err(f"{assembly}.{track}: hideAll request failed: {e}")
        return
    if status != 200:
        err(f"{assembly}.{track}: hideAll got HTTP {status}")
        return

    # Step 2: enable just this track.
    url2 = f"{base}?{urlencode({**common, track: 'full'})}"
    try:
        status, body = http_get(opener, url2)
    except Exception as e:
        err(f"{assembly}.{track}: enable request failed: {e}")
        return
    if status != 200:
        err(f"{assembly}.{track}: enable got HTTP {status}")
        return

    links = extract_hgc_links(body, url2)
    if not links:
        # No clickable items at default position — not an error, just no coverage.
        Counters.skipped += 1
        return

    check_hgc_urls(opener, links[:MAX_LINKS_PER_TRACK],
                   f"{assembly}.{track}")
    Counters.checked += 1


def main():
    ap = argparse.ArgumentParser(description="TrackCheck robot (python port)")
    ap.add_argument("propfile", help="Properties file path or 'default'")
    ap.add_argument(
        "--hgdb-conf",
        default=os.environ.get("HGDB_CONF") or os.path.expanduser("~/.hg.conf.beta"),
        help="hg.conf for hgsql (default: $HGDB_CONF or ~/.hg.conf.beta)",
    )
    args = ap.parse_args()

    props = read_props(args.propfile)
    log("Target is:")
    for k in ("httpProto", "server", "dbSpec", "table"):
        log(f"  {k} {props.get(k)}")

    if props["dbSpec"] == "all":
        try:
            assemblies = active_assemblies(args.hgdb_conf)
        except Exception as e:
            err(f"could not list active assemblies: {e}")
            return 1
    else:
        assemblies = [props["dbSpec"]]

    start_time = time.time()
    for asm in assemblies:
        log(f"\nAssembly = {asm}")
        pos = default_position(args.hgdb_conf, asm)
        if not pos:
            err(f"{asm}: no default position in dbDb")
            continue

        if props["table"] == "all":
            try:
                tracks = trackdb_tracks(args.hgdb_conf, asm)
            except Exception as e:
                err(f"{asm}: could not list tracks: {e}")
                continue
        else:
            tracks = [props["table"]]

        before_checked = Counters.checked
        before_skipped = Counters.skipped
        for t in tracks:
            try:
                exercise_track(props["httpProto"], props["server"], asm, t, pos)
            except Exception as e:
                err(f"{asm}.{t}: unexpected exception: {e}")
        log(f"checked {Counters.checked - before_checked} of {len(tracks)} "
            f"tracks ({Counters.skipped - before_skipped} had no clickable items)")

    elapsed = time.time() - start_time
    log(f"\nTotal: {Counters.checked} tracks checked, "
        f"{Counters.skipped} no-items, {Counters.errors} errors in {elapsed:.1f}s")
    return 0 if Counters.errors == 0 else 2


if __name__ == "__main__":
    sys.exit(main())
