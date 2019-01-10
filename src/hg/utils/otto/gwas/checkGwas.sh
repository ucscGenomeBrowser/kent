#!/bin/sh -e

#	Do not modify this script, modify the source tree copy:
#	src/hg/utils/omim/checkGwas.sh

# this script assumes that the snp*Coords files have already been built
# cut -f 1-4 /cluster/data/hg18/bed/snp130/snp130.bed | sort -k4,4 > snp130Coords.bed
# zcat /cluster/data/hg19/bed/snp138/snp138.bed.gz | cut -f 1-4,6,8,18,21-24   | sort -k4,4  > snp138Coords.bed
# zcat /cluster/data/hg19/bed/snp144/snp144.bed.gz | cut -f 1-4,6,8,18,21-24   | sort -k4,4  > snp144Coords.bed
# zcat /cluster/data/hg38/bed/snp144/snp144.bed.gz | cut -f 1-4,6,8,18,21-24   | sort -k4,4  > hg38.snp144Coords.bed

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

# Note: timestamping no longer works
wget -q --timestamping  -O gwascatalog.txt http://www.ebi.ac.uk/gwas/api/search/downloads/full
if [  ! gwascatalog.txt  -nt old.gwascatalog.txt ]; then
    echo "Not newer"
    exit 0
fi

today=`date +%y%m%d`
mkdir $today
cp gwascatalog.txt $today
mv gwascatalog.txt old.gwascatalog.txt

cd $today

head -1 gwascatalog.txt | sed -re 's/\t/\n/g' | tr -d '\r' > foundColumns.txt

if cmp foundColumns.txt ../expectedColumns.txt
then
    :
else
    echo COLUMNS HAVE CHANGED!!
    exit 1
fi

LANG=en_US.UTF-8 iconv -f "UTF-8" -t "ASCII//TRANSLIT" gwascatalog.txt | perl ../perlParser.pl > tmpFile
sort tmpFile > noCoords.tab
rm -f tmpFile

join -t "	" -1 4 ../snp130Coords.bed noCoords.tab -o 1.1,1.2,1.3,1.4,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9,2.10,2.11,2.12,2.13,2.14,2.15,2.16,2.17,2.18,2.19 | sort -k1,1 -k2n,2n > gwasCatalog.bed

hgLoadBed hg18 gwasCatalogNew gwasCatalog.bed -tab -sqlTable=$HOME/kent/src/hg/lib/gwasCatalog.sql -notItemRgb -allowStartEqualEnd -renameSqlTable


# Mapping to hg19 by joining hg19 SNP coords with catalog flatfile (see hg18.txt)
join -t "	" -1 4 ../snp151Coords.bed noCoords.tab -o 1.1,1.2,1.3,1.4,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9,2.10,2.11,2.12,2.13,2.14,2.15,2.16,2.17,2.18,2.19,1.5,1.6,1.7,1.8,1.9,1.10,1.11 | sort -k1,1 -k2n,2n > gwasCatalogPlus.bed


cut -f 1-22 gwasCatalogPlus.bed | hgLoadBed hg19 gwasCatalogNew stdin -tab -sqlTable=$HOME/kent/src/hg/lib/gwasCatalog.sql -notItemRgb -allowStartEqualEnd -renameSqlTable

join -t "	" -1 4 ../hg38.snp151Coords.bed noCoords.tab -o 1.1,1.2,1.3,1.4,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9,2.10,2.11,2.12,2.13,2.14,2.15,2.16,2.17,2.18,2.19,1.5,1.6,1.7,1.8,1.9,1.10,1.11 | sort -k1,1 -k2n,2n > hg38.gwasCatalog.bed
cut -f 1-22 hg38.gwasCatalog.bed | hgLoadBed hg38 gwasCatalogNew stdin -tab -sqlTable=$HOME/kent/src/hg/lib/gwasCatalog.sql -notItemRgb -allowStartEqualEnd -renameSqlTable

../validateGwas.sh hg18
../validateGwas.sh hg19
../validateGwas.sh hg38

# now install for hg18 and hg19 and hg38
for i in gwasCatalog
do 
    n=$i"New"
    o=$i"Old"
    hgsqlSwapTables hg18 $n $i $o -dropTable3
    hgsqlSwapTables hg19 $n $i $o -dropTable3
    hgsqlSwapTables hg38 $n $i $o -dropTable3
done

echo "Gwas Installed `date`" 


