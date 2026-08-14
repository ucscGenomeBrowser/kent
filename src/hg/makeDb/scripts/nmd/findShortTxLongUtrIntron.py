#!/usr/bin/env python3
"""
Find hg38 MANE transcripts that are relatively short (small mRNA) but have
a long intron inside the UTR (5' or 3'). Useful for eyeballing NMD Rule 1
edge cases where a 3'UTR intron sits far from the stop codon.

Reads the MANE bigGenePred from /gbdb/hg38/mane/mane.bb via bigBedToBed.

Usage:
  findShortTxLongUtrIntron.py [--max-mrna 3000] [--min-utr-intron 5000] [--limit 50]

Output (TSV, sorted by UTR intron length desc):
  name  geneSym  chrom  strand  txStart  txEnd  mrnaLen  utr3IntronLen  utr5IntronLen  cdsLen
"""
import argparse, subprocess, sys

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bb", default="/gbdb/hg38/mane/mane.bb")
    ap.add_argument("--max-mrna", type=int, default=3000,
                    help="only report transcripts whose spliced mRNA is <= this (default 3000)")
    ap.add_argument("--min-utr-intron", type=int, default=5000,
                    help="require at least one UTR intron >= this (default 5000)")
    ap.add_argument("--limit", type=int, default=50)
    args = ap.parse_args()

    proc = subprocess.run(["bigBedToBed", args.bb, "stdout"],
                          capture_output=True, text=True, check=True)

    rows = []
    for line in proc.stdout.splitlines():
        f = line.split("\t")
        # bigGenePred: chrom,chromStart,chromEnd,name,score,strand,thickStart,thickEnd,
        # reserved,blockCount,blockSizes,chromStarts,name2,cdsStartStat,cdsEndStat,
        # exonFrames,type,geneName,geneName2,geneType
        if len(f) < 20:
            continue
        chrom = f[0]
        txStart, txEnd = int(f[1]), int(f[2])
        name = f[3]
        strand = f[5]
        cdsStart, cdsEnd = int(f[6]), int(f[7])
        geneSym = f[18] if len(f) > 18 else ""
        if cdsStart == cdsEnd:
            continue
        blockSizes = [int(x) for x in f[10].rstrip(",").split(",") if x]
        blockStarts = [int(x) for x in f[11].rstrip(",").split(",") if x]
        starts = [txStart + b for b in blockStarts]
        ends = [s + sz for s, sz in zip(starts, blockSizes)]
        if len(starts) < 2:
            continue

        mrnaLen = sum(e - s for s, e in zip(starts, ends))
        if mrnaLen > args.max_mrna:
            continue
        cdsLen = 0
        for s, e in zip(starts, ends):
            s2, e2 = max(s, cdsStart), min(e, cdsEnd)
            if e2 > s2:
                cdsLen += e2 - s2

        # introns = gaps between consecutive exons
        utr5Max = 0
        utr3Max = 0
        for i in range(len(starts) - 1):
            iStart, iEnd = ends[i], starts[i + 1]
            iLen = iEnd - iStart
            # classify: entirely below cdsStart => 5' side genomic
            # entirely above cdsEnd => 3' side genomic
            if iEnd <= cdsStart:
                side = "gLow"
            elif iStart >= cdsEnd:
                side = "gHigh"
            else:
                continue  # CDS intron
            # map genomic side -> UTR side by strand
            if strand == "+":
                which = "utr5" if side == "gLow" else "utr3"
            else:
                which = "utr3" if side == "gLow" else "utr5"
            if which == "utr5" and iLen > utr5Max:
                utr5Max = iLen
            elif which == "utr3" and iLen > utr3Max:
                utr3Max = iLen

        if max(utr5Max, utr3Max) < args.min_utr_intron:
            continue
        rows.append((name, geneSym, chrom, strand, txStart, txEnd, mrnaLen,
                     utr3Max, utr5Max, cdsLen))

    rows.sort(key=lambda r: max(r[7], r[8]), reverse=True)
    print("\t".join(["name","geneSym","chrom","strand","txStart","txEnd",
                     "mrnaLen","utr3IntronLen","utr5IntronLen","cdsLen"]))
    for r in rows[:args.limit]:
        print("\t".join(str(x) for x in r))

if __name__ == "__main__":
    main()
