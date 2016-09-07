#!/bin/sh -e
touch alpha.gbLoaded beta.gbLoaded; 
list=`sed '/^#/d' /cluster/data/genbank/etc/hgwbeta.dbs `
for db in $list
do 
    echo "examining $db"
    hgsqlTableDate $db gbLoaded alpha.gbLoaded || continue
    HGDB_CONF=/cluster/home/braney/.hg.conf.beta hgsqlTableDate $db gbLoaded beta.gbLoaded   || touch alpha.gbLoaded
    if test alpha.gbLoaded -nt beta.gbLoaded; 
    then 
        echo "Updating $db"
        ./updateDb.sh $db gbTmp /cluster/home/braney/.hg.conf.beta perAss.txt
    fi; 
done 
rm alpha.gbLoaded beta.gbLoaded; 
