#!/usr/bin/env python3
"""
Build a bed9+ file of the genes that OMIM curates as imprinted.

The OMIM staff curate imprinting from the primary literature, but that call is
published only through GeneScout, which appends "(I)" to the coordinates in its
Location column. It is in neither the OMIM gene map nor any OMIM download file,
so the input is a GeneScout results table exported by hand from a browser
(genescout.omim.org sits behind a bot challenge that blocks scripted
downloads). See the makeDoc for the exact search.

Everything comes from that one export. Its coordinates are on GRCh38, 1-based
and inclusive, so the start of each interval is decremented by one.

GeneScout lists OMIM phenotype entries alongside genes. Those are mapped
disease regions, not loci: they run up to 90 Mb and would cover the whole
display, so they are dropped and listed in the report. They are recognised
without needing any other file, because a phenotype entry repeats the same MIM
number in the Gene MIM# and the Phenotype MIM# columns, while a gene entry
carries two different numbers.

Usage:
    omimImprintToBed.py <GeneScout-Results.tsv> <chrom.sizes> \
                        <out.bed> <out.report.txt>
"""
import sys, re
from collections import defaultdict

# One color scheme is shared by the whole Imprinting collection: vermillion
# means the maternal copy, blue means the paternal copy, neutral gray means the
# annotation carries no parent of origin. OMIM records that a gene is imprinted
# but not which copy is active, so every item here is gray.
NEUTRAL_COLOR = "85,85,85"

MAIN_CHROM = re.compile(r"^chr([0-9]+|X|Y|M)$")
IMPRINT_MARKER = re.compile(r"\(\s*I\s*\)")
SPANS_MARKER = re.compile(r"\(\s*S\s*\)")      # gene spans the edge of a search interval
LOCATION_RE = re.compile(r"^(chr[0-9XYM]+):([0-9,]+)-([0-9,]+)")


def findColumn(header, *wanted):
    " index of the first column whose name contains all the given words "
    for i, name in enumerate(header):
        low = name.strip().lower()
        if all(w in low for w in wanted):
            return i
    return None


def readHeader(fh):
    """ the export opens with a title, a date and the search coordinates, so
    walk down to the row that looks like the results header rather than
    trusting a fixed offset """
    for lineNum, line in enumerate(fh, 1):
        fields = [f.strip() for f in line.rstrip("\n").split("\t")]
        if len(fields) < 4:
            continue
        if findColumn(fields, "location") is not None and findColumn(fields, "gene") is not None:
            return fields, lineNum
    return None, 0


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    gsFname, sizesFname, outFname, reportFname = sys.argv[1:5]

    chromSizes = {}
    for line in open(sizesFname):
        f = line.split()
        chromSizes[f[0]] = int(f[1])

    fh = open(gsFname, encoding="utf8", errors="replace")
    header, headerLine = readHeader(fh)
    if header is None:
        sys.exit("error: no results header with Location and Gene columns found in %s.\n"
                 "Is this the tab-delimited GeneScout export?" % gsFname)

    cLoc = findColumn(header, "location")
    cCyto = findColumn(header, "cyto")
    cGene = findColumn(header, "gene")
    cName = findColumn(header, "gene", "name")
    cGeneMim = findColumn(header, "gene", "mim")
    cPheno = findColumn(header, "phenotype")
    cPhenoMim = findColumn(header, "phenotype", "mim")
    if cPheno == cPhenoMim:                     # "Phenotype" vs "Phenotype MIM#"
        cPheno = findColumn(header, "phenotype")
    cInher = findColumn(header, "inheritance")
    for label, idx in (("Location", cLoc), ("Gene", cGene)):
        if idx is None:
            sys.exit("error: no %s column in the export header:\n  %s" % (label, header))

    # collect the flagged rows, merging the extra rows a gene gets per phenotype
    entries, dataRows, flaggedRows, unparsed = {}, 0, 0, []
    for line in fh:
        f = line.rstrip("\n").split("\t")
        if len(f) <= cLoc:
            continue
        dataRows += 1
        if not IMPRINT_MARKER.search(f[cLoc]):
            continue
        flaggedRows += 1

        m = LOCATION_RE.match(f[cLoc].strip())
        if not m:
            unparsed.append((f[cGene] if len(f) > cGene else "?", f[cLoc]))
            continue

        def cell(idx):
            return f[idx].strip() if idx is not None and len(f) > idx else ""

        chrom = m.group(1)
        start1, end = int(m.group(2).replace(",", "")), int(m.group(3).replace(",", ""))
        symbol, geneMim = cell(cGene), re.sub(r"\D", "", cell(cGeneMim))
        key = (chrom, start1, end, symbol, geneMim)
        rec = entries.setdefault(key, dict(
            chrom=chrom, start1=start1, end=end, symbol=symbol, geneMim=geneMim,
            cyto=cell(cCyto), geneName=cell(cName),
            spansInterval=bool(SPANS_MARKER.search(f[cLoc])),
            phenotypes=[], inheritance=[], phenoMims=set()))
        for value, field in ((cell(cPheno), "phenotypes"), (cell(cInher), "inheritance")):
            if value and value not in rec[field]:
                rec[field].append(value)
        phenoMim = re.sub(r"\D", "", cell(cPhenoMim))
        if phenoMim:
            rec["phenoMims"].add(phenoMim)
    fh.close()

    beds, droppedPheno, droppedOther = [], [], []
    for key in sorted(entries):
        rec = entries[key]

        # a phenotype entry repeats its MIM number in both MIM columns
        if rec["geneMim"] and rec["phenoMims"] == {rec["geneMim"]}:
            droppedPheno.append(rec)
            continue

        start = rec["start1"] - 1
        if rec["chrom"] not in chromSizes or not MAIN_CHROM.match(rec["chrom"]):
            droppedOther.append((rec, "chromosome not in this assembly"))
            continue
        if start < 0 or rec["end"] > chromSizes[rec["chrom"]] or start >= rec["end"]:
            droppedOther.append((rec, "interval outside %s" % rec["chrom"]))
            continue

        beds.append([
            rec["chrom"], start, rec["end"], rec["symbol"], 0, ".",
            start, rec["end"], NEUTRAL_COLOR,
            rec["geneMim"], rec["cyto"], rec["geneName"],
            "OMIM gene" if rec["geneMim"] else "No OMIM entry",
            "; ".join(rec["phenotypes"]), "; ".join(rec["inheritance"]),
        ])

    beds.sort(key=lambda b: (b[0], b[1], b[2], b[3]))
    with open(outFname, "w") as out:
        for b in beds:
            out.write("\t".join(str(x) for x in b) + "\n")

    with open(reportFname, "w") as rep:
        def say(s=""):
            rep.write(s + "\n")
            print(s)

        say("OMIM imprinted genes to hg38 bed")
        say("GeneScout export:                 %s" % gsFname)
        say("  results header on line:         %d" % headerLine)
        say("  columns:                        %s" % ", ".join(header))
        say("  gene rows in the table:         %d" % dataRows)
        say("  rows carrying the (I) marker:   %d" % flaggedRows)
        say("  distinct flagged entries:       %d" % len(entries))
        say()
        say("features written:                 %d" % len(beds))
        byType = defaultdict(int)
        for b in beds:
            byType[b[12]] += 1
        for k in sorted(byType, key=lambda k: -byType[k]):
            say("    %-28s %d" % (k + ":", byType[k]))
        lengths = sorted(b[2] - b[1] for b in beds)
        if lengths:
            say("  feature length: min %d, median %d, max %d"
                % (lengths[0], lengths[len(lengths) // 2], lengths[-1]))
        spans = sum(1 for k in entries if entries[k]["spansInterval"])
        say("  flagged entries marked (S), spanning a search interval edge: %d" % spans)
        say()
        say("dropped, OMIM phenotype entries rather than genes (%d)." % len(droppedPheno))
        say("These repeat one MIM number in both the Gene MIM# and Phenotype MIM# columns:")
        for rec in sorted(droppedPheno, key=lambda r: -(r["end"] - r["start1"])):
            say("    %-16s MIM %-8s %s:%d-%d  %d bp"
                % (rec["symbol"], rec["geneMim"], rec["chrom"],
                   rec["start1"], rec["end"], rec["end"] - rec["start1"] + 1))
        say()
        say("dropped for other reasons (%d):" % len(droppedOther))
        for rec, why in droppedOther:
            say("    %-16s MIM %-8s %s" % (rec["symbol"], rec["geneMim"], why))
        say()
        say("flagged rows whose Location could not be parsed (%d):" % len(unparsed))
        for symbol, loc in unparsed:
            say("    %-16s %s" % (symbol, loc))


main()
