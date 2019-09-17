#!/bin/sh -e

PATH=/cluster/bin/x86_64:/cluster/bin/scripts:$PATH
EMAIL="jcasper@soe.ucsc.edu,chmalee@ucsc.edu"
WORKDIR="/hive/data/outside/otto/omim"

cd $WORKDIR
./checkOmim.sh $WORKDIR 2>&1 |  mail -s "OMIM Build" $EMAIL
