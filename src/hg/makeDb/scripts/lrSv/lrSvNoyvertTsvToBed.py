#!/usr/bin/env python3
"""Convert the Noyvert et al. 2025 multi-ancestry long-read SV panel TSV to BED9+.

Usage:
    lrSvNoyvertTsvToBed.py input.tsv.gz output.bed [chrom.sizes]

Input is the imputation-panel table shared by the authors: 107,445 SVs called
with Sniffles2 v2.0.7 (joint multi-sample calling) from 888 Oxford Nanopore
genomes of five 1000 Genomes superpopulations. Each row carries per-population
allele frequencies, Sniffles2 stdev metrics, Hardy-Weinberg p-values, internal
(leave-one-out) and UK Biobank imputation accuracy, and, where applicable, the
significant UK Biobank SV-WAS trait associations for that variant.

The file has no AC/AN columns, only allele frequencies, so AC and AN are
approximated from AF and the genotype missing rate:
    AN = round(2 * 888 * (1 - genotype_missing_rate))
    AC = round(AF * AN)
Both are stored as approximate values (documented in trackDb / the track doc).

Coordinates are hg38, VCF-style 1-based. chromStart = pos - 1. For DEL/INV/DUP
chromEnd = chromStart + abs(SVLEN) (the reference span). INS and BND are drawn
as a single reference base (chromEnd = chromStart + 1); the inserted-sequence
length lives in insLen for INS, and the BND mate breakend is stored in secondBp.
"""

import gzip
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType, svColor

N_SAMPLES = 888  # samples passing QC (Noyvert et al. 2025)


def toInt(s, default=0):
    if s is None or s == "" or s in (".", "NA"):
        return default
    try:
        return int(round(float(s)))
    except ValueError:
        return default


def toFloat(s, default=0.0):
    if s is None or s == "" or s in (".", "NA"):
        return default
    try:
        return float(s)
    except ValueError:
        return default


def cleanBreakend(s):
    """Lower-case the Sniffles2 'CHR' prefix in a breakend string so the mate
    locus reads as a UCSC-style chrom. e.g. ']CHR21:14056216]N' -> ']chr21:14056216]N'.
    Alt/random/Un contigs keep their upstream casing (display only)."""
    if not s:
        return ""
    return re.sub(r"CHR", "chr", s)


def countGwas(s):
    """Number of significant UK Biobank trait associations in the SV-WAS field.
    Entries are ';'-separated 'Trait:info=..,log10p=..' records."""
    if not s:
        return 0
    return sum(1 for part in s.split(";") if part.strip())


def formatGwas(s):
    """Reformat 'Trait:info=0.78,log10p=8.1;...' into a readable, comma-free
    display string: 'Trait (info 0.78, log10p 8.1); ...'. bigBed detail pages
    split on commas, so commas inside the field are replaced."""
    if not s:
        return ""
    out = []
    for part in s.split(";"):
        part = part.strip()
        if not part:
            continue
        m = re.match(r"^(.*?):info=([^,]+),log10p=(.+)$", part)
        if m:
            trait, info, log10p = m.group(1), m.group(2), m.group(3)
            out.append(f"{trait} (info {info}, log10p {log10p})")
        else:
            out.append(part.replace(",", " "))
    return "; ".join(out)


def main():
    if len(sys.argv) not in (3, 4):
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inFile, outFile = sys.argv[1], sys.argv[2]
    chromSizes = {}
    if len(sys.argv) == 4:
        with open(sys.argv[3]) as fs:
            for line in fs:
                c, n = line.split()[:2]
                chromSizes[c] = int(n)

    opener = gzip.open if inFile.endswith(".gz") else open

    nWritten = 0
    nOverEnd = 0
    byType = {}
    opener_fn = opener(inFile, "rt")
    with opener_fn as fIn, open(outFile, "w") as fOut:
        header = fIn.readline()  # skip header line
        for line in fIn:
            f = line.rstrip("\n").split("\t")
            chrom = f[0]
            pos = int(f[1])
            svType = normalizeSvType(f[3])
            svlenRaw = toInt(f[4]) if f[4] != "" else 0
            secondBp = cleanBreakend(f[7])

            chromStart = pos - 1
            if svType in ("DEL", "INV", "DUP"):
                chromEnd = chromStart + abs(svlenRaw)
                insLen = 0
            elif svType == "INS":
                chromEnd = chromStart + 1
                insLen = abs(svlenRaw)
            else:  # BND (and any other breakend-like type)
                chromEnd = chromStart + 1
                insLen = 0

            if chromEnd <= chromStart:
                chromEnd = chromStart + 1
            # svLen is the reference span (chromEnd - chromStart), matching the
            # shared lrSv convention: 1 bp for INS/BND, the interval for others.
            svLen = chromEnd - chromStart

            # Clamp to the chromosome end if a large call runs past it.
            if chrom in chromSizes and chromEnd > chromSizes[chrom]:
                over = chromEnd - chromSizes[chrom]
                nOverEnd += 1
                print(f"WARNING: {chrom}:{pos} {svType} end {chromEnd} "
                      f"exceeds chrom size {chromSizes[chrom]} by {over}, clamping",
                      file=sys.stderr)
                chromEnd = chromSizes[chrom]
                svLen = chromEnd - chromStart

            missingRate = toFloat(f[8])
            af = toFloat(f[9])
            an = int(round(2 * N_SAMPLES * (1 - missingRate)))
            ac = int(round(af * an))

            afAfr = toFloat(f[10])
            afAmr = toFloat(f[11])
            afEas = toFloat(f[12])
            afEur = toFloat(f[13])
            afSas = toFloat(f[14])

            hweParts = []
            for popName, idx in (("AFR", 15), ("AMR", 16), ("EAS", 17),
                                 ("EUR", 18), ("SAS", 19)):
                v = f[idx] if idx < len(f) else ""
                if v not in ("", ".", "NA"):
                    hweParts.append(f"{popName} {v}")
            hwe = "; ".join(hweParts)

            concordanceLoo = toFloat(f[20])
            r2Loo = toFloat(f[21])
            afUkb = toFloat(f[22])
            r2Ukb = toFloat(f[23])
            gwasRaw = f[24] if len(f) > 24 else ""
            nGwas = countGwas(gwasRaw)
            ukbGwas = formatGwas(gwasRaw)

            color = svColor(svType)
            featLen = insLen if svType == "INS" else svLen
            name = svName(svType, featLen, ac)

            row = [
                chrom, str(chromStart), str(chromEnd), name, "0", ".",
                str(chromStart), str(chromEnd), color,
                svType, str(svLen), str(insLen), str(ac), str(an),
                f"{af:.4f}", f"{afAfr:.4f}", f"{afAmr:.4f}", f"{afEas:.4f}",
                f"{afEur:.4f}", f"{afSas:.4f}",
                secondBp,
                f"{toFloat(f[5]):.3f}", f"{toFloat(f[6]):.3f}",
                f"{missingRate:.4f}", hwe,
                f"{r2Loo:.4f}", f"{concordanceLoo:.4f}",
                f"{afUkb:.4f}", f"{r2Ukb:.4f}",
                str(nGwas), ukbGwas,
            ]
            fOut.write("\t".join(row) + "\n")
            nWritten += 1
            byType[svType] = byType.get(svType, 0) + 1

    typeStr = ", ".join(f"{k}={v:,}" for k, v in sorted(byType.items()))
    print(f"Noyvert: wrote {nWritten:,} records to {outFile}", file=sys.stderr)
    print(f"Noyvert: by type: {typeStr}", file=sys.stderr)
    if nOverEnd:
        print(f"Noyvert: {nOverEnd} record(s) clamped to the chromosome end",
              file=sys.stderr)


if __name__ == "__main__":
    main()
