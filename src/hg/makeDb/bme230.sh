#!/bin/bash

# $1 = browser abbrev
# $2 = full name (with quotes)
# $3 = "Bacteria" or "Archaea"
# $4 = genbank genome URL
# $5 = shortened (genus) name (with quotes)

dom=`echo $3 | tr [:upper:] [:lower:]`
mkdir -p /cluster/store5/${dom}/$1
ln -s /cluster/store5/${dom}/$1 /cluster/data/$1
cd /cluster/data/$1
wget $4
fna=`basename $4`
sed "s/^>.*/>$1/" $fna > chr1.fa
rm $fna

echo "create database $1" | hgsql ''
hgNibSeq $1 /cluster/data/$1/nib chr1.fa
faSize -detailed chr1.fa > chrom.sizes
mkdir -p /gbdb/$1/nib
cd /gbdb/$1/nib
ln -s /cluster/store5/${dom}/$1/nib/chr1.nib chr1.nib
echo "create table grp (PRIMARY KEY(NAME)) select * from hg16.grp" \
            | hgsql $1
echo "INSERT INTO dbDb \
     (name, description, nibPath, organism, \
            defaultPos, active, orderKey, genome, scientificName, \
            htmlPath, hgNearOk) values \
     (\"$1\", \"$2\", \"/gbdb/$1/nib\", \"$5\", \
            \"chr1:50000-55000\", 1, 85, \"$3\", \
            \"$2\", \"\",0);" \
      | hgsql -h genome-testdb hgcentraltest
