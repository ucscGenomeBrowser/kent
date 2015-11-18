#!/bin/sh -e


db=$1
cd $db
tooMuch=0.1000   # how much change (either gain or loss) is too much

for i in `cat ../../isca.tables`
do 
    checkTableCoords $db -table=$i"New" || exit 1
    #/cluster/home/braney/bin/x86_64/checkTableCoords $db -table=$i"New" || exit 1
    positionalTblCheck $db $i"New" || exit 1
done 

for i in `cat ../../isca.tables`
do 
    if test $i == "iscaPathGainCum" -o $i == "iscaPathLossCum" \
         -o $i == "iscaBenignGainCum" -o $i == "iscaBenignLossCum"
    then
	fields='*'
    else
	fields="chrom,chromStart,chromEnd,name,score,strand,thickStart,thickEnd"
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
#    rm $i.out $f.out
done > newISCA.stats

cat newISCA.stats | awk -v db=$db -v tooMuch=$tooMuch ' 
{
    if (($4/$6 > tooMuch) || ($10/$6 > tooMuch))
	{
	print "validate on " db "." $1 " failed:" $4,$6,$4/$6,$10,$6,$10/$6;
	exit 1
	}
}'

exit 0
