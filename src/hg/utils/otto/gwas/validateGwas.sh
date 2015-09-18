#!/bin/sh -e

db=$1
tooMuch=0.1000   # how much change (either gain or loss) is too much

for i in gwasCatalog
do 
#    fields=' chrom, chromStart, chromEnd, name, pubMedID, author, pubDate, journal, title, trait, initSample, replSample, region, genes, riskAllele, riskAlFreq, pValue, pValueDesc, orOrBeta, ci95, platform, cnv'
   # fields=' chrom, chromStart, chromEnd, name, pubMedID, author, pubDate, journal, title, trait, initSample, replSample,  genes, riskAllele, riskAlFreq, pValue, pValueDesc, orOrBeta, ci95, platform, cnv'
    fields=' chrom, chromStart, chromEnd, name, pubMedID, author, pubDate, journal, title, trait, initSample,   genes, riskAllele, riskAlFreq, pValue, pValueDesc, orOrBeta, ci95, platform, cnv'
    echo "select $fields from $i" |  hgsql $db | tail -n +2 | sort > $i.out
    f=$i"New"
    echo "select $fields from $f" |hgsql $db | tail -n +2 | sort > $f.out
    oldCount=`cat $i.out | wc -l`
    newCount=`cat $f.out | wc -l`
    common=`join -t '\001'  $i.out $f.out | wc -l`
    onlyOld=`join -t '\001' -v 1 $i.out $f.out | wc -l`
    onlyNew=`join -t '\001' -v 2 $i.out $f.out | wc -l`
    echo $i $newCount "-" $onlyNew "=" $common "=" $oldCount "-" $onlyOld  
    rm $i.out $f.out
done > newCatalog.stats

cat newCatalog.stats | awk -v db=$db -v tooMuch=$tooMuch ' 
{
    if (($4/$6 > tooMuch) || ($10/$6 > tooMuch))
	{
	print "validate on " db "." $1 " failed:" $4,$6,$4/$6,$10,$6,$10/$6;
	exit 1
	}
}'

exit 0
