#!/usr/bin/env python3
"""Spot-check GPN-Star LLR bigWigs against the authors' Parquet tables.

For nSamples random rows of one Parquet shard (one-based pos), check that the genome base
at 0-based [pos-1,pos) equals the Parquet ref, that the ref allele bigWig reads 0 and that
the alt allele bigWig equals llr_calibrated to three decimals. Needs pyarrow.

usage: gpnStarCoordCheck.py shard.parquet db bigWigDir nSamples
"""
import random, subprocess, sys
import pyarrow.parquet as pq

pqFile, db, bwDir, nSample = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
pf = pq.ParquetFile(pqFile)
rgRows = [pf.metadata.row_group(i).num_rows for i in range(pf.num_row_groups)]
random.seed(1)
bad = 0
for _ in range(nSample):
    rg = random.choices(range(pf.num_row_groups), weights=rgRows)[0]
    r = random.choice(pf.read_row_group(rg).to_pylist())
    chrom = r["chrom"] if str(r["chrom"]).startswith("chr") else "chr" + str(r["chrom"])
    pos = r["pos"]
    twoBit = f"/hive/data/genomes/{db}/{db}.2bit:{chrom}:{pos-1}-{pos}"
    fa = subprocess.run(["twoBitToFa", twoBit, "stdout"], capture_output=True, text=True)
    genome = fa.stdout.split("\n", 1)[1].strip().upper()
    vals = {}
    for b in "ACGT":
        out = subprocess.run(["bigWigSummary", "-type=mean", f"{bwDir}/llr_{b}.bw", chrom,
                              str(pos-1), str(pos), "1"], capture_output=True, text=True)
        vals[b] = float(out.stdout) if out.stdout.strip() else None
    ok = (genome == r["ref"] and vals[r["ref"]] == 0 and vals[r["alt"]] is not None
          and abs(vals[r["alt"]] - r["llr_calibrated"]) < 0.0006)
    bad += not ok
    print(f"{chrom}:{pos} ref={r['ref']} genome={genome} alt={r['alt']} "
          f"parquet={r['llr_calibrated']:.4f} bw={vals} {'OK' if ok else 'MISMATCH'}")
print("mismatches:", bad, "of", nSample)
if bad:
    sys.exit(1)
