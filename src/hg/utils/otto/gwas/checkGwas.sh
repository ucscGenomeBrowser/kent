#!/bin/sh -ex

#	Do not modify this script, modify the source tree copy:
#	src/hg/utils/omim/checkGwas.sh

# this script assumes that the snp*Coords files have already been built
# cut -f 1-4 /cluster/data/hg18/bed/snp130/snp130.bed | sort -k4,4 > snp130Coords.bed
# zcat /cluster/data/hg19/bed/snp137/snp137.bed.gz | cut -f 1-4,6,8,18,21-24   | sort -k4,4  > snp137Coords.bed

#	cron jobs need to ensure this is true
umask 002

WORKDIR=$1
export WORKDIR
export PATH=$WORKDIR":$PATH"

#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in GWAS release watch, Can not find the directory:     ${WORKDIR}" 
    exit 255
fi

cd "${WORKDIR}"

wget --timestamping http://www.genome.gov/admin/gwascatalog.txt

if [  ! gwascatalog.txt  -nt old.gwascatalog.txt ]; then
    echo "Not newer"
    exit 0
fi

today=`date +%y%m%d`
mkdir $today
cp gwascatalog.txt $today
mv gwascatalog.txt old.gwascatalog.txt

cd $today

head -1 gwascatalog.txt | sed -re 's/\t/\n/g' > foundColumns.txt

if cmp foundColumns.txt ../expectedColumns.txt
then
    :
else
    echo COLUMNS HAVE CHANGED!!
    exit 1
fi

perl -MEncode ../perlParser.pl gwascatalog.txt  | sort > noCoords.tab

join -t "	" -1 4 ../snp130Coords.bed noCoords.tab -o 1.1,1.2,1.3,1.4,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9,2.10,2.11,2.12,2.13,2.14,2.15,2.16,2.17,2.18,2.19 | sort -k1,1 -k2n,2n > gwasCatalog.bed

hgLoadBed hg18 gwasCatalogNew gwasCatalog.bed -tab -sqlTable=$HOME/kent/src/hg/lib/gwasCatalog.sql -notItemRgb -allowStartEqualEnd -renameSqlTable


# Mapping to hg19 by joining hg19 SNP coords with catalog flatfile (see hg18.txt)
join -t "	" -1 4 ../snp137Coords.bed noCoords.tab -o 1.1,1.2,1.3,1.4,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9,2.10,2.11,2.12,2.13,2.14,2.15,2.16,2.17,2.18,2.19,1.5,1.6,1.7,1.8,1.9,1.10,1.11 | sort -k1,1 -k2n,2n > gwasCatalogPlus.bed


cut -f 1-22 gwasCatalogPlus.bed | hgLoadBed hg19 gwasCatalogNew stdin -tab -sqlTable=$HOME/kent/src/hg/lib/gwasCatalog.sql -notItemRgb -allowStartEqualEnd -renameSqlTable

../validateGwas.sh hg18
../validateGwas.sh hg19

# now install for both hg18 and hg19
for i in gwasCatalog
do 
    n=$i"New"
    o=$i"Old"
    hgsqlSwapTables hg18 $n $i $o -dropTable3
    hgsqlSwapTables hg19 $n $i $o -dropTable3
done

echo "Gwas Installed `date`" 


