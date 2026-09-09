#!/usr/bin/env python3
"""Build the pcLAI reference-panel scatterplot data file for the hprc2annot hub.

The pcLAI authors publish the PCA coordinates of the 1000 Genomes reference
haplotypes that define the ancestry space each pcLAI window is placed in:

  https://github.com/AI-sandbox/hprc-pclai/blob/main/reference_pca_metadata.tsv

That table is turned into the compact JSON the hgc.scatterPlot.js module reads,
which the pclai track points at with its detailsScript.scatterPlot setting.

Population names are held in a separate list and referenced by index, because the
descriptors are long and repeat across thousands of haplotypes; each point also
carries its own haplotype id, which hgc.scatterPlot.js shows on mouseover.

  hprc2annotMakePclaiRefPanel.py <reference_pca_metadata.tsv> <out.json>
"""
import sys
import csv
import json

# The published TSV has one mis-decoded en dash (U+00D0 where an en dash belongs),
# which would otherwise show up in the plot legend.
FIXUPS = {"Ð": "-"}


def clean(s):
    for bad, good in FIXUPS.items():
        s = s.replace(bad, good)
    # collapse the whitespace a fixup can leave behind
    return " ".join(s.split())


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    inFname, outFname = sys.argv[1], sys.argv[2]

    labels = []
    labelIdx = {}
    points = []
    with open(inFname, encoding="utf-8") as fh:
        for row in csv.DictReader(fh, delimiter="\t"):
            try:
                x = float(row["x1"])
                y = float(row["x2"])
            except (TypeError, ValueError):
                continue
            pop = clean(row.get("Population_descriptor") or "")
            if pop not in labelIdx:
                labelIdx[pop] = len(labels)
                labels.append(pop)
            hap = clean(row.get("Sample_hap") or row.get("Sample") or "")
            # 3 decimals is finer than one screen pixel over this ~2.6 unit range
            points.append([round(x, 3), round(y, 3), labelIdx[pop], hap])

    if not points:
        sys.exit("no usable rows in %s" % inFname)

    # Order labels by their cluster's PC1 then PC2, so the legend reads in the same
    # left-to-right order as the clusters appear in the plot.
    sums = {}
    for x, y, li, _hap in points:
        sx, sy, n = sums.get(li, (0.0, 0.0, 0))
        sums[li] = (sx + x, sy + y, n + 1)
    order = sorted(range(len(labels)),
                   key=lambda li: (sums[li][0] / sums[li][2], sums[li][1] / sums[li][2]))
    remap = {old: new for new, old in enumerate(order)}
    labels = [labels[old] for old in order]
    points = [[x, y, remap[li], hap] for x, y, li, hap in points]

    with open(outFname, "w", encoding="utf-8") as out:
        json.dump({"labels": labels, "points": points}, out, separators=(",", ":"))
        out.write("\n")

    sys.stderr.write("%s: %d points, %d populations\n"
                     % (outFname, len(points), len(labels)))


if __name__ == "__main__":
    main()
