#!/usr/bin/env python3
"""turn pngLevelBench -tab rows into per-level B and S for the whole corpus.

  levelReport.py bench.tsv logSizes.tsv [baseLevel]

bench.tsv    the -tab output of pngLevelBench over the corpus
logSizes.tsv one real track image size per line, from the access log

Two weightings are printed.  The plain one treats every corpus image alike;
the corpus was drawn in proportion to how often each URL was loaded, so that
is already traffic weighted.  The size corrected one reweights the corpus so
its size histogram matches the log's, which repairs the one way a replayed
corpus is known to differ from real traffic: a replayed URL renders the
trackDb default track set, not the reader's own.
"""
import collections
import sys

bench, logSizes = sys.argv[1], sys.argv[2]
base = int(sys.argv[3]) if len(sys.argv) > 3 else 6

rows = collections.defaultdict(dict)
fileBytes = {}
for line in open(bench):
    if line.startswith("#"):
        continue
    f, w, h, fb, level, encoded, ms = line.rstrip("\n").split("\t")
    rows[f][int(level)] = (int(encoded), float(ms))
    fileBytes[f] = int(fb)

files = sorted(rows)
levels = sorted(set(l for f in files for l in rows[f]))

# the check that makes the rest of this mean anything: the tool's base level
# has to reproduce the file the browser wrote, byte for byte
bad = [f for f in files if rows[f].get(base, (0,))[0] != fileBytes[f]]
print("%d corpus images, levels %d to %d" % (len(files), levels[0], levels[-1]))
print("level %d reproduces the file on disk for %d of %d images"
      % (base, len(files) - len(bad), len(files)))
for f in bad[:5]:
    print("  MISMATCH %s: level %d gives %d, file is %d"
          % (f, base, rows[f][base][0], fileBytes[f]))

def percentiles(values, ps=(10, 25, 50, 75, 90)):
    v = sorted(values)
    return [v[int(round((len(v) - 1) * p / 100.0))] for p in ps]

logv = sorted(int(x.split("\t")[-1]) for x in open(logSizes) if x.strip())
corpus = [rows[f][base][0] for f in files]
print("\nimage size in KB, at level %d:" % base)
print("  %-16s %7s %7s %7s %7s %7s %9s" % ("", "p10", "p25", "p50", "p75", "p90", "mean"))
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
    if corpusBin[b] == 0:
        weight[f] = 0.0
    else:
        weight[f] = (logShare[b] / float(len(logv))) / (corpusBin[b] / float(len(corpus)))
empty = [b for b in range(len(edges) + 1) if corpusBin[b] == 0]
if empty:
    print("\n  no corpus image falls in log size bin(s) %s, so %.0f%% of real"
          % (empty, 100.0 * sum(logShare[b] for b in empty) / len(logv)))
    print("  deliveries have no corpus image to speak for them")

def table(title, wt):
    total = sum(wt[f] for f in files) or 1.0
    print("\n%s" % title)
    print("%-6s %10s %9s %10s %9s %12s" %
          ("level", "KB/image", "ms/image", "KB vs b", "ms vs b", "break-even"))
    out = {}
    for level in levels:
        kb = sum(wt[f] * rows[f][level][0] for f in files) / total / 1024.0
        ms = sum(wt[f] * rows[f][level][1] for f in files) / total
        kb0 = sum(wt[f] * rows[f][base][0] for f in files) / total / 1024.0
        ms0 = sum(wt[f] * rows[f][base][1] for f in files) / total
        dKb, dMs = kb - kb0, ms0 - ms
        if dKb > 0 and dMs > 0:
            be = "%12.1f" % (dKb * 1024 * 8 / dMs / 1000.0)
        elif dKb < 0 and dMs < 0:
            be = "%11s%.1f" % ("<", dKb * 1024 * 8 / dMs / 1000.0)
        elif dKb == 0 and dMs == 0:
            be = "%12s" % "base"
        else:
            be = "%12s" % ("always" if dKb <= 0 else "never")
        print("%-6d %10.1f %9.2f %10.1f %9.2f %s" % (level, kb, ms, dKb, dMs, be))
        out[level] = (dKb, dMs)
    return out

plain = table("every corpus image counted alike (already traffic weighted):", 
              {f: 1.0 for f in files})
corrected = table("reweighted so the corpus size histogram matches the log:", weight)

print("\nbreak-even is Mbit/s: above it the reader gains from the level, below it")
print("the reader loses.  A number with < means the level is smaller and slower,")
print("so the gain is below that speed instead.")

# per image spread of the break-even, which the aggregate hides
print("\nper image break-even against level %d, Mbit/s:" % base)
print("%-6s %7s %7s %7s %7s %7s %8s" % ("level", "p10", "p25", "p50", "p75", "p90", "n"))
for level in levels:
    if level == base:
        continue
    be = []
    for f in files:
        dB = rows[f][level][0] - rows[f][base][0]
        dM = rows[f][base][1] - rows[f][level][1]
        if dB > 0 and dM > 0:
            be.append(dB * 8.0 / dM / 1000.0)
    if len(be) < 2:
        print("%-6d %7s %7s %7s %7s %7s %8d" % (level, "-", "-", "-", "-", "-", len(be)))
        continue
    print("%-6d %7.1f %7.1f %7.1f %7.1f %7.1f %8d"
          % (level, *percentiles(be), len(be)))
print("n is how many images trade bytes for time at that level; the rest are")
print("bigger and slower than level %d, which is never worth it." % base)
