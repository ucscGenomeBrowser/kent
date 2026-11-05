#!/usr/bin/env python3
"""turn pngLevelBench -tab rows into per-setting B and S for the whole corpus.

  levelReport.py bench.tsv logSizes.tsv [baseLevel] [baseFilter]

bench.tsv    the -tab output of pngLevelBench over the corpus
logSizes.tsv one real track image size per line, from the access log

A setting here is a row filter and a zlib level together, because the two
interact.  The default base is the pair the browser ships, filter up at level 6.

Two weightings are printed.  The plain one treats every corpus image alike; the
corpus was drawn in proportion to how often each URL was loaded, so that is
already traffic weighted.  The size corrected one reweights the corpus so its
size histogram matches the log's, which repairs the one way a replayed corpus is
known to differ from real traffic: a replayed URL renders the trackDb default
track set, not the reader's own.
"""
import collections
import sys

bench, logSizes = sys.argv[1], sys.argv[2]
baseLevel = int(sys.argv[3]) if len(sys.argv) > 3 else 6
baseFilter = sys.argv[4] if len(sys.argv) > 4 else "up"

rows = collections.defaultdict(dict)
fileBytes = {}
order = []
for line in open(bench):
    if line.startswith("#"):
        continue
    field = line.rstrip("\n").split("\t")
    if len(field) == 7:			# before the filter column existed
        f, w, h, fb, level, encoded, ms = field
        filt = "up"
    else:
        f, w, h, fb, filt, level, encoded, ms = field
    setting = (filt, int(level))
    if setting not in rows[f]:
        if setting not in order:
            order.append(setting)
    rows[f][setting] = (int(encoded), float(ms))
    fileBytes[f] = int(fb)

files = sorted(rows)
settings = [s for s in order if all(s in rows[f] for f in files)]
filters = []
for filt, level in settings:
    if filt not in filters:
        filters.append(filt)
levels = sorted(set(level for filt, level in settings))
base = (baseFilter, baseLevel)
if base not in settings:
    sys.exit("levelReport: the base setting %s at level %d is not in %s"
             % (baseFilter, baseLevel, bench))

# the check that makes the rest of this mean anything: the base setting has to
# reproduce the file the browser wrote, byte for byte
bad = [f for f in files if rows[f][base][0] != fileBytes[f]]
print("%d corpus images, %d filter%s, levels %d to %d"
      % (len(files), len(filters), "" if len(filters) == 1 else "s",
         levels[0], levels[-1]))
print("%s at level %d reproduces the file on disk for %d of %d images"
      % (baseFilter, baseLevel, len(files) - len(bad), len(files)))
for f in bad[:5]:
    print("  MISMATCH %s: base gives %d, file is %d"
          % (f, rows[f][base][0], fileBytes[f]))


def percentiles(values, ps=(10, 25, 50, 75, 90)):
    v = sorted(values)
    return [v[int(round((len(v) - 1) * p / 100.0))] for p in ps]


logv = sorted(int(x.split("\t")[-1]) for x in open(logSizes) if x.strip())
corpus = [rows[f][base][0] for f in files]
print("\nimage size in KB, at the base setting:")
print("  %-16s %7s %7s %7s %7s %7s %9s"
      % ("", "p10", "p25", "p50", "p75", "p90", "mean"))
for name, v in (("access log", logv), ("corpus", corpus)):
    print("  %-16s %7.0f %7.0f %7.0f %7.0f %7.0f %9.0f"
          % (name, *[x / 1024.0 for x in percentiles(v)], sum(v) / len(v) / 1024.0))

# size correction: bin on the log's own deciles and match the shares
edges = percentiles(logv, ps=(10, 20, 30, 40, 50, 60, 70, 80, 90))


def binOf(size):
    for i, e in enumerate(edges):
        if size <= e:
            return i
    return len(edges)


logShare = collections.Counter(binOf(s) for s in logv)
corpusBin = collections.Counter(binOf(s) for s in corpus)
weight = {}
for f, size in zip(files, corpus):
    b = binOf(size)
    weight[f] = 0.0 if corpusBin[b] == 0 else \
        (logShare[b] / float(len(logv))) / (corpusBin[b] / float(len(corpus)))
empty = [b for b in range(len(edges) + 1) if corpusBin[b] == 0]
if empty:
    print("\n  no corpus image falls in log size bin(s) %s, so %.0f%% of real"
          % (empty, 100.0 * sum(logShare[b] for b in empty) / len(logv)))
    print("  deliveries have no corpus image to speak for them")


def breakEven(dKb, dMs):
    """the reader throughput at which dKb more costs exactly dMs less"""
    if dKb > 0 and dMs > 0:
        return "%12.1f" % (dKb * 1024 * 8 / dMs / 1000.0)
    if dKb < 0 and dMs < 0:
        return "%11s%.1f" % ("<", dKb * 1024 * 8 / dMs / 1000.0)
    if dKb == 0 and dMs == 0:
        return "%12s" % "base"
    return "%12s" % ("always" if dKb <= 0 else "never")


def table(title, wt):
    total = sum(wt[f] for f in files) or 1.0

    def mean(setting, which):
        return sum(wt[f] * rows[f][setting][which] for f in files) / total

    kb0 = mean(base, 0) / 1024.0
    ms0 = mean(base, 1)
    print("\n%s" % title)
    print("%-7s %-6s %10s %9s %10s %9s %12s"
          % ("filter", "level", "KB/image", "ms/image", "KB vs b", "ms vs b",
             "break-even"))
    wins = []
    for setting in settings:
        kb = mean(setting, 0) / 1024.0
        ms = mean(setting, 1)
        dKb, dMs = kb - kb0, ms0 - ms
        print("%-7s %-6d %10.1f %9.2f %10.1f %9.2f %s"
              % (setting[0], setting[1], kb, ms, dKb, dMs, breakEven(dKb, dMs)))
        if dKb < 0 and dMs > 0:
            wins.append((setting, dKb, dMs))
    for setting, dKb, dMs in wins:
        print("  %s at level %d is %.1f KB smaller AND %.2f ms faster than the base"
              % (setting[0], setting[1], -dKb, dMs))
    return wins


table("every corpus image counted alike (already traffic weighted):",
      {f: 1.0 for f in files})
table("reweighted so the corpus size histogram matches the log:", weight)

print("\nbreak-even is Mbit/s: above it the reader gains from the setting, below")
print("it the reader loses.  A number with < means the setting is smaller and")
print("slower, so the gain is below that speed instead.")

# per image spread of the break-even, which the aggregate hides
print("\nper image break-even against the base, Mbit/s:")
print("%-7s %-6s %7s %7s %7s %7s %7s %8s"
      % ("filter", "level", "p10", "p25", "p50", "p75", "p90", "n"))
for setting in settings:
    if setting == base:
        continue
    be = []
    for f in files:
        dB = rows[f][setting][0] - rows[f][base][0]
        dM = rows[f][base][1] - rows[f][setting][1]
        if dB > 0 and dM > 0:
            be.append(dB * 8.0 / dM / 1000.0)
    if len(be) < 2:
        print("%-7s %-6d %7s %7s %7s %7s %7s %8d"
              % (setting[0], setting[1], "-", "-", "-", "-", "-", len(be)))
        continue
    print("%-7s %-6d %7.1f %7.1f %7.1f %7.1f %7.1f %8d"
          % (setting[0], setting[1], *percentiles(be), len(be)))
print("n is how many images trade bytes for time at that setting; the rest are")
print("either smaller and faster than the base, or bigger and slower.")
