#!/bin/bash
# Build the Monk et al 2018 imprinted DMR bigBed on hg38 from the hg19 table
set -euo pipefail
cd /hive/data/genomes/hg38/bed/imprinting/monk2018
S=$HOME/kent/src/hg/makeDb/scripts/monk2018Imprint
$S/monk2018ImprintToBed.py monk2018_table1.tsv monk2018.hg19.bed
liftOver -tab -bedPlus=9 monk2018.hg19.bed /gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz monk2018.hg38.unsorted.bed monk2018.unmapped.bed
sort -k1,1 -k2,2n monk2018.hg38.unsorted.bed > monk2018.hg38.bed
bedToBigBed -type=bed9+6 -tab -as=$S/monk2018Imprint.as -extraIndex=name monk2018.hg38.bed /hive/data/genomes/hg38/chrom.sizes monk2018Imprint.bb
wc -l monk2018.hg19.bed monk2018.hg38.bed monk2018.unmapped.bed
