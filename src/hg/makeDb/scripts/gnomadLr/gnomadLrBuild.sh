#!/bin/bash
# Build the gnomAD long-read SV and TR bigBeds from the release 0.1 VCFs.
# Run in /hive/data/genomes/hg38/bed/gnomad/lr, which has the VCFs in aou/ and hgsvc_hprc/.
# Usage: gnomadLrBuild.sh [chrom ...]   (default: chr1-22, X, Y)
set -beEu -o pipefail

scriptDir=$(dirname $(readlink -f $0))
chroms=${@:-$(echo chr{1..22} chrX chrY)}
mkdir -p perChrom

parallel -j 24 --halt now,fail=1 \
    "python3 -I $scriptDir/gnomadLrToBed.py {} aou/gnomad.lr.genomes.0.1.aou.sites.{}.vcf.gz \
        hgsvc_hprc/gnomad.lr.genomes.0.1.hgsvc_hprc.{}.vcf.gz perChrom/{}.sv.bed perChrom/{}.tr.bed \
        > perChrom/{}.counts 2> perChrom/{}.log" ::: $chroms

cat perChrom/*.counts > counts.tsv
# SV IDs are searchable. TR IDs are not indexed: they can be longer than 255 characters.
cat perChrom/chr*.sv.bed | sort -k1,1 -k2,2n > gnomadLrSv.bed
bedToBigBed -tab -type=bed9+ -as=$scriptDir/gnomadLrSv.as -extraIndex=vcfId \
    gnomadLrSv.bed /hive/data/genomes/hg38/chrom.sizes gnomadLrSv.bb
cat perChrom/chr*.tr.bed | sort -k1,1 -k2,2n > gnomadLrTr.bed
bedToBigBed -tab -type=bed9+ -as=$scriptDir/gnomadLrTr.as \
    gnomadLrTr.bed /hive/data/genomes/hg38/chrom.sizes gnomadLrTr.bb
