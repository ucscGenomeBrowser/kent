#!/usr/bin/env python3
"""pick a corpus of real hgTracks URLs out of one day of hgw1 access log lines.

Keeps the URLs a reader actually loaded, drops the ones this machine cannot
render the same way, and samples them in proportion to how often they were
loaded, so the corpus is weighted the way real traffic is.
"""
import gzip, random, sys, urllib.parse, collections

SRC   = sys.argv[1]
DBS   = sys.argv[2]
OUT   = sys.argv[3]
N     = int(sys.argv[4]) if len(sys.argv) > 4 else 300

# params that would make this render something other than what the reader saw,
# or would reach off the machine
DROP = {"hgsid", "pix", "hgt.customText", "hgct_customText", "hubUrl",
        "hgt.psOutput", "hgt.imageV1", "hgt.trackImgOnly", "hgt.trackNameFilter",
        "hgTracksConfigPage", "hgt.psOutput", "hgt.out1", "hgt.out2"}

local = set(x.strip() for x in open(DBS))
counts = collections.Counter()
kept = dropped = collections.Counter()
stat = collections.Counter()

with gzip.open(SRC, "rt", errors="replace") as f:
    for line in f:
        field = line.split()
        if len(field) < 2 or field[1] != "200":
            stat["not a 200"] += 1
            continue
        url = field[0]
        if "?" not in url:
            stat["no query string"] += 1
            continue
        query = urllib.parse.parse_qsl(url.split("?", 1)[1], keep_blank_values=True)
        keys = set(k for k, v in query)
        if keys & {"hgt.customText", "hgct_customText", "hubUrl"}:
            stat["custom track or hub url"] += 1
            continue
        param = [(k, v) for k, v in query if k not in DROP]
        db = dict(param).get("db", "")
        if not db:
            stat["no db"] += 1
            continue
        if db.startswith("hub_") or db.startswith("GC"):
            stat["hub or GenArk assembly"] += 1
            continue
        if db not in local:
            stat["db not on this machine"] += 1
            continue
        if not dict(param).get("position"):
            stat["no position"] += 1
            continue
        stat["kept"] += 1
        counts[urllib.parse.urlencode(sorted(param))] += 1

sys.stderr.write("lines read, by outcome:\n")
for why, n in stat.most_common():
    sys.stderr.write("  %8d  %s\n" % (n, why))
sys.stderr.write("%d distinct URLs\n" % len(counts))

# sample in proportion to how often each URL was loaded, without repeats
random.seed(38109)
urls = list(counts)
weights = [counts[u] for u in urls]
chosen = []
seen = set()
while len(chosen) < min(N, len(urls)):
    u = random.choices(urls, weights=weights, k=1)[0]
    if u in seen:
        continue
    seen.add(u)
    chosen.append(u)

with open(OUT, "w") as out:
    for u in chosen:
        out.write("%d\t%s\n" % (counts[u], u))
sys.stderr.write("%d URLs written to %s\n" % (len(chosen), OUT))
