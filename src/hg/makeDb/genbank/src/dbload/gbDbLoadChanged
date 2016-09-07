#!/bin/sh -e
cd /cluster/data/genbank; 
touch alpha.gbLoaded
export HGDB_CONF=`pwd`/etc/.hg.conf 
list=`sed '/^#/d' /cluster/data/genbank/etc/hgwdev.dbs `
for db in $list
do 
    if hgsql $db -e  "create table genbankLock (p int)" 
    then
        hgsqlTableDate $db gbLoaded alpha.gbLoaded; 
        aligned=`ls -dtr /cluster/data/genbank/data/aligned/*/$db | tail -1`
        if test $aligned -nt alpha.gbLoaded
        then 
            echo "Updating $db" 
                    #bin/gbDbLoadStep -allowLargeDeletes -noGrepIndex $db 
                    bin/gbDbLoadStep -noGrepIndex $db  
    #( rm /hive/data/outside/genbank/var/dbload/hgwdev/run/dbload.lock ; bin/gbDbLoadStep -noGrepIndex -drop -initialLoad $db  )
        fi
        hgsql $db -e  "drop table genbankLock"
    fi
done 
rm alpha.gbLoaded 
