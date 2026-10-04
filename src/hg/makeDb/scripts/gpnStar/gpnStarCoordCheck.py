#!/usr/bin/env python3
"""Spot-check GPN-Star LLR bigWigs against the authors' Parquet tables.

For random rows of one Parquet shard (one-based pos), check that the genome base at
0-based [pos-1,pos) equals the Parquet ref, that the ref allele bigWig reads 0 and that
the alt allele bigWig equals llr_calibrated to three decimals. Needs pyarrow.

usage: gpnStarCoordCheck.py shard.parquet db gbdbDir nSamples
"""
import sys, random, subprocess, pyarrow.parquet as pq
pqFile, db, gbdb, nSample = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
pf = pq.ParquetFile(pqFile)
random.seed(1)
bad = 0
rgs = random.sample(range(pf.num_row_groups), min(nSample, pf.num_row_groups))
for rg in rgs:
    t = pf.read_row_group(rg).to_pylist()
    r = random.choice(t)
    chrom = r["chrom"] if str(r["chrom"]).startswith("chr") else "chr" + str(r["chrom"])
    pos = r["pos"]
    genome = subprocess.run(["twoBitToFa", f"/hive/data/genomes/{db}/{db}.2bit:{chrom}:{pos-1}-{pos}", "stdout"],
                            capture_output=True, text=True).stdout.split("\n", 1)[1].strip().upper()
    vals = {}
    for b in "ACGT":
        out = subprocess.run(["bigWigSummary", "-type=mean", f"{gbdb}/llr_{b}.bw", chrom, str(pos-1), str(pos), "1"],
                             capture_output=True, text=True).stdout.strip()
        vals[b] = float(out) if out else None
    ok = genome == r["ref"] and vals[r["ref"]] == 0 and vals[r["alt"]] is not None and abs(vals[r["alt"]] - r["llr_calibrated"]) < 0.0006
    bad += not ok
    print(f"{chrom}:{pos} ref={r['ref']} genome={genome} alt={r['alt']} parquet={r['llr_calibrated']:.4f} bw={vals} {'OK' if ok else 'MISMATCH'}")
print("mismatches:", bad, "of", len(rgs))
