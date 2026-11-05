#!/bin/bash
# Build the Mackay et al 2024 imprinted DMR bigBed on hg38
set -euo pipefail
cd /hive/data/genomes/hg38/bed/imprinting/imprintTable2024
S=$HOME/kent/src/hg/makeDb/scripts/mackay2024Imprint
$S/mackay2024ImprintToBed.py table1.tsv table3.tsv ../monk2018/monk2018_table1.tsv mackay2024.unsorted.bed
sort -k1,1 -k2,2n mackay2024.unsorted.bed > mackay2024.bed
bedToBigBed -type=bed9+9 -tab -as=$S/mackay2024Imprint.as mackay2024.bed /hive/data/genomes/hg38/chrom.sizes mackay2024Imprint.bb
wc -l mackay2024.bed
