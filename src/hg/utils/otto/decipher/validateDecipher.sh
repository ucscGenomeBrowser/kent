#!/bin/bash
set -beEu -o pipefail

db=hg38
tooMuch=0.1000

for i in `cat ../decipher.tables`
do
    fields='*'
    echo "select $fields from $i" |  hgsql $db | tail -n +2 | sort > $i.out
    f=$i"New"
    echo "select $fields from $f" |hgsql $db | tail -n +2 | sort > $f.out
    oldCount=`cat $i.out | wc -l`
    newCount=`cat $f.out | wc -l`
    common=`join -t '\001'  $i.out $f.out | wc -l`
    onlyOld=`join -t '\001' -v 1 $i.out $f.out | wc -l`
    onlyNew=`join -t '\001' -v 2 $i.out $f.out | wc -l`
    echo $i $newCount "-" $onlyNew "=" $common "=" $oldCount "-" $onlyOld
    rm "$i.out" "$f.out"
done > newDecipher.stats

# bigBed validation

oldBed=`mktemp decipherValidateOldXXX.bed`
bigBedToBed ../release/hg38/decipherCnv.bb $oldBed
oldCount=`cat $oldBed | wc -l`
newCount=`cat decipherCnv.bed | wc -l`
common=`comm -12 <( sort $oldBed) <(sort decipherCnv.bed) | wc -l`
onlyOld=`comm -23 <(sort $oldBed) <(sort decipherCnv.bed) | wc -l`
onlyNew=`comm -13 <(sort $oldBed) <(sort decipherCnv.bed) | wc -l`
echo "decipherCnv.bed" $newCount "-" $onlyNew "=" $common "=" $oldCount "-" $onlyOld >> newDecipher.stats
rm "$oldBed"

cat newDecipher.stats | awk -v db=$db -v tooMuch=$tooMuch '
{
    if (($4/$6 > tooMuch) || ($10/$6 > tooMuch))
        {
        print "validate on " db "." $1 " failed: only new " $4 "/" $6 "=" $4/$6 ", only old " $10 "/" $6 "=" $10/$6;
        exit 1
        }
}'

exit 0
