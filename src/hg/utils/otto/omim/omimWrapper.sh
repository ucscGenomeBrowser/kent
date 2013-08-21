#!/bin/sh -e

PATH=/cluster/bin/x86_64:$PATH
EMAIL="braney@soe.ucsc.edu"
WORKDIR="/hive/data/outside/otto/omim"

cd $WORKDIR
./checkOmim.sh $WORKDIR 2>&1 |  mail -s "OMIM Build" $EMAIL
