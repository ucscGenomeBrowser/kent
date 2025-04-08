#!/bin/sh -e

PATH=/cluster/bin/x86_64:/cluster/bin/scripts:$PATH
EMAIL="otto-group@ucsc.edu chmalee@ucsc.edu"
WORKDIR="/hive/data/outside/otto/lrg"

cd $WORKDIR
./checkLrg.sh $WORKDIR 2>&1 |  mail -s "LRG Build" $EMAIL
