#!/usr/bin/env python3
"""Convert STRchive disease loci BED to bigBed-ready BED9+ format.

Reads the STRchive general BED file and writes a tab-separated BED9+
file to stdout, colored by inheritance mode.

Usage:
    strchiveToBed.py <input.bed> > strchive.bed
"""

import sys

# Colors by inheritance mode
INHERIT_COLORS = {
    "AD":    "0,0,200",       # blue - autosomal dominant
    "AR":    "200,0,0",       # red - autosomal recessive
    "AD,AR": "200,100,0",     # orange - both
    "XR":    "128,0,128",     # purple - X-linked recessive
    "XD":    "180,0,180",     # magenta - X-linked dominant
}
DEFAULT_COLOR = "128,128,128"  # gray - unknown

def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inFile = sys.argv[1]
    count = 0
    with open(inFile) as f:
        for line in f:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            chrom, start, end, locusId, gene, refMotif, pathMotif, pathMin, inherit, disease = fields
            color = INHERIT_COLORS.get(inherit, DEFAULT_COLOR)

            out = [
                chrom,
                start,
                end,
                locusId,        # name
                "0",            # score
                ".",            # strand
                start,          # thickStart
                end,            # thickEnd
                color,          # itemRgb
                gene,
                refMotif,
                pathMotif,
                pathMin if pathMin != "None" else "",
                inherit,
                disease,
            ]
            print("\t".join(out))
            count += 1

    print(f"Wrote {count} loci.", file=sys.stderr)

if __name__ == "__main__":
    main()
