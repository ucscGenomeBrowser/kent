#!/usr/bin/env python3
"""Make the bedGraph and color bed files of the APOGEE2 / nAPOGEE bigWig track
from the MitImpact CSV: one file pair per alternative allele (a, c, g, t).

Bar height is the raw score (APOGEE2_score for missense variants,
nAPOGEE_score for rRNA and tRNA variants), color is the class that goes with
the score, using the same colors as the MitImpact bigBed track. Variants that
have no score are left out of the bedGraph and drawn as gaps.

A variant in two overlapping genes (MT-ATP8/MT-ATP6, MT-ND4L/MT-ND4, a few tRNA
boundaries) has two CSV rows at one position and allele. A bigWig can hold one
value there, so the row with the highest score is used. The number of such
positions is printed.

Usage: apogeeToWig.py MitImpact4_V059.csv outDir
"""

import csv
import os
import sys
from collections import defaultdict

# class spelling of APOGEE 2 and nAPOGEE -> one spelling, and its color,
# the same as in mitImpactToBed.py
CLASSNORM = {
    "Benign": "Benign", "benign": "Benign",
    "Likely-benign": "Likely benign", "likely benign": "Likely benign",
    "VUS-": "VUS-", "VUS": "VUS", "VUS+": "VUS+",
    "Likely-pathogenic": "Likely pathogenic", "likely pathogenic": "Likely pathogenic",
    "Pathogenic": "Pathogenic", "pathogenic": "Pathogenic",
}
CLASSCOLORS = {
    "Benign": "0,107,27",
    "Likely benign": "0,170,60",
    "VUS-": "130,170,40",
    "VUS": "120,120,120",
    "VUS+": "210,140,0",
    "Likely pathogenic": "240,80,0",
    "Pathogenic": "200,0,0",
}

def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    csvFname, outDir = sys.argv[1:]
    os.makedirs(outDir, exist_ok=True)

    # (pos, alt) -> list of (score, class)
    scores = defaultdict(list)
    csvRows = unscored = 0
    for r in csv.DictReader(open(csvFname, newline="")):
        csvRows += 1
        if r["Chr"] != "chrM":
            sys.exit("Unexpected chromosome %s" % r["Chr"])
        if r["APOGEE2_score"] and r["nAPOGEE_score"]:
            sys.exit("%s has both an APOGEE 2 and an nAPOGEE score" % r["MitImpact_id"])
        if r["APOGEE2_score"]:
            score, cls = r["APOGEE2_score"], r["APOGEE2"]
        elif r["nAPOGEE_score"]:
            score, cls = r["nAPOGEE_score"], r["nAPOGEE"]
        else:
            unscored += 1
            continue
        scores[(int(r["Start"]), r["Alt"])].append((float(score), CLASSNORM[cls], score))

    nOverlap = nOverlapDiff = 0
    byAlt = defaultdict(list)
    for (pos, alt), lst in scores.items():
        if len(lst) > 1:
            nOverlap += 1
            if len(set(x[0] for x in lst)) > 1:
                nOverlapDiff += 1
        best = max(lst)
        byAlt[alt].append((pos, best))

    for alt, items in sorted(byAlt.items()):
        items.sort()
        a = alt.lower()
        with open(os.path.join(outDir, a + ".bedGraph"), "w") as bg, \
             open(os.path.join(outDir, a + ".color.bed"), "w") as cb:
            for pos, (val, cls, scoreStr) in items:
                bg.write("chrM\t%d\t%d\t%s\n" % (pos - 1, pos, scoreStr))
                cb.write("chrM\t%d\t%d\t%s\t0\t.\t%d\t%d\t%s\n" %
                         (pos - 1, pos, cls, pos - 1, pos, CLASSCOLORS[cls]))
        print("alt %s: %d scored variants" % (alt, len(items)))

    print("%d CSV rows, %d without a score, %d (position, allele) pairs with a score" %
          (csvRows, unscored, len(scores)))
    print("%d pairs have two rows (overlapping genes), %d of them with different scores; "
          "the highest score is used" % (nOverlap, nOverlapDiff))

if __name__ == "__main__":
    main()
