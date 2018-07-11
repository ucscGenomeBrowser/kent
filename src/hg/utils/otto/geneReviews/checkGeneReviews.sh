#!/bin/sh -e

#	Do not modify this script, modify the source tree copy:
#	src/utils/geneReviews/checkGeneReviews.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/

#	cron jobs need to ensure this is true
#       current login requires the user be chinhli
umask 002

WORKDIR=$1
export WORKDIR

function installGeneReviewTables() {
for i in `cat ../geneReviews.tables`
    do
    n=$i"New"
    o=$i"Old"
    hgsqlSwapTables $1 $n $i $o -dropTable3
    done
    echo "GENEREVIEWS Installed `date` in $1"
}


#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in GENEREVIEWS release watch, Can not find the directory:
    ${WORKDIR}" 
    exit 255
fi

cd "${WORKDIR}"
wget -q --timestamping ftp://ftp.ncbi.nih.gov/pub/GeneReviews/*.txt
chmod 660 *.txt
if test NBKid_shortname_genesymbol.txt -nt lastUpdate
then
    today=`date +%F`
    mkdir -p $today
    mv *.txt $today

    cd $today

    # build the new GENEREVIEWS track tables
    ../buildGeneReviews.sh
    ../validateGeneReviews.sh hg38
    ../validateGeneReviews.sh hg19
    ../validateGeneReviews.sh hg18
    # now install
    installGeneReviewTables "hg38"  
    installGeneReviewTables "hg19"    
    installGeneReviewTables "hg18"

    rm -f ../lastUpdate
    cp -p NBKid_shortname_genesymbol.txt ../lastUpdate

fi

exit 0 
