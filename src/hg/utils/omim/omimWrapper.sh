#!/bin/sh -e

EMAIL="braney@soe.ucsc.edu"
WORKDIR="/hive/data/outside/otto/omim"

./checkOmim.sh $WORKDIR 2>&1 |  mail -s "OMIM Build" $EMAIL
