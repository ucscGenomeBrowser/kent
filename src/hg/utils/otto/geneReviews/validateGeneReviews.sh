#!/bin/sh -e

# Pin the locale so sort and join agree, whether run by cron or by hand.
LC_ALL=C
export LC_ALL

db=$1
tooMuch=0.1000   # how much change (either gain or loss) is too much

# The comparison below is deliberately strict: for geneReviews a row counts as
# unchanged only if chrom, chromStart, chromEnd and name all match.  When the
# source data shifts wholesale, as it does after a knownGene rebuild, this is
# meant to fail and have someone look at the new data before it is installed.
# See #38098.  To let a reviewed change through, install it by hand rather than
# loosening the test.

for i in `cat ../geneReviews.tables`
do 
    if  test $i == "geneReviews"
    then
    fields='chrom, chromStart, chromEnd, name'
    else
        fields='*'
    fi

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
done > newGeneReviews$db.stats

cat newGeneReviews$db.stats | awk -v db=$db -v tooMuch=$tooMuch ' 
{
    if ($6 == 0)
	{
	print "validate on " db "." $1 " failed: no rows in common";
	exit 1
	}
    if (($4/$6 > tooMuch) || ($10/$6 > tooMuch))
	{
	print "validate on " db "." $1 " failed:" $4,$6,$4/$6,$10,$6,$10/$6;
	exit 1
	}
}'

exit 0
