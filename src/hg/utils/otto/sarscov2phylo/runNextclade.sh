#!/bin/bash
# Run nextclade on xz'd fasta input; produce $outBase.{nextclade.full.tsv.gz,nextalign.fasta.xz}

# nextclade is installed in conda (base environment), so we need ~/.bashrc setup:
source ~/.bashrc
set -beEu -o pipefail

faIn=$1
outBase=$2

xcat () {
  for f in $@; do
    if [ "${f##*.}" == "xz" ]; then
        xzcat $f
    elif [ "${f##*.}" == "gz" ]; then
        zcat $f
    else
        cat $f
    fi
  done
}
export -f xcat

logfile=$(mktemp)
outDir=$(mktemp -d)

nDataDir=~angie/github/nextclade/data/sars-cov-2
nextclade run -i <(xcat $faIn | sed -re 's@^>hCo[Vv]-19/+@>@;  s/[ '"'"',()]//g;') \
    --input-dataset $nDataDir \
    --output-dir $outDir \
    --output-tsv $outBase.nextclade.full.tsv \
    --output-basename out \
    --jobs 1 \
    >& $logfile

gzip -f $outBase.nextclade.full.tsv
xz -f $outDir/out.aligned.fasta
cp -p $outDir/out.aligned.fasta.xz $outBase.nextalign.fasta.xz

rm -rf $logfile $outDir
