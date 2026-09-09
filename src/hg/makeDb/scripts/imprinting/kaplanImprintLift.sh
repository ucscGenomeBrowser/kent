#!/bin/bash
# Lift the four Rosenski et al. 2025 imprinting BED files from hg19 to hg38 and
# build the bigBeds. The authors only published hg19 coordinates.
#
# Usage: kaplanImprintLift.sh <bedDir> <outDir>
#   bedDir  holds the hg19 BED files written by kaplanImprintToBed.py
#   outDir  where the hg38 BED files, the unmapped lists and the bigBeds go

set -beEu -o pipefail

bedDir=$1
outDir=$2
scriptDir=$(dirname $(readlink -f $0))
chain=/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz
chromSizes=/hive/data/genomes/hg38/chrom.sizes

mkdir -p $outDir $outDir/tmp

# The boundaries a control region had before this study revised them are lifted
# on their own, and then joined back onto the ICR track as a text field.
liftOver -tab -bedPlus=4 $bedDir/kaplanIcrOrig.hg19.bed $chain \
    $outDir/tmp/kaplanIcrOrig.hg38.bed $outDir/kaplanIcrOrig.unmapped

# liftOver needs -tab and -bedPlus so that it leaves the extra fields alone.
# The column added by awk carries the length before lifting, which is what
# kaplanLiftNote.py uses to spot regions stretched over sequence added in hg38.
for track in kaplanIcr kaplanBimodal kaplanAsm kaplanParentalAsm; do
    echo "== $track"
    awk -v OFS='\t' '{print $0, NR"|"($3-$2)}' $bedDir/$track.hg19.bed \
        > $outDir/tmp/$track.id.bed
    liftOver -tab -bedPlus=9 $outDir/tmp/$track.id.bed $chain \
        $outDir/tmp/$track.lifted.bed $outDir/$track.unmapped
    if [ $track = kaplanIcr ]; then
        $scriptDir/kaplanIcrAddOrig.py $outDir/tmp/$track.lifted.bed \
            $outDir/tmp/kaplanIcrOrig.hg38.bed $outDir/tmp/$track.withOrig.bed
        mv $outDir/tmp/$track.withOrig.bed $outDir/tmp/$track.lifted.bed
    fi
    $scriptDir/kaplanLiftNote.py $outDir/tmp/$track.lifted.bed \
        $outDir/tmp/$track.noted.bed
done

echo
for track in kaplanIcr kaplanBimodal kaplanAsm kaplanParentalAsm; do
    sort -k1,1 -k2,2n $outDir/tmp/$track.noted.bed > $outDir/$track.hg38.bed
    # the two small curated sets are searchable by name, the two genome-wide
    # ones have names like "3 cell types" that are not worth indexing
    extraIndex=""
    if [ $track = kaplanIcr -o $track = kaplanParentalAsm ]; then
        extraIndex="-extraIndex=name"
    fi
    bedToBigBed -type=bed9+ -tab -as=$scriptDir/$track.as $extraIndex \
        $outDir/$track.hg38.bed $chromSizes $outDir/$track.bb 2> $outDir/tmp/$track.bbLog
    printf "%-20s hg19 %8d -> hg38 %8d  (%d did not lift)\n" $track \
        $(wc -l < $bedDir/$track.hg19.bed) $(wc -l < $outDir/$track.hg38.bed) \
        $(grep -vc '^#' $outDir/$track.unmapped || true)
done
