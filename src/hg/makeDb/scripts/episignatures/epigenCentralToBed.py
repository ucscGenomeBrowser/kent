#!/usr/bin/env python3
"""Convert the EpigenCentral track hub bigBed into the bed we load as a native track.

Reads the 16-column bed that bigBedToBed writes for
https://github.com/ccmbioinfo/EpigenCentral-UCSC-Genome-Browser/blob/main/episignatures.bb
and writes a 17-column bed, sorted, for bedToBigBed with epigenCentral.as.

Four things change:

  - the OMIM column holds a full https://omim.org/entry/NNNNNN URL. Only the number is
    kept, so trackDb can make the link with "urls displayOmim=".
  - the hub's last-but-one column is a pre-rendered mouse-over, "NSD1|Loss|-0.346".
    It is replaced by just the direction, and trackDb builds the mouse-over from the
    fields. The direction is taken from the comparison table rather than by splitting
    the string, so it cannot drift from the table the user is shown.
  - rows of the comparison table are de-duplicated. 137 probes in the hub repeat a
    signature two or three times with identical values, while the sigCount and
    signatureList columns already count it once.
  - a displayDisorder column is added, read out of the comparison table, so the
    mouse-over can name the disorder and not only the gene.

The comparison table is the ";" rows / "|" cells encoding that detailsDynamicTable
expands inside a char[4096] in hgc.c, so the widest row is reported and checked.

--raOut and --htmlOut write the filterValues.signatureList line for episignatures.ra
and the "Included episignatures" table for epigenCentral.html straight from the data,
so neither can drift away from the bigBed the way a hand-edited list would.
"""

import argparse
import os
import sys
from collections import OrderedDict, Counter

# The upstream bed columns, 0-based.
OMIM_COL = 11
COUNT_COL = 12
LIST_COL = 13
MOUSEOVER_COL = 14
TABLE_COL = 15
N_COLS = 16

OMIM_PREFIX = "https://omim.org/entry/"

# printEmbeddedTable() in hg/hgc/hgc.c expands the encoded table in a fixed buffer.
HGC_TABLE_LIMIT = 4096

# Cells of a comparison-table row.
GENE, DISORDER, OMIM, DIRECTION, DELTA, PVAL, CORRECTION = range(7)


def parseTable(encoded, probe):
    """Split the ';'/'|' encoded table into a header and a list of rows."""
    parts = encoded.split(";")
    header = parts[0].split("|")
    rows = []
    for part in parts[1:]:
        cells = part.split("|")
        if len(cells) != len(header):
            raise ValueError("%s: comparison row %r has %d cells, header has %d"
                             % (probe, part, len(cells), len(header)))
        rows.append(cells)
    return header, rows


def dedupeRows(rows):
    """Drop repeated rows, keeping the first occurrence and the original order."""
    seen = OrderedDict()
    for cells in rows:
        seen.setdefault("|".join(cells), cells)
    return list(seen.values())


def readRefs(fname):
    """signature -> (pmid, doi) from the checked-in reference table."""
    refs = {}
    with open(fname) as fh:
        for line in fh:
            if line.startswith("#") or not line.strip():
                continue
            cells = line.rstrip("\n").split("\t")
            cells += [""] * (3 - len(cells))
            refs[cells[0]] = (cells[1].strip(), cells[2].strip())
    return refs


def loadAuthorYear():
    """id (PMID or DOI) -> "Lastname Year", from the checked-in lookup shared with
    makeHtmlTables.py."""
    fname = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pubAuthorYear.tsv")
    authorYear = {}
    with open(fname) as fh:
        for line in fh:
            if line.startswith("#") or not line.strip():
                continue
            pmid, ay = line.rstrip("\n").split("\t")
            authorYear[pmid] = ay
    return authorYear


def writeRa(fh, signatures):
    """The filterValues/filterType lines for the signature menu.

    The menu label is "Disorder (SIGNATURE)". A comma in a filterValues entry is the
    entry separator and, with a *List* filterType, cannot be escaped at all, so the
    commas inside a few disorder names become semicolons here rather than in the
    bigBed, as the methaDory menu does.
    """
    entries = []
    for sig in sorted(signatures, key=str.lower):
        disorder = signatures[sig]["disorder"].replace(",", ";")
        entries.append("%s|%s (%s)" % (sig, disorder, sig))
    fh.write("    filterValues.signatureList %s\n" % ",".join(entries))
    fh.write("    filterType.signatureList multipleListAnd\n")


def writeHtml(fh, signatures, refs):
    """The "Included episignatures" table of the description page."""
    authorYear = loadAuthorYear()
    fh.write('<table class="stdTbl">\n')
    fh.write("<tr><th>Episignature</th><th>Disorder</th><th>OMIM</th>"
             "<th>CpG probes</th><th>Reference</th></tr>\n")
    for sig in sorted(signatures, key=str.lower):
        rec = signatures[sig]
        pmid, doi = refs.get(sig, ("", ""))
        id_ = pmid or doi
        if not id_:
            raise ValueError("no reference for signature %s in the reference table" % sig)
        if id_ not in authorYear:
            raise ValueError("no author/year in pubAuthorYear.tsv for id %s" % id_)
        if pmid:
            ref = ('<a href="https://pubmed.ncbi.nlm.nih.gov/%s/" target="_blank">%s</a>'
                   % (pmid, authorYear[id_]))
        else:
            ref = ('<a href="https://doi.org/%s" target="_blank">%s</a>'
                   % (doi, authorYear[id_]))
        fh.write("<tr><td>%s</td><td>%s</td>"
                 '<td><a href="https://omim.org/entry/%s" target="_blank">%s</a></td>'
                 "<td>%d</td><td>%s</td></tr>\n"
                 % (sig, rec["disorder"], rec["omim"], rec["omim"], rec["probes"], ref))
    fh.write("</table>\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--raOut", help="write the signature filter menu for episignatures.ra")
    parser.add_argument("--htmlOut", help="write the episignature table for epigenCentral.html")
    parser.add_argument("--refs", help="signature to PMID/DOI table, needed for --htmlOut")
    args = parser.parse_args()

    dropped = 0          # duplicate comparison rows removed
    dedupedProbes = 0    # probes that had at least one
    naDeltas = 0         # rows the hub has no delta-beta for
    widest = 0
    widestProbe = None
    out = []
    signatures = {}      # signature -> disorder, omim, probe count
    directions = Counter()

    for lineNo, line in enumerate(sys.stdin, start=1):
        row = line.rstrip("\n").split("\t")
        if len(row) != N_COLS:
            raise ValueError("line %d: expected %d columns, found %d"
                             % (lineNo, N_COLS, len(row)))
        probe = row[3]

        header, rows = parseTable(row[TABLE_COL], probe)
        nRows = len(rows)
        rows = dedupeRows(rows)
        if len(rows) != nRows:
            dropped += nRows - len(rows)
            dedupedProbes += 1

        # The columns the hub derives from the table have to agree with it.
        listed = sorted(row[LIST_COL].split(","))
        inTable = sorted(cells[GENE] for cells in rows)
        if listed != inTable:
            raise ValueError("%s: signatureList %s does not match the comparison "
                             "table %s" % (probe, listed, inTable))
        if int(row[COUNT_COL]) != len(rows):
            raise ValueError("%s: sigCount is %s but the comparison table has %d rows"
                             % (probe, row[COUNT_COL], len(rows)))

        naDeltas += sum(1 for cells in rows if cells[DELTA] == "NA")

        for cells in rows:
            rec = signatures.setdefault(cells[GENE], {"disorder": cells[DISORDER],
                                                      "omim": cells[OMIM], "probes": 0})
            if rec["disorder"] != cells[DISORDER] or rec["omim"] != cells[OMIM]:
                raise ValueError("%s: signature %s is %r/%s here but %r/%s elsewhere"
                                 % (probe, cells[GENE], cells[DISORDER], cells[OMIM],
                                    rec["disorder"], rec["omim"]))
            rec["probes"] += 1

        # The strongest signature, as the hub picked it, gives the color, the
        # direction and the disorder shown on the mouse-over.
        display = [cells for cells in rows if cells[GENE] == row[10]]
        if not display:
            raise ValueError("%s: displayed signature %r is not in the comparison table"
                             % (probe, row[10]))
        display = display[0]

        if display[DELTA] == "NA":
            raise ValueError("%s: displayed signature %r has no delta-beta"
                             % (probe, row[10]))
        if abs(abs(float(display[DELTA])) - float(row[9])) > 1e-9:
            raise ValueError("%s: maxAbsDelta %s is not |%s|"
                             % (probe, row[9], display[DELTA]))

        omim = row[OMIM_COL]
        if omim.startswith(OMIM_PREFIX):
            omim = omim[len(OMIM_PREFIX):].strip("/")
        if omim != display[OMIM]:
            raise ValueError("%s: OMIM column %r disagrees with the comparison table %r"
                             % (probe, omim, display[OMIM]))

        encoded = ";".join(["|".join(header)] +
                           ["|".join(cells) for cells in rows])
        if len(encoded) > widest:
            widest, widestProbe = len(encoded), probe

        out.append(row[0:10] + [
            row[10],              # displaySignature
            display[DISORDER],    # displayDisorder
            omim,                 # displayOmim
            display[DIRECTION],   # direction
            row[COUNT_COL],       # sigCount
            row[LIST_COL],        # signatureList
            encoded,              # comparisonTable
        ])
        directions[display[DIRECTION]] += 1

    out.sort(key=lambda r: (r[0], int(r[1])))
    for row in out:
        sys.stdout.write("\t".join(row) + "\n")

    if args.raOut:
        with open(args.raOut, "w") as fh:
            writeRa(fh, signatures)
    if args.htmlOut:
        if not args.refs:
            raise SystemExit("--htmlOut needs --refs")
        with open(args.htmlOut, "w") as fh:
            writeHtml(fh, signatures, readRefs(args.refs))

    sys.stderr.write("features written:            %d\n" % len(out))
    sys.stderr.write("episignatures:               %d\n" % len(signatures))
    sys.stderr.write("displayed direction:         %s\n"
                     % ", ".join("%s %d" % kv for kv in sorted(directions.items())))
    sys.stderr.write("duplicate table rows dropped: %d, on %d probes\n"
                     % (dropped, dedupedProbes))
    sys.stderr.write("table rows with no delta-beta: %d\n" % naDeltas)
    sys.stderr.write("widest comparison table:      %d bytes (%s), hgc limit %d\n"
                     % (widest, widestProbe, HGC_TABLE_LIMIT))
    if widest >= HGC_TABLE_LIMIT:
        raise SystemExit("ERROR: a comparison table is too wide for hgc.c, it would "
                         "errAbort on the details page. Switch the field to the "
                         "_json encoding, see methaDoryToBed.py.")


if __name__ == "__main__":
    main()
