#!/bin/bash
# Build the Monk et al 2018 imprinted DMR bigBed on hg38 from the hg19 table, with the Mackay et al 2024 annotations
set -euo pipefail
cd /hive/data/genomes/hg38/bed/imprinting/monk2018
S=$HOME/kent/src/hg/makeDb/scripts/monk2018Imprint
$S/monk2018ImprintToBed.py monk2018_table1.tsv monk2018.hg19.bed
liftOver -tab -bedPlus=9 monk2018.hg19.bed /gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz monk2018.hg38.unsorted.bed monk2018.unmapped.bed
sort -k1,1 -k2,2n monk2018.hg38.unsorted.bed > monk2018.hg38.bed
# add the clinical classification and disorders of Mackay et al 2024 (needs ../imprintTable2024/mackay2024.bed,
# made by ~/kent/src/hg/makeDb/scripts/mackay2024Imprint/mackay2024ImprintBuild.sh)
$S/monk2018ImprintMerge.py monk2018.hg38.bed ../imprintTable2024/mackay2024.bed monk2018Merged.bed
bedToBigBed -type=bed9+12 -tab -as=$S/monk2018Imprint.as -extraIndex=name monk2018Merged.bed /hive/data/genomes/hg38/chrom.sizes monk2018Imprint.bb
wc -l monk2018.hg19.bed monk2018.hg38.bed monk2018.unmapped.bed monk2018Merged.bed
