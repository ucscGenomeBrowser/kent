#!/usr/bin/env python3
"""pngTimingReport.py - read the track image timing beacons out of an apache access log.

Refs #38109.  hgTracks measures how long its track image took to reach the
reader and puts the numbers on the query string of a 43 byte image:

    GET /images/DOT.gif?hgtPng=1&ts=77185&d=10&x=8

    ts   transferSize, the bytes the browser took off the wire.  Response
         headers are included, so this is 200-300 bytes more than the png on
         disk.  It is the right numerator for throughput all the same.
    d    duration, the whole fetch in ms: request, wait, and bytes.
    x    responseEnd - responseStart, the bytes arriving, in ms.

The measurement is client side, in reportPngTiming() in hg/js/hgTracks.js, and
is gated by the hg.conf setting pngTimingSampleRate (the N in one page load in
N; 0 or absent is off).  Our own logs cannot answer this question: apache stops
timing once the kernel has the bytes, so its duration field stays near 2 ms
whether the image is 20 KB or 500 KB.  ts / x is the reader's throughput, which
is what decides whether a lower png compression level helps them or hurts them.

This turns those log lines into a throughput distribution, split by continent,
so the compression level can be argued from the readers we actually have.

Usage:
    pngTimingReport.py                              # the live hgwdev log
    pngTimingReport.py --by browser                 # or country, site, none
    pngTimingReport.py /hive/data/inside/wwwstats/RR/2026/hgw*/access_log.2026*.gz
    pngTimingReport.py --raw samples.tsv LOG...     # keep the parsed samples

Reading the numbers
-------------------
Three things about the sample, all of them in the measurement and none of them
fixable here:

  - x = 0 is a real delivery, not a missing one.  It means the image arrived
    inside one clock tick; Firefox rounds resource timing to a millisecond, so
    a small image on a fast link lands there.  Those samples cannot give a
    throughput, so they are counted in their own column and left out of the
    percentiles.  They are the readers already fast enough that the compression
    level does not matter to them, which is a number worth having on its own.
    ts / d gives them a lower bound and is reported as one.

  - every sample is a cold load.  The beacon fires from $(document).ready, so
    drag-scroll and every other AJAX image refresh is never timed.  No sample
    has a warm TCP connection behind it.

  - one reader can send many samples.  The clients column says how many
    distinct addresses are behind a row; --one-per-client keeps each client's
    median sample if one address is drowning out a continent.

Country comes from hgcentraltest.geoIpCountry6, the same table the browser
itself uses to pick a mirror, looked up the same way (the last range that
starts at or before the address, then a check that it ends at or after it).
Continent comes from COUNTRY_TO_CONTINENT below, which is the geoIp
countryToContinent dump in one line per continent.
"""

import argparse
import bisect
import collections
import os
import re
import socket
import subprocess
import sys
import urllib.parse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime

# the live hgwdev log, used when no file is named
DEFAULT_LOG = "/usr/local/apache/logs/access_log"

# the country codes of each continent, from the geoIp countryToContinent table
# (/hive/data/outside/geoIp/geoIpTableDumps).  '--' is anonymizing proxies and
# satellite links, which have no location.
CONTINENT_COUNTRIES = {
    "AF": "AO BF BI BJ BW CD CF CG CI CM CV DJ DZ EG EH ER ET GA GH GM GN GQ GW KE KM"
          " LR LS LY MA MG ML MR MU MW MZ NA NE NG RE RW SC SD SH SL SN SO SS ST SZ TD"
          " TG TN TZ UG YT ZA ZM ZW",
    "AN": "AQ BV GS HM TF",
    "AS": "AE AF AM AP AZ BD BH BN BT CC CN CX CY GE HK ID IL IN IO IQ IR JO JP KG KH"
          " KP KR KW KZ LA LB LK MM MN MO MV MY NP OM PH PK PS QA SA SG SY TH TJ TL TM"
          " TW UZ VN YE",
    "EU": "AD AL AT AX BA BE BG BY CH CZ DE DK EE ES EU FI FO FR FX GB GG GI GR HR HU"
          " IE IM IS IT JE LI LT LU LV MC MD ME MK MT NL NO PL PT RO RS RU SE SI SJ SK"
          " SM TR UA VA XK",
    "NA": "AG AI AN AW BB BL BM BQ BS BZ CA CR CU CW DM DO GD GL GP GT HN HT JM KN KY"
          " LC MF MQ MS MX NI PA PM PR SV SX TC TT US VC VG VI",
    "OC": "AS AU CK FJ FM GU KI MH MP NC NF NR NU NZ PF PG PN PW SB TK TO TV UM VU WF WS",
    "SA": "AR BO BR CL CO EC FK GF GY PE PY SR UY VE",
    "--": "A1 A2 O1",
}

CONTINENT_NAMES = {
    "AF": "Africa", "AN": "Antarctica", "AS": "Asia", "EU": "Europe",
    "NA": "North America", "OC": "Oceania", "SA": "South America",
    "--": "no location", "??": "not in geoIp",
}

COUNTRY_TO_CONTINENT = {country: continent
                        for continent, countries in CONTINENT_COUNTRIES.items()
                        for country in countries.split()}

# combined log format, with the referer and the user agent optional
LOG_RE = re.compile(r'^(\S+) \S+ \S+ \[([^\]]+)\] "\S+ (\S+)[^"]*" (\d+) (\S+)'
                    r'(?: "([^"]*)" "([^"]*)")?')

# our own Playwright runs, and anything that says it is a robot.  Every real
# sample has to come from a real browser on a real connection.
BOT_RE = re.compile(r'Headless|bot\b|Bot\b|spider|Spider|crawl|Crawl|curl|Wget|python',
                    re.IGNORECASE)

MONTHS = {"Jan": 1, "Feb": 2, "Mar": 3, "Apr": 4, "May": 5, "Jun": 6,
          "Jul": 7, "Aug": 8, "Sep": 9, "Oct": 10, "Nov": 11, "Dec": 12}


class Sample(object):
    """one delivery of one track image to one reader"""
    __slots__ = ("when", "client", "bytes", "duration", "download", "site", "db",
                 "browser", "country")

    @property
    def mbits(self):
        """throughput in Mbit/s, or None when the delivery was too fast to time"""
        if not self.download:
            return None
        return self.bytes * 8.0 / self.download / 1000.0

    @property
    def boundMbits(self):
        """a lower bound on throughput from the whole fetch, headers and wait included"""
        if not self.duration:
            return None
        return self.bytes * 8.0 / self.duration / 1000.0

    @property
    def continent(self):
        if self.country is None:
            return "??"
        return COUNTRY_TO_CONTINENT.get(self.country, "??")


def parseLogTime(stamp):
    """31/Aug/2026:13:56:52 -0700 -> a naive datetime in the log's own zone"""
    day, month, rest = stamp[:2], stamp[3:6], stamp[7:]
    year, hour, minute, second = rest.split(":")[0], rest[5:7], rest[8:10], rest[11:13]
    return datetime(int(year), MONTHS[month], int(day),
                    int(hour), int(minute), int(second))


def browserOf(agent):
    """the browser family, which matters because they do not all round timing alike"""
    if not agent:
        return "unknown"
    if "Edg/" in agent or "Edge" in agent:
        return "Edge"
    if "OPR/" in agent or "Opera" in agent:
        return "Opera"
    if "Firefox" in agent:
        return "Firefox"
    if "Chrome" in agent or "Chromium" in agent:
        return "Chrome"
    if "Safari" in agent:
        return "Safari"
    return "other"


def scanFile(path, pattern="hgtPng=1"):
    """the log lines that carry a beacon.  zcat -f reads plain files too, and
    grep throws away the 99.99% of the log that is not a beacon, both of them in
    C rather than here."""
    if not os.path.exists(path):
        sys.stderr.write("pngTimingReport: no such log %s\n" % path)
        return []
    zcat = subprocess.Popen(["zcat", "-f", path], stdout=subprocess.PIPE)
    grep = subprocess.Popen(["grep", "-aF", pattern], stdin=zcat.stdout,
                            stdout=subprocess.PIPE)
    zcat.stdout.close()
    out = grep.communicate()[0].decode("utf8", "replace")
    zcat.wait()
    return out.splitlines()


def ipToBytes(text):
    """the 16 byte form of an address, so an IPv4 client compares against the
    same table as an IPv6 one.  geoIpCountry6 holds IPv4 as ::ffff:a.b.c.d."""
    try:
        if ":" in text:
            return socket.inet_pton(socket.AF_INET6, text)
        return b"\0" * 10 + b"\xff\xff" + socket.inet_pton(socket.AF_INET, text)
    except (socket.error, ValueError, OSError):
        return None


class GeoTable(object):
    """geoIpCountry6, read once and searched here.  One query per address would
    be more faithful to geoMirror.c, but a log sweep asks about thousands of
    them and the whole table is a quarter of a second."""

    def __init__(self, db="hgcentraltest", table="geoIpCountry6"):
        self.starts = []
        self.rows = []
        self.loaded = False
        self.db = db
        self.table = table

    def load(self):
        self.loaded = True
        sql = "select hex(ipStart), hex(ipEnd), countryId from %s order by ipStart" \
              % self.table
        try:
            out = subprocess.check_output(["hgsql", "-N", "-e", sql, self.db],
                                          stderr=subprocess.PIPE)
        except (subprocess.CalledProcessError, OSError) as e:
            sys.stderr.write("pngTimingReport: cannot read %s.%s (%s), "
                             "so no country or continent\n" % (self.db, self.table, e))
            return
        for line in out.decode("utf8", "replace").splitlines():
            field = line.split("\t")
            if len(field) != 3:
                continue
            self.starts.append(bytes.fromhex(field[0]))
            self.rows.append((bytes.fromhex(field[1]), field[2]))

    def country(self, text):
        """the 2 letter country of an address, or None when no range holds it"""
        if not self.loaded:
            self.load()
        if not self.starts:
            return None
        ip = ipToBytes(text)
        if ip is None:
            return None
        # the last range that starts at or before the address, then the check
        # geoMirror.c makes: it has to end at or after it too.  The table has
        # holes, and a hole must not answer with its neighbour's country.
        i = bisect.bisect_right(self.starts, ip) - 1
        if i < 0:
            return None
        end, country = self.rows[i]
        if ip > end:
            return None
        return country


def parseLine(line, geo, keepBots):
    """one log line -> a Sample, or None with a reason for dropping it"""
    m = LOG_RE.match(line)
    if not m:
        return None, "unparsed log line"
    client, stamp, url, status, size, referer, agent = m.groups()
    if "?" not in url:
        return None, "no query string"
    query = urllib.parse.parse_qs(url.split("?", 1)[1])
    if "hgtPng" not in query:
        return None, "not a track image beacon"
    try:
        s = Sample()
        s.bytes = int(query["ts"][0])
        s.duration = int(query["d"][0])
        s.download = int(query["x"][0])
    except (KeyError, IndexError, ValueError):
        return None, "beacon without ts, d and x"
    if s.bytes <= 0 or s.duration < 0 or s.download < 0:
        return None, "beacon with a number out of range"
    if s.download > s.duration:
        # the browser is contradicting itself; the bytes cannot take longer
        # than the whole fetch.  Nothing here can repair such a sample.
        return None, "download longer than the whole fetch"
    if not keepBots and agent and BOT_RE.search(agent):
        return None, "robot or test harness"
    s.client = client
    try:
        s.when = parseLogTime(stamp)
    except (ValueError, KeyError, IndexError):
        return None, "unparsed timestamp"
    s.browser = browserOf(agent)
    s.site, s.db = "", ""
    if referer:
        parts = urllib.parse.urlsplit(referer)
        s.site = parts.netloc
        dbs = urllib.parse.parse_qs(parts.query).get("db")
        if dbs:
            s.db = dbs[0]
    s.country = geo.country(client)
    return s, None


def percentile(values, p):
    """the p'th percentile of a sorted list, by nearest rank"""
    if not values:
        return None
    i = int(round((len(values) - 1) * p / 100.0))
    return values[i]


def groupKey(sample, by):
    if by == "continent":
        return CONTINENT_NAMES.get(sample.continent, sample.continent)
    if by == "country":
        return sample.country or "??"
    if by == "site":
        return sample.site or "none"
    if by == "browser":
        return sample.browser
    if by == "db":
        return sample.db or "none"
    return "all readers"


def report(samples, by, out):
    """the table: one row per group, sorted by how many samples it holds"""
    groups = collections.defaultdict(list)
    for s in samples:
        groups[groupKey(s, by)].append(s)

    label = "readers" if by == "none" else by
    # a site or a db name is much longer than a continent name
    width = max([len(label)] + [len(name) for name in groups])
    header = ("%-*s %6s %7s %6s %7s %8s %8s %8s %8s %8s"
              % (width, label, "n", "clients", "fast", "med KB",
                 "p10", "p25", "p50", "p75", "p90"))
    out.write(header + "\n")
    out.write("-" * len(header) + "\n")

    order = sorted(groups.items(), key=lambda kv: (-len(kv[1]), kv[0]))
    for name, rows in order + ([("ALL", samples)] if len(groups) > 1 else []):
        timed = sorted(s.mbits for s in rows if s.mbits is not None)
        kb = sorted(s.bytes / 1024.0 for s in rows)
        fast = sum(1 for s in rows if s.mbits is None)
        clients = len(set(s.client for s in rows))
        cells = ["%8.1f" % percentile(timed, p) if timed else "%8s" % "-"
                 for p in (10, 25, 50, 75, 90)]
        out.write("%-*s %6d %7d %6d %7.0f %s\n"
                  % (width, name, len(rows), clients, fast,
                     percentile(kb, 50), " ".join(cells)))
    out.write("\nn samples, fast = deliveries too fast to time (x=0, left out of\n"
              "the percentiles), med KB = median transferSize, p* = throughput\n"
              "percentiles in Mbit/s.\n")


def transferCost(samples, out):
    """what the distribution means for a compression change: the transfer time
    a reader at each percentile pays for every 10 KB the image grows."""
    timed = sorted(s.mbits for s in samples if s.mbits is not None)
    if not timed:
        return
    out.write("\nms of transfer time per extra 10 KB of image:\n")
    for p in (10, 25, 50, 75, 90):
        mbits = percentile(timed, p)
        out.write("  p%-3d %8.1f Mbit/s   %7.1f ms\n"
                  % (p, mbits, 10 * 1024 * 8 / (mbits * 1000.0)))

    fast = [s for s in samples if s.mbits is None]
    if fast:
        bounds = sorted(s.boundMbits for s in fast if s.boundMbits is not None)
        out.write("\n%d of %d deliveries (%.0f%%) finished inside one clock tick.\n"
                  % (len(fast), len(samples), 100.0 * len(fast) / len(samples)))
        if bounds:
            out.write("Their whole fetch bounds them at %.0f Mbit/s or better "
                      "(median of ts/d).\n" % percentile(bounds, 50))


def histogram(samples, out):
    """where the readers sit, which is the shape the percentiles flatten"""
    edges = [1, 2, 5, 10, 25, 50, 100]
    labels = ["< 1"] + ["%d - %d" % (edges[i], edges[i + 1])
                        for i in range(len(edges) - 1)] + ["100 +", "too fast"]
    counts = [0] * len(labels)
    for s in samples:
        mbits = s.mbits
        if mbits is None:
            counts[-1] += 1
            continue
        for i, edge in enumerate(edges):
            if mbits < edge:
                counts[i] += 1
                break
        else:
            counts[-2] += 1
    top = max(counts) or 1
    out.write("\nreaders by throughput, Mbit/s:\n")
    for label, count in zip(labels, counts):
        out.write("  %-9s %6d %5.1f%%  %s\n"
                  % (label, count, 100.0 * count / len(samples),
                     "#" * int(round(40.0 * count / top))))


def writeRaw(samples, path):
    with open(path, "w") as f:
        f.write("#date\tclient\tcountry\tcontinent\tsite\tdb\tbrowser"
                "\tbytes\tdurationMs\tdownloadMs\tmbits\n")
        for s in sorted(samples, key=lambda s: s.when):
            f.write("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%s\n"
                    % (s.when.isoformat(), s.client, s.country or "", s.continent,
                       s.site, s.db, s.browser, s.bytes, s.duration, s.download,
                       "" if s.mbits is None else "%.2f" % s.mbits))


def medianPerClient(samples):
    """one sample per client, the median throughput one, so that a single reader
    with a hundred page loads counts once"""
    byClient = collections.defaultdict(list)
    for s in samples:
        byClient[s.client].append(s)
    kept = []
    for rows in byClient.values():
        # a client with any timed sample is represented by a timed one
        timed = sorted((s for s in rows if s.mbits is not None),
                       key=lambda s: s.mbits)
        kept.append(timed[len(timed) // 2] if timed else rows[0])
    return kept


def main():
    parser = argparse.ArgumentParser(
        description="read the hgTracks png timing beacons out of an apache access log",
        epilog="with no LOG, reads " + DEFAULT_LOG)
    parser.add_argument("logs", metavar="LOG", nargs="*",
                        help="access log, plain or .gz")
    parser.add_argument("--by", default="continent",
                        choices=["continent", "country", "site", "browser", "db", "none"],
                        help="what to split the table by (default continent)")
    parser.add_argument("--site", metavar="HOST",
                        help="keep only beacons whose page was served by HOST")
    parser.add_argument("--since", metavar="YYYY-MM-DD", help="drop earlier samples")
    parser.add_argument("--until", metavar="YYYY-MM-DD", help="drop later samples")
    parser.add_argument("--keep-bots", action="store_true",
                        help="keep robots and our own Playwright runs")
    parser.add_argument("--one-per-client", action="store_true",
                        help="keep each client's median sample, not all of them")
    parser.add_argument("--no-geo", action="store_true",
                        help="skip the country lookup (implies --by none)")
    parser.add_argument("--raw", metavar="FILE", help="write the parsed samples as tsv")
    parser.add_argument("--jobs", type=int, default=8,
                        help="log files to read at once (default 8)")
    args = parser.parse_args()

    logs = args.logs or [DEFAULT_LOG]
    if args.no_geo and args.by in ("continent", "country"):
        args.by = "none"

    jobs = max(1, min(args.jobs, len(logs)))
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        lines = [line for chunk in pool.map(scanFile, logs) for line in chunk]

    geo = GeoTable()
    if args.no_geo:
        geo.loaded = True

    since = datetime.strptime(args.since, "%Y-%m-%d") if args.since else None
    until = datetime.strptime(args.until, "%Y-%m-%d") if args.until else None

    samples = []
    dropped = collections.Counter()
    for line in lines:
        s, why = parseLine(line, geo, args.keep_bots)
        if s is None:
            dropped[why] += 1
            continue
        if args.site and s.site != args.site:
            dropped["another site"] += 1
            continue
        if since and s.when < since:
            dropped["before --since"] += 1
            continue
        if until and s.when > until:
            dropped["after --until"] += 1
            continue
        samples.append(s)

    out = sys.stdout
    out.write("%d log %s, %d beacon %s, %d %s kept\n"
              % (len(logs), "file" if len(logs) == 1 else "files",
                 len(lines), "line" if len(lines) == 1 else "lines",
                 len(samples), "sample" if len(samples) == 1 else "samples"))
    for why, count in dropped.most_common():
        out.write("  dropped %6d  %s\n" % (count, why))
    if not samples:
        out.write("\nNothing to report.  pngTimingSampleRate has to be set in the\n"
                  "hg.conf of the machine that served the page, or no beacon is sent.\n")
        return 0

    if args.one_per_client:
        samples = medianPerClient(samples)
        out.write("%d clients, one sample each\n" % len(samples))
    out.write("%s to %s\n" % (min(s.when for s in samples).isoformat(sep=" "),
                              max(s.when for s in samples).isoformat(sep=" ")))
    out.write("\n")

    report(samples, args.by, out)
    histogram(samples, out)
    transferCost(samples, out)
    if args.raw:
        writeRaw(samples, args.raw)
        out.write("\n%d samples written to %s\n" % (len(samples), args.raw))
    return 0


if __name__ == "__main__":
    sys.exit(main())
