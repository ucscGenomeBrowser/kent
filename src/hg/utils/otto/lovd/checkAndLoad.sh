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
new38Cc=`cat lovd.hg38.bed | $TAWK '(NF!=6)' | wc -l`
new19Cc=`cat lovd.hg19.bed | $TAWK '(NF!=6)' | wc -l`
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

    cat lovd.${db}.clipped.bed | awk '(($3-$2)<50)' > lovd.${db}.short.bed
    cat lovd.${db}.clipped.bed | awk '(($3-$2)>=50)' > lovd.${db}.long.bed
    if [ ${db} == "hg19" ]
    then
        echo -e 'chrM\t0\t16571\tCheck chrMT\tPlease look at chrMT, not chrM, for LOVD annotations.\t' >> lovd.${db}.short.bed
        echo -e 'chrM\t0\t16571\tCheck chrMT\tPlease look at chrMT, not chrM, for LOVD annotations.\t' >> lovd.${db}.long.bed
    fi

    oldShortLc=`$KENTBIN/hgsql ${db} -e "SELECT COUNT(*) from lovdShort" -NB`
    oldLongLc=`$KENTBIN/hgsql ${db} -e "SELECT COUNT(*) from lovdLong" -NB`
    newShortLc=`wc -l lovd.${db}.short.bed | cut -d' ' -f1 `
    newLongLc=`wc -l lovd.${db}.long.bed | cut -d' ' -f1 `
    echo ${db} short rowcount: old $oldShortLc new: $newShortLc
    echo ${db} long rowcount: old $oldLongLc new: $newLongLc
    echo $oldShortLc $newShortLc | awk -v d=${db} '{if (($2-$1)/$1 > 0.1) {printf "validate on %s LOVD short failed: old count: %d, new count: %d\n", d,$1,$2; exit 1;}}'
    echo $oldLongLc $newLongLc | awk -v d=${db} '{if (($2-$1)/$1 > 0.1) {printf "validate on %s LOVD long failed: old count: %d, new count: %d\n", d,$1,$2; exit 1;}}'
    # bedDetail4.sql was generated like this:
    # egrep -v 'score|strand|thick|reserved|block|chromStarts' /cluster/home/max/kent/src/hg/lib/bedDetail.sql > bedDetail4.sql 
    cat lovd.${db}.short.bed | $KENTBIN/hgLoadBed ${db} lovdShort stdin -tab -sqlTable=../bedDetail4.sql -renameSqlTable -noBin
    cat lovd.${db}.long.bed | $KENTBIN/hgLoadBed ${db} lovdLong stdin -tab -sqlTable=../bedDetail4.sql -renameSqlTable -noBin
done

echo LOVD update: OK
