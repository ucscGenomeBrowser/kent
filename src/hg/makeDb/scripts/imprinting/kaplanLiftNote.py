#!/usr/bin/env python3
"""Flag regions that liftOver stretched across sequence added in hg38.

liftOver maps the two ends of a region independently, so when hg38 inserted
sequence inside a region the lifted interval comes out much longer than the
original. These regions are still at the right locus, but their boundaries no
longer mean what they meant in hg19, so rather than dropping them or showing
them as if nothing happened, the size change is written into a note field.

The input is the output of liftOver on a BED whose last column is
"<lineNumber>|<lengthBeforeLifting>". That column is replaced by the note.

Usage: kaplanLiftNote.py <liftedBed> <outBed> [maxRatio]
"""
import sys

liftedFname, outFname = sys.argv[1], sys.argv[2]
maxRatio = float(sys.argv[3]) if len(sys.argv) > 3 else 1.1

flagged = 0
total = 0
with open(outFname, "w") as ofh:
    for line in open(liftedFname):
        fields = line.rstrip("\n").split("\t")
        oldLen = int(fields.pop().split("|")[1])
        newLen = int(fields[2]) - int(fields[1])
        ratio = float(newLen) / oldLen
        total += 1
        # thickStart/thickEnd were copied from the hg19 coordinates by the
        # BED writer, so reset them to the lifted ones
        fields[6], fields[7] = fields[1], fields[2]
        if ratio > maxRatio or ratio < 1.0 / maxRatio:
            flagged += 1
            fields.append("region was %d bp on hg19 and is %d bp after lifting, "
                          "so its boundaries are not reliable" % (oldLen, newLen))
        else:
            fields.append("")
        ofh.write("\t".join(fields) + "\n")

print("    %d regions, %d whose size changed by more than %.0f%% when lifted"
      % (total, flagged, (maxRatio - 1) * 100))
