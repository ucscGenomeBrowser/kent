#!/bin/tcsh
# find tables with pileups
if ("$1" == "") then
    echo "must specify the database"
    exit 1
endif
if ("$2" == "") then
    echo "must specify the table"
    exit 1
endif
# assumes that there is a bin column that needs cutting out
#DEBUG IGNORE NAMES hgsql $1 -BN -e "select * from $2" | cut -f 2- | bedPileUps stdin > $1.$2.txt
hgsql $1 -BN -e "select * from $2" | cut -f 2- | bedPileUps -name stdin > $1.$2.txt
