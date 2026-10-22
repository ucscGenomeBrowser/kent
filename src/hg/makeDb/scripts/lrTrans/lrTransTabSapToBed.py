#!/usr/bin/env python3
"""Make the bigBed and barChart input files of the lrTransTabSap track.

Joins the isoform structures (BED12) of the Tabula Sapiens long-read atlas with
the SQANTI3 and pigeon feature metadata, the tube metadata and the per-tissue
read counts made by lrTransTabSapExpr.py, and writes:

    <outPrefix>.bed          bed12+ for lrTransTabSap.as, unsorted
    <outPrefix>.barChart.bed bed6+5 for barChartBed.as, unsorted, CPM per tissue
    <outPrefix>.categories   the category values, for the trackDb filter

The exon structures are always taken from the BED file. The SQANTI3 metadata
describe the structure that represented the isoform in one tube
("tissue_source"), whose ends can differ from the re-collapsed BED structure,
so the SQANTI3 ORF is drawn as thickStart/thickEnd only if both of its ends
fall into exons of the BED structure.
"""

import sys, csv, gzip, json, argparse, collections

csv.field_size_limit(sys.maxsize)

# SQANTI3 structural categories: display name and color
CATS = {
    "full-splice_match"       : ("Full splice match (FSM)", "107,174,214"),
    "incomplete-splice_match" : ("Incomplete splice match (ISM)", "252,141,89"),
    "novel_in_catalog"        : ("Novel in catalog (NIC)", "120,198,121"),
    "novel_not_in_catalog"    : ("Novel not in catalog (NNC)", "238,106,80"),
    "fusion"                  : ("Fusion", "255,193,37"),
    "genic"                   : ("Genic genomic", "150,150,150"),
    "genic_intron"            : ("Genic intron", "65,182,196"),
    "antisense"               : ("Antisense", "102,194,164"),
    "intergenic"              : ("Intergenic", "233,150,122"),
}

PIGEONCATS = {
    "full-splice_match"       : "Full splice match (FSM)",
    "incomplete-splice_match" : "Incomplete splice match (ISM)",
    "novel_in_catalog"        : "Novel in catalog (NIC)",
    "novel_not_in_catalog"    : "Novel not in catalog (NNC)",
    "fusion"                  : "Fusion",
    "genic"                   : "Genic genomic",
    "intergenic"              : "Intergenic",
    "antisense"               : "Antisense",
    "moreJunctions"           : "More junctions",
}

def subcat(s):
    " at_least_one_novel_splicesite -> at least one novel splice site "
    s = s.replace("splicesite", "splice site").replace("_", " ")
    return s

def readTubes(fname):
    " return tubeId -> (donor, tissue) "
    tubes = {}
    for r in csv.DictReader(open(fname)):
        tubes[r["tube_id"]] = (r["donor"], r["tissue"].replace("_", " "))
    return tubes

def readPigeon(fname):
    " return isoform -> (category, gene, transcript) "
    pig = {}
    for r in csv.DictReader(open(fname)):
        pig[r["isoform"]] = (r["structural_category"], r["associated_gene"], r["associated_transcript"])
    return pig

def readExpr(fname):
    " return list of tissues and isoform -> (readsList, cellsList) "
    ifh = open(fname)
    head = ifh.readline().rstrip("\n").split("\t")
    n = (len(head)-1)//2
    tissues = [h[:-len("_reads")] for h in head[1:n+1]]
    expr = {}
    for line in ifh:
        f = line.rstrip("\n").split("\t")
        expr[f[0]] = ([int(x) for x in f[1:n+1]], [int(x) for x in f[n+1:]])
    return tissues, expr

def readTissueTotals(fname):
    " return tissue -> total reads "
    tot = {}
    for r in csv.DictReader(open(fname), delimiter="\t"):
        tot[r["tissue"]] = int(r["reads"])
    return tot

def inExons(pos, starts, ends):
    " true if 0-based position is within an exon "
    for s, e in zip(starts, ends):
        if s <= pos < e:
            return True
    return False

def intOrEmpty(s):
    " '116.0' -> '116', '' -> '' "
    if s == "":
        return ""
    return "%d" % int(float(s))

def ensLinks(s):
    """ make a value for hgc 'urls': ENSG00000.5_ENSG0001.2 -> ENSG00000|ENSG00000.5,ENSG0001|ENSG0001.2
    The Ensembl website does not accept versioned IDs. 'novel' becomes empty. """
    if s == "novel" or s == "":
        return ""
    parts = []
    for id in s.split("_"):
        if id == "PAR" or id == "Y":   # ENSG00000002586.20_PAR_Y
            parts[-1] += "_"+id
            continue
        parts.append(id)
    return ",".join("%s|%s" % (p.split(".")[0], p) for p in parts)

def boolStr(s):
    return {"True":"yes", "False":"no", "TRUE":"yes", "FALSE":"no", "":""}[s]

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("origDir", help="directory with the downloaded files")
    parser.add_argument("exprTsv", help="output of lrTransTabSapExpr.py")
    parser.add_argument("chromSizes")
    parser.add_argument("outPrefix")
    args = parser.parse_args()
    d = args.origDir

    chromSizes = dict((l.split()[0], int(l.split()[1])) for l in open(args.chromSizes))
    tubes = readTubes(d+"/sample_metadata.csv")
    print("reading pigeon", file=sys.stderr)
    pigeon = readPigeon(d+"/all_samples_pbids_feature_metadata_pigeon.csv")
    print("reading expression", file=sys.stderr)
    tissues, expr = readExpr(args.exprTsv)
    tisTotals = readTissueTotals(args.exprTsv+".tissues.tsv")
    tisNames = [t.replace("_", " ") for t in tissues]

    print("reading sqanti3", file=sys.stderr)
    sqanti = {}
    for r in csv.DictReader(open(d+"/all_samples_pbids_feature_metadata_sqanti3.csv")):
        sqanti[r["isoform"]] = r

    bedOut = open(args.outPrefix+".bed", "w")
    barOut = open(args.outPrefix+".barChart.bed", "w")
    counts = collections.Counter()
    catCounts = collections.Counter()

    for line in gzip.open(d+"/all_samples_pbids_isoform_structures.bed.gz", "rt"):
        bf = line.rstrip("\n").split("\t")
        chrom, start, end, name, strand = bf[0], int(bf[1]), int(bf[2]), bf[3], bf[5]
        counts["inBed"] += 1
        if chrom not in chromSizes:
            sys.exit("unknown chrom: "+line)
        if end > chromSizes[chrom]:
            sys.exit("feature beyond chrom end: "+line)
        sizes = [int(x) for x in bf[10].rstrip(",").split(",")]
        relStarts = [int(x) for x in bf[11].rstrip(",").split(",")]
        exStarts = [start+s for s in relStarts]
        exEnds = [s+z for s, z in zip(exStarts, sizes)]

        r = sqanti[name]
        if r["chrom"] != chrom or r["strand"] != strand:
            sys.exit("chrom/strand differ between BED and SQANTI3 for %s" % name)
        pigCat, pigGene, pigTx = pigeon[name]

        # ORF
        thickStart = thickEnd = start
        if r["coding"] == "coding":
            counts["coding"] += 1
            a, b = int(float(r["CDS_genomic_start"])), int(float(r["CDS_genomic_end"]))
            cdsStart, cdsEnd = min(a, b)-1, max(a, b)
            if start <= cdsStart and cdsEnd <= end and inExons(cdsStart, exStarts, exEnds) \
                    and inExons(cdsEnd-1, exStarts, exEnds):
                thickStart, thickEnd = cdsStart, cdsEnd
                counts["codingDrawn"] += 1
            else:
                counts["codingNotDrawn"] += 1

        cat = r["structural_category"]
        catName, color = CATS[cat]
        catCounts[catName] += 1

        donor, tisSrc = tubes[r["tissue_source"]]
        tisSource = "%s (%s, tube %s)" % (tisSrc, donor, r["tissue_source"])

        # expression
        reads, cells = expr[name]
        totReads = sum(reads)
        totCells = sum(cells)
        if totReads == 0:
            counts["noReads"] += 1
        cpms = [1e6*rd/tisTotals[t] for rd, t in zip(reads, tissues)]
        rows = sorted([(cpm, tn, rd, ce) for cpm, tn, rd, ce in zip(cpms, tisNames, reads, cells) if rd > 0],
                      reverse=True)
        table = {"r000": ["Tissue", "UMIs", "Cells", "UMIs per million"]}
        for i, (cpm, tn, rd, ce) in enumerate(rows):
            table["r%03d" % (i+1)] = [tn, str(rd), str(ce), "%.2f" % cpm]
        jsonStr = json.dumps(table, ensure_ascii=True, separators=(",", ":"))

        gene = pigGene
        row = bf[:6] + [str(thickStart), str(thickEnd), color] + bf[9:12] + [
            gene,
            catName,
            subcat(r["subcategory"]),
            ensLinks(r["associated_gene"]),
            ensLinks(r["associated_transcript"]),
            PIGEONCATS[pigCat],
            ensLinks(pigTx),
            r["filter_result"],
            r["coding"].replace("_", "-"),
            r["CDS_type"].replace("_", " "),
            intOrEmpty(r["protein_length"]),
            r["psauron_score"],
            boolStr(r["predicted_NMD"]),
            r["all_canonical"].replace("_", "-"),
            boolStr(r["within_CAGE_peak"]),
            boolStr(r["within_polyA_site"]),
            r["polyA_motif"],
            intOrEmpty(r["FL"]),
            str(totReads),
            str(totCells),
            str(len(rows)),
            tisSource,
            r["n_tubes_structure_merged"],
            jsonStr,
        ]
        row[4] = "0"
        bedOut.write("\t".join(row)+"\n")

        barRow = [chrom, str(start), str(end), name, "0", strand, gene, str(len(tissues)),
                  ",".join("%.3f" % x for x in cpms), "0", "0"]
        barOut.write("\t".join(barRow)+"\n")
        counts["out"] += 1

    with open(args.outPrefix+".categories", "w") as ofh:
        for cat, (catName, color) in CATS.items():
            ofh.write("%s\t%s\t%d\n" % (catName, color, catCounts[catName]))

    if len(sqanti) != counts["inBed"]:
        print("warning: %d isoforms in SQANTI3 metadata, %d in BED" % (len(sqanti), counts["inBed"]), file=sys.stderr)
    for k, v in counts.items():
        print("%s\t%d" % (k, v), file=sys.stderr)

main()
