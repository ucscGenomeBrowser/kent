#!/usr/bin/env python3
"""
mergeTrf.py - Stitch together simpleRepeat.bed records that trfBig split
across its internal chunk boundaries.

Background: trfBig re-runs trf independently on each ~500 kb (or 5 Mb)
window, with a fixed 10 kb overlap between windows and the reported output
trimmed to meet exactly at the midpoint of that overlap. trf re-derives its
own consensus phase in each window, so one continuous biological tandem
array can come out as several adjacent BED records whose consensus strings
are cyclic rotations of one another (e.g. TCCAT / ATTCC / TTCCA are all the
same pentamer, just phased differently).

Merge criteria (deliberately conservative -- see conversation history):
  - same chromosome
  - same period size
  - exact adjacency: record2.start == record1.end (no gap, no overlap)
  - consensus sequences are the same length and match on their best-scoring
    cyclic rotation with identity >= minIdentity (default 0.90)

The identity threshold, not an exact match, matters in practice: two
independently-derived consensus calls for the same real array can differ by
a base or two even when they're unambiguously the same biological repeat --
we found a live example on CM038661.2 where a ~370 bp monomer's consensus
differed by exactly one substitution between two adjacent windows, which an
exact-rotation check would (wrongly) refuse to merge. A short, very clean
period (e.g. a 5 bp pentamer) still effectively requires an exact match at
the default threshold, since one mismatch out of 5 characters already fails
90% identity -- so this only loosens the check where the period is long
enough for that slack to matter.

Records of a different period that happen to be interleaved positionally
(e.g. a small period-24 element nested inside a period-5 array's span) do
not participate in, and do not interrupt, a period-5 merge chain. Records
with a genuinely different declared period are NOT merged even if adjacent
and clearly related (e.g. the 368/369/370/347 cluster seen on CM038661.2) --
that would need variable-length rotation alignment, a larger and riskier
change than the fix here, and is deliberately left alone for now.

Input may be a whole-genome BED file (all sequences concatenated, sorted by
chromosome the way this pipeline's output always is) and may be gzipped --
detected by a ".gz" suffix. Sequences are processed one at a time as the
file streams past, so peak memory is bounded by the largest single sequence's
record count, not the whole file's.

Usage:
    python3 mergeTrf.py input.bed > merged.bed
    python3 mergeTrf.py input.bed.gz > merged.bed
    python3 mergeTrf.py --report input.bed.gz > report.txt
    python3 mergeTrf.py --minIdentity 0.95 input.bed.gz > merged.bed
"""

import sys
import gzip
import argparse
import itertools

FIELDS = [
    "chrom", "start", "end", "name", "period", "copyNumber",
    "consensusSize", "pctMatch", "pctIndel", "score",
    "pctA", "pctC", "pctG", "pctT", "entropy", "consensus",
]


def openMaybeGzip(path):
    if path.endswith(".gz"):
        return gzip.open(path, "rt")
    return open(path)


def parseLine(line):
    raw = line.rstrip("\n")
    f = raw.split("\t")
    if len(f) != len(FIELDS):
        raise ValueError("expected %d fields, got %d: %r" % (len(FIELDS), len(f), raw))
    rec = dict(zip(FIELDS, f))
    rec["raw"] = raw
    rec["start"] = int(rec["start"])
    rec["end"] = int(rec["end"])
    rec["period"] = int(rec["period"])
    rec["copyNumber"] = float(rec["copyNumber"])
    rec["pctMatch"] = float(rec["pctMatch"])
    rec["pctIndel"] = float(rec["pctIndel"])
    rec["score"] = float(rec["score"])
    rec["pctA"] = float(rec["pctA"])
    rec["pctC"] = float(rec["pctC"])
    rec["pctG"] = float(rec["pctG"])
    rec["pctT"] = float(rec["pctT"])
    rec["entropy"] = float(rec["entropy"])
    return rec


def parseRecordStream(path):
    """Yield parsed records from a (possibly gzipped) BED file, one at a
    time, so the caller never has to hold the whole file in memory."""
    with openMaybeGzip(path) as fh:
        for line in fh:
            if line.strip():
                yield parseLine(line)


def rotationIdentity(a, b):
    """Best fraction of matching bases across all cyclic rotations of a
    against b. Caller must ensure len(a) == len(b)."""
    n = len(a)
    doubled = a + a
    bestMatches = 0
    for offset in range(n):
        rotated = doubled[offset:offset + n]
        matches = sum(1 for x, y in zip(rotated, b) if x == y)
        if matches > bestMatches:
            bestMatches = matches
    return bestMatches / n


def canMerge(prev, cur, minIdentity):
    if prev["chrom"] != cur["chrom"]:
        return False
    if prev["period"] != cur["period"]:
        return False
    if prev["end"] != cur["start"]:
        return False
    a, b = prev["consensus"], cur["consensus"]
    if len(a) != len(b):
        return False
    return rotationIdentity(a, b) >= minIdentity


def mergeGroup(group):
    """Combine a list of adjacent, rotation-matched records into one.
    A group of size 1 is passed through byte-for-byte unchanged."""
    if len(group) == 1:
        r = group[0]
        return r["raw"], 1

    totalSpan = sum(r["end"] - r["start"] for r in group)

    def weightedAvg(field):
        return sum(r[field] * (r["end"] - r["start"]) for r in group) / totalSpan

    merged = {
        "chrom": group[0]["chrom"],
        "start": group[0]["start"],
        "end": group[-1]["end"],
        "name": group[0]["name"],
        "period": group[0]["period"],
        "copyNumber": sum(r["copyNumber"] for r in group),
        "consensusSize": group[0]["consensusSize"],
        "pctMatch": round(weightedAvg("pctMatch")),
        "pctIndel": round(weightedAvg("pctIndel")),
        "score": round(sum(r["score"] for r in group)),
        "pctA": round(weightedAvg("pctA")),
        "pctC": round(weightedAvg("pctC")),
        "pctG": round(weightedAvg("pctG")),
        "pctT": round(weightedAvg("pctT")),
        "entropy": round(weightedAvg("entropy"), 2),
        # phase of the first piece is kept as representative; the merged
        # array's true register drifts internally, as we've seen directly.
        "consensus": group[0]["consensus"],
    }
    line = "\t".join([
        merged["chrom"], str(merged["start"]), str(merged["end"]), merged["name"],
        str(merged["period"]), "%.1f" % merged["copyNumber"], str(merged["consensusSize"]),
        str(merged["pctMatch"]), str(merged["pctIndel"]), str(merged["score"]),
        str(merged["pctA"]), str(merged["pctC"]), str(merged["pctG"]), str(merged["pctT"]),
        "%.2f" % merged["entropy"], merged["consensus"],
    ])
    return line, len(group)


def mergeOneChrom(records, minIdentity):
    """records: parsed dicts for a SINGLE chromosome, in file order (assumed
    sorted by start, as simpleRepeat.bed naturally is)."""
    openChains = {}   # period -> list of records in the current chain
    results = []      # list of (start, line, nMerged)

    def close(period):
        chain = openChains.pop(period)
        line, n = mergeGroup(chain)
        results.append((chain[0]["start"], line, n))

    for rec in records:
        period = rec["period"]
        chain = openChains.get(period)
        if chain is not None and canMerge(chain[-1], rec, minIdentity):
            chain.append(rec)
        else:
            if chain is not None:
                close(period)
            openChains[period] = [rec]

    for period in list(openChains.keys()):
        close(period)

    results.sort(key=lambda t: t[0])
    return results


def mergeStreamByChrom(recordStream, minIdentity):
    """Group the incoming record stream by chromosome (assumes the file is
    chromosome-sorted, as this pipeline's BED output always is) and merge
    each chromosome's records as soon as its group ends -- so only one
    chromosome's worth of records is ever held in memory at a time.

    Yields (chrom, mergedResultsForThatChrom, chromRecordCount) per group.
    Warns on stderr if the same chromosome name reappears in a later,
    non-adjacent group, which would mean the input wasn't actually sorted.
    """
    seenChroms = set()
    for chrom, group in itertools.groupby(recordStream, key=lambda r: r["chrom"]):
        if chrom in seenChroms:
            print("WARNING: %s reappears non-contiguously -- input does not "
                  "appear to be sorted by chromosome; merges for this "
                  "chromosome may be incomplete." % chrom, file=sys.stderr)
        seenChroms.add(chrom)
        chromRecords = list(group)
        chromInputBp = sum(r["end"] - r["start"] for r in chromRecords)
        merged = mergeOneChrom(chromRecords, minIdentity)
        yield chrom, merged, len(chromRecords), chromInputBp


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bedFile", help="BED file, plain or gzipped (.gz); may contain "
                                    "multiple sequences, sorted by chromosome")
    ap.add_argument("--report", action="store_true",
                     help="print a human-readable merge summary instead of BED output")
    ap.add_argument("--minIdentity", type=float, default=0.90,
                     help="minimum best-rotation identity fraction required to merge "
                          "two adjacent same-period records (default 0.90)")
    args = ap.parse_args()

    totalInputRecords = 0
    totalOutputRecords = 0
    inputTotalBp = 0
    outputTotalBp = 0
    nMergedGroups = 0
    nRecordsAbsorbed = 0

    recordStream = parseRecordStream(args.bedFile)
    for chrom, merged, chromRecordCount, chromInputBp in mergeStreamByChrom(recordStream, args.minIdentity):
        totalInputRecords += chromRecordCount
        totalOutputRecords += len(merged)
        inputTotalBp += chromInputBp

        for start, line, n in merged:
            fields = line.split("\t")
            outputTotalBp += int(fields[2]) - int(fields[1])

            if args.report:
                if n > 1:
                    nMergedGroups += 1
                    nRecordsAbsorbed += n
                    print("MERGED %d records -> %s:%s-%s  period=%s copies=%s score=%s consensus=%s" % (
                        n, fields[0], fields[1], fields[2], fields[4], fields[5], fields[9], fields[15]))
            else:
                print(line)

    if args.report:
        print()
        print("input records:        %d" % totalInputRecords)
        print("output records:       %d" % totalOutputRecords)
        print("merged groups (>1):   %d  (absorbing %d original records)" % (nMergedGroups, nRecordsAbsorbed))
        print("input total bp:       %d" % inputTotalBp)
        print("output total bp:      %d  (should equal input total bp exactly)" % outputTotalBp)


if __name__ == "__main__":
    main()
