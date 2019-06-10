#!/bin/sh -e
cd etc
touch alpha.gbLoaded build.gbLoaded; 
list=`sed '/^#/d' hgwdev.dbs `
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
        ./copyDbDev.sh $db gbTmp .hg.conf.hgwdev gbPerAssemblyTables.txt
    fi; 
done 
rm alpha.gbLoaded build.gbLoaded; 
