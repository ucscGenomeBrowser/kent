#!/bin/sh -e
# Otto cron wrapper for the Gene2Phenotype (G2P) track.
# Runs the worker and mails its combined output to otto-group.
# Edit + commit the kent-tree copy, then copy to the hive otto dir:
#   ~/kent/src/hg/utils/otto/g2p/g2pWrapper.sh

PATH=/cluster/bin/x86_64:$PATH
export PATH

EMAIL="otto-group@ucsc.edu"
WORKDIR="/hive/data/outside/otto/g2p"

umask 002
cd "$WORKDIR"
# -E suppresses mail when there is no output (silent no-op when G2P is unchanged).
./doG2p.py 2>&1 | mail -E -s "G2P otto build" $EMAIL
