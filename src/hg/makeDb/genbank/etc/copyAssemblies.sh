#!/bin/sh -e
touch alpha.gbLoaded build.gbLoaded; 
list=`sed '/^#/d' /cluster/data/genbank/etc/hgwdev.dbs `
for db in $list
do 
    echo "examining $db"
    hgsqlTableDate $db gbLoaded build.gbLoaded || continue
    set +e
    HGDB_CONF=.hg.conf.hgwdev hgsqlTableDate $db gbLoaded alpha.gbLoaded   || touch build.gbLoaded
    set -e
    if test build.gbLoaded -nt alpha.gbLoaded; 
    then 
        echo "Updating $db"
        ./updateDb.sh $db gbTmp /cluster/home/braney/.hg.conf.hgwdev perAss.txt
    fi; 
done 
rm alpha.gbLoaded build.gbLoaded; 
