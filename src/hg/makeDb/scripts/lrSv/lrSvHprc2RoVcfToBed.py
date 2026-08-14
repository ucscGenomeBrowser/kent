#!/usr/bin/env python3
"""Convert the HPRC release-2.1 minigraph-cactus pangenome VCF to BED9+.

This is the v2.1 "gref95.ro" callset (Glenn Hickey, HPRC). Unlike the v2.0
"wave" VCF, this file is the raw `vg deconstruct` output: it carries graph
traversals (AT), nested snarls (LV) and a parent-snarl pointer (PS), and it
has NO per-allele TYPE or LEN fields. We therefore classify each ALT by
parsimony-trimming the shared prefix/suffix of REF/ALT and looking at the
net length change:

    trimmed ref empty           -> INS   (pure insertion)
    trimmed alt empty           -> DEL   (pure deletion)
    both non-empty, alt longer  -> INS   (net insertion, ragged ends)
    both non-empty, ref longer  -> DEL   (net deletion, ragged ends)
    both non-empty, equal length-> CPX   (balanced substitution / MNP)

An allele is kept when abs(len(ALT) - len(REF)) >= 50 bp, matching the
length threshold used for the v2.0 track. AC / AF / AN / NS / LV are taken
from the INFO fields. There is no INV annotation in this file, so no
inversions are emitted (balanced events fall into CPX).

Usage:
    lrSvHprc2RoVcfToBed.py input.vcf.gz output.bed

Source:
    https://humanpangenome.org/hprc-data-release-2/
    hprc-v2.1-mc-grch38.gref95.ro.vcf.gz (233-sample minigraph-cactus graph).
"""

import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, svColor

MIN_SV_LEN = 50


def parseInfo(infoStr):
    d = {}
    for item in infoStr.split(";"):
        if "=" in item:
            k, v = item.split("=", 1)
            d[k] = v
        else:
            d[item] = True
    return d


def splitMulti(s, n):
    """Split a per-allele INFO value; pad with '' if missing."""
    if s is None:
        return [""] * n
    parts = s.split(",")
    if len(parts) < n:
        parts = parts + [""] * (n - len(parts))
    return parts


def classify(ref, alt):
    """Parsimony-trim REF/ALT and classify; return (svType, refSpan).

    refSpan is the number of trimmed reference bases the event spans, used
    to position chromEnd. The original chromStart is shifted by the trimmed
    prefix length so the feature sits on the actual changed bases.
    """
    # trim shared prefix
    p = 0
    while p < len(ref) and p < len(alt) and ref[p] == alt[p]:
        p += 1
    r = ref[p:]
    a = alt[p:]
    # trim shared suffix
    s = 0
    while s < len(r) and s < len(a) and r[-1 - s] == a[-1 - s]:
        s += 1
    if s:
        r = r[:len(r) - s]
        a = a[:len(a) - s]
    lr, la = len(r), len(a)
    if la > lr:
        svType = "INS"
    elif lr > la:
        svType = "DEL"
    else:
        svType = "CPX"
    return svType, p, lr


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inPath, outPath = sys.argv[1], sys.argv[2]
    opener = gzip.open if inPath.endswith(".gz") else open

    counts = {"INS": 0, "DEL": 0, "CPX": 0}
    nested = 0
    seen = set()
    nDup = 0

    with opener(inPath, "rt") as fIn, open(outPath, "w") as fOut:
        for line in fIn:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[1])
            rowId = fields[2]
            ref = fields[3]
            alts = fields[4].split(",")
            info = parseInfo(fields[7])

            n = len(alts)
            acList = splitMulti(info.get("AC"), n)
            afList = splitMulti(info.get("AF"), n)

            try:
                nSamples = int(info.get("NS", "0"))
            except ValueError:
                nSamples = 0
            try:
                an = int(info.get("AN", "0"))
            except ValueError:
                an = 0
            try:
                snarlLevel = int(info.get("LV", "0"))
            except ValueError:
                snarlLevel = 0

            for i, alt in enumerate(alts):
                if alt in (".", "*"):
                    continue

                svLenVal = abs(len(alt) - len(ref))
                # Filter: keep SV-sized alleles by net length change.
                if svLenVal < MIN_SV_LEN:
                    continue

                svType, prefixLen, refSpan = classify(ref, alt)
                color = svColor(svType)

                # chromStart shifts past the shared prefix to the first
                # changed base; INS gets a 1-bp anchor, DEL/CPX span the
                # trimmed reference interval.
                chromStart = (pos - 1) + prefixLen
                if svType == "INS":
                    chromEnd = chromStart + 1
                else:
                    chromEnd = chromStart + max(1, refSpan)
                if chromEnd <= chromStart:
                    chromEnd = chromStart + 1

                try:
                    ac = int(acList[i])
                except ValueError:
                    ac = 0
                try:
                    af = float(afList[i])
                except ValueError:
                    af = 0.0

                svLen = chromEnd - chromStart
                if svType == "INS":
                    insLen = abs(len(alt) - len(ref))
                else:
                    insLen = 0

                if rowId and rowId != ".":
                    baseId = f"{rowId}.{i + 1}" if n > 1 else rowId
                else:
                    baseId = f"{chrom}:{pos}:{svType}:{svLenVal}"
                if len(baseId) > 200:
                    baseId = f"{chrom}:{pos}:{svType}:{svLenVal}"

                featLen = insLen if svType == "INS" else svLen
                name = svName(svType, featLen, ac)

                row = [
                    chrom,
                    str(chromStart),
                    str(chromEnd),
                    name,
                    "0",
                    ".",
                    str(chromStart),
                    str(chromEnd),
                    color,
                    svType,
                    str(svLen),
                    str(insLen),
                    str(ac),
                    str(an),
                    f"{af:.6f}",
                    str(nSamples),
                    str(snarlLevel),
                ]
                # The raw vg deconstruct output decomposes some snarls into
                # byte-identical SV rows; collapse them.
                line_out = "\t".join(row)
                if line_out in seen:
                    nDup += 1
                    continue
                seen.add(line_out)

                counts[svType] += 1
                if snarlLevel > 0:
                    nested += 1
                fOut.write(line_out + "\n")

    total = sum(counts.values())
    print(f"kept {total} SV-sized alleles: "
          f"{counts['INS']} INS, {counts['DEL']} DEL, {counts['CPX']} CPX "
          f"({nested} at nested snarl levels LV>0); "
          f"{nDup} duplicate rows dropped", file=sys.stderr)


if __name__ == "__main__":
    main()
