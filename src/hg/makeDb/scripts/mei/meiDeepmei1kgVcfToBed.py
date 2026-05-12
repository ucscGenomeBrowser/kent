#!/usr/bin/env python3
"""Convert DeepMEI 1000 Genomes high-coverage MEI VCF to a bed9+ file.

Input: merge_1000g.latested.vcf.gz from the DeepMEI GitHub release
(https://github.com/xuxif/DeepMEI/raw/refs/heads/main/DeepMEI/1000g_high_callset/),
which contains 91,617 polymorphic Mobile Element Insertions identified
in 3,202 high-coverage 1000 Genomes samples (NYGC) by DeepMEI. Each
record uses a symbolic ALT (<INS:ME:ALU>, <INS:ME:LINE1>, <INS:ME:SVA>);
no inserted sequence or insertion length is reported. INFO supplies
SVTYPE, AF, AN and AC. FORMAT is GT:BPS:BPE:TL:CL:CR:ME per sample.

Insertions are drawn as a 1bp anchor block, matching the lrSv / HGSVC3
MEI tracks:
  chromStart = POS - 1   (1-based VCF anchor -> 0-based)
  chromEnd   = chromStart + 1
"""

import gzip
import sys


COLOR_BY_CLASS = {
    "Alu": "0,114,178",     # blue (Okabe-Ito)
    "L1":  "213,94,0",      # vermillion (Okabe-Ito)
    "SVA": "0,158,115",     # bluish green (Okabe-Ito)
}

ALT_TO_CLASS = {
    "<INS:ME:ALU>":   "Alu",
    "<INS:ME:LINE1>": "L1",
    "<INS:ME:SVA>":   "SVA",
}


def parseInfo(infoField):
    fields = {}
    for tok in infoField.split(";"):
        if "=" in tok:
            k, v = tok.split("=", 1)
            fields[k] = v
        else:
            fields[tok] = True
    return fields


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: meiDeepmei1kgVcfToBed.py input.vcf.gz chrom.sizes output.bed")

    inFn, sizesFn, outFn = sys.argv[1:]
    chromSizes = {}
    with open(sizesFn) as f:
        for line in f:
            chrom, size = line.rstrip("\n").split("\t")
            chromSizes[chrom] = int(size)

    nIn = 0
    nOut = 0
    nSkippedChrom = 0
    nSkippedBoundary = 0
    nSkippedAlt = 0

    opener = gzip.open if inFn.endswith(".gz") else open
    out = open(outFn, "w")

    sampleNames = None
    with opener(inFn, "rt") as f:
        for line in f:
            if line.startswith("##"):
                continue
            if line.startswith("#CHROM"):
                header = line.rstrip("\n").split("\t")
                sampleNames = header[9:]
                continue

            nIn += 1
            row = line.rstrip("\n").split("\t")
            chrom = row[0]
            pos = int(row[1])
            alt = row[4]
            info = parseInfo(row[7])
            sampleGts = row[9:]

            cls = ALT_TO_CLASS.get(alt)
            if cls is None:
                nSkippedAlt += 1
                sys.stderr.write(f"WARN: skipping unknown ALT {alt} at {chrom}:{pos}\n")
                continue

            chromStart = pos - 1
            chromEnd = chromStart + 1

            if chrom not in chromSizes:
                nSkippedChrom += 1
                sys.stderr.write(f"WARN: skipping {chrom}:{pos}, chrom not in chrom.sizes\n")
                continue
            if chromEnd > chromSizes[chrom]:
                nSkippedBoundary += 1
                sys.stderr.write(
                    f"WARN: skipping {chrom}:{pos}, end {chromEnd} > chromSize {chromSizes[chrom]}\n")
                continue

            altAC = 0
            AN = 0
            carrierCount = 0
            sampleCount = 0
            carrierSamples = []
            for sampleName, gtField in zip(sampleNames, sampleGts):
                gt = gtField.split(":", 1)[0]
                hapAlt = 0
                hapCalled = 0
                for h in gt.replace("|", "/").split("/"):
                    if h == ".":
                        continue
                    AN += 1
                    hapCalled += 1
                    if h == "1":
                        altAC += 1
                        hapAlt += 1
                if hapCalled > 0:
                    sampleCount += 1
                if hapAlt > 0:
                    carrierCount += 1
                    carrierSamples.append(sampleName)

            altAF = (altAC / AN) if AN > 0 else 0.0
            score = max(0, min(1000, int(round(altAF * 1000))))
            color = COLOR_BY_CLASS[cls]

            name = f"INS-{cls}-{carrierCount}"

            out.write("\t".join([
                chrom,
                str(chromStart),
                str(chromEnd),
                name,
                str(score),
                ".",
                str(chromStart),
                str(chromEnd),
                color,
                cls,
                str(altAC),
                str(AN),
                f"{altAF:.6f}",
                str(carrierCount),
                str(sampleCount),
                ",".join(carrierSamples),
            ]) + "\n")
            nOut += 1

    out.close()
    sys.stderr.write(
        f"Read {nIn} records, wrote {nOut}, "
        f"skipped {nSkippedAlt} (unknown ALT) + {nSkippedChrom} (chrom missing) + "
        f"{nSkippedBoundary} (off chrom end)\n")


if __name__ == "__main__":
    main()
