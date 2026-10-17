#!/usr/bin/env python3
"""Convert the EpiSign bedDetail file into the bed we load as the episign track.

Input is the file the EpiSign group (Sadikovic lab, London Health Sciences Centre) sent,
e.g. UCSC_EpiSign6.1.0_hg38.bed: a "track" line, then bed9 + two bedDetail columns:

  col 4   disorder code (ADNP_p.497, BAFopathy, ...), or "DMR: <id>" for the
          imprinting and promoter regions that EpiSign tests directly
  col 9   color: 63,99,127 for an episignature locus, 145,147,151 for a DMR
  col 10  ", "-separated episignature IDs; for a DMR row the disorder codes
  col 11  "<disorders separated by '; '>; gene_id=<gene>. <boilerplate>" or, for a
          DMR row, "<region description>; DMR signature. <boilerplate>"

The boilerplate paragraph is identical on every row and goes to the description page
instead. The output is bed9 + 7 fields, see episign.as. The provider's colors are kept.
"""

import argparse
import re
import sys
from collections import Counter

BOILER = " Episignatures are reproducible"
EPI_COLOR = "63,99,127"
DMR_COLOR = "145,147,151"


def warn(msg):
    sys.stderr.write(msg + "\n")


def parseDesc(desc):
    " return (disorders, gene) from an episignature row's description "
    desc = desc.rstrip()
    gene = ""
    m = re.search(r";? *gene_id=([A-Za-z0-9_.-]+?)\.?$", desc)
    if m:
        gene = m.group(1)
        desc = desc[:m.start()]
    desc = desc.rstrip(".").strip()
    return desc, gene


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("inBed")
    parser.add_argument("chromSizes")
    args = parser.parse_args()

    sizes = dict(l.split()[:2] for l in open(args.chromSizes))

    counts = Counter()
    rows = []
    for line in open(args.inBed, encoding="utf8"):
        if line.startswith("track") or line.startswith("#") or not line.strip():
            continue
        f = line.rstrip("\n").split("\t")
        if len(f) != 11:
            sys.exit("expected 11 columns, got %d: %s" % (len(f), line))
        chrom, start, end, name, score, strand, tStart, tEnd, color, ids, desc = f
        counts["input"] += 1

        if chrom not in sizes:
            sys.exit("unknown chromosome: " + line)
        if not 0 <= int(start) < int(end) <= int(sizes[chrom]):
            sys.exit("coordinates outside of chromosome: " + line)

        if BOILER not in desc:
            warn("no boilerplate text in row %s, kept as is" % name)
        desc = desc.split(BOILER)[0]
        ids = [x.strip() for x in ids.split(",") if x.strip()]

        if name.startswith("DMR: "):
            # e.g. "DMR: snrpn_5_utr", ids "PWS, Angelman", desc "SNRPN:alt-TSS-DMR; DMR signature."
            if color != DMR_COLOR:
                warn("DMR row %s has unexpected color %s" % (name, color))
            region = re.sub(r";? *DMR signature\.?$", "", desc.strip())
            newName = ", ".join(ids)
            rows.append([chrom, start, end, newName, "0", ".", start, end, color,
                         region, "", "", "0", "Imprinting or promoter DMR", name[5:]])
            counts["DMR"] += 1
        else:
            if color != EPI_COLOR:
                warn("episignature row %s has unexpected color %s" % (name, color))
            disorders, gene = parseDesc(desc)
            if not gene:
                counts["no gene"] += 1
            rows.append([chrom, start, end, name, "0", ".", start, end, color,
                         gene, disorders, ", ".join(ids), str(len(ids)), "Episignature", ""])
            counts["episignature"] += 1

    # label shown on the track: "code (gene)", or just the code if there is no gene
    for r in rows:
        r.append("%s (%s)" % (r[3], r[9]) if r[9] else r[3])

    rows.sort(key=lambda r: (r[0], int(r[1])))
    for r in rows:
        print("\t".join(r))

    for k, v in counts.items():
        warn("%s: %d" % (k, v))


main()
