#!/bin/sh -e

PATH=/cluster/bin/x86_64:$PATH
EMAIL="kate@soe.ucsc.edu"
WORKDIR="/hive/data/outside/otto/geneReviews"

cd $WORKDIR
./testCheck.sh $WORKDIR 2>&1 |  mail -s "Test GENEREVIEW Build" $EMAIL
