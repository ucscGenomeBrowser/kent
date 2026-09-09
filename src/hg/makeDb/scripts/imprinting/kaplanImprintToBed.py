#!/usr/bin/env python3
"""Build hg19 BED files for the four Rosenski et al. 2025 imprinting tracks.

The four region sets come from the authors' own hg19 custom tracks
(https://files.cs.huji.ac.il/tommy/UXM_hg19/bed_tracks/). Those files carry
almost no annotation: parental_ASM is a bare BED3, bimodal+ASM is a bare BED3,
ICRs-new is BED3 plus a name. All of the per-region annotation added here comes
from the supplementary data of the paper, dumped to TSV by
kaplanImprintXlsxToTsv.py. The region sets themselves are used unchanged, so
every output file has exactly as many features as its input bigBed.

Usage: kaplanImprintToBed.py <bedDir> <tsvDir> <outDir>
  bedDir  holds icr.hg19.bed, bimodal.hg19.bed, bimodalAsm.hg19.bed,
          parentalAsm.hg19.bed as produced by bigBedToBed on the source files
  tsvDir  holds the dataS*.tsv files written by kaplanImprintXlsxToTsv.py
  outDir  where the annotated hg19 BED files are written
"""
import sys, os, re, csv, collections

# Okabe-Ito, chosen so the three gamete-of-origin classes stay distinguishable
# under any of the three kinds of colour blindness.
COLOR = {
    "Oocyte gDMR"   : "213,94,0",     # vermillion, maternally methylated
    "Sperm gDMR"    : "0,114,178",    # blue, paternally methylated
    "Secondary DMR" : "0,158,115",    # bluish green, methylation acquired later
    ""              : "0,0,0",        # black, gamete of origin not established
}
BIMODAL_COLOR = "0,114,178"

def bimodalShade(count, maxCount):
    """ a light-to-dark ramp in the one bimodal colour, so that a region shared
    by many cell types stands out from one seen in a single cell type. hgTracks
    only knows how to shade a score in grey, brown or sea green, so the ramp is
    written into the itemRgb field instead of being left to useScore. The
    lightest step still has to be visible on white, hence the 0.35 floor. """
    base = [int(v) for v in BIMODAL_COLOR.split(",")]
    frac = 0.35 + 0.65 * (float(count - 1) / max(1, maxCount - 1))
    return ",".join(str(int(round(255 - frac * (255 - v)))) for v in base)

donorRe = re.compile(r"-Z[0-9A-Z]+$")

def readTsv(fname):
    " read a TSV whose first line is the header, padding short rows "
    rows = list(csv.reader(open(fname), delimiter="\t"))
    header = rows[0]
    width = len(header)
    return [r + [""] * (width - len(r)) for r in rows[1:]]

def readBed(fname):
    for line in open(fname):
        yield line.rstrip("\n").split("\t")

def regionKey(chrom, start, end):
    return "%s:%s-%s" % (chrom, start, end)

def cellTypesOfSamples(sampleStr):
    """ the supplement lists the samples that support a call, as
    <cellType>-<donorId>. Collapse to the distinct cell types, keeping the
    order in which they first appear so the field reads the same way as the
    supplement. """
    seen = []
    for sample in sampleStr.split(","):
        sample = sample.strip()
        if not sample:
            continue
        cellType = donorRe.sub("", sample)
        if cellType not in seen:
            seen.append(cellType)
    return seen

def countSamples(sampleStr):
    return len([s for s in sampleStr.split(",") if s.strip()])

def minPval(pvalStr):
    " the supplement gives one comma-separated p-value per supporting sample "
    vals = []
    for p in pvalStr.split(","):
        p = p.strip()
        if not p:
            continue
        try:
            vals.append(float(p))
        except ValueError:
            pass
    return min(vals) if vals else None

def fmtPval(val):
    return "" if val is None else "%.3g" % val

def fmtMeth(val):
    " gamete methylation is given as a fraction, and is missing for many regions "
    val = val.strip()
    if not val or val == ".":
        return ""
    try:
        return "%.3f" % float(val)
    except ValueError:
        return ""


def loadIcrInfo(tsvDir):
    """ ICR name -> (icrType, associated genes). Data S4 and Data S10 both carry
    the literature annotation of the known ICRs, S10 for a wider set of them. """
    icrType = {}
    icrGenes = collections.defaultdict(list)

    for row in readTsv(os.path.join(tsvDir, "dataS4_parentalAsmSnps.tsv")):
        name, typ, genes = row[9], row[10], row[11]
        if not name:
            continue
        if typ and name not in icrType:
            icrType[name] = typ
        for gene in genes.replace(";", ",").split(","):
            gene = gene.strip()
            if gene and gene not in icrGenes[name]:
                icrGenes[name].append(gene)

    for row in readTsv(os.path.join(tsvDir, "dataS10_imprintedGenes.tsv")):
        gene, name, typ = row[3], row[6], row[7]
        if not name:
            continue
        if typ and typ != "none" and name not in icrType:
            icrType[name] = typ
        if gene and gene not in icrGenes[name]:
            icrGenes[name].append(gene)

    return icrType, icrGenes


def loadGenesByParentalRegion(tsvDir):
    " parental ASM region -> imprinted genes within 200 kb (Data S10) "
    genes = collections.defaultdict(list)
    for row in readTsv(os.path.join(tsvDir, "dataS10_imprintedGenes.tsv")):
        gene, region = row[3], row[4]
        if region and gene and gene not in genes[region]:
            genes[region].append(gene)
    return genes


def loadGameteMeth(tsvDir):
    " parental ASM region -> methylation in ICM, blastocyst, sperm and oocyte "
    meth = {}
    for row in readTsv(os.path.join(tsvDir, "dataS_gameteMeth.tsv")):
        meth[row[3]] = (fmtMeth(row[4]), fmtMeth(row[5]), fmtMeth(row[6]), fmtMeth(row[7]))
    return meth


def groupSnps(tsvDir, fname, regionCol):
    """ collect the ASM SNPs of each region: rs ids, alleles, supporting samples
    and the best adjusted p-value over all SNPs of the region """
    byRegion = collections.defaultdict(lambda: {"snps": [], "alleles": [],
                                                "samples": [], "adjP": None})
    for row in readTsv(os.path.join(tsvDir, fname)):
        region = row[regionCol]
        if not region or region.startswith("."):
            continue
        rec = byRegion[region]
        rsId = row[2].strip()
        if rsId and rsId not in rec["snps"]:
            rec["snps"].append(rsId)
        alleles = row[3].strip()
        if alleles and alleles not in rec["alleles"]:
            rec["alleles"].append(alleles)
        for cellType in cellTypesOfSamples(row[7]):
            if cellType not in rec["samples"]:
                rec["samples"].append(cellType)
        best = minPval(row[5])
        if best is not None and (rec["adjP"] is None or best < rec["adjP"]):
            rec["adjP"] = best
    return byRegion


def writeIcr(bedDir, tsvDir, outDir, icrType, icrGenes, parentalRegions):
    """ the 72 ICRs with the boundaries revised in this study. Data S2 also
    gives the boundaries the ICR had before the revision; those are written to
    a second BED so they can be lifted to hg38 alongside the revised ones. """
    orig = {}
    for row in readTsv(os.path.join(tsvDir, "dataS2_icr.tsv")):
        orig[(row[0], row[1], row[2])] = (row[3], row[4], row[5])

    outPath = os.path.join(outDir, "kaplanIcr.hg19.bed")
    origPath = os.path.join(outDir, "kaplanIcrOrig.hg19.bed")
    noOrig = 0
    with open(outPath, "w") as ofh, open(origPath, "w") as origFh:
        for f in readBed(os.path.join(bedDir, "icr.hg19.bed")):
            chrom, start, end, name = f[0], f[1], f[2], f[3]
            key = (chrom, start, end)
            if key not in orig:
                # every region of the source file is in Data S2, so this cannot
                # happen without the two inputs having drifted apart
                raise Exception("ICR %s:%s-%s is not in Data S2" % key)
            s2Name, origStart, origEnd = orig[key]
            if s2Name != name:
                raise Exception("ICR name mismatch at %s:%s-%s: %s vs %s"
                                % (chrom, start, end, name, s2Name))
            typ = icrType.get(name, "")
            genes = ",".join(icrGenes.get(name, []))
            isParental = "yes" if name in parentalRegions else "no"
            ofh.write("\t".join([chrom, start, end, name, "0", ".", start, end,
                                 COLOR.get(typ, COLOR[""]),
                                 typ, genes, isParental]) + "\n")
            if origStart and origEnd:
                origFh.write("\t".join([chrom, origStart, origEnd, name]) + "\n")
            else:
                noOrig += 1
    print("kaplanIcr: %d regions, %d without original boundaries in Data S2"
          % (sum(1 for _ in readBed(outPath)), noOrig))


def writeBimodal(bedDir, outDir):
    """ the 385,564 bimodal regions. The source file names each region with the
    comma-separated list of the cell types in which it is bimodal, but that
    field is cut off at the 255-character limit of a BED string field in 125 of
    the regions, which is flagged here rather than silently repaired. """
    inPath = os.path.join(bedDir, "bimodal.hg19.bed")

    def parse(f):
        " returns the cell type list, and whether it was cut off in the source "
        cellTypes = f[3]
        isTrunc = len(cellTypes) >= 250
        cellTypeList = [c for c in cellTypes.split(",") if c]
        if isTrunc:
            # the last entry of a cut-off list is itself a partial name
            cellTypeList = cellTypeList[:-1]
        return cellTypeList, isTrunc

    # the shading below runs over the observed range rather than over the 39 cell
    # types of the atlas, because a cut-off list understates its own length
    maxCount = max(len(parse(f)[0]) for f in readBed(inPath))

    outPath = os.path.join(outDir, "kaplanBimodal.hg19.bed")
    truncCount = 0
    total = 0
    with open(outPath, "w") as ofh:
        for f in readBed(inPath):
            chrom, start, end = f[0], f[1], f[2]
            total += 1
            cellTypeList, isTrunc = parse(f)
            count = len(cellTypeList)
            if isTrunc:
                truncCount += 1
                truncFlag = "list cut off in the source file, more cell types apply"
                name = "%d+ cell types" % count
            else:
                truncFlag = ""
                name = cellTypeList[0] if count == 1 else "%d cell types" % count
            score = min(1000, int(round(1000.0 * count / maxCount)))
            ofh.write("\t".join([chrom, start, end, name, str(score), ".",
                                 start, end, bimodalShade(count, maxCount),
                                 ",".join(cellTypeList), str(count),
                                 truncFlag]) + "\n")
    print("kaplanBimodal: %d regions, %d with a cut-off cell type list, "
          "shading scaled to %d cell types" % (total, truncCount, maxCount))


def writeAsm(bedDir, tsvDir, outDir):
    " the 34,425 bimodal regions where ASM tracks a heterozygous SNP "
    snpData = groupSnps(tsvDir, "dataS3_asmSnps.tsv", 4)
    outPath = os.path.join(outDir, "kaplanAsm.hg19.bed")
    noData = 0
    total = 0
    with open(outPath, "w") as ofh:
        for f in readBed(os.path.join(bedDir, "bimodalAsm.hg19.bed")):
            chrom, start, end = f[0], f[1], f[2]
            total += 1
            rec = snpData.get(regionKey(chrom, start, end))
            if rec is None:
                noData += 1
                rec = {"snps": [], "alleles": [], "samples": [], "adjP": None}
            snps = ",".join(rec["snps"])
            name = rec["snps"][0] if len(rec["snps"]) == 1 else \
                   ("%d ASM SNPs" % len(rec["snps"]) if rec["snps"] else "ASM region")
            ofh.write("\t".join([chrom, start, end, name, "0", ".", start, end,
                                 BIMODAL_COLOR,
                                 snps, str(len(rec["snps"])),
                                 ",".join(rec["alleles"]),
                                 ",".join(rec["samples"]), str(len(rec["samples"])),
                                 fmtPval(rec["adjP"])]) + "\n")
    print("kaplanAsm: %d regions, %d without SNP annotation in Data S3" % (total, noData))


def writeParentalAsm(bedDir, tsvDir, outDir, icrType, icrGenes):
    " the 460 regions whose methylation follows the parent of origin "
    snpData = groupSnps(tsvDir, "dataS4_parentalAsmSnps.tsv", 4)
    geneData = loadGenesByParentalRegion(tsvDir)
    methData = loadGameteMeth(tsvDir)

    # the known-ICR annotation of a region: any of its SNP rows may carry it
    knownIcr = {}
    for row in readTsv(os.path.join(tsvDir, "dataS4_parentalAsmSnps.tsv")):
        region, icrName = row[4], row[9]
        if region and icrName and region not in knownIcr:
            knownIcr[region] = icrName

    outPath = os.path.join(outDir, "kaplanParentalAsm.hg19.bed")
    counts = collections.Counter()
    noMeth = 0
    total = 0
    with open(outPath, "w") as ofh:
        for f in readBed(os.path.join(bedDir, "parentalAsm.hg19.bed")):
            chrom, start, end = f[0], f[1], f[2]
            total += 1
            region = regionKey(chrom, start, end)
            rec = snpData.get(region, {"snps": [], "alleles": [], "samples": [], "adjP": None})
            genes = geneData.get(region, [])
            icrName = knownIcr.get(region, "")
            typ = icrType.get(icrName, "") if icrName else ""

            if icrName:
                regionType = "Known imprinting control region"
                name = icrName
            elif genes:
                regionType = "Novel region near an imprinted gene"
                name = genes[0] if len(genes) == 1 else "%s and %d more" % (genes[0], len(genes) - 1)
            else:
                regionType = "Novel region"
                name = "%s:%s" % (chrom, start)
            counts[regionType] += 1

            icm, blast, sperm, oocyte = methData.get(region, ("", "", "", ""))
            if not (sperm or oocyte):
                noMeth += 1

            ofh.write("\t".join([chrom, start, end, name, "0", ".", start, end,
                                 COLOR.get(typ, COLOR[""]),
                                 regionType, typ, icrName,
                                 ",".join(icrGenes.get(icrName, [])) if icrName else "",
                                 ",".join(genes),
                                 ",".join(rec["snps"]), str(len(rec["snps"])),
                                 ",".join(rec["alleles"]),
                                 ",".join(rec["samples"]), str(len(rec["samples"])),
                                 fmtPval(rec["adjP"]),
                                 oocyte, sperm, icm, blast]) + "\n")
    print("kaplanParentalAsm: %d regions, %d without gamete methylation" % (total, noMeth))
    for regionType, n in counts.most_common():
        print("    %-40s %d" % (regionType, n))


def main():
    bedDir, tsvDir, outDir = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(outDir, exist_ok=True)

    icrType, icrGenes = loadIcrInfo(tsvDir)
    print("literature annotation for %d ICR names" % len(icrType))

    # which ICRs were recovered as parent-of-origin ASM regions in this study
    parentalRegions = set()
    for row in readTsv(os.path.join(tsvDir, "dataS4_parentalAsmSnps.tsv")):
        if row[9]:
            parentalRegions.add(row[9])

    writeIcr(bedDir, tsvDir, outDir, icrType, icrGenes, parentalRegions)
    writeBimodal(bedDir, outDir)
    writeAsm(bedDir, tsvDir, outDir)
    writeParentalAsm(bedDir, tsvDir, outDir, icrType, icrGenes)

main()
