#!/bin/bash
# Convert the lifted HPRC2 DMR bed file to a bigBed with a signed methylation difference field
set -euo pipefail
d=/hive/data/genomes/hg38/bed/imprinting/phillipy
cd $d
zcat dmrs.all.absDiff_gt_0.05.srt.color.lifted_to_grch38.bed.gz \
 | awk -F'\t' -v OFS='\t' '{p=($9=="#e78080")?"Maternal":"Paternal"; c=($9=="#e78080")?"220,20,20":"0,60,200"; printf "%s\t%d\t%d\t%s\t0\t.\t%d\t%d\t%s\t%s\t%.3f\n",$1,$2,$3,"",$7,$8,c,p,$5}' \
 | sort -k1,1 -k2,2n > hprcDmr.bed
bedToBigBed -type=bed9+2 -tab -as=$HOME/kent/src/hg/makeDb/scripts/hprcImprint/hprcImprint.as \
   hprcDmr.bed /hive/data/genomes/hg38/chrom.sizes hprcDmr.bb
wc -l hprcDmr.bed
