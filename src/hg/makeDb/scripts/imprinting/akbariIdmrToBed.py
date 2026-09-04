#!/usr/bin/env python3
"""
Convert the imprinted DMR list of Akbari et al. 2023 (Cell Genomics, table S3,
also shipped with the PatMat tool as Imprinted_DMR_List_V1.GRCh38.tsv) into a
bed9+ file.

The input coordinates are 1-based inclusive, not BED. PatMat itself reads the
file with "start = int(field) - 1" before handing the interval to tabix, so the
start is shifted by one here to produce half-open BED coordinates.

Usage:
    akbariIdmrToBed.py <tableS3.tsv> <chrom.sizes> <out.bed> <out.report.txt>
"""
import sys, re
from collections import defaultdict

# One color scheme is shared by the whole Imprinting collection: vermillion
# means the maternal copy, blue means the paternal copy. Here the annotation
# names the METHYLATED copy, not the expressed one; each subtrack's description
# page says which copy its colors refer to. Okabe-Ito palette.
ALLELE_COLOR = {
    "Maternal": "213,94,0",   # vermillion
    "Paternal": "0,114,178",  # blue
}

EXPECT_HEADER = ["Chromosome", "Start", "End", "Methylated allele", "Name"]


def cleanStudies(raw):
    """ 'Akbari(32),Joshi(30),Zink(6),Akbari(32),Zink(6)' ->
        (['Akbari', 'Joshi', 'Zink'], 3). The reference numbers are the ones
        used in the paper's bibliography and mean nothing here; merged regions
        sometimes repeat a study name. """
    names = []
    for item in raw.split(","):
        name = re.sub(r"\(.*?\)", "", item).strip()
        if name and name not in names:
            names.append(name)
    return sorted(names), len(names)


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    inFname, sizesFname, outFname, reportFname = sys.argv[1:5]

    chromSizes = {}
    for line in open(sizesFname):
        f = line.split()
        chromSizes[f[0]] = int(f[1])

    fh = open(inFname)
    header = fh.readline().rstrip("\n").split("\t")
    if header[:5] != EXPECT_HEADER:
        sys.exit("error: unexpected header in %s:\n  %s" % (inFname, header))

    beds, skipped = [], []
    studyCounts, alleleCounts, evidenceCounts = defaultdict(int), defaultdict(int), defaultdict(int)
    naRows = 0
    rowCount = 0

    for line in fh:
        f = line.rstrip("\n").split("\t")
        if len(f) < 12 or not f[0]:
            continue
        rowCount += 1
        chrom, start1, end = f[0], int(f[1]), int(f[2])
        allele, name, studiesRaw = f[3], f[4], f[5]
        start = start1 - 1                      # 1-based inclusive -> BED

        if chrom not in chromSizes:
            skipped.append((name, chrom, start1, end, "chromosome not in this assembly"))
            continue
        if start < 0 or end > chromSizes[chrom] or start >= end:
            skipped.append((name, chrom, start1, end,
                            "interval outside %s (length %d)" % (chrom, chromSizes[chrom])))
            continue
        if allele not in ALLELE_COLOR:
            skipped.append((name, chrom, start1, end,
                            "methylated allele is '%s', expected Maternal or Paternal" % allele))
            continue

        studies, studyCount = cleanStudies(studiesRaw)
        evidence = "Multiple studies" if studyCount > 1 else "Single study"

        # the six validation columns are NA for regions that did not need the
        # WGBS check; a bigBed cannot hold an empty number, so they are strings
        stats = []
        for value in f[6:12]:
            stats.append("" if value.strip() in ("NA", "") else value.strip())
        if stats[0] == "":
            naRows += 1

        genes = [g for g in re.split(r"[,;]", name) if g]
        beds.append([
            chrom, start, end, ", ".join(genes), 0, ".", start, end,
            ALLELE_COLOR[allele],
            allele, genes[0], ",".join(studies), str(studyCount), evidence,
        ] + stats)

        alleleCounts[allele] += 1
        evidenceCounts[evidence] += 1
        for study in studies:
            studyCounts[study] += 1

    beds.sort(key=lambda b: (b[0], b[1], b[2]))
    with open(outFname, "w") as out:
        for b in beds:
            out.write("\t".join(str(x) for x in b) + "\n")

    with open(reportFname, "w") as rep:
        def say(s=""):
            rep.write(s + "\n")
            print(s)

        say("Akbari et al. 2023 imprinted DMR list to hg38 bed")
        say("rows in the input table:   %d" % rowCount)
        say("features written:          %d" % len(beds))
        say("rows skipped:              %d" % len(skipped))
        say("coordinates shifted from 1-based inclusive to 0-based half-open")
        say()
        say("regions by methylated allele:")
        for k in sorted(alleleCounts, key=lambda k: -alleleCounts[k]):
            say("    %-18s %d" % (k, alleleCounts[k]))
        say("regions by evidence:")
        for k in sorted(evidenceCounts, key=lambda k: -evidenceCounts[k]):
            say("    %-18s %d" % (k, evidenceCounts[k]))
        say("regions reported by each source study (regions can have several):")
        for k in sorted(studyCounts, key=lambda k: -studyCounts[k]):
            say("    %-18s %d" % (k, studyCounts[k]))
        say()
        say("regions without the WGBS validation counts: %d" % naRows)
        lengths = sorted(b[2] - b[1] for b in beds)
        if lengths:
            say("region length: min %d, median %d, max %d, total %d bp"
                % (lengths[0], lengths[len(lengths) // 2], lengths[-1], sum(lengths)))
        say()
        say("skipped rows (%d):" % len(skipped))
        for name, chrom, start1, end, why in skipped:
            say("    %-20s %s:%d-%d  %s" % (name, chrom, start1, end, why))


main()
