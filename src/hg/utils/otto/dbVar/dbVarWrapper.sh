#!/bin/sh -e

#	Do not modify this script, modify the source tree copy:
#	src/hg/utils/otto/dbVar/dbVarWrapper.sh
#	Install via 'make install' in that directory.

PATH=/cluster/bin/x86_64:$PATH
EMAIL="otto-group@ucsc.edu"
WORKDIR="/hive/data/outside/otto/dbVar"

cd $WORKDIR

# Capture checkDbVar.sh output. Only email when there's something to report
# (hub update detected, new files in hub, or error). No-op runs stay silent.
TMPLOG=$(mktemp --tmpdir=${WORKDIR} dbVarBuild.XXXXXX)
trap "rm -f $TMPLOG" EXIT

if ./checkDbVar.sh $WORKDIR > $TMPLOG 2>&1; then
    if [ -s $TMPLOG ]; then
        mail -s "dbVar Build" $EMAIL < $TMPLOG
    fi
else
    # The build errored out. The ERR trap inside checkDbVar.sh will have
    # echoed its failure message; email whatever output we captured.
    mail -s "dbVar Build FAILED" $EMAIL < $TMPLOG
    exit 1
fi
