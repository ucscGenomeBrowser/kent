#!/bin/sh -e
# ./updateMetaDev.sh hg38 hgFixed braNey1 ../metaTables.txt

if [ "$#" -ne 4 ]; then
    echo "Illegal number of parameters ($#) should be zero (4)"
    echo "usage: $0 sourceDatabase destinationDatabase temporaryDatabase tableList"
    exit 1
fi

srcDb=$1
destDb=$2
tmpDb=$3
tableList=$4

# make locks
if hgsql $srcDb -e  "create table genbankLock (p int)" 
then
    if hgsql $destDb -e  "create table genbankLock (p int)" 
    then
        if hgsql $tmpDb -e  "create table genbankLock (p int)" 
        then
            : # all locks successful
        else
            echo "can't create table genbankLock in $tmpDb"
            hgsql $srcDb -e  "drop table genbankLock"
            hgsql $destDb -e  "drop table genbankLock"
            exit 1
        fi
    else
        echo "can't create table genbankLock in $destDb"
        hgsql $srcDb -e  "drop table genbankLock"
        exit 1
    fi
else
    echo "can't create table genbankLock in $srcDb"
    exit 1
fi

# get the list of genbank tables in the source database
list=`hgsql $srcDb -e "show tables" | egrep -f $tableList`

# drop the tables in the temporary database 
for i in $list
do
echo "drop table if exists "$i | hgsql $tmpDb
done

# rsync contents of temporary database to temporary database
for i in $list
do  
    rsync -av --progress /var/lib/mysql/$srcDb/$i.* /var/lib/mysql/$tmpDb
done

# drop the tables from the destination database 
(echo -n "drop table if exists ";
for i in $list
do
echo -n $i","
done | sed 's/,$/;/') | hgsql $destDb

# mv the tables from the temporary database to the destination database
(echo -n "rename table ";
for i in $list
do
echo -n $i" to "$destDb.$i","
done | sed 's/,$/;/') | hgsql $tmpDb

# delete locks
hgsql $srcDb -e  "drop table genbankLock"
hgsql $destDb -e  "drop table genbankLock"
hgsql $tmpDb -e  "drop table genbankLock"
