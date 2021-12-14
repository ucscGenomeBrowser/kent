#!/bin/bash
set -e
set -o pipefail
umask 002
cd /hive/data/genomes/wuhCor1/bed/uniprot
mkdir -p archive
fname=archive/covid-19.`date -I`.xml
wget -q http://ftp.ebi.ac.uk/pub/databases/uniprot/pre_release/covid-19.xml -O $fname
/hive/data/outside/otto/uniprot/uniprotToTab -p $fname 2697049 tab/  && /hive/data/outside/otto/uniprot/doUniprot --dbs=wuhCor1 -p run --skipTrembl
echo UniProt Covid-19 pre-release `curl http://ftp.ebi.ac.uk/pub/databases/uniprot/pre_release/README | head -n1 | cut -d: -f2 ` > tab/version.txt
echo Release: `cat tab/version.txt`
echo UniProt wuhCor1 update: OK


