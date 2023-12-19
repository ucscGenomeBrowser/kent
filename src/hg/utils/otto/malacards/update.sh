#!/bin/bash
# redmine 14417
set -e -o pipefail
now=`date -I`
cd /hive/data/outside/otto/malacards
wget https://genecardscustomers.blob.core.windows.net/ucsc/UCSC_DiseaseCentric_dump_MC_current.csv -O oldVersions/$now.csv
# copied from ~/kent/src/hg/makeDb/doc/ucscGenes/hg38.ucscGenes16.sh  
# load malacards table
hgsql hg38 -e 'drop table malacards; create table malacards (geneSymbol varchar(255), maladySymbol varchar(255), urlSuffix varchar(255), mainName varchar(255), geneScore float, diseaseScore float, isElite bool)'
hgsql hg38 -e 'create index malacardsGeneIdx on malacards(geneSymbol);'
s='"'; hgsql hg38  -e "delete from malacards; load data local infile ${s}oldVersions/$now.csv${s} into table malacards columns terminated by ',' enclosed by '$s' escaped by '' ignore 1 lines"
# load knownToMalacards tables
kent=~/kent
for db in hg38 hg19; do 
        hgsql -e "select geneSymbol,kgId from kgXref" --skip-column-names $db | awk '{if (NF == 2) print}' | sort > geneSymbolToKgId.txt
        hgsql -e "select geneSymbol from malacards" --skip-column-names $db | sort > malacardExists.txt
        join malacardExists.txt  geneSymbolToKgId.txt | awk 'BEGIN {OFS="\t"} {print $2, $1}' > knownToMalacard.txt
        hgLoadSqlTab -notOnServer $db  knownToMalacards $kent/src/hg/lib/knownTo.sql  knownToMalacard.txt
done
echo Malacards update OK
