#!/bin/bash
source ~/.bashrc
conda activate pangolin
set -beEu -o pipefail

inputFasta=$1
outputHash=$2
if [ $# -ge 3 ]; then
    threads=$3
else
    threads=1
fi

pangoReference=~angie/github/pangolin/pangolin/data/reference.fasta
trimStart=265
trimEnd=29674

tmpSam=$(mktemp)
tmpLog=$(mktemp)
tmpAliMaskedFasta=$(mktemp)

minimap2 -a -x asm20 --sam-hit-only --secondary=no --score-N=0 -t $threads \
    $pangoReference $inputFasta -o $tmpSam &> $tmpLog
gofasta sam toMultiAlign \
    -s $tmpSam \
    -t $threads \
    --reference $pangoReference \
    --trimstart $trimStart \
    --trimend $trimEnd \
    --trim \
    --pad > $tmpAliMaskedFasta 2>>$tmpLog
faMd5 -threads=$threads $tmpAliMaskedFasta $outputHash
rm $tmpSam $tmpLog $tmpAliMaskedFasta
