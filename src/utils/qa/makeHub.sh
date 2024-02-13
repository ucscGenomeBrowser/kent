#!/usr/bin/env bash

DB=$1
SERVER=$2

PROGRAM_NAME=${0##*/} # Get the current file name for the usage statement

if [ "$DB" == "" ] || [ "$SERVER" == "" ] || (( "$#" > 3)) ; then
        echo "Usage:"
        echo "$ $PROGRAM_NAME db [alpha/beta/public]"
        echo ""
        echo "This program prints the required files to push curated"
        echo "hubs from dev --> beta --> RR."
        echo ""
        echo "This script is synonymous with running a 'make alpha'"
        echo "in /trackDb for a curtated hub, i.e. hs1"
        exit 1
fi

echo "Updating trackDb and friends for $DB"
echo ""

cd ~/kent/src/hg/makeDb/trackDb/
make $SERVER DBS=$DB
cd -
echo ""
echo "--------------------------------------------------------------------"
echo ""

for file in $(ls /gbdb/$DB/hubs/$SERVER/); do
        echo "/gbdb/$DB/hubs/$SERVER/$file"
done

if [ "$SERVER" == "beta" ] ; then
        echo ""
        echo "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"
        echo "When staging the assembly to hgwbeta, you will FIRST have to push"
        echo "the /gbdb files above from hgwdev --> hgwbeta."
        echo ""
        echo "Note: Please push /gbdb data files (i.e. bigBeds) BEFORE running this script."
fi

if [ "$SERVER" == "public" ] ; then
        echo ""
        echo "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"
        echo "When releasing the assembly to the RR, you will FIRST have to push"
        echo "the /gbdb files above from hgwdev --> hgwbeta --> RR/euro/asia."
        echo ""
        echo "Note: Please push /gbdb data files (i.e. bigBeds) BEFORE running this script."
fi
