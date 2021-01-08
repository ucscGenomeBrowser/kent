#!/bin/bash
set -beEu -o pipefail
today=`date +%F`

WORKDIR=$1
mkdir -p ${WORKDIR}/${today}
cd ${WORKDIR}/${today}

for db in hg38 hg19
do
    grc=""
    if [ $db == "hg38" ]; then
        grc="GRCh38"
    else
        grc="GRCh37"
    fi
    mkdir -p $db
    cd $db
    ~/kent/src/hg/utils/automation/parseLrgXml.pl $grc ../
    set +e
    genePredCheck lrgTranscriptsUnmapped.gp 2>genePred.failed
    set -e
    # use -gt 1 because one of the lines will be the "checked: X, failed: Y" line
    if [ `grep -v "LRG_7t1" genePred.failed | wc -l` -gt 1 ]; then
        printf "genePredCheck failed on %s. Check %s for more info\n" "${db}" "${WORKDIR}/${today}/${db}/genePred.failed"
    fi
    cut -f1,12 lrgTranscriptsUnmapped.gp | sort > transcript.gene.name.txt

    awk -F$'\t' '{printf "%s\t%s\t%s\t%s\t%s\t%s %s %s %s\n", $1,$16,$17,$18,$19, $16,$18,$17,$19}' \
        lrgTranscriptsUnmapped.gp | sort \
        | join -t$'\t' - transcript.gene.name.txt \
        | awk -F$'\t' '{printf "%s\t%s\t%s\t%s\t%s\t%s\t%s %s\n", $1,$2,$3,$4,$5,$7,$6,$7}' \
        > lrgTransExtraFields.tsv

    printf "Creating lrg.bb for %s\n" $db
    bedToBigBed lrg.bed /hive/data/genomes/$db/chrom.sizes lrg.bb \
        -tab -type=bed12+ -as=$HOME/kent/src/hg/lib/lrg.as -extraIndex=name

    lrgToPsl lrg.bed /hive/data/genomes/$db/chrom.sizes lrg.psl
    set +e
    pslCheck -fail=lrg.psl.failed lrg.psl
    if [ `wc -l lrg.psl.failed | cut -d' ' -f1` -gt 0 ]; then
        printf "pslCheck failed for %s. Check %s for more info\n" "${db}" "${WORKDIR}/${today}/${db}/lrg.psl.failed"
    fi
    set -e
    awk '{print $10 "\t" $11;}' lrg.psl > lrg.sizes
    genePredToFakePsl -chromSize=lrg.sizes placeholder \
        lrgTranscriptsUnmapped.gp lrgTranscriptsFakePsl.psl lrgTranscripts.cds
    pslMap lrgTranscriptsFakePsl.psl lrg.psl lrgTranscripts${db}.psl
    awk '{printf ">%s\n%s\n", $1,$2}' lrgCdna.tab > lrgCdna.fa
    pslToBigPsl -cds=lrgTranscripts.cds -fa=lrgCdna.fa lrgTranscripts${db}.psl bigPsl.txt
    join -t$'\t' -1 4 \
        -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,1.10,1.11,1.12,1.13,1.14,1.15\
,1.16,1.17,1.18,1.19,1.20,1.21,1.22,1.23,1.24,1.25,2.2,2.3,2.4,2.5,2.6,2.7\
        <(sort -k4 bigPsl.txt) lrgTransExtraFields.tsv \
        | sort -k1,1 -k2,2n > lrgExtraTranscripts${db}.bigPsl.bed

    printf "Creating lrgBigPsl.bb for %s\n" $db
    bedToBigBed -as=${WORKDIR}/bigPsl+6.as -type=bed12+19 -tab \
        lrgExtraTranscripts${db}.bigPsl.bed /hive/data/genomes/${db}/chrom.sizes lrgBigPsl.bb

    # The lrg table already exists as a one line table pointing to /gbdb/$db/bbi/lrg.bb, which
    # is in turn a symlink into $WORKDIR/release/$db/lrg.bb. Thus validate now before overwriting:
    oldCount=`bigBedInfo ${WORKDIR}/release/$db/lrg.bb | grep itemCount | cut -d' ' -f2`
    oldPslCount=`bigBedInfo ${WORKDIR}/release/$db/lrgBigPsl.bb | grep itemCount | cut -d' ' -f2`
    newCount=`bigBedInfo lrg.bb | grep itemCount | cut -d' ' -f2`
    newPslCount=`bigBedInfo lrgBigPsl.bb | grep itemCount | cut -d' ' -f2`
    echo LRG rowcounts: old $oldCount new: $newCount
    echo $oldCount $newCount | awk '{if (($2-$1)/$1 > 0.1) {printf "validate on LRG BigBed failed: old count: %d, new count: %d\n", $1,$2; exit 1;}}'
    echo LRG PSL rowcounts: old $oldPslCount new: $newPslCount
    echo $oldPslCount $newPslCount | awk '{if (($2-$1)/$1 > 0.1) {printf "validate on DECIPHER CNV failed: old count: %d, new count: %d\n", $1,$2; exit 1;}}'
    
    cp lrg.bb ${WORKDIR}/release/${db}/
    cp lrgBigPsl.bb ${WORKDIR}/release/${db}/

    cd ${WORKDIR}/${today}
done
