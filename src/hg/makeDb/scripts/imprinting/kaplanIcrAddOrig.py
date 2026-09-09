#!/usr/bin/env python3
"""Add the pre-revision boundaries of each imprinting control region to the ICR
BED file, as a chrom:start-end string in the assembly of the BED file.

This runs on the raw liftOver output, whose last column is the id that
kaplanLiftNote.py still needs, so the new field is inserted before it.

Both input files were lifted from hg19 with the same chain, so a control region
whose old boundaries failed to lift gets an empty field.

Usage: kaplanIcrAddOrig.py <icrBed> <origBed> <outBed>
"""
import sys

icrFname, origFname, outFname = sys.argv[1], sys.argv[2], sys.argv[3]

orig = {}
for line in open(origFname):
    chrom, start, end, name = line.rstrip("\n").split("\t")[:4]
    orig[name] = "%s:%s-%s" % (chrom, int(start) + 1, end)

missing = 0
changed = 0
with open(outFname, "w") as ofh:
    for line in open(icrFname):
        fields = line.rstrip("\n").split("\t")
        name = fields[3]
        if name not in orig:
            missing += 1
        origCoords = orig.get(name, "")
        # the field is only worth showing where the revision moved a boundary
        if origCoords == "%s:%d-%s" % (fields[0], int(fields[1]) + 1, fields[2]):
            origCoords = ""
        else:
            changed += 1
        fields.insert(len(fields) - 1, origCoords)
        ofh.write("\t".join(fields) + "\n")

print("    %d control regions had a boundary moved, %d have no lifted "
      "pre-revision boundaries" % (changed, missing))
