#!/bin/bash
# Randomly sample K rel2 haplotype indel files, collapse deletions across the sample
# (recurrence = # sampled haplotypes carrying the deletion), unbounded left-normalize,
# and emit a canonical (key<TAB>recurrence) file comparable to rel1/rel2.
# Redmine #35415
# Usage: buildSample.sh <outPrefix> <K> [seedTag]
set -beEu -o pipefail
D=/hive/data/genomes/hg38/bed/hprc2X; TB=/hive/data/genomes/hg38/hg38.2bit
export PATH=$HOME/bin/x86_64:/cluster/bin/x86_64:$PATH
export TMPDIR=$D/sortTmp
out="$1"; K="$2"; tag="${3:-x}"
ls $D/arr/*.indel.txt | shuf | head -$K > ${out}.files
# deletions only ($3>$2 & $5==0); dedupe within haplotype (uniq on chrom,start,end,label);
# then count distinct haplotypes per position
cat $(cat ${out}.files) | awk -F'\t' '($3>$2)&&($5==0){print $1"\t"$2"\t"$3"\t"$4}' \
  | sort -S24G --parallel=16 -u \
  | awk -F'\t' 'BEGIN{OFS="\t"}{k=$1"\t"$2"\t"$3; if(k!=pk){if(pk!="")print pk,c; c=0; pk=k} c++} END{if(pk!="")print pk,c}' \
  > ${out}.collapsed
echo "[$tag] sampled $K haplotypes -> $(wc -l < ${out}.collapsed) collapsed deletion events"
# unbounded left-normalize (annot col4 = recurrence), merge by canonical key
python3 ~/kent/src/hg/makeDb/scripts/hprc2X/leftNormDel.py $TB ${out}.collapsed 4 \
  | awk 'BEGIN{OFS="\t"}{k=$1":"$2":"$4; c[k]+=$5} END{for(x in c)print x,c[x]}' \
  | sort > ${out}.canon.tsv
echo "[$tag] canonical events: $(wc -l < ${out}.canon.tsv)"
