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
new19Cc=`cat lovd.hg19.bed | $TAWK '(NF!=6)' | wc -l`
new18Cc=`cat lovd.hg19.bed | $TAWK '(NF!=6)' | wc -l`
if [ "$new19Cc" -ne "0" ]; then
        echo LVOD hg19 $today: found rows with not six columns, quitting
        exit 255
fi
if [ "$new18Cc" -ne "0" ]; then
        echo LVOD hg18 $today: found rows with not six columns, quitting
        exit 255
fi

# compare old and new line counts and abort if no increase
old19Lc=`$KENTBIN/hgsql hg19 -e "SELECT COUNT(*) from lovd" -NB`
new19Lc=`wc -l lovd.hg19.bed | cut -d' ' -f1 `
old18Lc=`$KENTBIN/hgsql hg18 -e "SELECT COUNT(*) from lovd" -NB`
new18Lc=`wc -l lovd.hg18.bed | cut -d' ' -f1 `

echo hg19 rowcount: old $old19Lc new: $new19Lc
echo hg18 rowcount: old $old18Lc new: $new18Lc

if [ "$new19Lc" -eq "$old19Lc" ]; then
        echo LVOD hg19: rowcount for $today is equal to old rowcount in mysql, quitting
        exit 0
fi

# commenting out, as LOVD count will go down in the future, because they have a 
# db cleanup procedure in place now
#if [ "$new19Lc" -lt "$old19Lc" ]; then
        #echo LVOD hg19: rowcount for $today is smaller to old rowcount in mysql, quitting
        #exit 255
#fi

# bedDetail4.sql was generated like this:
# egrep -v 'score|strand|thick|reserved|block|chromStarts' /cluster/home/max/kent/src/hg/lib/bedDetail.sql > bedDetail4.sql 
# need to use bedClip as current files include invalid coords which LOVD won't fix.
$KENTBIN/bedClip lovd.hg19.bed /cluster/data/hg19/chrom.sizes stdout | $KENTBIN/hgLoadBed hg19 lovd stdin -tab -sqlTable=../bedDetail4.sql -renameSqlTable -noBin
$KENTBIN/bedClip lovd.hg18.bed /cluster/data/hg18/chrom.sizes stdout | $KENTBIN/hgLoadBed hg18 lovd stdin  -tab -sqlTable=../bedDetail4.sql -renameSqlTable -noBin
