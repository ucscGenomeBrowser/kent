#!/bin/sh -e

#	Do not modify this script, modify the source tree copy:
#	src/hg/utils/omim/checkOmim.sh

# set EMAIL here for notification list
#EMAIL="braney@soe.ucsc.edu"
# set DEBUG_EMAIL here for notification of potential errors in the process
#DEBUG_EMAIL="braney@soe.ucsc.edu"

#	cron jobs need to ensure this is true
umask 002

WORKDIR=$1
export WORKDIR
export PATH=$WORKDIR":$PATH"

#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    echo "ERROR in OMIM release watch, Can not find the directory:     ${WORKDIR}" 
    exit 255
fi

cd "${WORKDIR}"


######################
# Checking if there's a new release

today=`date +%F`
mkdir -p $today
cd $today

# fetch remote files
echo fetching files ...
wget -nv -i ../omimUrls.txt -o wget.log
if [ $? -ne 0 ]; then
    echo "Potential error in OMIM release fetch,
check wget.log in ${WORKDIR}/${today}"
    exit 255
fi

ls | grep -v wget.log | xargs -I % sh -c "echo -ne '%\t'; (grep -v '^#' % || true) | md5sum | awk '{print \$1}'" | sort > md5sum.txt

if [ ! -e ../prev.md5sum.txt ]
then
    touch ../prev.md5sum.txt
fi

diff md5sum.txt ../prev.md5sum.txt > release.diff || true

WC=`cat release.diff | wc -l`
if [ "${WC}" -gt 0 ]; then
#####################
# There's a new release!
    echo -e "New OMIM update noted at:\n" \
"http://omim.org/\n"`comm -13 ../prev.md5sum.txt md5sum.txt`"/" 

# build the new OMIM track tables for hg18, hg19, hg38
for db in hg18 hg19 hg38
do
  echo Running OMIM build for $db
  rm -rf $db
  mkdir -p $db
  cd $db

  ln -s ../genemap.txt ./genemap.txt
  ln -s ../genemap2.txt ./genemap2.txt
  ln -s ../allelicVariants.txt ./allelicVariants.txt
  ln -s ../mim2gene.txt ./mim2gene.txt
  ln -s ../../doOmimPhenotype.pl ./doOmimPhenotype.pl

  ../../buildOmimTracks.sh $db
  ../../flagOmimGene.py $db > omimGene2.prev.flagged
  ../../validateOmim.sh $db
  cd ..

  # install new tables
  for table in `cat ../omim.tables`
  do 
    new=$table"New"
    old=$table"Old"
    hgsqlSwapTables $db $new $table $old -dropTable3
  done
done


rm -f "${WORKDIR}"/prev.md5sum.txt
cp -p "${WORKDIR}/${today}"/md5sum.txt "${WORKDIR}"/prev.md5sum.txt
echo "Omim Installed `date`" 

else
    echo "No update `date` "
    cd "${WORKDIR}"
    rm -rf "${today}"
fi

