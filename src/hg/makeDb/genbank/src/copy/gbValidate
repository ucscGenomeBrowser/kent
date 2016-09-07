#!/bin/sh -e

betaCfg=$1
db=$2
tmpDb=$3
tableList=$4
force=$5

tooMuch=0.10000   # how much change (either gain or loss) is too much

# get the list of genbank tables in the source database
list=`hgsql $db -e "show tables" | egrep -f $tableList`

for i in $list
do 
    fields='*'
    if  test $i != "gbLoaded"
    then
#    if  test $i == "decipherRaw"
#    then
#	fields='id,start,end,chr,mean_ratio,classification_type'
#	#fields='id,start,end,chr,mean_ratio'
#    else
#        fields='*'
#    fi

    echo "select count(*) from $i" | HGDB_CONF=$betaCfg  hgsql $db > $i.out
#    f=$i"New"
    echo "select count(*) from $i" | HGDB_CONF=$betaCfg hgsql $tmpDb > $i.new.out
    echo $i `cat $i.out` `cat $i.new.out`
    rm $i.out $i.new.out
    fi
done > change.stats

cat change.stats | awk -v db=$db -v tooMuch=$tooMuch -v force=$force ' 
function abs(v) {return v < 0 ? -v : v} 

{
if ($3 > 0)
    {
    diff= abs($5 - $3)/$3 ; 
    if (diff > tooMuch) 
        {
        print "validate on " db "." $1 " failed:" $3, $5, diff " is bigger than " tooMuch
        if ( force != "yes")
            exit 1
        else
            print "ignored"
        }
    }
}'

exit 0
