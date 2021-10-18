#!/bin/sh -e
#	Do not modify this script, modify the source tree copy:
#	src/utils/otto/lovd/check.sh
#	This script is used via a cron job 

umask 002
WORKDIR="/hive/data/outside/otto/lovd"
TAWK=/cluster/bin/scripts/tawk
KENTBIN=/cluster/bin/x86_64/

if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in lovd release, Can not find the directory: ${WORKDIR}" 
    exit 255
fi

cd "${WORKDIR}"
today=`date +%F`
cd $today

# count columns and make sure new files have 6 columns
new38Cc=`cat lovd.hg38.bed | $TAWK '(NF!=7)' | wc -l`
new19Cc=`cat lovd.hg19.bed | $TAWK '(NF!=7)' | wc -l`
if [ "$new38Cc" -ne "0" ]; then
        echo LVOD hg38 $today: found rows with not six columns, quitting
        exit 255
fi
if [ "$new19Cc" -ne "0" ]; then
        echo LVOD hg19 $today: found rows with not six columns, quitting
        exit 255
fi

for db in hg38 hg19
do
    if [ ${db} == "hg19" ]
    then
        $KENTBIN/bedClip lovd.${db}.bed /cluster/data/${db}/chrom.sizes stdout | sed -e 's/^chrM/chrMT/g' > lovd.${db}.clipped.bed
    else
        $KENTBIN/bedClip lovd.${db}.bed /cluster/data/${db}/chrom.sizes lovd.${db}.clipped.bed
    fi

    sort -k1,1 -k2,2n lovd.${db}.clipped.bed | tawk '(($3-$2)<50)' > lovd.${db}.short.bed
    # the long variants turn into bed9+ so that mergeSpannedItems works:
    sort -k1,1 -k2,2n lovd.${db}.clipped.bed | tawk '(($3-$2)>=50){print $1,$2,$3,$4,0,".",$2,$3,"0,0,0",$5,$6,$7}' > lovd.${db}.long.bed
    if [ ${db} == "hg19" ]
    then
        echo -e 'chrM\t0\t16571\tCheck chrMT\tPlease look at chrMT, not chrM, for LOVD annotations.\t\t' >> lovd.${db}.short.bed
        echo -e 'chrM\t0\t16571\tCheck chrMT\t0\t.\t0\t16571\t0,0,0\tPlease look at chrMT, not chrM, for LOVD annotations.\t\t' >> lovd.${db}.long.bed
    fi

    sort -k1,1 -k2,2n lovd.${db}.short.bed > lovd.${db}.short.bed.sorted
    sort -k1,1 -k2,2n lovd.${db}.long.bed > lovd.${db}.long.bed.sorted
    oldShortLc=`bigBedToBed ../release/${db}/lovd.${db}.short.bb stdout | wc -l`
    oldLongLc=`bigBedToBed ../release/${db}/lovd.${db}.long.bb stdout | wc -l`
    newShortLc=`wc -l lovd.${db}.short.bed.sorted | cut -d' ' -f1`
    newLongLc=`wc -l lovd.${db}.long.bed.sorted | cut -d' ' -f1`
    echo ${db} short rowcount: old $oldShortLc new: $newShortLc
    echo ${db} long rowcount: old $oldLongLc new: $newLongLc
    if [ $newShortLc -lt "1000" ] ; then
            echo short variant file: final BED file is too small; exit 1 
    fi
    if [ $newLongLc -lt "1000" ] ; then
            echo long variant file: final BED file is too small; exit 1 
    fi
    # less then 2, because both 0 and 1 can happen if the download failed
    # only run this part if the previous download had something useful
    if [ $oldShortLc -gt "1000" ] ; then
            echo $oldShortLc $newShortLc | awk -v d=${db} '{if (($2-$1)/$1 > 0.1) {printf "validate on %s LOVD short failed: old count: %d, new count: %d\n", d,$1,$2; exit 1;}}'
            echo $oldLongLc $newLongLc | awk -v d=${db} '{if (($2-$1)/$1 > 0.1) {printf "validate on %s LOVD long failed: old count: %d, new count: %d\n", d,$1,$2; exit 1;}}'
    fi

    bedToBigBed -type=bed4+3 -tab -as=../lovd.short.as lovd.${db}.short.bed.sorted /cluster/data/${db}/chrom.sizes lovd.${db}.short.bb
    bedToBigBed -type=bed9+3 -tab -as=../lovd.long.as lovd.${db}.long.bed.sorted /cluster/data/${db}/chrom.sizes lovd.${db}.long.bb
    mkdir -p ${WORKDIR}/release/${db}
    cp lovd.${db}.{short,long}.bb ${WORKDIR}/release/${db}/
done

echo LOVD update: OK
