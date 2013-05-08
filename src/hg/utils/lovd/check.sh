#!/bin/sh -ex
#	Do not modify this script, modify the source tree copy:
#	src/utils/lovd/check.sh
#	This script is used via a cron job 

umask 002
WORKDIR="/hive/data/outside/otto/lovd"
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in lovd release, Can not find the directory: ${WORKDIR}" 
    exit 255
fi

cd "${WORKDIR}"
today=`date +%F`
cd $today

# count columns and make sure new files have 6 columns
new19Cc=`cat lovd.hg19.bed | tawk '(NF!=6)' | wc -l`
new18Cc=`cat lovd.hg19.bed | tawk '(NF!=6)' | wc -l`
if [ "$new19Cc" -ne "0" ]; then
        echo LVOD hg19 $today: found rows with not six columns, quitting
        exit 255
fi
if [ "$new18Cc" -ne "0" ]; then
        echo LVOD hg18 $today: found rows with not six columns, quitting
        exit 255
fi

# compare old and new line counts and abort if no increase
old19Lc=`hgsql hg19 -e "SELECT COUNT(*) from lovd" -NB`
new19Lc=`wc -l lovd.hg19.bed | cut -d' ' -f1 `
old18Lc=`hgsql hg18 -e "SELECT COUNT(*) from lovd" -NB`
new18Lc=`wc -l lovd.hg18.bed | cut -d' ' -f1 `

echo hg19 rowcount: old $old19Lc new: $new19Lc
echo hg18 rowcount: old $old18Lc new: $new18Lc

if [ "$new19Lc" -le "$old19Lc" ]; then
        echo LVOD hg19: rowcount for $today is smaller or equal to old rowcount in mysql, quitting
        exit 255
fi

if [ "$new18Lc" -le "$old18Lc" ]; then
        echo LVOD hg18: rowcount for $today is smaller or equal to old rowcount in mysql, quitting
        exit 255
fi

