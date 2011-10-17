#!/bin/sh

# these settings will make this script exit on any error at any time
set -beEu -o pipefail

export scriptName="process"
export previousStep="download"
export TOP="/hive/data/outside/ncbi/incidentDb/testAuto"

. ${TOP}/commonHeader.inc

#############################################################################
xmlToBed5() {
export name=$1
export db=$2
export Id=$3
cd "${workDir}/${name}"

for F in `ls ${TOP}/${name}/*.xml | sed -e "s/.xml//"`
do
    C=`basename ${F}`
    C=${C/_issues/}
    ${ECHO} $F ${C}
    rm -fr ${C}
    mkdir ${C}
    /cluster/bin/x86_64/autoDtd ${F}.xml ${C}/${C}.dtd ${C}/${C}.stats
    /cluster/bin/x86_64/xmlToSql ${F}.xml ${C}/${C}.dtd ${C}/${C}.stats ${C}
done
    # the awk filters out bad coordinates in mm9 data
    /cluster/bin/scripts/ncbiIncidentDb.pl -${Id} chr[0-9XY]* 2> dbg.txt \
	| sort -k1,1 -k2,2n | awk '$3>=$2' > ${db}.bed5

    case "${db}" in
	mm9)
    # illegal coordinates have crept into the mm9 data:
    sed -e "s/132060741/131737871/; s/132085098/131738871/" ${db}.bed5 \
	> t$$.bed5
    /cluster/bin/x86_64/bedToBigBed -bedFields=4 \
	-as=$HOME/kent/src/hg/lib/ncbiIncidentDb.as \
	    t$$.bed5 /hive/data/genomes/${db}/chrom.sizes ncbiIncidentDb.bb
	    rm -f t$$.bed5
	    ;;
	*)
    /cluster/bin/x86_64/bedToBigBed -bedFields=4 \
	-as=$HOME/kent/src/hg/lib/ncbiIncidentDb.as \
	    ${db}.bed5 /hive/data/genomes/${db}/chrom.sizes ncbiIncidentDb.bb
	    ;;
    esac
}

#############################################################################
############  Get to work here

xmlToBed5 human hg19 GRCh37
xmlToBed5 mouse mm9 MGSCv37
xmlToBed5 zebrafish danRer7 Zv9

cd "${workDir}"
${ECHO} "# ${scriptName} complete "`date '+%Y-%m-%d %H:%M:%S'` > "${signalDone}"
rm -f "${signalRunning}"
exit 0
