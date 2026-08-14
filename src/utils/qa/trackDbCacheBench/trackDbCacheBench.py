#!/usr/bin/env python3
"""trackDbCacheBench - benchmark hgTracks with and without the trackDb cache.

Toggles the cache by setting CACHE_TRACK_DB_DIR in the child environment.
trackDbCacheOn() in src/hg/lib/trackDbCache.c honors that variable: if it is
set (even to the empty string), its value overrides the cacheTrackDbDir
setting from hg.conf. An empty value disables the cache.

Each mode does `--warmup` discarded runs followed by `--iterations` timed
runs. Wall time is measured around each hgTracks subprocess; if measureTiming=1
is set in the CGI args, the script also extracts the "Overall total time"
millisecond reading from the rendered HTML for a cleaner number.

Typical use:

    trackDbCacheBench.py /dev/shm/myCache
    trackDbCacheBench.py /dev/shm/myCache --cgi db=hg38 --cgi position=chr1:1-2000000

The default CGI args render hg38 chr1:1-1000000 with measureTiming=1.
"""

import argparse
import os
import re
import shutil
import statistics
import subprocess
import sys
import time

def _default_hgtracks():
    """Locate hgTracks: prefer the user's CGI sandbox, fall back to ~/bin."""
    user = os.environ.get("USER", "")
    candidates = [
        "/usr/local/apache/cgi-bin-{0}/hgTracks".format(user) if user else None,
        os.path.expanduser(
            "~/bin/" + os.environ.get("MACHTYPE", "x86_64") + "/hgTracks"),
    ]
    for c in candidates:
        if c and os.path.exists(c):
            return c
    return candidates[0] or candidates[1]


DEFAULT_HGTRACKS = _default_hgtracks()

OVERALL_RE = re.compile(
    r"<span class=['\"]timing['\"]>Overall total time:\s*(\d+)\s*millis"
)


def parse_args():
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "cache_dir",
        help="trackDb cache directory to test with (created if absent)",
    )
    p.add_argument(
        "--hgtracks",
        default=DEFAULT_HGTRACKS,
        help="path to hgTracks binary (default: %(default)s)",
    )
    p.add_argument(
        "--hg-conf",
        default=None,
        help="hg.conf to use (HGDB_CONF). Default: the hg.conf next to the "
             "hgTracks binary, falling back to inherited HGDB_CONF or "
             "~/.hg.conf. Using the CGI's own hg.conf is usually what you "
             "want -- ~/.hg.conf may point db.trackDb at a personal table "
             "that is far larger than what apache serves.",
    )
    p.add_argument(
        "--cgi",
        action="append",
        default=None,
        help="CGI var of the form name=value (repeatable). "
             "Default: db=hg38 position=chr1:1-1000000 measureTiming=1",
    )
    p.add_argument(
        "-n", "--iterations",
        type=int, default=5,
        help="timed iterations per mode (default: %(default)s)",
    )
    p.add_argument(
        "-w", "--warmup",
        type=int, default=1,
        help="warmup iterations per mode (default: %(default)s)",
    )
    p.add_argument(
        "--clear-cache-before",
        action="store_true",
        help="delete the cache directory before the cached run (forces a cold "
             "cache rebuild on its first warmup)",
    )
    p.add_argument(
        "--evict-cache",
        action="store_true",
        help="before each cached-mode iteration, fsync the cache files and "
             "drop them from the OS page cache (posix_fadvise DONTNEED). "
             "This forces a cold read from the backing filesystem each "
             "iteration -- useful for measuring real I/O cost of /dev/shm "
             "vs disk-backed cache directories.",
    )
    p.add_argument(
        "--mode",
        choices=("both", "cached", "no-cache"),
        default="both",
        help="which mode(s) to run (default: %(default)s)",
    )
    p.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="print each run's stderr/stdout snippets",
    )
    return p.parse_args()


def run_one(hgtracks, cgi_args, env):
    """Invoke hgTracks once. Return (wall_ms, internal_ms_or_None, rc, stderr_text)."""
    start = time.monotonic()
    r = subprocess.run(
        [hgtracks] + cgi_args,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    wall_ms = int((time.monotonic() - start) * 1000)
    out = r.stdout.decode("utf-8", errors="replace") if r.stdout else ""
    m = OVERALL_RE.search(out)
    internal_ms = int(m.group(1)) if m else None
    err = r.stderr.decode("utf-8", errors="replace") if r.stderr else ""
    return wall_ms, internal_ms, r.returncode, err


def evict_cache_files(cache_dir):
    """fsync then posix_fadvise(DONTNEED) every file under cache_dir.

    POSIX_FADV_DONTNEED only evicts clean pages from the page cache; freshly
    written files have dirty pages that survive eviction until written back.
    fsync forces write-back so the subsequent DONTNEED actually drops them.
    """
    if not os.path.isdir(cache_dir):
        return 0
    n = 0
    for root, _, files in os.walk(cache_dir):
        for name in files:
            path = os.path.join(root, name)
            try:
                fd = os.open(path, os.O_RDONLY)
            except OSError:
                continue
            try:
                os.fsync(fd)
                os.posix_fadvise(fd, 0, 0, os.POSIX_FADV_DONTNEED)
                n += 1
            except (OSError, AttributeError):
                pass
            finally:
                os.close(fd)
    return n


def child_env(cache_value, hg_conf):
    """Build an env dict for the hgTracks subprocess.

    Strip CGI-ish vars from the parent so hgTracks' cgiIsOnWeb() returns false
    and cgiSpoof() processes our command-line args.
    """
    env = {k: v for k, v in os.environ.items()
           if not k.startswith("HTTP_")
              and k not in ("QUERY_STRING", "REQUEST_METHOD",
                            "GATEWAY_INTERFACE", "CONTENT_TYPE",
                            "CONTENT_LENGTH", "SERVER_NAME")}
    env["CACHE_TRACK_DB_DIR"] = cache_value
    if hg_conf is not None:
        env["HGDB_CONF"] = hg_conf
    return env


def fmt_stats(label, samples):
    n = len(samples)
    samples = [s for s in samples if s is not None]
    if not samples:
        return "  {0:>10s}  n={1}  no timing data".format(label, n)
    return ("  {0:>10s}  n={1}/{2}  min={3}ms  median={4}ms  "
            "mean={5}ms  max={6}ms").format(
                label, len(samples), n,
                min(samples),
                int(statistics.median(samples)),
                int(statistics.mean(samples)),
                max(samples))


def run_mode(label, env, args):
    cache_dir = env["CACHE_TRACK_DB_DIR"]
    evict = args.evict_cache and label == "cached" and cache_dir
    print("=== mode={0}  CACHE_TRACK_DB_DIR={1!r}{2}".format(
              label, cache_dir,
              "  (evicting page cache between iterations)" if evict else ""),
          file=sys.stderr)
    for i in range(args.warmup):
        wall_ms, internal_ms, rc, err = run_one(args.hgtracks, args.cgi, env)
        print("  warmup {0}: wall={1}ms internal={2} rc={3}".format(
                  i + 1, wall_ms, internal_ms, rc), file=sys.stderr)
        if args.verbose and err.strip():
            print("    stderr: {0}".format(err.strip()[:300]), file=sys.stderr)
        if rc != 0:
            sys.exit("hgTracks exited rc={0}; aborting. stderr:\n{1}".format(
                rc, err))

    walls, internals = [], []
    for i in range(args.iterations):
        if evict:
            n = evict_cache_files(cache_dir)
            if args.verbose:
                print("    evicted {0} cache file(s) from page cache".format(n),
                      file=sys.stderr)
        wall_ms, internal_ms, rc, err = run_one(args.hgtracks, args.cgi, env)
        walls.append(wall_ms)
        internals.append(internal_ms)
        marker = "" if rc == 0 else " rc={0}".format(rc)
        print("  iter   {0}: wall={1}ms internal={2}{3}".format(
                  i + 1, wall_ms, internal_ms, marker), file=sys.stderr)
        if args.verbose and err.strip():
            print("    stderr: {0}".format(err.strip()[:300]), file=sys.stderr)
    return walls, internals


def main():
    args = parse_args()

    if not os.path.exists(args.hgtracks):
        sys.exit("hgTracks binary not found: {0}".format(args.hgtracks))
    if not os.access(args.hgtracks, os.X_OK):
        sys.exit("hgTracks binary not executable: {0}".format(args.hgtracks))

    args.cache_dir = os.path.abspath(args.cache_dir)

    if args.hg_conf is None:
        sibling = os.path.join(os.path.dirname(args.hgtracks), "hg.conf")
        if os.path.exists(sibling):
            args.hg_conf = sibling
    if args.hg_conf is not None:
        args.hg_conf = os.path.abspath(args.hg_conf)
        if not os.path.exists(args.hg_conf):
            sys.exit("hg.conf not found: {0}".format(args.hg_conf))

    args.cgi = args.cgi or [
        "db=hg38",
        "position=chr1:1-1000000",
        "measureTiming=1",
        "hgt.trackImgOnly=1",
    ]

    print("hgTracks:    {0}".format(args.hgtracks))
    print("hg.conf:     {0}".format(args.hg_conf or "(inherit / ~/.hg.conf)"))
    print("cache dir:   {0}".format(args.cache_dir))
    print("cgi args:    {0}".format(" ".join(args.cgi)))
    print("warmup={0}  iterations={1}  mode={2}\n".format(
              args.warmup, args.iterations, args.mode))

    modes = []
    if args.mode in ("cached", "both"):
        modes.append(("cached", args.cache_dir))
    if args.mode in ("no-cache", "both"):
        modes.append(("no-cache", ""))

    results = {}
    for label, cache_value in modes:
        if label == "cached" and args.clear_cache_before:
            if os.path.isdir(args.cache_dir):
                shutil.rmtree(args.cache_dir)
                print("cleared cache dir: {0}".format(args.cache_dir),
                      file=sys.stderr)
        env = child_env(cache_value, args.hg_conf)
        results[label] = run_mode(label, env, args)

    print()
    print("Wall-clock (subprocess) time:")
    for label, (walls, _) in results.items():
        print(fmt_stats(label, walls))

    if any(any(i is not None for i in r[1]) for r in results.values()):
        print()
        print("hgTracks internal time (measureTiming=1 'Overall total time'):")
        for label, (_, internals) in results.items():
            print(fmt_stats(label, internals))

    if "cached" in results and "no-cache" in results:
        cw = [w for w in results["cached"][0] if w is not None]
        nw = [w for w in results["no-cache"][0] if w is not None]
        if cw and nw:
            cm = statistics.median(cw)
            nm = statistics.median(nw)
            if nm > 0:
                print()
                print("cached/no-cache wall median ratio: {0:.2f}  "
                      "(saving {1}ms / {2:.0%})".format(
                          cm / nm, int(nm - cm), 1 - cm / nm))


if __name__ == "__main__":
    main()
