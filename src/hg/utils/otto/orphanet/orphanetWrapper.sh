#!/bin/sh -e

PATH=/cluster/bin/x86_64:/cluster/bin/scripts:$PATH
EMAIL="lrnassar@ucsc.edu"
WORKDIR="/hive/data/outside/otto/orphanet"

cd $WORKDIR
./checkOrphanet.sh $WORKDIR 2>&1 |  mail -s "Orphanet Build" $EMAIL
