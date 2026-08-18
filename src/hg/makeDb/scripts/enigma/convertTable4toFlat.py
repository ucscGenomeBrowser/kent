#RM#38130
# Convert the v1.2 CSpec Table 4 "Annotated Exons" sheet (visual two-sided-per-exon
# layout, exported by exportV12Sheets.py with merged cells expanded) back into the flat
# 8-column format that BRCAsplicing.py consumes:
#   Gene, Transcript, Exon, Position, Variant, ACMG code for IVS variant,
#   Observations, Warnings
# matching the layout of the V1.1 flat table that Anna exported for RM #32919.
#
# Layout facts (0-based columns), established against the v1.2 sheet:
#   acceptor-side splice records: notes=2, code=3, site=4, alleles=5
#   donor-side splice records:    site=15, alleles=16, code=17, notes=18
#   structural records (DEL/DUP/PTC/START codon): code=9, variant=10, extra=11
#   exon c. range: start=9, end=11, only when col9 matches ^c\. (protein ranges like
#       p.71/p.101 also appear in those columns and must be rejected)
#   One physical row can carry an acceptor record, a donor record, AND the exon range
#   at the same time, so every row is scanned for all record types independently.
#   The exon range row can come AFTER structural rows of the same exon, hence two passes.
# Notes columns carry both per-record observations and block-level warnings:
#   - a note on a row where the same side has a splice record = that record's Observations
#   - a note on a row with a structural record = that structural record's Warnings
#   - a note on a row with no record on that side and no structural record = an
#     exon-level warning, applied to the Warnings of every splice record of that
#     side of the exon (this reproduces how the V1.1 flat table repeated warnings
#     like "Conflicting functional/clinical data" across whole row groups)

import csv
import re

inPath = "/hive/data/inside/enigmaTracksData/v1.2/Table4_V1.2_annotatedExons.tsv"
outPath = "/hive/data/inside/enigmaTracksData/v1.2/Table4_V1.2_flat.txt"

transcripts = {"BRCA1": "NM_007294.4", "BRCA2": "NM_000059.4"}
splicePat = re.compile(r"^c\.[0-9-]+[+-][0-9]+[ACGT]>?$")
# a bare c. position like c.-113, c.301* (footnote asterisk) - NOT a splice site
rangeEndPat = re.compile(r"^c\.-?[0-9]+\*?$")
# the exon's c. range columns depend on how wide the exon's visual box is drawn:
# 9/11 for most exons, 8/12 for medium boxes (e.g. BRCA2 E10), 7/13 for the giant
# exons (BRCA1 E10(11), BRCA2 E11)
rangeColPairs = ((9, 11), (8, 12), (7, 13))

def clean(cell):
    return re.sub(r"\s+", " ", cell).strip()

rows = []
for row in csv.reader(open(inPath), delimiter="\t"):
    row = [clean(c) for c in row] + [""] * 20
    rows.append(row)

# pass 1: exon c. ranges and exon-level warnings
exonRange = {}
exonWarn = {"acceptor": {}, "donor": {}}
for row in rows:
    gene, exon = row[0], row[1]
    if gene not in transcripts or exon == "":
        continue
    for startCol, endCol in rangeColPairs:
        if rangeEndPat.match(row[startCol]) and rangeEndPat.match(row[endCol]):
            exonRange[(gene, exon)] = row[startCol].rstrip("*") + "-" + row[endCol].rstrip("*")
            break
    structural = row[10].startswith(("DEL", "DUP", "PTC", "START"))
    for side, noteCol, siteCol in (("acceptor", 2, 4), ("donor", 18, 15)):
        note = row[noteCol]
        if note and not splicePat.match(row[siteCol]) and not structural:
            exonWarn[side].setdefault((gene, exon), [])
            if note not in exonWarn[side][(gene, exon)]:
                exonWarn[side][(gene, exon)].append(note)

# pass 2: emit records in sheet order
outRows = []
seen = set()
def emit(fields):
    key = tuple(fields)
    if key not in seen:
        seen.add(key)
        outRows.append(fields)

for row in rows:
    gene, exon = row[0], row[1]
    if gene not in transcripts or exon == "":
        continue
    nm = transcripts[gene]
    # splice records, both sides scanned independently
    for side, noteCol, codeCol, siteCol, alleleCol in (
            ("acceptor", 2, 3, 4, 5), ("donor", 18, 17, 15, 16)):
        site, alleles, code = row[siteCol], row[alleleCol], row[codeCol]
        if splicePat.match(site) and code:
            obs = row[noteCol]
            warn = "; ".join(exonWarn[side].get((gene, exon), []))
            emit([gene, nm, exon, site, alleles, code, obs, warn])
    # structural record
    var, extra, code = row[10], row[11], row[9]
    if code and var.startswith(("DEL", "DUP", "PTC", "START")):
        warnNotes = []
        for noteCol, siteCol in ((2, 4), (18, 15)):
            note = row[noteCol]
            if note and not splicePat.match(row[siteCol]) and note not in warnNotes:
                warnNotes.append(note)
        warn = "; ".join(warnNotes)
        if var.startswith("PTC"):
            pm5 = extra.replace(" (PTC)", "")
            fullCode = code + (", " + pm5 if pm5 else "")
            rangeStart, rangeEnd = exonRange[(gene, exon)].split("-c.")
            rangeEnd = "c." + rangeEnd
            # exons split by the NMD-escape boundary carry a codon qualifier, e.g.
            # PTC<p.I1855 (PTCs before codon 1855, i.e. up to c.5562) and PTC>p.L1854
            # (PTCs after codon 1854, i.e. from c.5563). Convert back to the c. sub-
            # ranges the V1.1 flat table used: codon N spans c.(3N-2)..c.(3N).
            boundary = re.match(r"PTC([<>])p\.[A-Za-z]([0-9]+)$", var)
            if boundary:
                codon = int(boundary.group(2))
                if boundary.group(1) == "<":
                    rangeEnd = "c.%d" % (3 * (codon - 1))
                else:
                    rangeStart = "c.%d" % (3 * codon + 1)
            emit([gene, nm, exon, rangeStart + "-" + rangeEnd, "PTC", fullCode, "", warn])
        elif var.startswith("START"):
            emit([gene, nm, exon, "c.1", "Start Codon", code, "", warn])
        else:  # DEL / DUP
            variant = var + (" " + extra if extra else "")
            emit([gene, nm, exon, exonRange[(gene, exon)], variant, code, extra, warn])

with open(outPath, "w") as f:
    # header: first 8 fields identical to the V1.1 flat file's header, because
    # BRCAsplicing.py generates the .as field descriptions from it
    f.write("Gene \tTranscript\tExon\tPosition\tVariant\tACMG code for IVS variant\tObservations\tWarnings\n")
    for fields in outRows:
        f.write("\t".join(fields) + "\n")
print("wrote %d records to %s" % (len(outRows), outPath))
