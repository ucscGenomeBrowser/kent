#!/bin/bash
# Mark insertions whose inserted sequence is not human with FILTER=NonHumanIns.
# Usage: sparkWgs45kFlagNonHumanIns.sh <in.vcf.gz> <bwaIndexPrefix> <out.vcf.gz> [minLen] [threads]
#   minLen: only insertions with VARLEN >= minLen are tested (default 20; shorter
#           sequences cannot be placed reliably by bwa mem)
# In the SPARK WGS 2026_08 GATK/GLnexus calls, millions of long insertions are
# oral bacteria DNA from the saliva samples: the non-human end of a read is
# soft-clipped and HaplotypeCaller turns it into an insertion. Every distinct
# inserted sequence is aligned to hg38 with bwa mem; it counts as human if it
# aligns over at least 80% of its length with at most 10% mismatches and gaps
# (sparkWgs45kNonHumanIns.py). A sequence missing from hg38 can also be a real
# insertion where the reference carries the other allele (one at chr21:40348226
# has AF 0.42), so insertions that gnomAD v4.1 genomes (different pipeline,
# mostly blood DNA) has too are never flagged. The records of the others get
# FILTER NonHumanIns (added to, not replacing, MONOALLELIC); all other records
# get PASS.
set -euo pipefail

inVcf=$1
bwaIndex=$2
outVcf=$3
minLen=${4:-20}
threads=${5:-32}
scriptDir=$(dirname "$(readlink -f "$0")")
B=/cluster/software/src/bcftools-1.22/bcftools
BWA=~max/software/bwa/bwa
work=$outVcf.work
mkdir -p "$work"

# 1. the insertions to test, per chromosome in parallel
chroms=$($B index -s "$inVcf" | cut -f1)
for c in $chroms; do echo $c; done | parallel -j 24 \
    "$B query -r {} -i 'VARLEN>=$minLen' -f '%CHROM\t%POS\t%REF\t%ALT\n' $inVcf > $work/ins.{}.tsv"
for c in $chroms; do cat "$work/ins.$c.tsv"; done > "$work/ins.tsv"
rm -f "$work"/ins.chr*.tsv
# the insertions in gnomAD v4.1 genomes; its bed intervals are zero-length for
# insertions, so VCF POS = chromEnd - length(REF) + 1
gnomad=/gbdb/hg38/gnomAD/v4.1/genomes/genomes.bb
for c in $chroms; do echo $c; done | parallel -j 24 \
    "bigBedToBed -chrom={} $gnomad stdout | awk -F'\t' -v m=$minLen 'length(\$11)-length(\$10)>=m {print \$1\"\t\"\$3-length(\$10)+1\"\t\"\$10\"\t\"\$11}' > $work/gnomad.{}.tsv"
for c in $chroms; do cat "$work/gnomad.$c.tsv"; done > "$work/gnomadIns.tsv"
rm -f "$work"/gnomad.chr*.tsv

# 2. align each distinct inserted sequence once
awk -F'\t' '{print substr($4, length($3)+1)}' "$work/ins.tsv" \
    | sort -u -S 20G --parallel=8 -T "$work" | awk '{print ">"NR"\n"$0}' > "$work/uniq.fa"
$BWA mem -t "$threads" -T 20 "$bwaIndex" "$work/uniq.fa" 2> "$work/bwa.log" \
    | python3 "$scriptDir/sparkWgs45kNonHumanIns.py" "$work/uniq.fa" "$work/ins.tsv" "$work/gnomadIns.tsv" \
    > "$work/nonHuman.tsv" 2> "$work/classify.log"
sort -k1,1 -k2,2n -S 20G -T "$work" "$work/nonHuman.tsv" | bgzip > "$work/nonHuman.tsv.gz"
tabix -s1 -b2 -e2 "$work/nonHuman.tsv.gz"

# 3. set FILTER, per chromosome in parallel
cat > "$work/hdr.txt" <<'EOF'
##INFO=<ID=NHI,Number=0,Type=Flag,Description="Temporary">
##FILTER=<ID=NonHumanIns,Description="Insertion whose inserted sequence does not align to the human genome (bwa mem to hg38); most are oral bacteria DNA from the saliva samples">
EOF
for c in $chroms; do echo $c; done | parallel -j 24 \
    "$B annotate -r {} -a $work/nonHuman.tsv.gz -c CHROM,POS,REF,ALT,INFO/NHI -h $work/hdr.txt $inVcf -Ou \
     | $B filter -m+ -s NonHumanIns -e 'INFO/NHI=1' -Ou \
     | $B annotate -x INFO/NHI -Ob -o $work/{}.bcf"
$B concat -Oz --threads 8 -o "$work/out.vcf.gz" $(for c in $chroms; do echo "$work/$c.bcf"; done)
tabix -p vcf "$work/out.vcf.gz"
mv -f "$work/out.vcf.gz" "$outVcf"
mv -f "$work/out.vcf.gz.tbi" "$outVcf.tbi"
cat "$work/classify.log"
