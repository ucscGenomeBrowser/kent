#!/bin/sh -e

PATH=/cluster/bin/x86_64:$PATH
EMAIL="otto-group@ucsc.edu"
WORKDIR="/hive/data/outside/otto/decipher"

cd $WORKDIR
./checkDecipher.sh $WORKDIR 2>&1
