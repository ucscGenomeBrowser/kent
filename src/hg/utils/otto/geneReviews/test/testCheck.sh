#!/bin/sh -e

#	Do not modify this script, modify the source tree copy:
#	src/utils/geneReviews/checkGeneReviews.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/

#	cron jobs need to ensure this is true
#       current login requires the user be chinhli
umask 002

WORKDIR=$1
export WORKDIR

#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in GENEREVIEWS release watch, Can not find the directory:
    ${WORKDIR}" 
    exit 255
fi

cd "${WORKDIR}"
wget -q --timestamping ftp://ftp.ncbi.nih.gov/pub/GeneReviews/*.txt
chmod 660 *.txt

    today=`date +%F`
    mkdir -p $today
    mv *.txt $today

    cd $today

    # build the new GENEREVIEWS bigBed
    ../testBuildBb.sh
exit 0 
