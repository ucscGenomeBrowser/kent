#!/usr/bin/env python3
"""Sum the Tabula Sapiens long-read per-cell isoform counts by tissue.

Reads the pbid-level AnnData (.h5ad) file with h5py, without loading it with
anndata, and writes one row per isoform (pbid) with the number of reads (UMIs)
and the number of cells with at least one read in every tissue.

Output: a tab-separated file with the header
    isoform  <tissue1>_reads ... <tissueN>_reads  <tissue1>_cells ... <tissueN>_cells
and a second file <out>.tissues.tsv with the tissue name, the number of cells
and the total number of reads of all isoforms in that tissue (for CPM).
"""

import sys, argparse
import numpy as np
import h5py

def readCategorical(grp):
    " return (codes, categories) of an AnnData categorical column "
    cats = [x.decode() for x in grp["categories"][:]]
    return grp["codes"][:], cats

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("h5ad", help="pbids h5ad file")
    parser.add_argument("out", help="output tsv")
    args = parser.parse_args()

    f = h5py.File(args.h5ad, "r")
    nCells, nIso = f["X"].attrs["shape"]
    isoIds = [x.decode() for x in f["var/_index"][:]]
    tisCodes, tissues = readCategorical(f["obs/tissue"])
    if (tisCodes < 0).any():
        sys.exit("cells without a tissue annotation: %d" % (tisCodes < 0).sum())
    nTis = len(tissues)

    print("reading X", file=sys.stderr)
    indptr = f["X/indptr"][:]
    indices = f["X/indices"][:]
    data = f["X/data"][:]
    if not np.all(data == np.round(data)):
        sys.exit("X does not contain integer counts")

    # tissue of every non-zero entry, then bin by tissue*nIso+isoform
    rowTis = np.repeat(tisCodes.astype(np.int64), np.diff(indptr))
    key = rowTis * nIso + indices
    del rowTis
    reads = np.bincount(key, weights=data, minlength=nTis*nIso).reshape(nTis, nIso)
    cells = np.bincount(key, minlength=nTis*nIso).reshape(nTis, nIso)
    del key

    cellsPerTis = np.bincount(tisCodes, minlength=nTis)
    readsPerTis = reads.sum(axis=1)

    with open(args.out + ".tissues.tsv", "w") as ofh:
        ofh.write("tissue\tcells\treads\n")
        for i, t in enumerate(tissues):
            ofh.write("%s\t%d\t%d\n" % (t, cellsPerTis[i], readsPerTis[i]))

    with open(args.out, "w") as ofh:
        ofh.write("\t".join(["isoform"] + [t+"_reads" for t in tissues] + [t+"_cells" for t in tissues]) + "\n")
        for j, iso in enumerate(isoIds):
            row = [iso] + ["%d" % x for x in reads[:, j]] + ["%d" % x for x in cells[:, j]]
            ofh.write("\t".join(row) + "\n")
    print("%d cells, %d isoforms, %d tissues" % (nCells, nIso, nTis), file=sys.stderr)

main()
