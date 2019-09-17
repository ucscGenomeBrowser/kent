#!/bin/sh
#
#	$Id: fetchHgCentral.sh,v 1.2 2010/03/31 18:18:31 hiram Exp $
#

if [ $# -ne 1 ]; then
    echo "usage: fetchCentral.sh go"
    echo "need to use the argument go in order to get this to run."
    echo "  This script simply runs a wget command to fetch the hgcentral.sql"
    echo "database definition from UCSC.  Output is to stdout, reroute the"
    echo "output to a file to save, e.g.:"
    echo "./fetchHgCentral.sh > ucsc.hgcentral.sql"
    exit 255
fi

if [ $1 = "go" ]; then
    wget --timestamping -O /dev/stdout \
	http://hgdownload.soe.ucsc.edu/admin/hgcentral.sql
else
    echo "ERROR: need to use the argument: go"
    echo "in order to get this to run"
fi
