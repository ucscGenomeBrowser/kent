#!/usr/bin/env python3
""" Convert the IntOGen compendium of mutational cancer driver genes to a bed12+ file, one
item per gene, drawn on the Ensembl transcript that IntOGen used. """

import sys, gzip, json, argparse, urllib.parse
from collections import defaultdict, Counter, OrderedDict

# colors by mode of action
COLORS = {"Act": "216,38,38", "LoF": "0,0,255", "ambiguous": "46,0,184"}
ROLENAMES = {"Act": "Activating", "LoF": "Loss of function", "ambiguous": "Ambiguous"}
METHODS = {"cbase": "CBaSE", "combination": "combination", "dndscv": "dNdScv",
           "hotmaps": "HotMAPS", "mutpanning": "MutPanning", "oncodriveclustl": "OncodriveCLUSTL",
           "oncodrivefml": "OncodriveFML", "smregions": "smRegions"}

def readTsv(fname):
    with open(fname, encoding="utf-8-sig") as fh:
        head = fh.readline().rstrip("\n").split("\t")
        for line in fh:
            yield dict(zip(head, line.rstrip("\n").split("\t")))

def parseGff(fname, txIds):
    """ return dict transcript ID -> (chrom, strand, exons, cdsStart, cdsEnd), 0-based half-open """
    exons = defaultdict(list)
    cds = defaultdict(list)
    loc = {}
    for line in gzip.open(fname, "rt"):
        if line.startswith("#"):
            continue
        f = line.rstrip("\n").split("\t")
        if len(f) < 9 or f[2] not in ("exon", "CDS"):
            continue
        attrs = dict(a.split("=", 1) for a in f[8].split(";") if "=" in a)
        tid = attrs.get("Parent", "").replace("transcript:", "")
        if tid not in txIds:
            continue
        start, end = int(f[3]) - 1, int(f[4])
        loc[tid] = ("chr" + f[0] if f[0] != "MT" else "chrM", f[6])
        (exons if f[2] == "exon" else cds)[tid].append((start, end))
    res = {}
    for tid, exList in exons.items():
        exList.sort()
        chrom, strand = loc[tid]
        cdsList = cds.get(tid)
        if cdsList:
            cdsStart, cdsEnd = min(s for s, e in cdsList), max(e for s, e in cdsList)
        else:
            cdsStart = cdsEnd = exList[0][0]
        res[tid] = (chrom, strand, exList, cdsStart, cdsEnd)
    return res

def jsonTable(header, rows):
    """ JSON for detailsDynamicTable: object of arrays, the first is the header. Keys are not
    integer-like so they keep their order. """
    d = OrderedDict()
    d["r0000"] = header
    for i, r in enumerate(rows):
        d["r%04d" % (i + 1)] = r
    return json.dumps(d, ensure_ascii=True)

def fmtQ(q):
    q = float(q)
    return "%.1e" % q if q < 0.001 else "%.3f" % q

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("compendium", help="Compendium_Cancer_Genes.tsv")
    p.add_argument("cohorts", help="cohorts.tsv")
    p.add_argument("gff", help="Ensembl GFF3, gzipped")
    p.add_argument("outBed")
    args = p.parse_args()

    cohorts = {r["COHORT"]: r for r in readTsv(args.cohorts)}
    # the cancer type list is a comma-separated filter field: no commas or quotes in the names
    cancerNames = {r["CANCER"]: r["CANCER_NAME"].replace(", ", " ").replace("'", "")
                   for r in cohorts.values()}

    geneRows = defaultdict(list)
    for r in readTsv(args.compendium):
        geneRows[r["SYMBOL"]].append(r)

    txIds = set(r["TRANSCRIPT"] for rows in geneRows.values() for r in rows)
    txs = parseGff(args.gff, txIds)

    out = open(args.outBed, "w")
    missing = 0
    for gene, rows in sorted(geneRows.items()):
        tid = Counter(r["TRANSCRIPT"] for r in rows).most_common(1)[0][0]
        if tid not in txs:
            print("transcript %s of %s not in GFF, skipping" % (tid, gene), file=sys.stderr)
            missing += 1
            continue
        chrom, strand, exList, cdsStart, cdsEnd = txs[tid]
        start, end = exList[0][0], exList[-1][1]

        # sort cohorts by fraction of mutated samples
        rows.sort(key=lambda r: -float(r["%_SAMPLES_COHORT"]))
        roleCounts = Counter(r["ROLE"] for r in rows)
        role = roleCounts.most_common(1)[0][0]
        roleDesc = ", ".join("%s (%d)" % (ROLENAMES[k], v) for k, v in roleCounts.most_common())

        # per cancer type: highest fraction of mutated samples over its cohorts
        typeBest = OrderedDict()
        for r in rows:
            name = cancerNames.get(r["CANCER_TYPE"], r["CANCER_TYPE"])
            if name not in typeBest:
                typeBest[name] = float(r["%_SAMPLES_COHORT"])
        topTypes = ", ".join("%s (%.0f%%)" % (n, 100 * f) for n, f in list(typeBest.items())[:3])

        methods = Counter(m for r in rows for m in r["METHODS"].split(","))
        methodDesc = ", ".join("%s (%d)" % (METHODS.get(m, m), c) for m, c in methods.most_common())

        mutSamples = sum(int(float(r["SAMPLES"])) for r in rows)
        totSamples = sum(int(r["TOTAL_SAMPLES"]) for r in rows)
        maxPct = 100 * float(rows[0]["%_SAMPLES_COHORT"])
        minQ = min(float(r["QVALUE_COMBINATION"]) for r in rows)

        cohortRows = []
        for r in rows:
            c = cohorts.get(r["COHORT"], {})
            ctype = r["CANCER_TYPE"]
            cohortRows.append([r["COHORT"], cancerNames.get(ctype, ctype),
                c.get("SOURCE", ""), c.get("PLATFORM", ""), c.get("TYPE", ""),
                "%d / %s" % (int(float(r["SAMPLES"])), r["TOTAL_SAMPLES"]),
                "%.1f%%" % (100 * float(r["%_SAMPLES_COHORT"])), fmtQ(r["QVALUE_COMBINATION"]),
                ROLENAMES[r["ROLE"]],
                ", ".join(METHODS.get(m, m) for m in r["METHODS"].split(","))])
        cohortJson = jsonTable(["Cohort", "Cancer type", "Source", "Sequencing", "Tumors",
            "Mutated tumors", "Mutated", "q-value", "Mode of action", "Methods"], cohortRows)

        # significant regions of the protein, per cohort
        regRows = []
        for r in rows:
            for col, label in (("DOMAINS", "Pfam domain (smRegions)"),
                    ("2D_CLUSTERS", "Linear cluster (OncodriveCLUSTL)"),
                    ("3D_CLUSTERS", "3D cluster (HotMAPS)")):
                if r[col]:
                    regRows.append([label, r[col].replace(",", ", "), r["COHORT"]])
        regJson = jsonTable(["Type", "Amino acids", "Cohort"], regRows) if regRows else ""

        cancerTypes = sorted(typeBest)
        blockSizes = ",".join(str(e - s) for s, e in exList)
        blockStarts = ",".join(str(s - start) for s, e in exList)
        bed = [chrom, start, end, gene, min(1000, len(rows) * 10), strand, cdsStart, cdsEnd,
               COLORS[role], len(exList), blockSizes, blockStarts,
               gene, tid, ROLENAMES[role], roleDesc, len(rows), len(cancerTypes),
               "%.1f" % maxPct, topTypes, "%d of %d" % (mutSamples, totSamples),
               fmtQ(minQ), methodDesc, ",".join(cancerTypes), cohortJson, regJson]
        out.write("\t".join(str(x) for x in bed) + "\n")
    out.close()
    print("%d genes written, %d skipped" % (len(geneRows) - missing, missing), file=sys.stderr)

main()
