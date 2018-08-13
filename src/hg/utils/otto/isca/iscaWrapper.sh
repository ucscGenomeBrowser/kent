#!/bin/sh -e

PATH=/cluster/bin/x86_64:$PATH
EMAIL="jcasper@soe.ucsc.edu,chmalee@ucsc.edu"
WORKDIR="/hive/data/outside/otto/isca"

cd $WORKDIR
./checkISCA.sh $WORKDIR 2>&1 |  mail -s "ISCA Build" $EMAIL
