#!/bin/sh -e

PATH=/cluster/bin/x86_64:$PATH
EMAIL="lrnassar@ucsc.edu,chmalee@ucsc.edu,kate@soe.ucsc.edu"
WORKDIR="/hive/data/outside/otto/geneReviews"

cd $WORKDIR
./checkGeneReviews.sh $WORKDIR 2>&1 |  mail -s "GENEREVIEW Build" $EMAIL
