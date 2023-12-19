#!/bin/sh -e

PATH=/cluster/bin/x86_64:$PATH
EMAIL="otto-group@ucsc.edu"
WORKDIR="/hive/data/outside/otto/dbVar"

cd $WORKDIR
./checkDbVar.sh $WORKDIR 2>&1 |  mail -s "dbVar Build" $EMAIL
