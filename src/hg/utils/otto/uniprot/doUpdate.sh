#!/bin/sh
# configuration setup for the doUniprot script
cd /hive/data/outside/otto/uniprot
#echo WARNING: NOT DOWNLOADING
#./doUniprot run --skipDownload
./doUniprot run  2> lastRun.log
if [ $? == "0" ] ; then
    echo Big Uniprot update OK.
else
    echo Big Uniprot update failed. Look at /hive/data/outside/otto/uniprot/lastRun.log and restart manually.
fi
