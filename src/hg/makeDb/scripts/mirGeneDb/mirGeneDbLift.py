#!/usr/bin/env python3
"""Lift a MirGeneDB <species>-all.bed to another assembly, one gene at a time.

  mirGeneDbLift.py in.bed over.chain.gz out.bed

All features of a gene (pri, pre, arms, seeds, loops, motifs; named <gene>_<feature>)
are lifted with liftOver. A gene is kept only if every feature lifts, to the chromosome
and strand of the gene's lifted pri region, with its length unchanged, so the arm
positions in the pri sequence stay the same as in the source assembly. The other genes
are left out and listed on stderr.
"""
import subprocess
import sys
import tempfile
from collections import defaultdict

def main(inBed, chain, outBed):
    feats = [l.rstrip("\n").split("\t") for l in open(inBed)]
    with tempfile.TemporaryDirectory() as tmp:
        # liftOver keeps the name; make it unique per line to match the lifted features back
        with open(tmp + "/in.bed", "w") as f:
            for i, c in enumerate(feats):
                f.write("\t".join(c[:3] + ["%d|%s" % (i, c[3])] + c[4:6]) + "\n")
        subprocess.run(["liftOver", tmp + "/in.bed", chain, tmp + "/out.bed", tmp + "/unmapped"],
                       check=True, capture_output=True)
        lifted = {}
        for l in open(tmp + "/out.bed"):
            c = l.rstrip("\n").split("\t")
            i, name = c[3].split("|", 1)
            lifted[int(i)] = c[:3] + [name] + c[4:6]

    byGene = defaultdict(list)
    for i, c in enumerate(feats):
        byGene[c[3].split("_", 1)[0]].append(i)
    keep, dropped = set(), {}
    for gene, idx in byGene.items():
        pri = [i for i in idx if feats[i][3] == gene + "_pri"][0]
        reason = None
        for i in idx:
            src, dst = feats[i], lifted.get(i)
            if dst is None:
                reason = "%s not lifted" % src[3]
            elif int(dst[2]) - int(dst[1]) != int(src[2]) - int(src[1]):
                reason = "%s changes length" % src[3]
            elif pri in lifted and (dst[0], dst[5]) != (lifted[pri][0], lifted[pri][5]):
                reason = "%s on another chromosome or strand" % src[3]
            if reason:
                break
        if reason:
            dropped[gene] = reason
        else:
            keep.update(idx)
    with open(outBed, "w") as out:
        for i in sorted(keep):
            out.write("\t".join(lifted[i]) + "\n")
    print("%d genes (loci), %d lifted, %d left out" % (len(byGene), len(byGene) - len(dropped),
          len(dropped)), file=sys.stderr)
    for gene in sorted(dropped):
        print("left out: %s, %s" % (gene, dropped[gene]), file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
