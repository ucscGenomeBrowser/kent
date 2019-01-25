# #!/bin/sh -e

set -e

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
        echo "select count(*) from $i" | HGDB_CONF=$betaCfg  hgsql $db > $i.out
        echo "select count(*) from $i" | HGDB_CONF=$betaCfg hgsql $tmpDb > $i.new.out
        echo $i `cat $i.out` `cat $i.new.out`
        rm $i.out $i.new.out
    fi
done | awk -v db=$db -v tooMuch=$tooMuch -v force=$force ' 
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
