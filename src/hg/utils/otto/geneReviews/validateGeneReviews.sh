#!/bin/sh -e

db=$1
tooMuch=0.1000   # how much change (either gain or loss) is too much

tab=`printf '\t'`

# Write one line per gene per chromosome: "gene|chrom", lowest start, highest end.
function geneSpans() {
hgsql -N $db -e \
    "select name, chrom, min(chromStart), max(chromEnd) from $1 group by name, chrom" \
    | awk -F'\t' '{OFS="\t"; print $1"|"$2, $3, $4}' | sort -t "$tab" -k1,1
}

for i in `cat ../geneReviews.tables`
do
    f=$i"New"
    if  test $i == "geneReviews"
    then
        # hg19 and hg18 take their coordinates from knownGene, so a knownGene
        # rebuild moves the ends of most genes by a few bases.  Comparing
        # coordinates exactly then counts nearly every gene as changed and the
        # run fails even though the data is fine.  Compare the overall span of
        # each gene on each chromosome instead, and call a gene unchanged when
        # its old and new spans overlap.
        geneSpans $i > $i.out
        geneSpans $f > $f.out
        oldCount=`cat $i.out | wc -l`
        newCount=`cat $f.out | wc -l`
        # join gives gene|chrom, oldStart, oldEnd, newStart, newEnd
        common=`join -t "$tab" $i.out $f.out | awk -F'\t' '$2 < $5 && $4 < $3' | wc -l`
        onlyOld=$((oldCount - common))
        onlyNew=$((newCount - common))
    else
        echo "select * from $i" |  hgsql $db | tail -n +2 | sort > $i.out
        echo "select * from $f" |hgsql $db | tail -n +2 | sort > $f.out
        oldCount=`cat $i.out | wc -l`
        newCount=`cat $f.out | wc -l`
        common=`join -t '\001'  $i.out $f.out | wc -l`
        onlyOld=`join -t '\001' -v 1 $i.out $f.out | wc -l`
        onlyNew=`join -t '\001' -v 2 $i.out $f.out | wc -l`
    fi
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
