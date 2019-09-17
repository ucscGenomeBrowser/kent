#!/bin/sh -e

PATH=/cluster/bin/x86_64:$PATH
EMAIL="jcasper@soe.ucsc.edu,chmalee@ucsc.edu"
WORKDIR="/hive/data/outside/otto/gwas"

cd $WORKDIR
./checkGwas.sh $WORKDIR 2>&1 |  mail -s "GWAS Build" $EMAIL
