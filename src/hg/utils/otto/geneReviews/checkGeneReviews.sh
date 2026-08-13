#!/bin/sh -e

#	Do not modify this script, modify the source tree copy:
#	src/utils/geneReviews/checkGeneReviews.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/

#	cron jobs need to ensure this is true
#       current login requires the user be chinhli
umask 002

# Pin the locale so a hand-run from a login shell behaves like the cron run.
# See the note in buildGeneReviews.sh: the NCBI files are Latin-1 and a UTF-8
# locale makes grep drop lines, and sort and join order differently.
LC_ALL=C
export LC_ALL

WORKDIR=$1
export WORKDIR

# Emit an error line on any failure so the wrapper's "mail -E" sends an alert. The
# wget -q is silent and set -e (from the #!/bin/sh -e shebang) would otherwise abort
# with no output, which mail -E suppresses entirely. set -E (errtrace) makes the ERR
# trap fire for failures inside functions too. No-update runs stay silent.
set -E
trap 'echo "ERROR: GeneReviews build failed (exit $?)"' ERR

function installGeneReviewTables() {
for i in `cat ../geneReviews.tables`
    do
    n=$i"New"
    o=$i"Old"
    hgsqlSwapTables $1 $n $i $o -dropTable3
    done
    echo "GENEREVIEWS Installed `date` in $1"
}

function installGeneReviewsBigBed() {
# Point /gbdb at the bigBed built in the current directory.  buildGeneReviews.sh
# makes the file but does not move the link, so this runs only after validation
# passes and the tables are installed.
gbdb="/gbdb/$1/geneReviews"
mkdir -p $gbdb
rm -f $gbdb/geneReviews.bb
ln -s `pwd`/geneReviews.$1.bb $gbdb/geneReviews.bb
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

    # Validate all three assemblies before deciding, so one bad assembly does
    # not hide the state of the others.
    validateFailed=0
    for db in "hg38" "hg19" "hg18"
    do
        ../validateGeneReviews.sh $db || validateFailed=1
    done
    if [ $validateFailed -ne 0 ]; then
        echo "ERROR: GeneReviews validation failed, nothing installed"
        exit 1
    fi

    # now install
    installGeneReviewTables "hg38"
    installGeneReviewTables "hg19"
    installGeneReviewTables "hg18"
    installGeneReviewsBigBed "hg38"
    installGeneReviewsBigBed "hg19"
    installGeneReviewsBigBed "hg18"
    # now archive
    for db in "hg18" "hg19" "hg38"
    do
        if [ ! -d ${WORKDIR}/archive/${db} ]; then
            mkdir -p ${WORKDIR}/archive/${db}
        fi
        cd ${WORKDIR}/archive/${db}
        mkdir ${today}
        cd ${today}
        printf "This directory contains a backup of the geneReviews track built on %s" "${today}" > README 
        for i in `cat ${WORKDIR}/geneReviews.tables`
        do
            hgsql -Ne "show create table ${i}" ${db} > ${i}.sql
            hgsql -Ne "select * from ${i}" ${db} | gzip >  ${i}.txt.gz
        done
    done
    cd ${WORKDIR}/${today}

    rm -f ../lastUpdate
    cp -p NBKid_shortname_genesymbol.txt ../lastUpdate

fi

exit 0 
