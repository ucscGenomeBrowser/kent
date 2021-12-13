#!/bin/sh -ex
cd $dir
{
mkdir -p $dir/cgap
cd $dir/cgap

wget --timestamping -O $cgapFile "ftp://ftp1.nci.nih.gov/pub/CGAP/$cgapFile"
hgCGAP $cgapFile

cat cgapSEQUENCE.tab cgapSYMBOL.tab cgapALIAS.tab|sort -u > cgapAlias.tab
hgLoadSqlTab -notOnServer $tempDb cgapAlias $kent/src/hg/lib/cgapAlias.sql ./cgapAlias.tab

hgLoadSqlTab -notOnServer $tempDb cgapBiocPathway $kent/src/hg/lib/cgapBiocPathway.sql ./cgapBIOCARTA.tab

cat cgapBIOCARTAdesc.tab|sort -u > cgapBIOCARTAdescSorted.tab
hgLoadSqlTab -notOnServer $tempDb cgapBiocDesc $kent/src/hg/lib/cgapBiocDesc.sql cgapBIOCARTAdescSorted.tab

echo "BuildCgap successfully finished"
} > doCgap.log 2>&1
