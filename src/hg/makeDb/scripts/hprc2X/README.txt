# hprc2X - HPRC Release 2 left-shifted deletion analysis (Redmine #35415, track #37891)
# Claude (braney) 2026-07-18
#
# hprc2X is a deletion-only re-derivation of the HPRC r2 rearrangement track built to
# (a) test left-shifting indel placement and (b) correlate the deletions with external
# variant catalogs (dbSNP, DGV, ClinVar) and the previous release (rel1).
#
# WORKDIR (data, large intermediates, cached maps, and the results file):
#   /hive/data/genomes/hg38/bed/hprc2X
#   -> full findings: RESULTS.dbSnpCorrelation.txt
# Reuses max's /hive/data/genomes/hg38/bed/hprc2 chain/ + net/ (462 assemblies) unchanged.
#
# TOOL NOTE: chainInDel -t2bit/-q2bit is committed on master (f977ac0a764) but the
# /cluster/bin copy is stale; use ~/bin/x86_64/chainInDel (rebuild: make in
# src/hg/utils/chainInDel) and put ~/bin/x86_64 first on PATH.
#
# ---- pipeline (build the left-shifted deletion track) ----
# hprc2XIndelsOne.sh / hprc2XIndelsAll.sh  regenerate per-assembly indels with -t2bit
#                                          (deletion left-normalization; reuses hprc2 chain/net)
# hprc2XArrange.as                         autoSql for the deletion bigBed
# buildDel.sh                              aggregate indels -> del.bed -> out/hprc2XDel.bb
#
# ---- canonicalization (the key step for cross-set comparison) ----
# leftNormDel.py <2bit> <in.tsv> <annotCol>  UNBOUNDED whole-chrom left-normalizer (matches
#                                            chainInDel baseEq). Emits chrom:start:len canonical
#                                            key so two deletion sets can be matched exactly.
# normHprc.sh   -> hprc2X.canon.tsv   (rel2 462-hap canonical events + recurrence)
# normHprc1.sh  -> hprc1.canon.tsv    (rel1 88-hap, from hprcDeletionsV1.bed)
#
# ---- dbSNP correlation + stability ----
# windowMatch.py <canon.tsv>          windowed dbSNP recovery (exact/+-2/20/50/200 bp)
# dbsnpStats.sh <canon.tsv> <N> <lbl> one-line recovery+Pearson-r summary for a canon set
# buildSample.sh / runSamples.sh      subsample rel2 to N=88 (algorithm-vs-panel-size control)
# presentInRel2.py                    rel1->rel2 carryover by recurrence (exact/window)
# dumpAbsentFixed.py                  the ~90 fixed rel1 events with no rel2 match (repeat/size analysis)
#
# ---- external ID cross-reference (feasibility for the #37891 xref field) ----
# xrefDbSnp.sh                        arrDel -> dbSNP-Common rs (canonical match)
# buildFullXref.sh                    arrDel -> full dbSnp155 rs (by-chrom parallel; builds the
#                                     reusable 20.3M-key cache dbSnp155.canon.rs in the workdir)
#
# OPEN ITEMS: (1) insertion left-shift (chainInDel -q2bit, needs 462 query 2bits);
# (2) complex/"double" indels not normalized by either option (code extension?);
# (3) wire an rs/DGV/ClinVar/rel1-id xref field into the hprc2XDel bigBed;
# (4) hprc2XDel.bb not yet a viewable track (no gbdb symlink / trackDb);
# (5) #37891 second enhancement: jump-to-assembly via liftOverChain table.
