#!/usr/bin/env python3
"""Merge the Geneimprint and OMIM imprinted gene beds into one bed9+16 for the imprintedGenes track.

Usage: imprintedGenesMerge.py geneimprint.bed omimImprint.bed out.bed

Inputs are the outputs of geneimprintToBed.py (bed9+9) and omimImprintToBed.py (bed9+6).
Every input row becomes one output row, nothing is merged or dropped. Colors are by source:
OMIM green, Geneimprint blue, Geneimprint genes with status Predicted gray.
"""
import sys
from html import escape

OMIM_COLOR = "0,140,0"
GI_COLOR = "0,60,200"
GI_PRED_COLOR = "128,128,128"

def mo(parts):
    " build the mouseover html from (label, value) pairs, leaving out empty values "
    return "<br>".join(f"<b>{escape(l)}</b>: {escape(v)}" if l else escape(v) for l, v in parts if v)

def main():
    giFn, omimFn, outFn = sys.argv[1:4]
    rows = []
    nGi = nOmim = 0
    for line in open(giFn):
        f = line.rstrip("\n").split("\t")
        assert len(f) == 18, line
        status, allele, aliases, band, sym, hgncId, geneName, locusType, resolvedBy = f[9:18]
        color = GI_PRED_COLOR if status == "Predicted" else GI_COLOR
        resolved = f"{sym} ({resolvedBy})" if sym else ""
        mouse = f"<b>{escape(f[3])}</b> (Geneimprint)<br>" + mo([("Imprint status", status),
            ("Expressed allele", allele), ("Band in catalog", band), ("Resolved to", resolved)])
        rows.append(f[:8] + [color, "Geneimprint", status, allele, aliases, band, geneName, sym,
            hgncId, locusType, resolvedBy, "", "", "", "", f[3], mouse])
        nGi += 1
    for line in open(omimFn):
        f = line.rstrip("\n").split("\t")
        assert len(f) == 15, line
        mim, band, geneName, entryType, phenos, inherit = f[9:15]
        mouse = f"<b>{escape(f[3])}</b> (OMIM)<br>" + mo([("", geneName), ("OMIM", entryType),
            ("Band", band)])
        rows.append(f[:8] + [OMIM_COLOR, "OMIM", "", "", "", band, geneName, "", "", "", "",
            mim, entryType, phenos, inherit, "", mouse])
        nOmim += 1
    rows.sort(key=lambda r: (r[0], int(r[1])))
    with open(outFn, "w") as out:
        for r in rows:
            out.write("\t".join(r) + "\n")
    print(f"{nGi} Geneimprint + {nOmim} OMIM = {len(rows)} rows written", file=sys.stderr)

main()
