#!/bin/sh

#	"$Id: removeObsoleteClones.sh,v 1.1 2006/11/04 00:30:01 hiram Exp $";

#	usage: ./removeObsoleteClones.sh contig_overlaps.agp \
#		obsolete.clones > clean_overlaps.agp
TOP=`pwd`
mkdir -p /scratch/tmp
rm -f /scratch/tmp/t.agp
sort -u $1 > /scratch/tmp/t.agp
cd /scratch/tmp

for C in `sort -u $TOP/$2 | awk '{print $1}'`
do
    grep -v "	${C}	" t.agp | egrep -v "	${C}_[0-9][0-9]*	" > tt.agp
    rm -f t.agp
    mv tt.agp t.agp
done

sort -k1,1 -k2,2n /scratch/tmp/t.agp
