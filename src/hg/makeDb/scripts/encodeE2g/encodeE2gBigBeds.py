#!/usr/bin/env python3
# Build the two bigBeds per annotation from the ENCODE downloads, reading
# samples.tsv in the current directory:
#   bb/<ENCSR>.interact.bb   from the thresholded bigInteract
#   bb/<ENCSR>.elements.bb   from the thresholded bed, bed9+15, encodeE2gElements.as
#
# The bigInteract is rebuilt rather than used as-is for two reasons.  Its item
# name is "gene/element/celltype", too long for a label, so the name becomes
# just the gene.  And the upstream R code wrote some element coordinates in
# scientific notation ("chr15:7.8e+07-78000500"), in the bigInteract sourceName
# as well as in the bed name column; both are rebuilt from the coordinates.
# The lab's files all carry the same color, #C5204C, so both bigBeds are
# recolored by biosample classification, with the facet swatch colors from
# encodeE2g_colors.json.  Run encodeE2gTrackDb.py first, it writes that file.
import csv, json, os, subprocess, sys, tempfile
from multiprocessing import Pool

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
INTERACT_AS = os.path.join(SCRIPT_DIR, "../../../lib/interact.as")
ELEMENTS_AS = os.path.join(SCRIPT_DIR, "encodeE2gElements.as")
CHROM_SIZES = "/hive/data/genomes/hg38/chrom.sizes"
BED_HEADER = ["#chr", "start", "end", "name", "class", "TargetGene",
              "TargetGeneEnsemblID", "TargetGeneTSS", "isSelfPromoter", "CellType",
              "distanceToTSS.Feature", "normalizedDNase_prom.Feature",
              "3DContact.Feature", "ABC.Score.Feature", "numCandidateEnhGene.Feature",
              "numTSSEnhGene.Feature", "sumNearbyEnhancers.Feature",
              "ubiquitousExpressedGene.Feature", "Score"]


def toBigBed(rows, asFile, bedType, out):
    with tempfile.NamedTemporaryFile("w", dir="bb", suffix=".bed", delete=False) as tmp:
        for r in rows:
            tmp.write("\t".join(r) + "\n")
    subprocess.run("LC_ALL=C sort -k1,1 -k2,2n -o %s %s" % (tmp.name, tmp.name),
                   shell=True, check=True)
    subprocess.run(["bedToBigBed", "-tab", "-type=" + bedType, "-as=" + asFile,
                    tmp.name, CHROM_SIZES, out + ".tmp"], check=True,
                   stdout=subprocess.DEVNULL)
    os.rename(out + ".tmp", out)
    os.unlink(tmp.name)


def hexToRgb(color):
    c = color.lstrip("#")
    return "%d,%d,%d" % (int(c[0:2], 16), int(c[2:4], 16), int(c[4:6], 16))


def build(s):
    acc = s["accession"]
    interOut = "bb/%s.interact.bb" % acc
    elemOut = "bb/%s.elements.bb" % acc
    if os.path.exists(interOut) and os.path.exists(elemOut):
        return acc, None
    src = "files/bigInteract/%s.bigInteract" % s["bigInteract"]
    lines = subprocess.run(["bigBedToBed", src, "stdout"], check=True,
                           capture_output=True, text=True).stdout.splitlines()
    color = s["color"]
    rgb = hexToRgb(color)
    rows = []
    for line in lines:
        r = line.split("\t")
        r[11] = "%s:%s-%s" % (r[8], r[9], r[10])   # sourceName
        r[3] = r[16]                               # name = targetName (gene)
        r[7] = color
        rows.append(r)
    toBigBed(rows, INTERACT_AS, "bed5+13", interOut)

    proc = subprocess.run(["zcat", "files/bed/%s.bed.gz" % s["bed"]], check=True,
                          capture_output=True, text=True)
    rows = []
    for i, line in enumerate(proc.stdout.splitlines()):
        r = line.split("\t")
        if i == 0:
            if r != BED_HEADER:
                return acc, "unexpected bed header in %s" % s["bed"]
            continue
        chrom, start, end = r[0], r[1], r[2]
        score = str(min(1000, int(round(float(r[18]) * 1000))))
        rows.append([chrom, start, end, r[5], score, ".", start, end, rgb,
                     "%s:%s-%s" % (chrom, start, end), r[4], r[5], r[6], r[7], r[8]]
                    + r[10:19])
    toBigBed(rows, ELEMENTS_AS, "bed9+15", elemOut)
    return acc, None


def main():
    " args: [samplesFile [nproc]] "
    os.makedirs("bb", exist_ok=True)
    samplesFile = sys.argv[1] if len(sys.argv) > 1 else "samples.tsv"
    nproc = int(sys.argv[2]) if len(sys.argv) > 2 else 12
    samples = list(csv.DictReader(open(samplesFile), delimiter="\t"))
    classColors = json.load(open("encodeE2g_colors.json"))["Classification"]
    for s in samples:
        s["color"] = classColors[s["classification"]]
    errors = 0
    with Pool(nproc) as p:
        for acc, err in p.imap_unordered(build, samples):
            if err:
                print("ERROR", acc, err)
                errors += 1
    print("done, %d samples, %d errors" % (len(samples), errors))
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
