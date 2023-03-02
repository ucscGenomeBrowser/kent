#!/bin/sh
# configuration setup for the doUniprot script
cd /hive/data/outside/otto/uniprot
#echo WARNING: NOT DOWNLOADING
#./doUniprot run --skipDownload
# activate a virtual python environment with the lxml XML parser
source venv/bin/activate
umask 002 
echo uniprot start at `date`
./doUniprot run > lastRun.log 2>&1
echo uniprot end at `date`
exitCode=$?
if [ $exitCode -eq 0 ] ; then
    echo Big Uniprot update OK, exit code $exitCode
else
    echo Big Uniprot update failed. Look at /hive/data/outside/otto/uniprot/lastRun.log and restart manually with: cd /hive/data/outside/otto/uniprot followed by ./doUniprot run, usually with the -p option to skip download and parsing of the gigantic XML.
    echo Exit code was: $exitCode
fi
