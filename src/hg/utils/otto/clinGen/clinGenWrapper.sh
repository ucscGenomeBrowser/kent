#!/bin/sh -e
PATH=/cluster/bin/x86_64:$PATH
EMAIL="chmalee@ucsc.edu,abenet@ucsc.edu"
WORKDIR="/hive/data/outside/otto/clinGen"

cd $WORKDIR
./checkClinGen.sh $WORKDIR 2>&1 |  mail -s "ClinGen Build" $EMAIL
