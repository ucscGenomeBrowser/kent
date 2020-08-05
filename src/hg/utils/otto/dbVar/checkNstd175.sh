#!/bin/bash

#	Do not modify this script, modify the source tree copy:
#	src/hg/utils/dbVar/checkNstd175.sh

set -beEu -o pipefail
WORKDIR=$1
today=`date +%F`

cleanUpOnError () {
    echo "dbVar nstd175 build failed"
} 

trap cleanUpOnError ERR
trap "cleanUpOnError; exit 1" SIGINT SIGTERM
umask 002

mkdir -p ${WORKDIR}/${today}/giab
cd ${WORKDIR}/${today}/giab
rm -f ftp.giab.rsp
echo "user anonymous otto@soe.ucsc.edu
cd /pub/dbVar/data/Homo_sapiens/by_study/gvf
ls nstd175*
bye" > ftp.giab.rsp

#	reorganize results files
rm -f ls.check
rm -f old.release.list
if [ -e prev.release.list ]
then
    mv prev.release.list old.release.list
else
    touch release.list
fi
cp -p release.list prev.release.list
rm -f release.list

#	connect and list a directory, result to file: ls.check
ftp -n -v -i ftp.ncbi.nlm.nih.gov 2>&1 < ftp.giab.rsp &> ls.check

#	fetch the release directory names from the ls.check result file
grep "gvf.gz" ls.check | sort > release.list || echo "Error - no gvf files found"
touch release.list
chmod o+w release.list

#	verify we are getting a proper list
WC=`cat release.list | wc -l`
if [ "${WC}" -lt 1 ]; then
    echo "potential error in dbVar GIAB sv data, no gvf files found. Check ls.check in ${WORKDIR}/${today}/giab" 
    cleanUpOnError
    exit 255
fi

#	see if anything is changing, if so, email notify, download, and build
diff prev.release.list release.list > release.diff || true
WC=`cat release.diff | wc -l`
if [ "${WC}" -gt 1 ]; then
    echo -e "New dbVar GIAB Structural Variant update at:\n" \
        "ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/gvf/nstd175.*\n"
    
    for grc in GRCh38 GRCh37
    do
        db=""
        if [ ${grc} == "GRCh37" ]; then
            db="hg19"
        else
            db="hg38"
        fi
        mkdir -p ${db}
        pushd ${db} > /dev/null
        echo "processing nstd175 for ${db}"
        hgsql -Ne 'select 0, ca.alias, size, ca.chrom, size from chromInfo ci join chromAlias ca on ci.chrom = ca.chrom where source = "refseq"' ${db} > ${db}.lift
        wget -N -q "ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/gvf/nstd175.${grc}.variant*.gvf.gz"
        zcat nstd175.${grc}.* | ../../../processNstd175.py stdin ${db}.lift | sort -k1,1 -k2,2n | bedClip -truncate stdin /hive/data/genomes/${db}/chrom.sizes stdout > giabSv.bed
        bedToBigBed -type=bed9+11 -as=../../../giabSv.as -tab giabSv.bed /hive/data/genomes/${db}/chrom.sizes giabSv.bb
        cp giabSv.bb ${WORKDIR}/release/${db}/giabSv.bb
        popd > /dev/null
    done
fi
echo "dbVar nstd175 update done"
